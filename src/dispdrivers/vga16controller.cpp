/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */



#include <alloca.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "soc/rtc.h"
#include "esp_spi_flash.h"
#include "esp_heap_caps.h"

#include "fabutils.h"
#include "vga16controller.h"
#include "devdrivers/swgenerator.h"




#ifdef VGATextController_PERFORMANCE_CHECK
  volatile uint64_t s_cycles = 0;
#endif




namespace fabgl {



// high nibble is pixel 0, low nibble is pixel 1

static inline __attribute__((always_inline)) void VGA16_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

static inline __attribute__((always_inline)) int VGA16_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 1;
  return ((x & 1) ? (row[brow] & 0x0f) : ((row[brow] & 0xf0) >> 4));
}

#define VGA16_INVERTPIXELINROW(row, x)       (row)[(x) >> 1] ^= (0xf0 >> (((x) & 1) << 2))

static inline __attribute__((always_inline)) void VGA16_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) VGA16Controller::getScanline(y);
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

#define VGA16_GETPIXEL(x, y)                 VGA16_GETPIXELINROW((uint8_t*)VGA16Controller::s_viewPort[(y)], (x))

#define VGA16_INVERT_PIXEL(x, y)             VGA16_INVERTPIXELINROW((uint8_t*)VGA16Controller::s_viewPort[(y)], (x))




/*************************************************************************************/
/* VGA16Controller definitions */


VGA16Controller *    VGA16Controller::s_instance = nullptr;
volatile int         VGA16Controller::s_scanLine;
lldesc_t volatile *  VGA16Controller::s_frameResetDesc;
volatile uint8_t * * VGA16Controller::s_viewPort;
volatile uint8_t * * VGA16Controller::s_viewPortVisible;



VGA16Controller::VGA16Controller()
{
  s_instance = this;
}


void VGA16Controller::init(gpio_num_t VSyncGPIO)
{
  m_DMABuffers                   = nullptr;
  m_DMABuffersCount              = 0;
  m_VSyncGPIO                    = VSyncGPIO;
  m_primitiveProcessingSuspended = 1; // >0 suspended
  m_isr_handle                   = nullptr;
  m_taskProcessingPrimitives     = false;
  m_processPrimitivesOnBlank     = false;

  m_GPIOStream.begin();
}


// initializer for 8 colors configuration
void VGA16Controller::begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  init(VSyncGPIO);

  // GPIO configuration for bit 0
  setupGPIO(redGPIO,   VGA_RED_BIT,   GPIO_MODE_OUTPUT);
  setupGPIO(greenGPIO, VGA_GREEN_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(blueGPIO,  VGA_BLUE_BIT,  GPIO_MODE_OUTPUT);

  // GPIO configuration for VSync and HSync
  setupGPIO(HSyncGPIO, VGA_HSYNC_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(VSyncGPIO, VGA_VSYNC_BIT, GPIO_MODE_INPUT_OUTPUT);  // input/output so can be generated interrupt on falling/rising edge

  RGB222::lowBitOnly = true;
  m_bitsPerChannel = 1;
}


// initializer for 64 colors configuration
void VGA16Controller::begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  begin(red0GPIO, green0GPIO, blue0GPIO, HSyncGPIO, VSyncGPIO);

  // GPIO configuration for bit 1
  setupGPIO(red1GPIO,   VGA_RED_BIT + 1,   GPIO_MODE_OUTPUT);
  setupGPIO(green1GPIO, VGA_GREEN_BIT + 1, GPIO_MODE_OUTPUT);
  setupGPIO(blue1GPIO,  VGA_BLUE_BIT + 1,  GPIO_MODE_OUTPUT);

  RGB222::lowBitOnly = false;
  m_bitsPerChannel = 2;
}


// initializer for default configuration
void VGA16Controller::begin()
{
  begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
}


