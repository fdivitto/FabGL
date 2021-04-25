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
#include "esp_spi_flash.h"
#include "esp_heap_caps.h"

#include "fabutils.h"
#include "vgapalettedcontroller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")



namespace fabgl {





/*************************************************************************************/
/* VGAPalettedController definitions */


volatile uint8_t * * VGAPalettedController::s_viewPort;
volatile uint8_t * * VGAPalettedController::s_viewPortVisible;
lldesc_t volatile *  VGAPalettedController::s_frameResetDesc;
volatile int         VGAPalettedController::s_scanLine;




VGAPalettedController::VGAPalettedController(int linesCount, NativePixelFormat nativePixelFormat, int viewPortRatioDiv, int viewPortRatioMul, intr_handler_t isrHandler)
  : m_linesCount(linesCount),
    m_nativePixelFormat(nativePixelFormat),
    m_viewPortRatioDiv(viewPortRatioDiv),
    m_viewPortRatioMul(viewPortRatioMul),
    m_isrHandler(isrHandler)
{
  m_lines   = (volatile uint8_t**) heap_caps_malloc(sizeof(uint8_t*) * m_linesCount, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  m_palette = (RGB222*) heap_caps_malloc(sizeof(RGB222) * getPaletteSize(), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


VGAPalettedController::~VGAPalettedController()
{
  heap_caps_free(m_palette);
  heap_caps_free(m_lines);
}


void VGAPalettedController::init()
{
  VGABaseController::init();

  m_doubleBufferOverDMA      = false;
  m_taskProcessingPrimitives = false;
  m_processPrimitivesOnBlank = false;
  m_primitiveExecTask        = nullptr;
}


void VGAPalettedController::end()
{
  if (m_primitiveExecTask) {
    vTaskDelete(m_primitiveExecTask);
    m_primitiveExecTask = nullptr;
    m_taskProcessingPrimitives = false;
  }
  VGABaseController::end();
}


void VGAPalettedController::suspendBackgroundPrimitiveExecution()
{
  VGABaseController::suspendBackgroundPrimitiveExecution();
  while (m_taskProcessingPrimitives)
    ;
}

// make sure view port height is divisible by VGA16_LinesCount
void VGAPalettedController::checkViewPortSize()
{
  m_viewPortHeight &= ~(m_linesCount - 1);
}


void VGAPalettedController::allocateViewPort()
{
  VGABaseController::allocateViewPort(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);

  for (int i = 0; i < m_linesCount; ++i)
    m_lines[i] = (uint8_t*) heap_caps_malloc(m_viewPortWidth, MALLOC_CAP_DMA);
}


void VGAPalettedController::freeViewPort()
{
  VGABaseController::freeViewPort();

  for (int i = 0; i < m_linesCount; ++i) {
    heap_caps_free((void*)m_lines[i]);
    m_lines[i] = nullptr;
  }
}


void VGAPalettedController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGABaseController::setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);

  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;

  // fill view port
  for (int i = 0; i < m_viewPortHeight; ++i)
    memset((void*)(m_viewPort[i]), 0, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);

  setupDefaultPalette();
  updateRGB2PaletteLUT();

  calculateAvailableCyclesForDrawings();

  // must be started before interrupt alloc
  startGPIOStream();

  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  if (m_isr_handle == nullptr) {
    CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, m_isrHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }

  if (m_primitiveExecTask == nullptr) {
    xTaskCreatePinnedToCore(primitiveExecTask, "" , FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_STACK_SIZE, this, FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_PRIORITY, &m_primitiveExecTask, CoreUsage::quietCore());
  }

  resumeBackgroundPrimitiveExecution();
}


void VGAPalettedController::onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow)
{
  if (isVisible) {
    buffer->buf = (uint8_t *) m_lines[visibleRow % m_linesCount];

    // generate interrupt every half m_linesCount
    if ((scan == 0 && (visibleRow % (m_linesCount / 2)) == 0)) {
      if (visibleRow == 0)
        s_frameResetDesc = buffer;
      buffer->eof = 1;
    }
  }
}


int VGAPalettedController::getPaletteSize()
{
  switch (nativePixelFormat()) {
    case NativePixelFormat::PALETTE2:
      return 2;
    case NativePixelFormat::PALETTE4:
      return 4;
    case NativePixelFormat::PALETTE8:
      return 8;
    case NativePixelFormat::PALETTE16:
      return 16;
    default:
      return 0;
  }
}


// rebuild m_packedRGB222_to_PaletteIndex
void VGAPalettedController::updateRGB2PaletteLUT()
{
  auto paletteSize = getPaletteSize();
  for (int r = 0; r < 4; ++r)
    for (int g = 0; g < 4; ++g)
      for (int b = 0; b < 4; ++b) {
        double H1, S1, V1;
        rgb222_to_hsv(r, g, b, &H1, &S1, &V1);
        int bestIdx = 0;
        int bestDst = 1000000000;
        for (int i = 0; i < paletteSize; ++i) {
          double H2, S2, V2;
          rgb222_to_hsv(m_palette[i].R, m_palette[i].G, m_palette[i].B, &H2, &S2, &V2);
          double AH = H1 - H2;
          double AS = S1 - S2;
          double AV = V1 - V2;
          int dst = AH * AH + AS * AS + AV * AV;
          if (dst <= bestDst) {  // "<=" to prioritize higher indexes
            bestIdx = i;
            bestDst = dst;
            if (bestDst == 0)
              break;
          }
        }
        m_packedRGB222_to_PaletteIndex[r | (g << 2) | (b << 4)] = bestIdx;
      }
}


// calculates number of CPU cycles usable to draw primitives
void VGAPalettedController::calculateAvailableCyclesForDrawings()
{
  int availtime_us;

  if (m_processPrimitivesOnBlank) {
    // allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_linesCount / 2 + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch + m_viewPortRow));
  } else {
    // allowed time is the half of an entire frame. Fast, but may flick
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch));
    availtime_us /= 2;
  }

  m_primitiveExecTimeoutCycles = getCPUFrequencyMHz() * availtime_us;  // at 240Mhz, there are 240 cycles every microsecond
}


// we can use getCycleCount here because primitiveExecTask is pinned to a specific core (so cycle counter is the same)
// getCycleCount() requires 0.07us, while esp_timer_get_time() requires 0.78us
void VGAPalettedController::primitiveExecTask(void * arg)
{
  auto ctrl = (VGAPalettedController *) arg;

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


void VGAPalettedController::swapBuffers()
{
  VGABaseController::swapBuffers();
  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;
}



} // end of namespace

