/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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
#include "soc/rtc.h"

#include "fabutils.h"
#include "devdrivers/swgenerator.h"
#include "dispdrivers/vgabasecontroller.h"


#pragma GCC optimize ("O2")


namespace fabgl {


#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  volatile uint64_t s_vgapalctrlcycles = 0;
#endif



VGABaseController::VGABaseController()
{
}


void VGABaseController::init()
{
  m_DMABuffers                   = nullptr;
  m_DMABuffersCount              = 0;
  m_DMABuffersHead               = nullptr;
  m_DMABuffersVisible            = nullptr;
  m_primitiveProcessingSuspended = 1; // >0 suspended
  m_isr_handle                   = nullptr;
  m_doubleBufferOverDMA          = false;

  m_GPIOStream.begin();
}


// initializer for 8 colors configuration
void VGABaseController::begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  init();

  // GPIO configuration for bit 0
  setupGPIO(redGPIO,   VGA_RED_BIT,   GPIO_MODE_OUTPUT);
  setupGPIO(greenGPIO, VGA_GREEN_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(blueGPIO,  VGA_BLUE_BIT,  GPIO_MODE_OUTPUT);

  // GPIO configuration for VSync and HSync
  setupGPIO(HSyncGPIO, VGA_HSYNC_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(VSyncGPIO, VGA_VSYNC_BIT, GPIO_MODE_OUTPUT);

  RGB222::lowBitOnly = true;
  m_bitsPerChannel = 1;
}


// initializer for 64 colors configuration
void VGABaseController::begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
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
void VGABaseController::begin()
{
  begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
}


void VGABaseController::end()
{
  if (m_DMABuffers) {
    suspendBackgroundPrimitiveExecution();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    m_GPIOStream.stop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if (m_isr_handle) {
      esp_intr_free(m_isr_handle);
      m_isr_handle = nullptr;
    }
    freeBuffers();
  }
}


void VGABaseController::setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  configureGPIO(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


void VGABaseController::freeBuffers()
{
  if (m_DMABuffersCount > 0) {
    heap_caps_free((void*)m_HBlankLine_withVSync);
    heap_caps_free((void*)m_HBlankLine);

    freeViewPort();

    setDMABuffersCount(0);
  }
}


void VGABaseController::freeViewPort()
{
  for (uint8_t * * poolPtr = m_viewPortMemoryPool; *poolPtr; ++poolPtr) {
    heap_caps_free((void*) *poolPtr);
    *poolPtr = nullptr;
  }
  heap_caps_free(m_viewPort);
  m_viewPort = nullptr;
  if (isDoubleBuffered())
    heap_caps_free(m_viewPortVisible);
  m_viewPortVisible = nullptr;
}


// Can be used to change buffers count, maintainig already set pointers.
// If m_doubleBufferOverDMA = true, uses m_DMABuffersHead and m_DMABuffersVisible to implement
// double buffer on DMA level.
bool VGABaseController::setDMABuffersCount(int buffersCount)
{
  if (buffersCount == 0) {
    if (m_DMABuffersVisible && m_DMABuffersVisible != m_DMABuffers)
      heap_caps_free( (void*) m_DMABuffersVisible );
    heap_caps_free( (void*) m_DMABuffers );
    m_DMABuffers = nullptr;
    m_DMABuffersVisible = nullptr;
    m_DMABuffersCount = 0;
    return true;
  }

  if (buffersCount != m_DMABuffersCount) {

    // buffers head
    if (m_doubleBufferOverDMA && m_DMABuffersHead == nullptr) {
      m_DMABuffersHead = (lldesc_t*) heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
      m_DMABuffersHead->eof    = m_DMABuffersHead->sosf = m_DMABuffersHead->offset = 0;
      m_DMABuffersHead->owner  = 1;
      m_DMABuffersHead->size   = 0;
      m_DMABuffersHead->length = 0;
      m_DMABuffersHead->buf    = m_HBlankLine;   // dummy valid address. Setting nullptr crashes DMA!
      m_DMABuffersHead->qe.stqe_next = nullptr;  // this will be set before the first frame
    }

    // (re)allocate and initialize DMA descs
    m_DMABuffers = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffers, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (m_doubleBufferOverDMA && isDoubleBuffered())
      m_DMABuffersVisible = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffersVisible, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    else
      m_DMABuffersVisible = m_DMABuffers;
    if (!m_DMABuffers || !m_DMABuffersVisible)
      return false;

    auto buffersHead = m_DMABuffersHead ? m_DMABuffersHead : &m_DMABuffers[0];

    for (int i = 0; i < buffersCount; ++i) {
      m_DMABuffers[i].eof          = 0;
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? buffersHead : &m_DMABuffers[i + 1]);
      if (m_doubleBufferOverDMA && isDoubleBuffered()) {
        m_DMABuffersVisible[i].eof          = 0;
        m_DMABuffersVisible[i].sosf         = 0;
        m_DMABuffersVisible[i].offset       = 0;
        m_DMABuffersVisible[i].owner        = 1;
        m_DMABuffersVisible[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? buffersHead : &m_DMABuffersVisible[i + 1]);
      }
    }

    m_DMABuffersCount = buffersCount;
  }

  return true;
}


// modeline syntax:
//   "label" clock_mhz hdisp hsyncstart hsyncend htotal vdisp vsyncstart vsyncend vtotal (+HSync | -HSync) (+VSync | -VSync) [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
bool VGABaseController::convertModelineToTimings(char const * modeline, VGATimings * timings)
{
  float freq;
  int hdisp, hsyncstart, hsyncend, htotal, vdisp, vsyncstart, vsyncend, vtotal;
  char HSyncPol = 0, VSyncPol = 0;
  int pos = 0;

  int count = sscanf(modeline, "\"%[^\"]\" %g %d %d %d %d %d %d %d %d %n", timings->label, &freq, &hdisp, &hsyncstart, &hsyncend, &htotal, &vdisp, &vsyncstart, &vsyncend, &vtotal, &pos);

  if (count == 10 && pos > 0) {

    timings->frequency      = freq * 1000000;
    timings->HVisibleArea   = hdisp;
    timings->HFrontPorch    = hsyncstart - hdisp;
    timings->HSyncPulse     = hsyncend - hsyncstart;
    timings->HBackPorch     = htotal - hsyncend;
    timings->VVisibleArea   = vdisp;
    timings->VFrontPorch    = vsyncstart - vdisp;
    timings->VSyncPulse     = vsyncend - vsyncstart;
    timings->VBackPorch     = vtotal - vsyncend;
    timings->HSyncLogic     = '-';
    timings->VSyncLogic     = '-';
    timings->scanCount      = 1;
    timings->multiScanBlack = 0;
    timings->HStartingBlock = VGAScanStart::FrontPorch;

    // get (+HSync | -HSync) (+VSync | -VSync)
    char const * pc = modeline + pos;
    for (; *pc; ++pc) {
      if (*pc == '+' || *pc == '-') {
        if (!HSyncPol)
          timings->HSyncLogic = HSyncPol = *pc;
        else if (!VSyncPol) {
          timings->VSyncLogic = VSyncPol = *pc;
          while (*pc && *pc != ' ')
            ++pc;
          break;
        }
      }
    }

    // get [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
    // actually this gets only the first character
    while (*pc) {
      switch (*pc) {
        case 'D':
        case 'd':
          timings->scanCount = 2;
          break;
        case 'Q':
        case 'q':
          timings->scanCount = 4;
          break;
        case 'F':
        case 'f':
          timings->HStartingBlock = VGAScanStart::FrontPorch;
          break;
        case 'S':
        case 's':
          timings->HStartingBlock = VGAScanStart::Sync;
          break;
        case 'B':
        case 'b':
          timings->HStartingBlock = VGAScanStart::BackPorch;
          break;
        case 'V':
        case 'v':
          timings->HStartingBlock = VGAScanStart::VisibleArea;
          break;
        case 'M':
        case 'm':
          timings->multiScanBlack = 1;
          break;
        case ' ':
          ++pc;
          continue;
      }
      ++pc;
      while (*pc && *pc != ' ')
        ++pc;
    }

    return true;

  }
  return false;
}


// Suspend vertical sync interrupt
// Warning: After call to suspendBackgroundPrimitiveExecution() adding primitives may cause a deadlock.
// To avoid this a call to "processPrimitives()" should be performed very often.
// Can be nested
void VGABaseController::suspendBackgroundPrimitiveExecution()
{
  ++m_primitiveProcessingSuspended;
}


// Resume vertical sync interrupt after suspendBackgroundPrimitiveExecution()
// Can be nested
void VGABaseController::resumeBackgroundPrimitiveExecution()
{
  m_primitiveProcessingSuspended = tmax(0, m_primitiveProcessingSuspended - 1);
}


void VGABaseController::startGPIOStream()
{
  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);
}


void VGABaseController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGATimings timings;
  if (convertModelineToTimings(modeline, &timings))
    setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);
}