void VGA16Controller::end()
{
  if (m_DMABuffers) {
    if (m_isr_handle) {
      esp_intr_free(m_isr_handle);
      m_isr_handle = nullptr;
    }
    m_GPIOStream.stop();
    suspendBackgroundPrimitiveExecution();
    if (m_primitiveExecTask) {
      vTaskDelete(m_primitiveExecTask);
      m_primitiveExecTask = nullptr;
    }
    freeBuffers();
  }
}


void VGA16Controller::setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


// Suspend vertical sync interrupt
// Warning: After call to suspendBackgroundPrimitiveExecution() adding primitives may cause a deadlock.
// To avoid this a call to "processPrimitives()" should be performed very often.
// Can be nested
void VGA16Controller::suspendBackgroundPrimitiveExecution()
{
  ++m_primitiveProcessingSuspended;
  while (m_taskProcessingPrimitives)
    ;
}


// Resume vertical sync interrupt after suspendBackgroundPrimitiveExecution()
// Can be nested
void VGA16Controller::resumeBackgroundPrimitiveExecution()
{
  m_primitiveProcessingSuspended = tmax(0, m_primitiveProcessingSuspended - 1);
}


void VGA16Controller::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGATimings timings;
  if (VGAController::convertModelineToTimings(modeline, &timings))
    setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);
}


// this method may adjust m_viewPortHeight to the actual number of allocated rows.
// to reduce memory allocation overhead try to allocate the minimum number of blocks.
void VGA16Controller::allocateViewPort()
{
  int linesCount[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT]; // where store number of lines for each pool
  int poolsCount = 0; // number of allocated pools
  int remainingLines = m_viewPortHeight;
  m_viewPortHeight = 0; // m_viewPortHeight needs to be recalculated

  if (isDoubleBuffered())
    remainingLines *= 2;

  // allocate pools
  while (remainingLines > 0 && poolsCount < FABGLIB_VIEWPORT_MEMORY_POOL_COUNT) {
    int largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    linesCount[poolsCount] = tmin(remainingLines, largestBlock / m_viewPortWidth / 2);
    if (linesCount[poolsCount] == 0)  // no more memory available for lines
      break;
    m_viewPortMemoryPool[poolsCount] = (uint8_t*) heap_caps_malloc(linesCount[poolsCount] * m_viewPortWidth / 2, MALLOC_CAP_8BIT);  // divided by 2 because each byte contains two pixels
    remainingLines -= linesCount[poolsCount];
    m_viewPortHeight += linesCount[poolsCount];
    ++poolsCount;
  }
  m_viewPortMemoryPool[poolsCount] = nullptr;

  // fill m_viewPort[] with line pointers
  if (isDoubleBuffered()) {
    m_viewPortHeight /= 2;
    s_viewPortVisible = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT);
  }
  s_viewPort = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT);
  if (!isDoubleBuffered())
    s_viewPortVisible = s_viewPort;
  for (int p = 0, l = 0; p < poolsCount; ++p) {
    uint8_t * pool = m_viewPortMemoryPool[p];
    for (int i = 0; i < linesCount[p]; ++i) {
      if (l + i < m_viewPortHeight)
        s_viewPort[l + i] = pool;
      else
        s_viewPortVisible[l + i - m_viewPortHeight] = pool; // set only when double buffered is enabled
      pool += m_viewPortWidth / 2;
    }
    l += linesCount[p];
  }

  for (int i = 0; i < VGA16_LinesCount; ++i)
    m_lines[i] = (uint8_t*) heap_caps_malloc(m_viewPortWidth, MALLOC_CAP_DMA);
}


void VGA16Controller::freeViewPort()
{
  for (uint8_t * * poolPtr = m_viewPortMemoryPool; *poolPtr; ++poolPtr) {
    heap_caps_free((void*) *poolPtr);
    *poolPtr = nullptr;
  }
  heap_caps_free(s_viewPort);
  s_viewPort = nullptr;
  if (isDoubleBuffered())
    heap_caps_free(s_viewPortVisible);
  s_viewPortVisible = nullptr;
  for (int i = 0; i < VGA16_LinesCount; ++i) {
    heap_caps_free((void*)m_lines[i]);
    m_lines[i] = nullptr;
  }
}


