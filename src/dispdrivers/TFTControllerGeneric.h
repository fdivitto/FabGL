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


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::TFTController definition.
 */


#include <stdint.h>
#include <stddef.h>

#ifdef ARDUINO
  #include "SPI.h"
#endif

#include "freertos/FreeRTOS.h"

#include "driver/spi_master.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "displaycontroller.h"


#define TFT_CASET      0x2A
#define TFT_RASET      0x2B
#define TFT_RAMWR      0x2C
#define TFT_MADCTL     0x36



namespace fabgl {



/** \ingroup Enumerations
 * @brief This enum defines TFT orientation
 */
enum class TFTOrientation {
  Rotate0,            /**< Rotate 0 degrees */
  Rotate90,           /**< Rotate 90 degrees */
  Rotate180,          /**< Rotate 180 degrees */
  Rotate270,          /**< Rotate 270 degrees */
};


/**
 * @brief Base abstract class for TFT drivers with SPI connection.
 *
 * Example:
 *
 *     fabgl::ST7789Controller DisplayController;
 *
 *     void setup() {
 *       // SCK = 18, MOSI = 23, D/C = 22, RESET = 21, no CS  (WARN: disconnect VGA connector!!)
 *       DisplayController.begin(18, 23, 22, 21, -1, VSPI_HOST);
 *       DisplayController.setResolution(TFT_240x240);
 *
 *       Canvas cv(&DisplayController);
 *       cv.clear();
 *       cv.drawText(0, 0, "Hello World!");
 *     }
 */
class TFTController : public GenericBitmappedDisplayController {

public:

  // unwanted methods
  TFTController(TFTController const&) = delete;
  void operator=(TFTController const&)   = delete;

  TFTController();

  ~TFTController();

  /**
   * @brief Initializes TFT display controller with Arduino style SPIClass object
   *
   * @param spi SPIClass object.
   * @param DC GPIO of D/C signal (Data/Command).
   * @param RESX GPIO of Reset signal (can be GPIO_UNUSED).
   * @param CS GPIO of Optional select signal (can be GPIO_UNUSED). Without CS signal it is impossible to share SPI channel with other devices.
   *
   * Example:
   *
   *     fabgl::ST7789Controller DisplayController;
   *
   *     // SCK = 18, MOSI = 23, D/C = 22, RESX = 21
   *     SPI.begin(18, -1, 23);
   *     DisplayController.begin(&SPI, GPIO_NUM_22, GPIO_NUM_21);
   *     DisplayController.setResolution(TFT_240x240);
   */
  #ifdef ARDUINO
  void begin(SPIClass * spi, gpio_num_t DC, gpio_num_t RESX = GPIO_UNUSED, gpio_num_t CS = GPIO_UNUSED);
  #endif

  /**
   * @brief Initializes TFT display controller with Arduino style SPIClass object
   *
   * @param spi SPIClass object.
   * @param DC GPIO of D/C signal (Data/Command).
   * @param RESX GPIO of Reset signal (can be GPIO_UNUSED).
   * @param CS GPIO of optional select signal (can be GPIO_UNUSED). Without CS signal it is impossible to share SPI channel with other devices.
   *
   * Example:
   *
   *     fabgl::ST7789Controller DisplayController;
   *
   *     // SCK = 18, MOSI = 23, D/C = 22, RESX = 21
   *     SPI.begin(18, -1, 23);
   *     DisplayController.begin(&SPI, 22, 21);
   *     DisplayController.setResolution(TFT_240x240);
   */
  #ifdef ARDUINO
  void begin(SPIClass * spi, int DC, int RESX = -1, int CS = -1);
  #endif

  /**
   * @brief Initializes TFT display controller
   *
   * This initializer uses SDK API to get access to the SPI channel.
   *
   * @param SCK GPIO of clock signal (sometimes named SCL).
   * @param MOSI GPIO of data out signal (sometimes named SDA).
   * @param DC GPIO of D/C signal.
   * @param RESX GPIO of optional reset signal (sometimes named RESX, can be -1).
   * @param CS GPIO of optional selet signal (can be -1). Without CS signal it is impossible to share SPI channel with other devices.
   * @param host SPI bus to use (1 = HSPI, 2 = VSPI).
   *
   * Example:
   *
   *     // setup using VSPI (compatible with SD Card, which uses HSPI)
   *     // SCK = 18, MOSI = 23, D/C = 22, RESET = 21, no CS  (WARN: disconnect VGA connector!!)
   *     DisplayController.begin(18, 23, 22, 21, -1, VSPI_HOST);
   *     DisplayController.setResolution(TFT_240x240);
   */
  void begin(int SCK, int MOSI, int DC, int RESX, int CS, int host);

