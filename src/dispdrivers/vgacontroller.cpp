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



#include "Arduino.h"

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

#include "fabutils.h"
#include "vgacontroller.h"
#include "devdrivers/swgenerator.h"







namespace fabgl {





/*************************************************************************************/
/* VGAController definitions */


VGAController * VGAController::s_instance = nullptr;


VGAController::VGAController()
{
  s_instance = this;
}


void VGAController::init(gpio_num_t VSyncGPIO)
{
  m_DMABuffersHead          = nullptr;
  m_DMABuffers              = nullptr;
  m_DMABuffersVisible       = nullptr;
  m_DMABuffersCount         = 0;
  m_VSyncGPIO               = VSyncGPIO;
  m_VSyncInterruptSuspended = 1; // >0 suspended

  m_GPIOStream.begin();
}


// initializer for 8 colors configuration
void VGAController::begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
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
void VGAController::begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
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
void VGAController::begin()
{
  begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
}


void VGAController::setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


// Suspend vertical sync interrupt
// Warning: After call to suspendBackgroundPrimitiveExecution() adding primitives may cause a deadlock.
// To avoid this a call to "processPrimitives()" should be performed very often.
// Can be nested
void VGAController::suspendBackgroundPrimitiveExecution()
{
  ++m_VSyncInterruptSuspended;
  if (m_VSyncInterruptSuspended == 1)
    detachInterrupt(digitalPinToInterrupt(m_VSyncGPIO));
}


// Resume vertical sync interrupt after suspendBackgroundPrimitiveExecution()
// Can be nested
void VGAController::resumeBackgroundPrimitiveExecution()
{
  m_VSyncInterruptSuspended = tmax(0, m_VSyncInterruptSuspended - 1);
  if (m_VSyncInterruptSuspended == 0)
    attachInterrupt(digitalPinToInterrupt(m_VSyncGPIO), VSyncInterrupt, m_timings.VSyncLogic == '-' ? FALLING : RISING);
}


// modeline syntax:
//   "label" clock_mhz hdisp hsyncstart hsyncend htotal vdisp vsyncstart vsyncend vtotal (+HSync | -HSync) (+VSync | -VSync) [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
bool VGAController::convertModelineToTimings(char const * modeline, VGATimings * timings)
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


void VGAController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGATimings timings;
  if (convertModelineToTimings(modeline, &timings))
    setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);
}


// this method may adjust m_viewPortHeight to the actual number of allocated rows.
// to reduce memory allocation overhead try to allocate the minimum number of blocks.
void VGAController::allocateViewPort()
{
  int linesCount[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT]; // where store number of lines for each pool
  int poolsCount = 0; // number of allocated pools
  int remainingLines = m_viewPortHeight;
  m_viewPortHeight = 0; // m_viewPortHeight needs to be recalculated

  if (isDoubleBuffered())
    remainingLines *= 2;

  // allocate pools
  while (remainingLines > 0 && poolsCount < FABGLIB_VIEWPORT_MEMORY_POOL_COUNT) {
    int largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    linesCount[poolsCount] = tmin(remainingLines, largestBlock / m_viewPortWidth);
    if (linesCount[poolsCount] == 0)  // no more memory available for lines
      break;
    m_viewPortMemoryPool[poolsCount] = (uint8_t*) heap_caps_malloc(linesCount[poolsCount] * m_viewPortWidth, MALLOC_CAP_DMA);
    remainingLines -= linesCount[poolsCount];
    m_viewPortHeight += linesCount[poolsCount];
    ++poolsCount;
  }
  m_viewPortMemoryPool[poolsCount] = nullptr;

  // fill m_viewPort[] with line pointers
  if (isDoubleBuffered()) {
    m_viewPortHeight /= 2;
    m_viewPortVisible = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT);
  }
  m_viewPort = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT);
  for (int p = 0, l = 0; p < poolsCount; ++p) {
    uint8_t * pool = m_viewPortMemoryPool[p];
    for (int i = 0; i < linesCount[p]; ++i) {
      if (l + i < m_viewPortHeight)
        m_viewPort[l + i] = pool;
      else
        m_viewPortVisible[l + i - m_viewPortHeight] = pool; // set only when double buffered is enabled
      pool += m_viewPortWidth;
    }
    l += linesCount[p];
  }
}