void VGA16Controller::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  end();

  m_timings = timings;
  setDoubleBuffered(doubleBuffered);

  m_HVSync = packHVSync(false, false);

  m_HLineSize = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;

  m_HBlankLine_withVSync = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);
  m_HBlankLine           = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);

  m_viewPortWidth  = ~3 & (viewPortWidth <= 0 || viewPortWidth >= m_timings.HVisibleArea ? m_timings.HVisibleArea : viewPortWidth); // view port width must be 32 bit aligned
  m_viewPortHeight = viewPortHeight <= 0 || viewPortHeight >= m_timings.VVisibleArea ? m_timings.VVisibleArea : viewPortHeight;

  // need to center viewport?
  m_viewPortCol = (m_timings.HVisibleArea - m_viewPortWidth) / 2;
  m_viewPortRow = (m_timings.VVisibleArea - m_viewPortHeight) / 2;

  // view port col and row must be 32 bit aligned
  m_viewPortCol = m_viewPortCol & ~3;
  m_viewPortRow = m_viewPortRow & ~3;

  m_rawFrameHeight = m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch;

  // allocate DMA descriptors
  setDMABuffersCount(calcRequiredDMABuffersCount(m_viewPortHeight));

  // allocate the viewport
  allocateViewPort();

  // this may free space if m_viewPortHeight has been reduced
  setDMABuffersCount(calcRequiredDMABuffersCount(m_viewPortHeight));

  // fill buffers
  fillVertBuffers(0);
  fillHorizBuffers(0);

  // fill view port
  for (int i = 0; i < m_viewPortHeight; ++i)
    memset((void*)(s_viewPort[i]), 0, m_viewPortWidth / 2);

  resetPaintState();

  setupDefaultPalette();

  calculateAvailableCyclesForDrawings();

  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);
  resumeBackgroundPrimitiveExecution();

  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  if (m_isr_handle == nullptr) {
    CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, I2SInterrupt, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }

  if (m_primitiveExecTask == nullptr) {
    xTaskCreatePinnedToCore(primitiveExecTask, "" , 1024, this, 5, &m_primitiveExecTask, CoreUsage::quietCore());
  }
}


// calculates number of CPU cycles usable to draw primitives
void VGA16Controller::calculateAvailableCyclesForDrawings()
{
  int availtime_us;

  if (m_processPrimitivesOnBlank) {
    // allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (VGA16_LinesCount / 2 + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch + m_viewPortRow));
  } else {
    // allowed time is the half of an entire frame. Fast, but may flick
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch));
    availtime_us /= 2;
  }

  m_primitiveExecTimeoutCycles = F_CPU / 1000000 * availtime_us;  // at 250Mhz, there are 250 cycles every microsecond
}


void VGA16Controller::freeBuffers()
{
  if (m_DMABuffersCount > 0) {
    heap_caps_free((void*)m_HBlankLine_withVSync);
    heap_caps_free((void*)m_HBlankLine);

    freeViewPort();

    setDMABuffersCount(0);
  }
}


int VGA16Controller::calcRequiredDMABuffersCount(int viewPortHeight)
{
  int rightPadSize = m_timings.HVisibleArea - m_viewPortWidth - m_viewPortCol;
  int buffersCount = m_timings.scanCount * (m_rawFrameHeight + viewPortHeight);

  switch (m_timings.HStartingBlock) {
    case VGAScanStart::FrontPorch:
      // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
      buffersCount += m_timings.scanCount * (rightPadSize > 0 ? viewPortHeight : 0);
      break;
    case VGAScanStart::Sync:
      // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
      buffersCount += m_timings.scanCount * viewPortHeight;
      break;
    case VGAScanStart::BackPorch:
      // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
      buffersCount += m_timings.scanCount * viewPortHeight;
      break;
    case VGAScanStart::VisibleArea:
      // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
      buffersCount += m_timings.scanCount * (m_viewPortCol > 0 ? viewPortHeight : 0);
      break;
  }

  return buffersCount;
}


