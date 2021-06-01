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
 * @brief This file contains fabgl::VGABaseController definition.
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




#define VGA_RED_BIT    0
#define VGA_GREEN_BIT  2
#define VGA_BLUE_BIT   4
#define VGA_HSYNC_BIT  6
#define VGA_VSYNC_BIT  7

#define VGA_SYNC_MASK  ((1 << VGA_HSYNC_BIT) | (1 << VGA_VSYNC_BIT))


// pixel 0 = byte 2, pixel 1 = byte 3, pixel 2 = byte 0, pixel 3 = byte 1 :
// pixel : 0  1  2  3  4  5  6  7  8  9 10 11 ...etc...
// byte  : 2  3  0  1  6  7  4  5 10 11  8  9 ...etc...
// dword : 0           1           2          ...etc...
// Thanks to https://github.com/paulscottrobson for the new macro. Before was: (row[((X) & 0xFFFC) + ((2 + (X)) & 3)])
#define VGA_PIXELINROW(row, X)   (row[(X) ^ 2])

// requires variables: m_viewPort
#define VGA_PIXEL(X, Y)          VGA_PIXELINROW(m_viewPort[(Y)], X)
#define VGA_INVERT_PIXEL(X, Y)   { auto px = &VGA_PIXEL((X), (Y)); *px = ~(*px ^ VGA_SYNC_MASK); }



namespace fabgl {



#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  extern volatile uint64_t s_vgapalctrlcycles;
#endif



/** \ingroup Enumerations
 * @brief Represents one of the four blocks of horizontal or vertical line
 */
enum VGAScanStart {
  FrontPorch,   /**< Horizontal line sequence is: FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA */
  Sync,         /**< Horizontal line sequence is: SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH */
  BackPorch,    /**< Horizontal line sequence is: BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC */
  VisibleArea   /**< Horizontal line sequence is: VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH */
};


/** @brief Specifies the VGA timings. This is a modeline decoded. */
struct VGATimings {
  char          label[22];       /**< Resolution text description */
  int           frequency;       /**< Pixel frequency (in Hz) */
  int16_t       HVisibleArea;    /**< Horizontal visible area length in pixels */
  int16_t       HFrontPorch;     /**< Horizontal Front Porch duration in pixels */
  int16_t       HSyncPulse;      /**< Horizontal Sync Pulse duration in pixels */
  int16_t       HBackPorch;      /**< Horizontal Back Porch duration in pixels */
  int16_t       VVisibleArea;    /**< Vertical number of visible lines */
  int16_t       VFrontPorch;     /**< Vertical Front Porch duration in lines */
  int16_t       VSyncPulse;      /**< Vertical Sync Pulse duration in lines */
  int16_t       VBackPorch;      /**< Vertical Back Porch duration in lines */
  char          HSyncLogic;      /**< Horizontal Sync polarity '+' or '-' */
  char          VSyncLogic;      /**< Vertical Sync polarity '+' or '-' */
  uint8_t       scanCount;       /**< Scan count. 1 = single scan, 2 = double scan (allowing low resolutions like 320x240...) */
  uint8_t       multiScanBlack;  /**< 0 = Additional rows are the repetition of the first. 1 = Additional rows are blank. */
  VGAScanStart  HStartingBlock;  /**< Horizontal starting block. DetermineshHorizontal order of signals */
};



class VGABaseController : public GenericBitmappedDisplayController {

public:

  VGABaseController();

  // unwanted methods
  VGABaseController(VGABaseController const&) = delete;
  void operator=(VGABaseController const&)    = delete;

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
   *     // Use GPIO 22 for red, GPIO 19 for green, GPIO 5 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_19, GPIO_NUM_5, GPIO_NUM_23, GPIO_NUM_15);
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

  virtual void end();

  static bool convertModelineToTimings(char const * modeline, VGATimings * timings);

  // abstract method of BitmappedDisplayController
  virtual void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  virtual void resumeBackgroundPrimitiveExecution();

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

  virtual void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  /**
   * @brief Determines horizontal position of the viewport.
   *
   * @return Horizontal position of the viewport (in case viewport width is less than screen width).
   */
  int getViewPortCol()                             { return m_viewPortCol; }

  /**
   * @brief Determines vertical position of the viewport.
   *
   * @return Vertical position of the viewport (in case viewport height is less than screen height).
   */
  int getViewPortRow()                             { return m_viewPortRow; }

  // abstract method of BitmappedDisplayController
  int getViewPortWidth()                           { return m_viewPortWidth; }

  // abstract method of BitmappedDisplayController
  int getViewPortHeight()                          { return m_viewPortHeight; }

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

  VGATimings * getResolutionTimings()             { return &m_timings; }

  /**
   * @brief Gets number of bits allocated for each channel.
   *
   * Number of bits depends by which begin() initializer has been called.
   *
   * @return 1 (8 colors) or 2 (64 colors).
   */
  uint8_t getBitsPerChannel()                     { return m_bitsPerChannel; }

