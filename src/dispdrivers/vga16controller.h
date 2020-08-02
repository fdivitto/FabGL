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
 * @brief This file contains fabgl::VGA16Controller definition.
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
#include "dispdrivers/vgacontroller.h"



#define VGA16_LinesCount 4

//#define VGATextController_PERFORMANCE_CHECK


namespace fabgl {



/**
* @brief Represents the VGA 16 colors bitmapped controller
*
* This VGA controller allows just 16 colors, but requires less (1/2) RAM than VGAController, at the same resolution. Each pixel is represented
* by 4 bits, which is an index to a palette of 64 colors. Each byte of the frame buffer contains two pixels.
* For example, a frame buffer of 640x480 requires about 153K of RAM.
*
* VGA16Controller output is very CPU intensive process and consumes up to 30% of one CPU core. Anyway this allows to have
* more RAM free for your application.
*
*
* This example initializes VGA Controller with 64 colors (16 visible at the same time) at 640x480:
*
*     fabgl::VGA16Controller displayController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     displayController.begin();
*     displayController.setResolution(VGA_640x480_60Hz);

*/
class VGA16Controller : public GenericBitmappedDisplayController {

public:

  VGA16Controller();

  // unwanted methods
  VGA16Controller(VGA16Controller const&)   = delete;
  void operator=(VGA16Controller const&)  = delete;


  /**
   * @brief Returns the singleton instance of VGA16Controller class
   *
   * @return A pointer to VGA16Controller singleton object
   */
  static VGA16Controller * instance() { return s_instance; }