void VGA16Controller::fillVertBuffers(int offsetY)
{
  int16_t porchSum = m_timings.VFrontPorch + m_timings.VBackPorch;
  m_timings.VFrontPorch = tmax(1, (int16_t)m_timings.VFrontPorch - offsetY);
  m_timings.VBackPorch  = tmax(1, porchSum - m_timings.VFrontPorch);
  m_timings.VFrontPorch = porchSum - m_timings.VBackPorch;

  // associate buffers pointer to DMA info buffers
  //
  //  Vertical order:
  //    VisibleArea
  //    Front Porch
  //    Sync
  //    Back Porch

  int VVisibleArea_pos = 0;
  int VFrontPorch_pos  = VVisibleArea_pos + m_timings.VVisibleArea;
  int VSync_pos        = VFrontPorch_pos  + m_timings.VFrontPorch;
  int VBackPorch_pos   = VSync_pos        + m_timings.VSyncPulse;

  int rightPadSize = m_timings.HVisibleArea - m_viewPortWidth - m_viewPortCol;

  for (int line = 0, DMABufIdx = 0; line < m_rawFrameHeight; ++line) {

    bool isVVisibleArea = (line < VFrontPorch_pos);
    bool isVFrontPorch  = (line >= VFrontPorch_pos && line < VSync_pos);
    bool isVSync        = (line >= VSync_pos && line < VBackPorch_pos);
    bool isVBackPorch   = (line >= VBackPorch_pos);

    for (int scan = 0; scan < m_timings.scanCount; ++scan) {

      if (isVSync) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine_withVSync, m_HLineSize);

      } else if (isVFrontPorch || isVBackPorch) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize);

      } else if (isVVisibleArea) {

        int visibleAreaLine = line - VVisibleArea_pos;
        bool isViewport = visibleAreaLine >= m_viewPortRow && visibleAreaLine < m_viewPortRow + m_viewPortHeight;
        int HInvisibleAreaSize = m_HLineSize - m_timings.HVisibleArea;

        if (isViewport) {
          // visible, this is the viewport
          switch (m_timings.HStartingBlock) {
            case VGAScanStart::FrontPorch:
              // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, HInvisibleAreaSize + m_viewPortCol);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan);
              if (rightPadSize > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine + HInvisibleAreaSize, rightPadSize);
              break;
            case VGAScanStart::Sync:
              // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HSyncPulse + m_timings.HBackPorch + m_viewPortCol);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - rightPadSize, m_timings.HFrontPorch + rightPadSize);
              break;
            case VGAScanStart::BackPorch:
              // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HBackPorch + m_viewPortCol);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - m_timings.HSyncPulse - rightPadSize, m_timings.HFrontPorch + m_timings.HSyncPulse + rightPadSize);
              break;
            case VGAScanStart::VisibleArea:
              // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
              if (m_viewPortCol > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_viewPortCol);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_timings.HVisibleArea - rightPadSize, HInvisibleAreaSize + rightPadSize);
              break;
          }

        } else {
          // not visible
          setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize);
        }

      }

    }

  }
}


