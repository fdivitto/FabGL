/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
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
 * @brief This file contains fabgl::SSD1306Controller definition.
 */


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"

#include "esp32-hal.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "displaycontroller.h"
#include "comdrivers/tsi2c.h"




namespace fabgl {




/**
 * @brief Display driver for SSD1306 based OLED display, with I2C connection.
 *
 * Example:
 *
 *     fabgl::I2C               I2C;
 *     fabgl::SSD1306Controller SSD1306Controller;
 *
 *     void setup() {
 *       // SDA = gpio-4, SCL = gpio-15
 *       I2C.begin(GPIO_NUM_4, GPIO_NUM_15);
 *
 *       // default OLED address is 0x3C
 *       SSD1306Controller.begin(&I2C);
 *       SSD1306Controller.setResolution(OLED_128x64);
 *
 *       Canvas cv(&SSD1306Controller);
 *       cv.drawText(0, 0, "Hello World!");
 *
 */
class SSD1306Controller : public DisplayController {

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
   * @param resetGPIO Reset pin (use GPIO_NUM_39 to disable). Default if disabled.
   */
  void begin(I2C * i2c, int address = 0x3C, gpio_num_t resetGPIO = GPIO_NUM_39);

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

  // abstract method of DisplayController
  void suspendBackgroundPrimitiveExecution();

  // abstract method of DisplayController
  void resumeBackgroundPrimitiveExecution();

  // abstract method of DisplayController
  NativePixelFormat nativePixelFormat() { return NativePixelFormat::Mono; }

  // abstract method of DisplayController
  int getViewPortWidth()  { return m_viewPortWidth; }

  // abstract method of DisplayController
  int getViewPortHeight() { return m_viewPortHeight; }

  // abstract method of DisplayController
  int getScreenWidth()    { return m_screenWidth; }

  // abstract method of DisplayController
  int getScreenHeight()   { return m_screenHeight; }

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


private:

  // abstract method of DisplayController
  PixelFormat getBitmapSavePixelFormat() { return PixelFormat::RGBA2222; }


  bool SSD1306_sendData(uint8_t * buf, int count, uint8_t ctrl);
  bool SSD1306_sendCmd(uint8_t c);
  bool SSD1306_sendCmd(uint8_t c1, uint8_t c2);
  bool SSD1306_sendCmd(uint8_t c1, uint8_t c2, uint8_t c3);

  void SSD1306_hardReset();
  bool SSD1306_softReset();

  void SSD1306_sendScreenBuffer();

  void allocScreenBuffer();

  static void updateTaskFunc(void * pvParameters);

  // abstract method of DisplayController
  void setPixelAt(PixelDesc const & pixelDesc);

  // abstract method of DisplayController
  void clear();

  // abstract method of DisplayController
  void drawEllipse(Size const & size);

  void VScroll(int scroll);
  
  void HScroll(int scroll);

  // abstract method of DisplayController
  void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor);

  void drawGlyph_full(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor);

  void drawGlyph_light(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor);

  // abstract method of DisplayController
  void swapBuffers();

  // abstract method of DisplayController
  void invertRect(Rect const & rect);

  // abstract method of DisplayController
  void copyRect(Rect const & source);

  // abstract method of DisplayController
  void swapFGBG(Rect const & rect);

  // abstract method of DisplayController
  void drawLine(int X1, int Y1, int X2, int Y2, RGB888 color);

  // abstract method of DisplayController
  void fillRow(int y, int x1, int x2, RGB888 color);

  // abstract method of DisplayController
  void drawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of DisplayController
  void drawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of DisplayController
  void drawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount);

  void copyRow(int x1, int x2, int srcY, int dstY);

  uint8_t preparePixel(RGB888 const & rgb);


  I2C *              m_i2c;
  uint8_t            m_i2cAddress;
  gpio_num_t         m_resetGPIO;

  uint8_t *          m_screenBuffer;
  uint8_t *          m_altScreenBuffer; // used to implement double buffer

  int16_t            m_screenWidth;
  int16_t            m_screenHeight;
  int16_t            m_screenCol;
  int16_t            m_screenRow;

  int16_t            m_viewPortWidth;
  int16_t            m_viewPortHeight;

  TaskHandle_t       m_updateTaskHandle;

  int                m_updateTaskFuncSuspended;             // 0 = enabled, >0 suspended

};


} // end of namespace