void VGAController::freeViewPort()
{
  for (uint8_t * * poolPtr = m_viewPortMemoryPool; *poolPtr; ++poolPtr) {
    heap_caps_free((void*) *poolPtr);
    *poolPtr = nullptr;
  }
  heap_caps_free(m_viewPort);
  if (isDoubleBuffered())
    heap_caps_free(m_viewPortVisible);
}


void VGAController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  if (m_DMABuffers) {
    suspendBackgroundPrimitiveExecution();
    m_GPIOStream.stop();
    freeBuffers();
  }

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

  m_linesCount = m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch;

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
    fill(m_viewPort[i], 0, m_viewPortWidth, 0, 0, 0, false, false);

  m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];

  resetPaintState();

  // number of microseconds usable in VSynch ISR
  m_maxVSyncISRTime = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_timings.VSyncPulse + m_timings.VBackPorch + m_viewPortRow));

  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);
  resumeBackgroundPrimitiveExecution();
}


void VGAController::freeBuffers()
{
  if (m_DMABuffersCount > 0) {
    heap_caps_free((void*)m_HBlankLine_withVSync);
    heap_caps_free((void*)m_HBlankLine);

    freeViewPort();

    setDMABuffersCount(0);
  }
}


int VGAController::calcRequiredDMABuffersCount(int viewPortHeight)
{
  int rightPadSize = m_timings.HVisibleArea - m_viewPortWidth - m_viewPortCol;
  int buffersCount = m_timings.scanCount * (m_linesCount + viewPortHeight);

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


void VGAController::fillVertBuffers(int offsetY)
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

  for (int line = 0, DMABufIdx = 0; line < m_linesCount; ++line) {

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
void VGAController::fillHorizBuffers(int offsetX)
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


void VGAController::moveScreen(int offsetX, int offsetY)
{
  suspendBackgroundPrimitiveExecution();
  fillVertBuffers(offsetY);
  fillHorizBuffers(offsetX);
  resumeBackgroundPrimitiveExecution();
}


void VGAController::shrinkScreen(int shrinkX, int shrinkY)
{
  VGATimings * currTimings = getResolutionTimings();

  currTimings->HBackPorch  = tmax(currTimings->HBackPorch + 4 * shrinkX, 4);
  currTimings->HFrontPorch = tmax(currTimings->HFrontPorch + 4 * shrinkX, 4);

  currTimings->VBackPorch  = tmax(currTimings->VBackPorch + shrinkY, 1);
  currTimings->VFrontPorch = tmax(currTimings->VFrontPorch + shrinkY, 1);

  setResolution(*currTimings, m_viewPortWidth, m_viewPortHeight, isDoubleBuffered());
}


// Can be used to change buffers count, maintainig already set pointers.
bool VGAController::setDMABuffersCount(int buffersCount)
{
  if (buffersCount == 0) {
    heap_caps_free( (void*) m_DMABuffers );
    if (isDoubleBuffered())
      heap_caps_free( (void*) m_DMABuffersVisible );
    m_DMABuffers = nullptr;
    m_DMABuffersVisible = nullptr;
    m_DMABuffersCount = 0;
    return true;
  }

  if (buffersCount != m_DMABuffersCount) {

    // buffers head
    if (m_DMABuffersHead == nullptr) {
      m_DMABuffersHead = (lldesc_t*) heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
      m_DMABuffersHead->eof    = m_DMABuffersHead->sosf = m_DMABuffersHead->offset = 0;
      m_DMABuffersHead->owner  = 1;
      m_DMABuffersHead->size   = 0;
      m_DMABuffersHead->length = 0;
      m_DMABuffersHead->buf    = m_HBlankLine;  // dummy valid address. Setting nullptr crashes DMA!
      m_DMABuffersHead->qe.stqe_next = nullptr;  // this will be set before the first frame
    }

    // (re)allocate and initialize DMA descs
    m_DMABuffers = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffers, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (isDoubleBuffered())
      m_DMABuffersVisible = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffersVisible, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    else
      m_DMABuffersVisible = m_DMABuffers;
    if (!m_DMABuffers || !m_DMABuffersVisible)
      return false;

    for (int i = 0; i < buffersCount; ++i) {
      m_DMABuffers[i].eof          = m_DMABuffers[i].sosf = m_DMABuffers[i].offset = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? m_DMABuffersHead : &m_DMABuffers[i + 1]);
      if (isDoubleBuffered()) {
        m_DMABuffersVisible[i].eof          = m_DMABuffersVisible[i].sosf = m_DMABuffersVisible[i].offset = 0;
        m_DMABuffersVisible[i].owner        = 1;
        m_DMABuffersVisible[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? m_DMABuffersHead : &m_DMABuffersVisible[i + 1]);
      }
    }

    m_DMABuffersCount = buffersCount;
  }

  return true;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGAController::setDMABufferBlank(int index, void volatile * address, int length)
{
  int size = (length + 3) & (~3);
  m_DMABuffers[index].size   = size;
  m_DMABuffers[index].length = length;
  m_DMABuffers[index].buf    = (uint8_t*) address;
  if (isDoubleBuffered()) {
    m_DMABuffersVisible[index].size   = size;
    m_DMABuffersVisible[index].length = length;
    m_DMABuffersVisible[index].buf    = (uint8_t*) address;
  }
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGAController::setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA)
{
  uint8_t * bufferPtr;
  if (scan > 0 && m_timings.multiScanBlack == 1 && m_timings.HStartingBlock == FrontPorch)
    bufferPtr = (uint8_t *) (m_HBlankLine + m_HLineSize - m_timings.HVisibleArea);  // this works only when HSYNC, FrontPorch and BackPorch are at the beginning of m_HBlankLine
  else
    bufferPtr = (uint8_t *) viewPort[row];
  lldesc_t volatile * DMABuffers = onVisibleDMA ? m_DMABuffersVisible : m_DMABuffers;
  DMABuffers[index].size   = (m_viewPortWidth + 3) & (~3);
  DMABuffers[index].length = m_viewPortWidth;
  DMABuffers[index].buf    = bufferPtr;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VGAController::setDMABufferView(int index, int row, int scan)
{
  setDMABufferView(index, row, scan, m_viewPort, false);
  if (isDoubleBuffered())
    setDMABufferView(index, row, scan, m_viewPortVisible, true);
}


void volatile * VGAController::getDMABuffer(int index, int * length)
{
  *length = m_DMABuffers[index].length;
  return m_DMABuffers[index].buf;
}


uint8_t IRAM_ATTR VGAController::packHVSync(bool HSync, bool VSync)
{
  uint8_t hsync_value = (m_timings.HSyncLogic == '+' ? (HSync ? 1 : 0) : (HSync ? 0 : 1));
  uint8_t vsync_value = (m_timings.VSyncLogic == '+' ? (VSync ? 1 : 0) : (VSync ? 0 : 1));
  return (vsync_value << VGA_VSYNC_BIT) | (hsync_value << VGA_HSYNC_BIT);
}


uint8_t IRAM_ATTR VGAController::preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync)
{
  return packHVSync(HSync, VSync) | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT);
}


// buffer: buffer to fill (buffer size must be 32 bit aligned)
// startPos: initial position (in pixels)
// length: number of pixels to fill
//
// Returns next pos to fill (startPos + length)
int VGAController::fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool HSync, bool VSync)
{
  uint8_t pattern = preparePixelWithSync((RGB222){red, green, blue}, HSync, VSync);
  for (int i = 0; i < length; ++i, ++startPos)
    VGA_PIXELINROW(buffer, startPos) = pattern;
  return startPos;
}