  /**
   * @brief Initializes TFT display controller
   *
   * This initializer uses SDK API to get access to the SPI channel, assigning following configuration:
   *   SCK    = 18
   *   MOSI   = 23
   *   DC     = 22
   *   RESET  = 21
   *   CS     =  5
   *   host   = VSPI_HOST
   */
  void begin();

  void end();

  /**
   * @brief Sets TFT resolution and viewport size
   *
   * Viewport size can be larger than display size. You can pan the view using TFTController.setScreenCol() and TFTController.setScreenRow().
   *
   * @param modeline Native display reoslution. Allowed values like TFT_240x240, TFT_240x320...
   * @param viewPortWidth Virtual viewport width. Should be larger or equal to display native width.
   * @param viewPortHeight Virtual viewport height. Should be larger or equal to display native height.
   * @param doubleBuffered if True allocates another viewport of the same size to use as back buffer.
   *
   * Example:
   *
   *     DisplayController.setResolution(TFT_240x240);
   */
  void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  void resumeBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat() { return NativePixelFormat::RGB565BE; }

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
   * @brief Performs display hardware and software
   */
  void reset() { hardReset(); softReset(); }

  /**
   * @brief Set display orientation and rotation
   *
   * @param value Display orientation and rotation
   * @param force
   *
   * Example:
   *
   *     // rotate by 180 degrees
   *     DisplayController.setOrientation(fabgl::TFTOrientation::Rotate180);
   */
  void setOrientation(TFTOrientation value, bool force = false);

  /**
   * @brief Inverts horizontal axis
   *
   * Default value depends by the specific display.
   *
   * @param value True inverts horizontal axis.
   */
  void setReverseHorizontal(bool value);


protected:

  virtual void softReset() = 0;

  virtual void setupOrientation();

  // abstract method of BitmappedDisplayController
  int getBitmapSavePixelSize() { return 2; }

  void setupGPIO();

  void hardReset();

  void sendRefresh();

  void sendScreenBuffer(Rect updateRect);
  void writeCommand(uint8_t cmd);
  void writeByte(uint8_t data);
  void writeWord(uint16_t data);
  void writeData(void * data, size_t size);

  void SPIBegin();
  void SPIEnd();

  void SPIBeginWrite();
  void SPIEndWrite();
  void SPIWriteByte(uint8_t data);
  void SPIWriteWord(uint16_t data);
  void SPIWriteBuffer(void * data, size_t size);

  void allocViewPort();
  void freeViewPort();

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

  void rawFillRow(int y, int x1, int x2, uint16_t pattern);

  void swapRows(int yA, int yB, int x1, int x2);

  void rawInvertRow(int y, int x1, int x2);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  // abstract method of BitmappedDisplayController
  void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);


  #ifdef ARDUINO
  SPIClass *         m_spi;
  #endif

  spi_host_device_t  m_SPIHost;
  gpio_num_t         m_SCK;
  gpio_num_t         m_MOSI;
  gpio_num_t         m_DC;
  gpio_num_t         m_RESX;
  gpio_num_t         m_CS;

  spi_device_handle_t m_SPIDevHandle;

  uint16_t * *       m_viewPort;

  int16_t            m_screenWidth;
  int16_t            m_screenHeight;
  int16_t            m_screenCol;
  int16_t            m_screenRow;

  int16_t            m_viewPortWidth;
  int16_t            m_viewPortHeight;

  // view port size when rotation is 0 degrees
  int16_t            m_rot0ViewPortWidth;
  int16_t            m_rot0ViewPortHeight;

  // maximum width and height the controller can handle (ie 240x320)
  int16_t            m_controllerWidth;
  int16_t            m_controllerHeight;

  // offsets used on rotating
  int16_t            m_rotOffsetX;
  int16_t            m_rotOffsetY;

  TaskHandle_t       m_updateTaskHandle;

  volatile int       m_updateTaskFuncSuspended;             // 0 = enabled, >0 suspended
  volatile bool      m_updateTaskRunning;

  TFTOrientation     m_orientation;
  bool               m_reverseHorizontal;

};


} // end of namespace