void VGABaseController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  // just in case setResolution() was called before
  end();

  m_timings = timings;

  // inform base class about screen size
  setScreenSize(m_timings.HVisibleArea, m_timings.VVisibleArea);

  setDoubleBuffered(doubleBuffered);

  m_HVSync = packHVSync(false, false);

  m_HLineSize = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;

  m_HBlankLine_withVSync = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);
  m_HBlankLine           = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);

  m_viewPortWidth  = ~3 & (viewPortWidth <= 0 || viewPortWidth >= m_timings.HVisibleArea ? m_timings.HVisibleArea : viewPortWidth); // view port width must be 32 bit aligned
  m_viewPortHeight = viewPortHeight <= 0 || viewPortHeight >= m_timings.VVisibleArea ? m_timings.VVisibleArea : viewPortHeight;

  // adjust view port size if necessary
  checkViewPortSize();

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

  // adjust again view port size if necessary
  checkViewPortSize();

  // this may free space if m_viewPortHeight has been reduced
  setDMABuffersCount(calcRequiredDMABuffersCount(m_viewPortHeight));

  // fill buffers
  fillVertBuffers(0);
  fillHorizBuffers(0);

  resetPaintState();

  if (m_doubleBufferOverDMA)
    m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];
}


// this method may adjust m_viewPortHeight to the actual number of allocated rows.
// to reduce memory allocation overhead try to allocate the minimum number of blocks.
void VGABaseController::allocateViewPort(uint32_t allocCaps, int rowlen)
{
  int linesCount[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT]; // where store number of lines for each pool
  int poolsCount = 0; // number of allocated pools
  int remainingLines = m_viewPortHeight;
  m_viewPortHeight = 0; // m_viewPortHeight needs to be recalculated

  if (isDoubleBuffered())
    remainingLines *= 2;

  // allocate pools
  while (remainingLines > 0 && poolsCount < FABGLIB_VIEWPORT_MEMORY_POOL_COUNT) {
    int largestBlock = heap_caps_get_largest_free_block(allocCaps);
    linesCount[poolsCount] = tmin(remainingLines, largestBlock / rowlen);
    if (linesCount[poolsCount] == 0)  // no more memory available for lines
      break;
    m_viewPortMemoryPool[poolsCount] = (uint8_t*) heap_caps_malloc(linesCount[poolsCount] * rowlen, allocCaps);
    remainingLines -= linesCount[poolsCount];
    m_viewPortHeight += linesCount[poolsCount];
    ++poolsCount;
  }
  m_viewPortMemoryPool[poolsCount] = nullptr;

  // fill m_viewPort[] with line pointers
  if (isDoubleBuffered()) {
    m_viewPortHeight /= 2;
    m_viewPortVisible = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  }
  m_viewPort = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  if (!isDoubleBuffered())
    m_viewPortVisible = m_viewPort;
  for (int p = 0, l = 0; p < poolsCount; ++p) {
    uint8_t * pool = m_viewPortMemoryPool[p];
    for (int i = 0; i < linesCount[p]; ++i) {
      if (l + i < m_viewPortHeight)
        m_viewPort[l + i] = pool;
      else
        m_viewPortVisible[l + i - m_viewPortHeight] = pool; // set only when double buffered is enabled
      pool += rowlen;
    }
    l += linesCount[p];
  }
}