void IRAM_ATTR VGAController::VSyncInterrupt()
{
  auto VGACtrl = VGAController::instance();
  int64_t startTime = VGACtrl->backgroundPrimitiveTimeoutEnabled() ? esp_timer_get_time() : 0;
  Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
  do {
    Primitive prim;
    if (VGACtrl->getPrimitiveISR(&prim) == false)
      break;

    VGACtrl->execPrimitive(prim, updateRect, true);

    if (VGACtrl->m_VSyncInterruptSuspended)
      break;

  } while (!VGACtrl->backgroundPrimitiveTimeoutEnabled() || (startTime + VGACtrl->m_maxVSyncISRTime > esp_timer_get_time()));
  VGACtrl->showSprites(updateRect);
}


void IRAM_ATTR VGAController::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color)          { return preparePixel(color); },
                    [&] (int X, int Y, uint8_t pattern) { VGA_PIXEL(X, Y) = pattern; }
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void IRAM_ATTR VGAController::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                   { return preparePixel(color); },
                     [&] (int Y, int X1, int X2, uint8_t pattern) { rawFillRow(Y, X1, X2, pattern); },
                     [&] (int Y, int X1, int X2)                  { rawInvertRow(Y, X1, X2); },
                     [&] (int X, int Y, uint8_t pattern)          { VGA_PIXEL(X, Y) = pattern; },
                     [&] (int X, int Y)                           { VGA_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void IRAM_ATTR VGAController::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, preparePixel(color));
}