  /**
   * @brief This is the 8 colors (5 GPIOs) initializer.
   *
   * One GPIO per channel, plus horizontal and vertical sync signals.
   *
   * @param redGPIO GPIO to use for red channel.
   * @param greenGPIO GPIO to use for green channel.
   * @param blueGPIO GPIO to use for blue channel.
   * @param HSyncGPIO GPIO to use for horizontal sync signal.
   * @param VSyncGPIO GPIO to use for vertical sync signal.
   *
   * Example:
   *
   *     // Use GPIO 22 for red, GPIO 21 for green, GPIO 19 for blue, GPIO 18 for HSync and GPIO 5 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
   */
  void begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO);

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   *
   * @param red1GPIO GPIO to use for red channel, MSB bit.
   * @param red0GPIO GPIO to use for red channel, LSB bit.
   * @param green1GPIO GPIO to use for green channel, MSB bit.
   * @param green0GPIO GPIO to use for green channel, LSB bit.
   * @param blue1GPIO GPIO to use for blue channel, MSB bit.
   * @param blue0GPIO GPIO to use for blue channel, LSB bit.
   * @param HSyncGPIO GPIO to use for horizontal sync signal.
   * @param VSyncGPIO GPIO to use for vertical sync signal.
   *
   * Example:
   *
   *     // Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
   */
  void begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO);

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer using default pinout.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   * Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *
   * Example:
   *
   *     VGAController.begin();
   */
  void begin();

  void end();

  // abstract method of BitmappedDisplayController
  void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  void resumeBackgroundPrimitiveExecution();

  /**
   * @brief Gets number of bits allocated for each channel.
   *
   * Number of bits depends by which begin() initializer has been called.
   *
   * @return 1 (8 colors) or 2 (64 colors).
   */
  uint8_t getBitsPerChannel() { return m_bitsPerChannel; }

  // abstract method of BitmappedDisplayController
  NativePixelFormat nativePixelFormat() { return NativePixelFormat::PALETTE16; }

  /**
   * @brief Sets current resolution using linux-like modeline.
   *
   * Modeline must have following syntax (non case sensitive):
   *
   *     "label" clock_mhz hdisp hsyncstart hsyncend htotal vdisp vsyncstart vsyncend vtotal (+HSync | -HSync) (+VSync | -VSync) [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
   *
   * In fabglconf.h there are macros with some predefined modelines for common resolutions.
   * When MultiScanBlank and DoubleScan is specified then additional rows are not repeated, but just filled with blank lines.
   *
   * @param modeline Linux-like modeline as specified above.
   * @param viewPortWidth Horizontal viewport size in pixels. If less than zero (-1) it is sized to modeline visible area width.
   * @param viewPortHeight Vertical viewport size in pixels. If less then zero (-1) it is sized to maximum allocable.
   * @param doubleBuffered if True allocates another viewport of the same size to use as back buffer. Make sure there is enough free memory.
   *
   * Example:
   *
   *     // Use predefined modeline for 640x480@60Hz
   *     VGAController.setResolution(VGA_640x480_60Hz);
   *
   *     // The same of above using modeline string
   *     VGAController.setResolution("\"640x480@60Hz\" 25.175 640 656 752 800 480 490 492 525 -HSync -VSync");
   *
   *     // Set 640x382@60Hz but limit the viewport to 640x350
   *     VGAController.setResolution(VGA_640x382_60Hz, 640, 350);
   *
   */
  void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  VGATimings * getResolutionTimings() { return &m_timings; }

  // abstract method of BitmappedDisplayController
  int getScreenWidth()    { return m_timings.HVisibleArea; }

  // abstract method of BitmappedDisplayController
  int getScreenHeight()   { return m_timings.VVisibleArea; }

  /**
   * @brief Determines horizontal position of the viewport.
   *
   * @return Horizontal position of the viewport (in case viewport width is less than screen width).
   */
  int getViewPortCol()    { return m_viewPortCol; }

  /**
   * @brief Determines vertical position of the viewport.
   *
   * @return Vertical position of the viewport (in case viewport height is less than screen height).
   */
  int getViewPortRow()    { return m_viewPortRow; }

  // abstract method of BitmappedDisplayController
  int getViewPortWidth()  { return m_viewPortWidth; }

  // abstract method of BitmappedDisplayController
  int getViewPortHeight() { return m_viewPortHeight; }

  /**
   * @brief Moves screen by specified horizontal and vertical offset.
   *
   * Screen moving is performed moving horizontal and vertical Front and Back porchs.
   *
   * @param offsetX Horizontal offset in pixels. < 0 goes left, > 0 goes right.
   * @param offsetY Vertical offset in pixels. < 0 goes up, > 0 goes down.
   *
   * Example:
   *
   *     // Move screen 4 pixels right, 1 pixel left
   *     VGAController.moveScreen(4, -1);
   */
  void moveScreen(int offsetX, int offsetY);

  /**
   * @brief Reduces or expands screen size by the specified horizontal and vertical offset.
   *
   * Screen shrinking is performed changing horizontal and vertical Front and Back porchs.
   *
   * @param shrinkX Horizontal offset in pixels. > 0 shrinks, < 0 expands.
   * @param shrinkY Vertical offset in pixels. > 0 shrinks, < 0 expands.
   *
   * Example:
   *
   *     // Shrink screen by 8 pixels and move by 8 pixels to the left
   *     VGAController.shrinkScreen(8, 0);
   *     VGAController.moveScreen(8, 0);
   */
  void shrinkScreen(int shrinkX, int shrinkY);

  static uint8_t * getScanline(int y)                { return (uint8_t*) s_viewPort[y]; }

  void readScreen(Rect const & rect, RGB888 * destBuf);

  /**
   * @brief Determines color of specified palette item
   *
   * @param index Palette item (0..15)
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



  void init(gpio_num_t VSyncGPIO);

  uint8_t packHVSync(bool HSync, bool VSync);
  uint8_t preparePixel(RGB222 rgb) { return m_HVSync | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT); }
  uint8_t preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync);

  uint8_t RGB888toPaletteIndex(RGB888 const & rgb);
  uint8_t RGB2222toPaletteIndex(uint8_t value);
  uint8_t RGB8888toPaletteIndex(RGBA8888 value);


  void freeBuffers();
  void fillHorizBuffers(int offsetX);
  void fillVertBuffers(int offsetY);
  int fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool hsync, bool vsync);
  void allocateViewPort();
  void freeViewPort();
  int calcRequiredDMABuffersCount(int viewPortHeight);

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

  static void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode);

  void setupDefaultPalette();
  void updateRGB2PaletteLUT();

  // DMA related methods
  bool setDMABuffersCount(int buffersCount);
  void setDMABufferBlank(int index, void volatile * address, int length);
  void setDMABufferView(int index, int row, int scan);
  void volatile * getDMABuffer(int index, int * length);


  static VGA16Controller *   s_instance;
  static volatile int        s_scanLine;
  static lldesc_t volatile * s_frameResetDesc;

  intr_handle_t          m_isr_handle;

  TaskHandle_t           m_primitiveExecTask;

  volatile int           m_primitiveProcessingSuspended;             // 0 = enabled, >0 suspended

  GPIOStream             m_GPIOStream;

  int                    m_bitsPerChannel;  // 1 = 8 colors, 2 = 64 colors, set by begin()
  VGATimings             m_timings;
  int16_t                m_rawFrameHeight;

  volatile uint32_t      m_primitiveExecTimeoutCycles; // Maximum time (in CPU cycles) available for primitives drawing

  // true = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
  // false = allowed time is the half of an entire frame. Fast, but may flick
  bool                   m_processPrimitivesOnBlank;

  // These buffers contains a full line, with FrontPorch, Sync, BackPorch and blank visible area, in the
  // order specified by timings.HStartingBlock
  volatile uint8_t *     m_HBlankLine_withVSync;
  volatile uint8_t *     m_HBlankLine;
  int16_t                m_HLineSize;

  // contains H and V signals for visible line
  volatile uint8_t       m_HVSync;

  volatile int16_t       m_viewPortCol;
  volatile int16_t       m_viewPortRow;
  volatile int16_t       m_viewPortWidth;
  volatile int16_t       m_viewPortHeight;

  // when double buffer is enabled the "drawing" view port is always m_viewPort, while the "visible" view port is always m_viewPortVisible
  // when double buffer is not enabled then m_viewPort = m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  volatile uint8_t *     m_lines[VGA16_LinesCount];

  uint8_t *              m_viewPortMemoryPool[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT + 1];  // last allocated pool is nullptr

  lldesc_t volatile *    m_DMABuffers;

  int                    m_DMABuffersCount;

  gpio_num_t             m_VSyncGPIO;

  RGB222                 m_palette[16];
  uint8_t                m_packedRGB222_to_PaletteIndex[64];
  volatile uint16_t      m_packedPaletteIndexPair_to_signals[256];

  volatile bool          m_taskProcessingPrimitives;

};



} // end of namespace







