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
 * @brief This file contains fabgl::VGADirectController definition.
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
 * @brief Callback used when VGADirectController needs to prepare a new scanline to be sent to the VGA output.
 *
 * @param arg Parameter arg passed to VGADirectController::setDrawScanlineCallback()
 * @param dest Buffer to fill with raw pixels
 * @param scanLine Line index (0 to screen height - 1)
 */
typedef void (*DrawScanlineCallback)(void * arg, uint8_t * dest, int scanLine);


/**
 * @brief Represents an base abstract class for direct draw VGA controller
 *
 * A direct draw VGA controller draws paint the screen in real time, for each scanline. Every two scanlines an interrupt is generated.
 * Put your drawing code inside a callback and call setDrawScanlineCallback() to assign it.
 *
 * See DirectVGA.ino example.
 */
class VGADirectController : public VGABaseController {

public:

  /**
   * @brief Initializes a new instance of VGADirectController
   *
   * @param autoRun If True display is active after setResolution() has been called. If False you need to call VGADirectController.run() to start the display.
   */
  VGADirectController(bool autoRun = true);

  // unwanted methods
  VGADirectController(VGADirectController const&) = delete;
  void operator=(VGADirectController const&)  = delete;


  /**
   * @brief Returns the singleton instance of VGADirectController class
   *
   * @return A pointer to VGADirectController singleton object
   */
  static VGADirectController * instance() { return s_instance; }

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat()               { return NativePixelFormat::SBGR2222; }

  // import "modeline" version of setResolution
  using VGABaseController::setResolution;

  void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  void readScreen(Rect const & rect, RGB888 * destBuf);

  /**
   * @brief Sets the callback used when VGADirectController needs to prepare a new scanline to be sent to the VGA output.
   *
   * @param drawScanlineCallback The callback to call
   * @param arg Argument to pass to the callback
   */
  void setDrawScanlineCallback(DrawScanlineCallback drawScanlineCallback, void * arg = nullptr) { m_drawScanlineCallback = drawScanlineCallback; m_drawScanlineArg = arg; }

  /**
   * @brief Begins to call the callback function and to display video frames
   *
   * You need to call this only when VGADirectController constructor has been called with autoRun = false.
   */
  void run();

  /**
   * @brief Sets number of scanlines to draw in a single callback
   *
   * Default scanlines per callback is 1.
   *
   * @param value Number of scanlines to fill in DrawScanlineCallback callback
   */
  void setScanlinesPerCallBack(int value)    { m_linesCount = value * 2; }

  /**
   * @brief Determines if retracing is in progress
   *
   * @return True when retracing (vertical sync) is active
   */
  static bool VSync()                        { return s_VSync; }

private:

  void init();

  void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow);
  void allocateViewPort();
  void freeViewPort();


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

  static void ISRHandler(void * arg);


  static VGADirectController * s_instance;
  static volatile int          s_scanLine;
  static lldesc_t volatile *   s_frameResetDesc;
  static bool                  s_VSync;

  int32_t                      m_linesCount;
  uint8_t *  *                 m_lines;

  // here we use callbacks in place of virtual methods because vtables are stored in Flash and
  // so it would not have been possible to put ISR into IRAM.
  DrawScanlineCallback         m_drawScanlineCallback;
  void *                       m_drawScanlineArg;

  bool                         m_autoRun;

};



} // end of namespace