// parameters not checked
void IRAM_ATTR VGAController::rawFillRow(int y, int x1, int x2, uint8_t pattern)
{
  auto row = m_viewPort[y];
  // fill first bytes before full 32 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA_PIXELINROW(row, x) = pattern;
  }
  // fill whole 32 bits words (don't care about VGA_PIXELINROW adjusted alignment)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x), pattern, sz);
    x += sz;
  }
  // fill last unaligned bytes
  for (; x <= x2; ++x) {
    VGA_PIXELINROW(row, x) = pattern;
  }
}


// parameters not checked
void IRAM_ATTR VGAController::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x) {
    uint8_t * px = (uint8_t*) &VGA_PIXELINROW(row, x);
    *px = m_HVSync | ~(*px);
  }
}


// swaps all pixels inside the range x1...x2 of yA and yB
// parameters not checked
void IRAM_ATTR VGAController::swapRows(int yA, int yB, int x1, int x2)
{
  uint8_t * rowA = (uint8_t*) m_viewPort[yA];
  uint8_t * rowB = (uint8_t*) m_viewPort[yB];
  // swap first bytes before full 32 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x)
    tswap(VGA_PIXELINROW(rowA, x), VGA_PIXELINROW(rowB, x));
  // swap whole 32 bits words (don't care about VGA_PIXELINROW adjusted alignment)
  uint32_t * a = (uint32_t*)(rowA + x);
  uint32_t * b = (uint32_t*)(rowB + x);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned bytes
  for (x = (x2 & ~3); x <= x2; ++x)
    tswap(VGA_PIXELINROW(rowA, x), VGA_PIXELINROW(rowB, x));
}


void IRAM_ATTR VGAController::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)          { return preparePixel(color); },
                     [&] (int X, int Y, uint8_t pattern) { VGA_PIXEL(X, Y) = pattern; }
                    );
}


void IRAM_ATTR VGAController::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t pattern = preparePixel(getActualBrushColor());
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern, m_viewPortWidth);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
// Speciying horizontal scrolling region slow-down scrolling!
void IRAM_ATTR VGAController::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 pattern) { rawFillRow(y, x1, x2, pattern); }         // rawFillRow
                );

  if (scroll != 0) {
    // reassign DMA pointers
    int viewPortBuffersPerLine = 0;
    int linePos = 1;
    switch (m_timings.HStartingBlock) {
      case VGAScanStart::FrontPorch:
        // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
        viewPortBuffersPerLine = (m_viewPortCol + m_viewPortWidth) < m_timings.HVisibleArea ? 3 : 2;
        break;
      case VGAScanStart::Sync:
        // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
        viewPortBuffersPerLine = 3;
        break;
      case VGAScanStart::BackPorch:
        // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
        viewPortBuffersPerLine = 3;
        break;
      case VGAScanStart::VisibleArea:
        // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
        viewPortBuffersPerLine = m_viewPortCol > 0 ? 3 : 2;
        linePos = m_viewPortCol > 0 ? 1 : 0;
        break;
    }
    const int Y1 = paintState().scrollingRegion.Y1;
    const int Y2 = paintState().scrollingRegion.Y2;
    for (int i = Y1, idx = Y1 * m_timings.scanCount; i <= Y2; ++i)
      for (int scan = 0; scan < m_timings.scanCount; ++scan, ++idx)
        setDMABufferView(m_viewPortRow * m_timings.scanCount + idx * viewPortBuffersPerLine + linePos, i, scan, m_viewPort, false);
  }
}


