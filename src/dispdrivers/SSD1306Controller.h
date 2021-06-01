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



#ifdef ARDUINO


/**
 * @file
 *
 * @brief This file contains fabgl::SSD1306Controller definition.
 */


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "displaycontroller.h"
#include "comdrivers/tsi2c.h"




namespace fabgl {



/** \ingroup Enumerations
* @brief This enum defines SSD1306 orientation
*/
enum class SSD1306Orientation {
  Normal,             /**< Normal orientation */
  ReverseHorizontal,  /**< Reverse horizontal */
  ReverseVertical,    /**< Reverse vertical */
  Rotate180,          /**< Rotate 180 degrees */
};


/**
 * @brief Display driver for SSD1306 based OLED display, with I2C connection.
 *
 * This driver should also work with SH1106 (untested).
 *
 * Example:
 *
 *     fabgl::I2C               I2C;
 *     fabgl::SSD1306Controller SSD1306Controller;
 *
 *     void setup() {
 *       // SDA = gpio-4, SCL = gpio-15  (WARN: disconnect VGA connector!!)
 *       I2C.begin(GPIO_NUM_4, GPIO_NUM_15);
 *
 *       // default OLED address is 0x3C
 *       SSD1306Controller.begin(&I2C);
 *       SSD1306Controller.setResolution(OLED_128x64);
 *
 *       Canvas cv(&SSD1306Controller);
 *       cv.clear();
 *       cv.drawText(0, 0, "Hello World!");
 *     }
 */
class SSD1306Controller : public GenericBitmappedDisplayController {

public:

  // unwanted methods
  SSD1306Controller(SSD1306Controller const&)   = delete;
  void operator=(SSD1306Controller const&)      = delete;


  SSD1306Controller();

  ~SSD1306Controller();

  /**
   * @brief Initializes SSD1306 assigning I2C bus, reset pin and address.
   *
   * @param i2c I2C pointer
   * @param address Device address. Default is 0x3C.
   * @param resetGPIO Reset pin (use GPIO_UNUSED to disable). Default if disabled.
   */
  void begin(I2C * i2c, int address = 0x3C, gpio_num_t resetGPIO = GPIO_UNUSED);

  /**
   * @brief Initializes SSD1306.
   *
   * This initializer uses following configuration:
   *   SDA = 4
   *   SCL = 15
   *   Address = 0x3C
   *   no reset
   */
  void begin();

  void end();

  /**
   * @brief Sets SSD1306 resolution and viewport size
   *
   * Viewport size can be larger than display size. You can pan the view using SSD1306Controller.setScreenCol() and SSD1306Controller.setScreenRow().
   *
   * @param modeline Native display reoslution. Possible values: OLED_128x64 and OLED_128x32.
   * @param viewPortWidth Virtual viewport width. Should be larger or equal to display native width.
   * @param viewPortHeight Virtual viewport height. Should be larger or equal to display native height.
   * @param doubleBuffered if True allocates another viewport of the same size to use as back buffer.
   */
  void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  /**
   * @brief Checks the SSD1306 device availability
   *
   * @return True is SSD1306 OLED device has been found and initialized.
   */
  bool available() { return m_screenBuffer != nullptr; }

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  void resumeBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat() { return NativePixelFormat::Mono; }

  // abstract method of BitmappedDisplayController
  int getViewPortWidth()  { return m_viewPortWidth; }

  // abstract method of BitmappedDisplayController
  int getViewPortHeight() { return m_viewPortHeight; }

  /**
   * @brief Set initial left column of the viewport
   *
   * Use this method only when viewport is larger than display size.
   *
   * @param value First column to display
   */
  void setScreenCol(int value);

  /**
   * @brief Set initial top row of the viewport
   *
   * Use this method only when viewport is larger than display size.
   *
   * @param value First row to display
   */
  void setScreenRow(int value);

  /**
   * @brief Gets initial left column of the viewport
   *
   * @return First column displayed
   */
  int screenCol() { return m_screenCol; }

  /**
   * @brief Gets initial top row of the viewport
   *
   * @return First row displayed
   */
  int screenRow() { return m_screenRow; }

  void readScreen(Rect const & rect, RGB888 * destBuf);

  /**
   * @brief Inverts display colors
   *
   * @param value True enables invertion.
   */
  void invert(bool value);

  /**
   * @brief Set display orientation and rotation
   *
   * @param value Display orientation and rotation
   *
   * Example:
   * 
   *     // rotate by 180 degrees
   *     DisplayController.setOrientation(fabgl::SSD1306Orientation::Rotate180);
   */
  void setOrientation(SSD1306Orientation value);


private:

  // abstract method of BitmappedDisplayController
  int getBitmapSavePixelSize() { return 1; }


  bool SSD1306_sendData(uint8_t * buf, int count, uint8_t ctrl);
  bool SSD1306_sendCmd(uint8_t c);
  bool SSD1306_sendCmd(uint8_t c1, uint8_t c2);
  bool SSD1306_sendCmd(uint8_t c1, uint8_t c2, uint8_t c3);

  void SSD1306_hardReset();
  bool SSD1306_softReset();

  void SSD1306_sendScreenBuffer(Rect updateRect);

  void sendRefresh();

  void setupOrientation();

  void allocScreenBuffer();

  static void updateTaskFunc(void * pvParameters);

  // abstract method of BitmappedDisplayController
  void setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void clear(Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void drawEllipse(Size const & size, Rect & updateRect);

  void VScroll(int scroll, Rect & updateRect);
  
  void HScroll(int scroll, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void swapBuffers();

  // abstract method of BitmappedDisplayController
  void invertRect(Rect const & rect, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void copyRect(Rect const & source, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void swapFGBG(Rect const & rect, Rect & updateRect);

  // abstract method of BitmappedDisplayController
  void absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color);

  // abstract method of BitmappedDisplayController
  void rawFillRow(int y, int x1, int x2, RGB888 color);

  void rawFillRow(int y, int x1, int x2, uint8_t pattern);

  void rawInvertRow(int y, int x1, int x2);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  void rawCopyRow(int x1, int x2, int srcY, int dstY);


  I2C *              m_i2c;
  uint8_t            m_i2cAddress;
  gpio_num_t         m_resetGPIO;

  uint8_t *          m_screenBuffer;

  int16_t            m_screenWidth;
  int16_t            m_screenHeight;
  int16_t            m_screenCol;
  int16_t            m_screenRow;

  int16_t            m_viewPortWidth;
  int16_t            m_viewPortHeight;

  TaskHandle_t       m_updateTaskHandle;

  volatile int       m_updateTaskFuncSuspended;             // 0 = enabled, >0 suspended
  volatile bool      m_updateTaskRunning;

  SSD1306Orientation m_orientation;

};


} // end of namespace



#endif // #ifdef ARDUINO




