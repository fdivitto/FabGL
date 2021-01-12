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
#include "vgadirectcontroller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")


namespace fabgl {





/*************************************************************************************/
/* VGADirectController definitions */


VGADirectController *    VGADirectController::s_instance = nullptr;
volatile int             VGADirectController::s_scanLine;
lldesc_t volatile *      VGADirectController::s_frameResetDesc;



VGADirectController::VGADirectController(bool autoRun)
  : m_drawScanlineCallback(nullptr),
    m_autoRun(autoRun)
{
  s_instance = this;
}


void VGADirectController::init()
{
  VGABaseController::init();
  m_doubleBufferOverDMA = false;
}


void VGADirectController::allocateViewPort()
{
  m_lines[0] = (uint8_t*) heap_caps_malloc(m_viewPortWidth * VGAD_LinesCount, MALLOC_CAP_DMA);
  for (int i = 1; i < VGAD_LinesCount; ++i)
    m_lines[i] = m_lines[0] + i * m_viewPortWidth;
}


void VGADirectController::freeViewPort()
{
  VGABaseController::freeViewPort();

  heap_caps_free((void*)m_lines[0]);
  for (int i = 0; i < VGAD_LinesCount; ++i)
    m_lines[i] = nullptr;
}


void VGADirectController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  // fail if setDrawScanlineCallback() has not been called
  if (!m_drawScanlineCallback)
    return;

  VGABaseController::setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);

  if (m_autoRun)
    run();
}


void VGADirectController::run()
{
  // must be started before interrupt alloc
  startGPIOStream();

  s_scanLine = 0;

  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  if (m_isr_handle == nullptr) {
    CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, ISRHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }
}


void VGADirectController::onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow)
{
  if (isVisible) {
    buffer->buf = (uint8_t *) m_lines[visibleRow % VGAD_LinesCount];

    // generate interrupt every half VGAD_LinesCount
    if ((scan == 0 && (visibleRow % (VGAD_LinesCount / 2)) == 0)) {
      if (visibleRow == 0)
        s_frameResetDesc = buffer;
      buffer->eof = 1;
    }
  }
}


void VGADirectController::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
}


void VGADirectController::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
}


void VGADirectController::rawFillRow(int y, int x1, int x2, RGB888 color)
{
}


void VGADirectController::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
}


void VGADirectController::rawInvertRow(int y, int x1, int x2)
{
}


void VGADirectController::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
}


void VGADirectController::swapRows(int yA, int yB, int x1, int x2)
{
}


void VGADirectController::drawEllipse(Size const & size, Rect & updateRect)
{
}


void VGADirectController::clear(Rect & updateRect)
{
}


void VGADirectController::VScroll(int scroll, Rect & updateRect)
{
}


void VGADirectController::HScroll(int scroll, Rect & updateRect)
{
}


void VGADirectController::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
}


void VGADirectController::invertRect(Rect const & rect, Rect & updateRect)
{
}


void VGADirectController::swapFGBG(Rect const & rect, Rect & updateRect)
{
}


void VGADirectController::copyRect(Rect const & source, Rect & updateRect)
{
}


void VGADirectController::readScreen(Rect const & rect, RGB888 * destBuf)
{
}


void VGADirectController::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
}


void VGADirectController::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
}


void VGADirectController::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
}


void VGADirectController::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
}


void IRAM_ATTR VGADirectController::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGADirectController *) arg;

  if (I2S1.int_st.out_eof) {

    auto desc = (volatile lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    int scanLine = (s_scanLine + VGAD_LinesCount / 2) % ctrl->m_viewPortHeight;

    auto lineIndex = scanLine & (VGAD_LinesCount - 1);

    for (int i = 0; i < VGAD_LinesCount / 2; ++i) {

      ctrl->m_drawScanlineCallback(ctrl->m_drawScanlineArg, (uint8_t*)(ctrl->m_lines[lineIndex]), scanLine);

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGAD_LinesCount / 2;

  }

  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  s_vgapalctrlcycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}



} // end of namespace