// refill buffers changing Front Porch and Back Porch
// offsetX : (< 0 : left  > 0 right)
void VGA16Controller::fillHorizBuffers(int offsetX)
{
  // fill all with no hsync

  fill(m_HBlankLine,           0, m_HLineSize, 0, 0, 0, false, false);
  fill(m_HBlankLine_withVSync, 0, m_HLineSize, 0, 0, 0, false,  true);

  // calculate hsync pos and fill it

  int16_t porchSum = m_timings.HFrontPorch + m_timings.HBackPorch;
  m_timings.HFrontPorch = tmax(8, (int16_t)m_timings.HFrontPorch - offsetX);
  m_timings.HBackPorch  = tmax(8, porchSum - m_timings.HFrontPorch);
  m_timings.HFrontPorch = porchSum - m_timings.HBackPorch;

  int syncPos = 0;

  switch (m_timings.HStartingBlock) {
    case VGAScanStart::FrontPorch:
      // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
      syncPos = m_timings.HFrontPorch;
      break;
    case VGAScanStart::Sync:
      // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
      syncPos = 0;
      break;
    case VGAScanStart::BackPorch:
      // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
      syncPos = m_timings.HBackPorch + m_timings.HVisibleArea + m_timings.HFrontPorch;
      break;
    case VGAScanStart::VisibleArea:
      // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
      syncPos = m_timings.HVisibleArea + m_timings.HFrontPorch;
      break;
  }

  fill(m_HBlankLine,           syncPos, m_timings.HSyncPulse, 0, 0, 0, true,  false);
  fill(m_HBlankLine_withVSync, syncPos, m_timings.HSyncPulse, 0, 0, 0, true,   true);
}


void VGA16Controller::moveScreen(int offsetX, int offsetY)
{
  suspendBackgroundPrimitiveExecution();
  fillVertBuffers(offsetY);
  fillHorizBuffers(offsetX);
  resumeBackgroundPrimitiveExecution();
}


void VGA16Controller::shrinkScreen(int shrinkX, int shrinkY)
{
  VGATimings * currTimings = getResolutionTimings();

  currTimings->HBackPorch  = tmax(currTimings->HBackPorch + 4 * shrinkX, 4);
  currTimings->HFrontPorch = tmax(currTimings->HFrontPorch + 4 * shrinkX, 4);

  currTimings->VBackPorch  = tmax(currTimings->VBackPorch + shrinkY, 1);
  currTimings->VFrontPorch = tmax(currTimings->VFrontPorch + shrinkY, 1);

  setResolution(*currTimings, m_viewPortWidth, m_viewPortHeight, isDoubleBuffered());
}


// Can be used to change buffers count, maintainig already set pointers.
bool VGA16Controller::setDMABuffersCount(int buffersCount)
{
  if (buffersCount == 0) {
    heap_caps_free( (void*) m_DMABuffers );
    m_DMABuffers = nullptr;
    m_DMABuffersCount = 0;
    return true;
  }

  if (buffersCount != m_DMABuffersCount) {

    // (re)allocate and initialize DMA descs
    m_DMABuffers = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffers, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (!m_DMABuffers)
      return false;

    for (int i = 0; i < buffersCount; ++i) {
      m_DMABuffers[i].eof          = 0;
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? &m_DMABuffers[0] : &m_DMABuffers[i + 1]);
    }

    m_DMABuffersCount = buffersCount;
  }

  return true;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGA16Controller::setDMABufferBlank(int index, void volatile * address, int length)
{
  int size = (length + 3) & (~3);
  m_DMABuffers[index].eof    = 0;
  m_DMABuffers[index].size   = size;
  m_DMABuffers[index].length = length;
  m_DMABuffers[index].buf    = (uint8_t*) address;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGA16Controller::setDMABufferView(int index, int row, int scan)
{
  uint8_t * bufferPtr;
  if (scan > 0 && m_timings.multiScanBlack == 1 && m_timings.HStartingBlock == FrontPorch)
    bufferPtr = (uint8_t *) (m_HBlankLine + m_HLineSize - m_timings.HVisibleArea);  // this works only when HSYNC, FrontPorch and BackPorch are at the beginning of m_HBlankLine
  else
    bufferPtr = (uint8_t *) m_lines[row % VGA16_LinesCount];
  bool intr = (scan == 0 && (row % (VGA16_LinesCount / 2)) == 0); // generate interrupt every half VGA16_LinesCount
  if (intr && row == 0)
    s_frameResetDesc = &m_DMABuffers[index];
  m_DMABuffers[index].size   = (m_viewPortWidth + 3) & (~3);
  m_DMABuffers[index].length = m_viewPortWidth;
  m_DMABuffers[index].buf    = bufferPtr;
  m_DMABuffers[index].eof    = intr ? 1 : 0;
}


void volatile * VGA16Controller::getDMABuffer(int index, int * length)
{
  *length = m_DMABuffers[index].length;
  return m_DMABuffers[index].buf;
}


uint8_t VGA16Controller::packHVSync(bool HSync, bool VSync)
{
  uint8_t hsync_value = (m_timings.HSyncLogic == '+' ? (HSync ? 1 : 0) : (HSync ? 0 : 1));
  uint8_t vsync_value = (m_timings.VSyncLogic == '+' ? (VSync ? 1 : 0) : (VSync ? 0 : 1));
  return (vsync_value << VGA_VSYNC_BIT) | (hsync_value << VGA_HSYNC_BIT);
}


uint8_t VGA16Controller::preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync)
{
  return packHVSync(HSync, VSync) | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT);
}