uint8_t IRAM_ATTR VGABaseController::packHVSync(bool HSync, bool VSync)
{
  uint8_t hsync_value = (m_timings.HSyncLogic == '+' ? (HSync ? 1 : 0) : (HSync ? 0 : 1));
  uint8_t vsync_value = (m_timings.VSyncLogic == '+' ? (VSync ? 1 : 0) : (VSync ? 0 : 1));
  return (vsync_value << VGA_VSYNC_BIT) | (hsync_value << VGA_HSYNC_BIT);
}


uint8_t IRAM_ATTR VGABaseController::preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync)
{
  return packHVSync(HSync, VSync) | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT);
}


int VGABaseController::calcRequiredDMABuffersCount(int viewPortHeight)
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


// refill buffers changing Front Porch and Back Porch
// offsetX : (< 0 : left  > 0 right)
void VGABaseController::fillHorizBuffers(int offsetX)
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


void VGABaseController::fillVertBuffers(int offsetY)
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

      bool isStartOfVertFrontPorch = (line == VFrontPorch_pos && scan == 0);

      if (isVSync) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine_withVSync, m_HLineSize, scan, isStartOfVertFrontPorch);

      } else if (isVFrontPorch || isVBackPorch) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize, scan, isStartOfVertFrontPorch);

      } else if (isVVisibleArea) {

        int visibleAreaLine = line - VVisibleArea_pos;
        bool isViewport = visibleAreaLine >= m_viewPortRow && visibleAreaLine < m_viewPortRow + m_viewPortHeight;
        int HInvisibleAreaSize = m_HLineSize - m_timings.HVisibleArea;

        if (isViewport) {
          // visible, this is the viewport
          switch (m_timings.HStartingBlock) {
            case VGAScanStart::FrontPorch:
              // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, HInvisibleAreaSize + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              if (rightPadSize > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine + HInvisibleAreaSize, rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::Sync:
              // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HSyncPulse + m_timings.HBackPorch + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - rightPadSize, m_timings.HFrontPorch + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::BackPorch:
              // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HBackPorch + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - m_timings.HSyncPulse - rightPadSize, m_timings.HFrontPorch + m_timings.HSyncPulse + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::VisibleArea:
              // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
              if (m_viewPortCol > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_timings.HVisibleArea - rightPadSize, HInvisibleAreaSize + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
          }

        } else {
          // not visible
          setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize, scan, isStartOfVertFrontPorch);
        }

      }

    }

  }
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGABaseController::setDMABufferBlank(int index, void volatile * address, int length, int scan, bool isStartOfVertFrontPorch)
{
  int size = (length + 3) & (~3);
  m_DMABuffers[index].eof    = 0;
  m_DMABuffers[index].size   = size;
  m_DMABuffers[index].length = length;
  m_DMABuffers[index].buf    = (uint8_t*) address;
  onSetupDMABuffer(&m_DMABuffers[index], isStartOfVertFrontPorch, scan, false, 0);
  if (m_doubleBufferOverDMA && isDoubleBuffered()) {
    m_DMABuffersVisible[index].eof    = 0;
    m_DMABuffersVisible[index].size   = size;
    m_DMABuffersVisible[index].length = length;
    m_DMABuffersVisible[index].buf    = (uint8_t*) address;
    onSetupDMABuffer(&m_DMABuffersVisible[index], isStartOfVertFrontPorch, scan, false, 0);
  }
}