  /**
   * @brief Gets a raw scanline pointer.
   *
   * A raw scanline must be filled with raw pixel colors. Use VGAController.createRawPixel to create raw pixel colors.
   * A raw pixel (or raw color) is a byte (uint8_t) that contains color information and synchronization signals.
   * Pixels are arranged in 32 bit packes as follows:
   *   pixel 0 = byte 2, pixel 1 = byte 3, pixel 2 = byte 0, pixel 3 = byte 1 :
   *   pixel : 0  1  2  3  4  5  6  7  8  9 10 11 ...etc...
   *   byte  : 2  3  0  1  6  7  4  5 10 11  8  9 ...etc...
   *   dword : 0           1           2          ...etc...
   *
   * @param y Vertical scanline position (0 = top row)
   */
  uint8_t * getScanline(int y)                    { return (uint8_t*) m_viewPort[y]; }

  /**
   * @brief Creates a raw pixel to use with VGAController.setRawPixel
   *
   * A raw pixel (or raw color) is a byte (uint8_t) that contains color information and synchronization signals.
   *
   * @param rgb Pixel RGB222 color
   *
   * Example:
   *
   *     // Set color of pixel at 100, 100
   *     VGAController.setRawPixel(100, 100, VGAController.createRawPixel(RGB222(3, 0, 0));
   */
  uint8_t createRawPixel(RGB222 rgb)             { return preparePixel(rgb); }

  /**
   * @brief Sets a raw pixel prepared using VGAController.createRawPixel.
   *
   * A raw pixel (or raw color) is a byte (uint8_t) that contains color information and synchronization signals.
   *
   * @param x Horizontal pixel position
   * @param y Vertical pixel position
   * @param rgb Raw pixel color
   *
   * Example:
   *
   *     // Set color of pixel at 100, 100
   *     VGAController.setRawPixel(100, 100, VGAController.createRawPixel(RGB222(3, 0, 0));
   */
  void setRawPixel(int x, int y, uint8_t rgb)    { VGA_PIXEL(x, y) = rgb; }


protected:

  static void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode);

  void startGPIOStream();

  void freeBuffers();

  virtual void freeViewPort();

  virtual void init();

  bool setDMABuffersCount(int buffersCount);

  uint8_t packHVSync(bool HSync, bool VSync);

  uint8_t inline __attribute__((always_inline)) preparePixel(RGB222 rgb) { return m_HVSync | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT); }

  uint8_t preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync);

  void fillVertBuffers(int offsetY);

  void fillHorizBuffers(int offsetX);

  int fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool hsync, bool vsync);

  int calcRequiredDMABuffersCount(int viewPortHeight);

  bool isMultiScanBlackLine(int scan);

  void setDMABufferBlank(int index, void volatile * address, int length, int scan, bool isStartOfVertFrontPorch);
  void setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA);
  void setDMABufferView(int index, int row, int scan, bool isStartOfVertFrontPorch);

  virtual void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow) = 0;

  void volatile * getDMABuffer(int index, int * length);

  void allocateViewPort(uint32_t allocCaps, int rowlen);
  virtual void allocateViewPort() = 0;
  virtual void checkViewPortSize() { };

  // abstract method of BitmappedDisplayController
  virtual void swapBuffers();


  // when double buffer is enabled the "drawing" view port is always m_viewPort, while the "visible" view port is always m_viewPortVisible
  // when double buffer is not enabled then m_viewPort = m_viewPortVisible
  volatile uint8_t * *   m_viewPort;
  volatile uint8_t * *   m_viewPortVisible;

  // true: double buffering is implemented in DMA
  bool                   m_doubleBufferOverDMA;

  volatile int           m_primitiveProcessingSuspended;             // 0 = enabled, >0 suspended

  volatile int16_t       m_viewPortWidth;
  volatile int16_t       m_viewPortHeight;

  intr_handle_t          m_isr_handle;

  VGATimings             m_timings;
  int16_t                m_HLineSize;

  volatile int16_t       m_viewPortCol;
  volatile int16_t       m_viewPortRow;

  // contains H and V signals for visible line
  volatile uint8_t       m_HVSync;


private:


  // bits per channel on VGA output
  // 1 = 8 colors, 2 = 64 colors, set by begin()
  int                    m_bitsPerChannel;

  GPIOStream             m_GPIOStream;

  lldesc_t volatile *    m_DMABuffers;
  int                    m_DMABuffersCount;

  // when double buffer is enabled at DMA level the running DMA buffer is always m_DMABuffersVisible
  // when double buffer is not enabled then m_DMABuffers = m_DMABuffersVisible
  lldesc_t volatile *    m_DMABuffersHead;
  lldesc_t volatile *    m_DMABuffersVisible;

  // These buffers contains a full line, with FrontPorch, Sync, BackPorch and blank visible area, in the
  // order specified by timings.HStartingBlock
  volatile uint8_t *     m_HBlankLine_withVSync;
  volatile uint8_t *     m_HBlankLine;

  uint8_t *              m_viewPortMemoryPool[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT + 1];  // last allocated pool is nullptr

  int16_t                m_rawFrameHeight;

};








} // end of namespace