// buffer: buffer to fill (buffer size must be 32 bit aligned)
// startPos: initial position (in pixels)
// length: number of pixels to fill
//
// Returns next pos to fill (startPos + length)
int VGA16Controller::fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool HSync, bool VSync)
{
  uint8_t pattern = preparePixelWithSync((RGB222){red, green, blue}, HSync, VSync);
  for (int i = 0; i < length; ++i, ++startPos)
    VGA_PIXELINROW(buffer, startPos) = pattern;
  return startPos;
}


void VGA16Controller::setupDefaultPalette()
{
  for (int colorIndex = 0; colorIndex < 16; ++colorIndex) {
    RGB888 rgb888((Color)colorIndex);
    setPaletteItem(colorIndex, rgb888);
  }
  updateRGB2PaletteLUT();
}


//   0        => 0
//   1 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
static uint8_t RGB888toPackedRGB222(RGB888 const & rgb)
{
  static const int CONVR[4] = { 1 << 0,    // 00XXXXXX (0..63)
                                1 << 0,    // 01XXXXXX (64..127)
                                2 << 0,    // 10XXXXXX (128..191)
                                3 << 0, }; // 11XXXXXX (192..255)
  static const int CONVG[4] = { 1 << 2,    // 00XXXXXX (0..63)
                                1 << 2,    // 01XXXXXX (64..127)
                                2 << 2,    // 10XXXXXX (128..191)
                                3 << 2, }; // 11XXXXXX (192..255)
  static const int CONVB[4] = { 1 << 4,    // 00XXXXXX (0..63)
                                1 << 4,    // 01XXXXXX (64..127)
                                2 << 4,    // 10XXXXXX (128..191)
                                3 << 4, }; // 11XXXXXX (192..255)
  return (rgb.R ? CONVR[rgb.R >> 6] : 0) | (rgb.G ? CONVG[rgb.G >> 6] : 0) | (rgb.B ? CONVB[rgb.B >> 6] : 0);
}


void VGA16Controller::setPaletteItem(int index, RGB888 const & color)
{
  m_palette[index] = color;
  auto packed222 = RGB888toPackedRGB222(color);
  for (int i = 0; i < 16; ++i) {
    m_packedPaletteIndexPair_to_signals[(index << 4) | i] &= 0xFF00;
    m_packedPaletteIndexPair_to_signals[(index << 4) | i] |= (m_HVSync | packed222);
    m_packedPaletteIndexPair_to_signals[(i << 4) | index] &= 0x00FF;
    m_packedPaletteIndexPair_to_signals[(i << 4) | index] |= (m_HVSync | packed222) << 8;
  }
}