bool VGABaseController::isMultiScanBlackLine(int scan)
{
  return (scan > 0 && m_timings.multiScanBlack == 1 && m_timings.HStartingBlock == FrontPorch);
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGABaseController::setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA)
{
  uint8_t * bufferPtr = nullptr;
  if (isMultiScanBlackLine(scan))
    bufferPtr = (uint8_t *) (m_HBlankLine + m_HLineSize - m_timings.HVisibleArea);  // this works only when HSYNC, FrontPorch and BackPorch are at the beginning of m_HBlankLine
  else if (viewPort)
    bufferPtr = (uint8_t *) viewPort[row];
  lldesc_t volatile * DMABuffers = onVisibleDMA ? m_DMABuffersVisible : m_DMABuffers;
  DMABuffers[index].size   = (m_viewPortWidth + 3) & (~3);
  DMABuffers[index].length = m_viewPortWidth;
  DMABuffers[index].buf    = bufferPtr;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGABaseController::setDMABufferView(int index, int row, int scan, bool isStartOfVertFrontPorch)
{
  setDMABufferView(index, row, scan, m_viewPort, false);
  if (!isMultiScanBlackLine(scan))
    onSetupDMABuffer(&m_DMABuffers[index], isStartOfVertFrontPorch, scan, true, row);
  if (isDoubleBuffered()) {
    setDMABufferView(index, row, scan, m_viewPortVisible, true);
    if (!isMultiScanBlackLine(scan))
      onSetupDMABuffer(&m_DMABuffersVisible[index], isStartOfVertFrontPorch, scan, true, row);
  }
}


void volatile * VGABaseController::getDMABuffer(int index, int * length)
{
  *length = m_DMABuffers[index].length;
  return m_DMABuffers[index].buf;
}


// buffer: buffer to fill (buffer size must be 32 bit aligned)
// startPos: initial position (in pixels)
// length: number of pixels to fill
//
// Returns next pos to fill (startPos + length)
int VGABaseController::fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool HSync, bool VSync)
{
  uint8_t pattern = preparePixelWithSync((RGB222){red, green, blue}, HSync, VSync);
  for (int i = 0; i < length; ++i, ++startPos)
    VGA_PIXELINROW(buffer, startPos) = pattern;
  return startPos;
}


void VGABaseController::moveScreen(int offsetX, int offsetY)
{
  suspendBackgroundPrimitiveExecution();
  fillVertBuffers(offsetY);
  fillHorizBuffers(offsetX);
  resumeBackgroundPrimitiveExecution();
}


void VGABaseController::shrinkScreen(int shrinkX, int shrinkY)
{
  VGATimings * currTimings = getResolutionTimings();

  currTimings->HBackPorch  = tmax(currTimings->HBackPorch + 4 * shrinkX, 4);
  currTimings->HFrontPorch = tmax(currTimings->HFrontPorch + 4 * shrinkX, 4);

  currTimings->VBackPorch  = tmax(currTimings->VBackPorch + shrinkY, 1);
  currTimings->VFrontPorch = tmax(currTimings->VFrontPorch + shrinkY, 1);

  setResolution(*currTimings, m_viewPortWidth, m_viewPortHeight, isDoubleBuffered());
}


void IRAM_ATTR VGABaseController::swapBuffers()
{
  tswap(m_viewPort, m_viewPortVisible);
  if (m_doubleBufferOverDMA) {
    tswap(m_DMABuffers, m_DMABuffersVisible);
    m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];
  }
}




} // end of namespace


