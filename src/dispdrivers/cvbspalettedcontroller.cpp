/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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



#include <stdarg.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabutils.h"
#include "cvbspalettedcontroller.h"



#pragma GCC optimize ("O2")



namespace fabgl {





/*************************************************************************************/
/* CVBSPalettedController definitions */


volatile uint8_t * * CVBSPalettedController::s_viewPort;
volatile uint8_t * * CVBSPalettedController::s_viewPortVisible;




CVBSPalettedController::CVBSPalettedController(int columnsQuantum, NativePixelFormat nativePixelFormat, int viewPortRatioDiv, int viewPortRatioMul)
  : CVBSBaseController(),
    m_columnsQuantum(columnsQuantum),
    m_nativePixelFormat(nativePixelFormat),
    m_viewPortRatioDiv(viewPortRatioDiv),
    m_viewPortRatioMul(viewPortRatioMul)
{
  m_palette = (RGB222*) heap_caps_malloc(sizeof(RGB222) * getPaletteSize(), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


CVBSPalettedController::~CVBSPalettedController()
{
  heap_caps_free(m_palette);
}


void CVBSPalettedController::init()
{
  CVBSBaseController::init();

  m_taskProcessingPrimitives = false;
  m_processPrimitivesOnBlank = false;
  m_primitiveExecTask        = nullptr;
}


void CVBSPalettedController::end()
{
  if (m_primitiveExecTask) {
    vTaskDelete(m_primitiveExecTask);
    m_primitiveExecTask = nullptr;
    m_taskProcessingPrimitives = false;
  }
  CVBSBaseController::end();
}


void CVBSPalettedController::suspendBackgroundPrimitiveExecution()
{
  CVBSBaseController::suspendBackgroundPrimitiveExecution();
  while (m_taskProcessingPrimitives)
    ;
}

// make sure view port width is divisible by m_columnsQuantum
void CVBSPalettedController::checkViewPortSize()
{
  m_viewPortWidth &= ~(m_columnsQuantum - 1);  
}


void CVBSPalettedController::allocateViewPort()
{
  CVBSBaseController::allocateViewPort(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);
}


void CVBSPalettedController::freeViewPort()
{
  CVBSBaseController::freeViewPort();
}


void CVBSPalettedController::setResolution(CVBSParams const * params, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  CVBSBaseController::setResolution(params, viewPortWidth, viewPortHeight, doubleBuffered);

  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;

  // fill view port
  for (int i = 0; i < m_viewPortHeight; ++i)
    memset((void*)(m_viewPort[i]), 0, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);

  setupDefaultPalette();
  updateRGB2PaletteLUT();

  calculateAvailableCyclesForDrawings();

  if (m_primitiveExecTask == nullptr) {
    xTaskCreatePinnedToCore(primitiveExecTask, "" , FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_STACK_SIZE, this, FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_PRIORITY, &m_primitiveExecTask, CoreUsage::quietCore());
  }

  resumeBackgroundPrimitiveExecution();
  
  run();
}


int CVBSPalettedController::getPaletteSize()
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
void CVBSPalettedController::updateRGB2PaletteLUT()
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
void CVBSPalettedController::calculateAvailableCyclesForDrawings()
{
  int availtime_us;

  if (m_processPrimitivesOnBlank) {
    // allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
    availtime_us = params()->blankLines * params()->line_us;
  } else {
    // allowed time is the half of an entire frame. Fast, but may flick
    availtime_us = params()->fieldLines * params()->line_us;
  }
  
  availtime_us = params()->blankLines * params()->line_us;

  m_primitiveExecTimeoutCycles = getCPUFrequencyMHz() * availtime_us;  // at 240Mhz, there are 240 cycles every microsecond
}


// we can use getCycleCount here because primitiveExecTask is pinned to a specific core (so cycle counter is the same)
// getCycleCount() requires 0.07us, while esp_timer_get_time() requires 0.78us
void CVBSPalettedController::primitiveExecTask(void * arg)
{
  auto ctrl = (CVBSPalettedController *) arg;

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


void CVBSPalettedController::swapBuffers()
{
  CVBSBaseController::swapBuffers();
  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;
}



} // end of namespace