// Scrolling by 1, 2, 3 and 4 pixels is optimized. Also scrolling multiples of 4 (8, 16, 24...) is optimized.
// Scrolling by other values requires up to three steps (scopose scrolling by 1, 2, 3 or 4): for example scrolling by 5 is scomposed to 4 and 1, scrolling
// by 6 is 4 + 2, etc.
// Horizontal scrolling region start and size (X2-X1+1) must be aligned to 32 bits, otherwise the unoptimized (very slow) version is used.
void IRAM_ATTR VGAController::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t pattern8   = preparePixel(getActualBrushColor());
  uint16_t pattern16 = pattern8 << 8 | pattern8;
  uint32_t pattern32 = pattern16 << 16 | pattern16;

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width   = X2 - X1 + 1;
  int width32 = width >> 2;
  bool HScrolllingRegionAligned = ((X1 & 3) == 0 && (width & 3) == 0);

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y] + X1);
        for (int s = -scroll; s > 0;) {
          if (s >= 4) {
            // scroll left 4, 8 12, etc.. pixels moving 32 bit words
            uint8_t * w = row;
            int sz = (s & ~3) >> 2;
            for (int i = 0; i < width32 - sz; ++i, w += 4)
              *((uint32_t*)w) = *((uint32_t*)w + sz);
            for (int i = tmax(0, width32 - sz); i < width32; ++i, w += 4)
              *((uint32_t*)w) = pattern32;
            s -= (s & ~3);
          } else if ((s & 3) == 3) {
            // scroll left 3 pixels swapping 8 bit words
            uint8_t * b = row;
            for (int i = 1; i < width32; ++i, b += 4) {
              b[2] = b[1];
              b[1] = b[4];
              b[0] = b[7];
              b[3] = b[6];
            }
            b[2] = b[1];
            b[1] = b[0] = b[3] = pattern8;
            s -= 3;
          } else if (s & 2) {
            // scroll left 2 pixels swapping 16 bit words
            uint16_t * w = (uint16_t*) row;
            for (int i = 1; i < width32; ++i, w += 2) {
              w[1] = w[0];
              w[0] = w[3];
            }
            w[1] = w[0];
            w[0] = pattern16;
            s -= 2;
          } else if (s & 1) {
            // scroll left 1 pixel by rotating 32 bit words
            uint8_t * w = row;
            for (int i = 1; i < width32; ++i, w += 4) {
              *((uint32_t*)w) = *((uint32_t*)w) >> 8 | *((uint32_t*)w) << 24;
              w[1] = w[6];
            }
            *((uint32_t*)w) = *((uint32_t*)w) >> 8 | *((uint32_t*)w) << 24;
            w[1] = pattern8;
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        uint8_t * row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA_PIXELINROW(row, x) = VGA_PIXELINROW(row, x - scroll);
        // fill right area with brush color
        for (int x = X2 + 1 + scroll; x <= X2; ++x)
          VGA_PIXELINROW(row, x) = pattern8;
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y] + X1);
        for (int s = scroll; s > 0;) {
          if (s >= 4) {
            // scroll right 4, 8 12, etc.. pixels moving 32 bit words
            int sz = (s & ~3) >> 2;
            uint8_t * w = row + width - 4;
            for (int i = 0; i < width32 - sz; ++i, w -= 4)
              *((uint32_t*)w) = *((uint32_t*)w - sz);
            for (int i = tmax(0, width32 - sz); i < width32; ++i, w -= 4)
              *((uint32_t*)w) = pattern32;
            s -= (s & ~3);
          } else if ((s & 3) == 3) {
            // scroll right 3 pixels swapping 8 bit words
            uint8_t * b = row + width - 4;
            for (int i = 1; i < width32; ++i, b -= 4) {
              b[0] = b[-3];
              b[1] = b[2];
              b[2] = b[-1];
              b[3] = b[-4];
            }
            b[1] = b[2];
            b[0] = b[2] = b[3] = pattern8;
            s -= 3;
          } else if (s & 2) {
            // scroll right 2 pixels swapping 16 bit words
            uint16_t * w = (uint16_t*) (row + width - 4);
            for (int i = 1; i < width32; ++i, w -= 2) {
              w[0] = w[1];
              w[1] = w[-2];
            }
            w[0] = w[1];
            w[1] = pattern16;
            s -= 2;
          } else if (s & 1) {
            // scroll right 1 pixel by rotating 32 bit words
            uint8_t * w = row + width - 4;
            for (int i = 1; i < width32; ++i, w -= 4) {
              *((uint32_t*)w) = *((uint32_t*)w) << 8 | *((uint32_t*)w) >> 24;
              w[2] = w[-3];
            }
            *((uint32_t*)w) = *((uint32_t*)w) << 8 | *((uint32_t*)w) >> 24;
            w[2] = pattern8;
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        uint8_t * row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA_PIXELINROW(row, x + scroll) = VGA_PIXELINROW(row, x);
        // fill left area with brush color
        for (int x = X1; x < X1 + scroll; ++x)
          VGA_PIXELINROW(row, x) = pattern8;
      }
    }

  }
}


