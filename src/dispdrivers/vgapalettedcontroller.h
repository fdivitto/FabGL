/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

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


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::VGAPalettedController definition.
 */


#include <stdint.h>
#include <stddef.h>
#include <atomic>

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "devdrivers/swgenerator.h"
#include "displaycontroller.h"
#include "dispdrivers/vgabasecontroller.h"




namespace fabgl {






/**
* @brief Represents the base class for paletted bitmapped controllers like VGA16Controller, VGA8Controller, etc..
*/
class VGAPalettedController : public VGABaseController {

public:

  VGAPalettedController(int linesCount, NativePixelFormat nativePixelFormat, int viewPortRatioDiv, int viewPortRatioMul, intr_handler_t isrHandler);
  ~VGAPalettedController();

  // unwanted methods
  VGAPalettedController(VGAPalettedController const&) = delete;
  void operator=(VGAPalettedController const&)        = delete;

  void end();

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // import "modeline" version of setResolution
  using VGABaseController::setResolution;

  void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  int getPaletteSize();

  /**
   * @brief Determines the maximum time allowed to process primitives
   *
   * Primitives processing is always started at the beginning of vertical blank.
   * Unfortunately this time is limited and not all primitive may be processed, so processing all primitives may required more frames.
   * This method expands the allowed time to half of a frame. This increase drawing speed but may show some flickering.
   *
   * The default is False (fast drawings, possible flickering).
   *
   * @param value True = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering. False = allowed time is the half of an entire frame. Fast, but may flick.
   */
  void setProcessPrimitivesOnBlank(bool value)          { m_processPrimitivesOnBlank = value; }

  // returns "static" version of m_viewPort
  static uint8_t * sgetScanline(int y)                  { return (uint8_t*) s_viewPort[y]; }

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat()                 { return m_nativePixelFormat; }


protected:

  void init();

  virtual void setupDefaultPalette() = 0;

  void updateRGB2PaletteLUT();
  void calculateAvailableCyclesForDrawings();
  static void primitiveExecTask(void * arg);

  uint8_t RGB888toPaletteIndex(RGB888 const & rgb) {
    return m_packedRGB222_to_PaletteIndex[RGB888toPackedRGB222(rgb)];
  }

  uint8_t RGB2222toPaletteIndex(uint8_t value) {
    return m_packedRGB222_to_PaletteIndex[value & 0b00111111];
  }

  uint8_t RGB8888toPaletteIndex(RGBA8888 value) {
    return RGB888toPaletteIndex(RGB888(value.R, value.G, value.B));
  }

  // abstract method of BitmappedDisplayController
  void swapBuffers();


  TaskHandle_t                m_primitiveExecTask;

  volatile uint8_t * *        m_lines;

  // optimization: clones of m_viewPort and m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  static lldesc_t volatile *  s_frameResetDesc;

  static volatile int         s_scanLine;

  RGB222 *                    m_palette;


private:

  void allocateViewPort();
  void freeViewPort();
  void checkViewPortSize();
  void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow);

  // Maximum time (in CPU cycles) available for primitives drawing
  volatile uint32_t           m_primitiveExecTimeoutCycles;

  volatile bool               m_taskProcessingPrimitives;

  // true = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
  // false = allowed time is the half of an entire frame. Fast, but may flick
  bool                        m_processPrimitivesOnBlank;

  uint8_t                     m_packedRGB222_to_PaletteIndex[64];

  // configuration
  int                         m_linesCount;
  NativePixelFormat           m_nativePixelFormat;
  int                         m_viewPortRatioDiv;
  int                         m_viewPortRatioMul;
  intr_handler_t              m_isrHandler;


};



} // end of namespace