// rebuild m_packedRGB222_to_PaletteIndex
void VGA16Controller::updateRGB2PaletteLUT()
{
  for (int r = 0; r < 4; ++r)
    for (int g = 0; g < 4; ++g)
      for (int b = 0; b < 4; ++b) {
        int bestIdx = 0;
        int bestDst = 1000;
        for (int i = 0; i < 16; ++i) {
          int dst = abs(r - m_palette[i].R) + abs(g - m_palette[i].G) + abs(b - m_palette[i].B);
          if (dst < bestDst) {
            bestIdx = i;
            bestDst = dst;
            if (bestDst == 0)
              break;
          }
        }
        m_packedRGB222_to_PaletteIndex[r | (g << 2) | (b << 4)] = bestIdx;
      }
}


uint8_t VGA16Controller::RGB888toPaletteIndex(RGB888 const & rgb)
{
  return m_packedRGB222_to_PaletteIndex[RGB888toPackedRGB222(rgb)];
}

uint8_t VGA16Controller::RGB2222toPaletteIndex(uint8_t value)
{
  return m_packedRGB222_to_PaletteIndex[value & 0b00111111];
}


uint8_t VGA16Controller::RGB8888toPaletteIndex(RGBA8888 value)
{
  return RGB888toPaletteIndex(RGB888(value.R, value.G, value.B));
}


void VGA16Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color)             { return RGB888toPaletteIndex(color); },
                    VGA16_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void VGA16Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     VGA16_SETPIXEL,
                     [&] (int X, int Y)                              { VGA16_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void VGA16Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void VGA16Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) s_viewPort[y];
  // fill first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 16 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 2), colorIndex | (colorIndex << 4), sz / 2);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
}


// parameters not checked
void VGA16Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = s_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA16_INVERTPIXELINROW(row, x);
}


void VGA16Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) s_viewPort[srcY];
  auto dstRow = (uint8_t*) s_viewPort[dstY];
  // copy first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
  // copy whole 16 bits words (4 pixels)
  auto src = (uint16_t*)(srcRow + x / 2);
  auto dst = (uint16_t*)(dstRow + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
}


void VGA16Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) s_viewPort[yA];
  auto rowB = (uint8_t*) s_viewPort[yB];
  // swap first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 16 bits words (4 pixels)
  auto a = (uint16_t*)(rowA + x / 2);
  auto b = (uint16_t*)(rowB + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
}


void VGA16Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)               { return RGB888toPaletteIndex(color); },
                     VGA16_SETPIXEL
                    );
}


void VGA16Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern = paletteIndex | (paletteIndex << 4);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) s_viewPort[y], pattern, m_viewPortWidth / 2);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void VGA16Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(s_viewPort[yA], s_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


void VGA16Controller::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t  back4 = RGB888toPaletteIndex(getActualBrushColor());

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 3) == 0 && (width & 3) == 0);  // 4 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (s_viewPort[y]) + X1 / 2;
        for (int s = -scroll; s > 0;) {
          if (s > 1) {
            // scroll left by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row, row + sc / 2, (sz - sc) / 2);
            rawFillRow(y, X2 + 1 + scroll, X2, back4);
            s -= sz;
          } else if (s & 1) {
            // scroll left 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> 12 34 56 7P
            // word (little endian CPU) : 2301 6745  ->  3412 7P56
            auto prev = back4;
            auto w = (uint16_t *) (row + width / 2) - 1;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w-- = (p4 << 4 & 0xf000) | (prev << 8 & 0x0f00) | (p4 << 4 & 0x00f0) | (p4 >> 12 & 0x000f);
              prev = p4 >> 4 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) s_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back4);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (s_viewPort[y]) + X1 / 2;
        for (int s = scroll; s > 0;) {
          if (s > 1) {
            // scroll right by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row + sc / 2, row, (sz - sc) / 2);
            rawFillRow(y, X1, X1 + scroll - 1, back4);
            s -= sz;
          } else if (s & 1) {
            // scroll right 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> P0 12 34 56 7...
            // word (little endian CPU) : 2301 6745  ->  12P0 5634 ...
            auto prev = back4;
            auto w = (uint16_t *) row;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w++ = (p4 << 12 & 0xf000) | (p4 >> 4 & 0x0f00) | (prev << 4) | (p4 >> 4 & 0x000f);
              prev = p4 >> 8 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) s_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA16_SETPIXELINROW(row, x + scroll, VGA16_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back4);
      }
    }

  }
}