void IRAM_ATTR VGAController::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color) { return preparePixel(color); },
                   [&] (int y)                { return (uint8_t*) m_viewPort[y]; },
                   [&] (uint8_t * row, int x, uint8_t pattern) { VGA_PIXELINROW(row, x) = pattern; }
                  );
}


void IRAM_ATTR VGAController::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void IRAM_ATTR VGAController::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                  { return preparePixel(color); },
                  [&] (int y)                                 { return (uint8_t*) m_viewPort[y]; },
                  [&] (uint8_t * row, int x)                  { return VGA_PIXELINROW(row, x); },
                  [&] (uint8_t * row, int x, uint8_t pattern) { VGA_PIXELINROW(row, x) = pattern; }
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void IRAM_ATTR VGAController::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                 { return (uint8_t*) m_viewPort[y]; },
                  [&] (uint8_t * row, int x)                  { return VGA_PIXELINROW(row, x); },
                  [&] (uint8_t * row, int x, uint8_t pattern) { VGA_PIXELINROW(row, x) = pattern; }
                 );
}


// no bounds check is done!
void VGAController::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    uint8_t * row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      uint8_t rawpix = VGA_PIXELINROW(row, x);
      *destBuf = RGB888((rawpix & 3) * 85, ((rawpix >> 2) & 3) * 85, ((rawpix >> 4) & 3) * 85);
    }
  }
}


// no bounds check is done!
void VGAController::readScreen(Rect const & rect, RGB222 * destBuf)
{
  uint8_t * dbuf = (uint8_t*) destBuf;
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    uint8_t * row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++dbuf)
      *dbuf = VGA_PIXELINROW(row, x) & ~VGA_SYNC_MASK;
  }
}


// no bounds check is done!
void VGAController::writeScreen(Rect const & rect, RGB222 * srcBuf)
{
  uint8_t * sbuf = (uint8_t*) srcBuf;
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    uint8_t * row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++sbuf)
      VGA_PIXELINROW(row, x) = *sbuf | m_HVSync;
  }
}


void IRAM_ATTR VGAController::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },         // rawGetRow
                              [&] (uint8_t * row, int x, uint8_t src) { VGA_PIXELINROW(row, x) = m_HVSync | src; }  // rawSetPixelInRow
                             );
}


void IRAM_ATTR VGAController::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundPattern = preparePixel(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                { return (uint8_t*) m_viewPort[y]; },             // rawGetRow
                            [&] (uint8_t * row, int x) { return VGA_PIXELINROW(row, x); },               // rawGetPixelInRow
                            [&] (uint8_t * row, int x) { VGA_PIXELINROW(row, x) = foregroundPattern; }   // rawSetPixelInRow
                           );
}


void IRAM_ATTR VGAController::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                   // rawGetRow
                                [&] (uint8_t * row, int x)              { return VGA_PIXELINROW(row, x); },                     // rawGetPixelInRow
                                [&] (uint8_t * row, int x, uint8_t src) { VGA_PIXELINROW(row, x) = m_HVSync | (src & 0x3f); }   // rawSetPixelInRow
                               );
}


void IRAM_ATTR VGAController::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },   // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA_PIXELINROW(row, x); },     // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA_PIXELINROW(row, x) = m_HVSync | (src.R >> 6) | (src.G >> 6 << 2) | (src.B >> 6 << 4); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR VGAController::swapBuffers()
{
  tswap(m_DMABuffers, m_DMABuffersVisible);
  tswap(m_viewPort, m_viewPortVisible);
  m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];
}



} // end of namespace
