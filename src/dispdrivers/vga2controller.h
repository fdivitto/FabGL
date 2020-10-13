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


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::VGA2Controller definition.
 */


#include <stdint.h>
#include <stddef.h>
#include <atomic>

#include "rom/lldesc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "devdrivers/swgenerator.h"
#include "displaycontroller.h"
#include "dispdrivers/vgabasecontroller.h"



#define VGA2_LinesCount 4

//#define VGAXController_PERFORMANCE_CHECK


namespace fabgl {



/**
* @brief Represents the VGA 2 colors bitmapped controller
*
* This VGA controller allows just 2 colors, but requires less (1/8) RAM than VGAController, at the same resolution. Each pixel is represented
* by 1 bit, which is an index to a palette of 2 colors. Each byte of the frame buffer contains eight pixels.
* For example, a frame buffer of 640x480 requires about 40K of RAM.
* This controller also allows higher resolutions, up to 1280x768.
*
* VGA2Controller output consumes up to 11% of one CPU core (measured at 640x480x60Hz).
*
*
* This example initializes VGA Controller with 64 colors (2 visible at the same time) at 640x480:
*
*     fabgl::VGA2Controller displayController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     displayController.begin();
*     displayController.setResolution(VGA_640x480_60Hz);
*/
class VGA2Controller : public VGABaseController {

public:

  VGA2Controller();
  ~VGA2Controller();

  // unwanted methods
  VGA2Controller(VGA2Controller const&) = delete;
  void operator=(VGA2Controller const&)  = delete;


  /**
   * @brief Returns the singleton instance of VGA2Controller class
   *
   * @return A pointer to VGA2Controller singleton object
   */
  static VGA2Controller * instance() { return s_instance; }

  void end();

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat()               { return NativePixelFormat::PALETTE2; }

  // import "modeline" v3rsion of setResolution
  using VGABaseController::setResolution;

  void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  // returns "static" version of m_viewPort
  static uint8_t * sgetScanline(int y)                { return (uint8_t*) s_viewPort[y]; }

  void readScreen(Rect const & rect, RGB888 * destBuf);

  /**
   * @brief Determines color of specified palette item
   *
   * @param index Palette item (0..1)
   * @param color Color to assign to this item
   *
   * Example:
   *
   *     // Color item 0 is pure Red
   *     displayController.setPaletteItem(0, RGB(255, 0, 0));
   */
  void setPaletteItem(int index, RGB888 const & color);

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

private:

  void init();

  void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow);
  void allocateViewPort();
  void freeViewPort();
  void checkViewPortSize();


  uint8_t RGB888toPaletteIndex(RGB888 const & rgb);
  uint8_t RGB2222toPaletteIndex(uint8_t value);
  uint8_t RGB8888toPaletteIndex(RGBA8888 value);



  // abstract method of BitmappedDisplayController
  void setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void drawEllipse(Size const & size, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void clear(Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void VScroll(int scroll, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void HScroll(int scroll, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void invertRect(Rect const & rect, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void copyRect(Rect const & source, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void swapFGBG(Rect const & rect, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void swapBuffers();

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawFillRow(int y, int x1, int x2, RGB888 color);

  void rawFillRow(int y, int x1, int x2, uint8_t colorIndex);

  void rawInvertRow(int y, int x1, int x2);

  void rawCopyRow(int x1, int x2, int srcY, int dstY);

  void swapRows(int yA, int yB, int x1, int x2);

  // abstract method of BitmappedDisplayController
  void absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color);

  // abstract method of BitmappedDisplayController
  int getBitmapSavePixelSize() { return 1; }

  static void I2SInterrupt(void * arg);

  static void primitiveExecTask(void * arg);

  void calculateAvailableCyclesForDrawings();

  void setupDefaultPalette();
  void updateRGB2PaletteLUT();


  static VGA2Controller *     s_instance;
  static volatile int         s_scanLine;
  static lldesc_t volatile *  s_frameResetDesc;

  TaskHandle_t                m_primitiveExecTask;

  volatile uint32_t           m_primitiveExecTimeoutCycles; // Maximum time (in CPU cycles) available for primitives drawing

  // true = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
  // false = allowed time is the half of an entire frame. Fast, but may flick
  bool                        m_processPrimitivesOnBlank;

  // optimization: clones of m_viewPort and m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  volatile uint8_t *          m_lines[VGA2_LinesCount];

  RGB222                      m_palette[2];
  uint8_t                     m_packedRGB222_to_PaletteIndex[64];
  volatile uint64_t *         m_packedPaletteIndexOctet_to_signals;

  volatile bool               m_taskProcessingPrimitives;

};



} // end of namespace