void VGA16Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) s_viewPort[y]; },
                   VGA16_SETPIXELINROW
                  );
}


void VGA16Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void VGA16Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) s_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void VGA16Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) s_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}


// no bounds check is done!
void VGA16Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) s_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA16_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void VGA16Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) s_viewPort[y]; },  // rawGetRow
                              VGA16_SETPIXELINROW
                             );
}


void VGA16Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                { return (uint8_t*) s_viewPort[y]; },                   // rawGetRow
                            VGA16_GETPIXELINROW,
                            [&] (uint8_t * row, int x) { VGA16_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void VGA16Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) s_viewPort[y]; },       // rawGetRow
                                VGA16_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { VGA16_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void VGA16Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) s_viewPort[y]; },   // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA_PIXELINROW(row, x); },     // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA16_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void VGA16Controller::swapBuffers()
{
  tswap(s_viewPort, s_viewPortVisible);
}


void IRAM_ATTR VGA16Controller::I2SInterrupt(void * arg)
{
  #ifdef VGATextController_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA16Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto desc = (volatile lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width = ctrl->m_viewPortWidth;
    auto const packedPaletteIndexPair_to_signals = (uint16_t const *) ctrl->m_packedPaletteIndexPair_to_signals;
    int scanLine = (s_scanLine + VGA16_LinesCount / 2) % ctrl->m_viewPortHeight;

    auto lineIndex = scanLine % VGA16_LinesCount;

    for (int i = 0; i < VGA16_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) VGA16Controller::s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) ctrl->m_lines[lineIndex];

      // optimizazion warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {
        *(dest + 1) = packedPaletteIndexPair_to_signals[*src++];
        *(dest    ) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 3) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 2) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 5) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 4) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 7) = packedPaletteIndexPair_to_signals[*src++];
        *(dest + 6) = packedPaletteIndexPair_to_signals[*src++];
        dest += 8;
      }

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA16_LinesCount / 2;

    if (scanLine >= ctrl->m_viewPortHeight && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled()) {
      // vertical sync, unlock primitive execution task
      // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
      vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
    }

  }

  #ifdef VGATextController_PERFORMANCE_CHECK
  s_cycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}


// we can use getCycleCount here because primitiveExecTask is pinned to a specific core (so cycle counter is the same)
// getCycleCount() requires 0.07us, while esp_timer_get_time() requires 0.78us
void VGA16Controller::primitiveExecTask(void * arg)
{
  auto ctrl = (VGA16Controller *) arg;

  while (true) {
    if (!ctrl->m_primitiveProcessingSuspended) {
      auto startCycle = ctrl->backgroundPrimitiveTimeoutEnabled() ? getCycleCount() : 0;
      Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
      ctrl->m_taskProcessingPrimitives = true;
      do {
        Primitive prim;
        if (ctrl->getPrimitive(&prim, 0) == false)
          break;
        ctrl->execPrimitive(prim, updateRect, false);
        if (ctrl->m_primitiveProcessingSuspended)
          break;
      } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startCycle + ctrl->m_primitiveExecTimeoutCycles > getCycleCount()));
      ctrl->showSprites(updateRect);
      ctrl->m_taskProcessingPrimitives = false;
    }

    // wait for vertical sync
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }

}



} // end of namespace
