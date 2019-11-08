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
 * @brief This file contains fabgl::VGAController definition.
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
#include "swgenerator.h"
#include "displaycontroller.h"




namespace fabgl {


#define RED_BIT   0
#define GREEN_BIT 2
#define BLUE_BIT  4
#define HSYNC_BIT 6
#define VSYNC_BIT 7

#define SYNC_MASK ((1 << HSYNC_BIT) | (1 << VSYNC_BIT))


// pixel 0 = byte 2, pixel 1 = byte 3, pixel 2 = byte 0, pixel 3 = byte 1 :
// pixel : 0  1  2  3  4  5  6  7  8  9 10 11 ...etc...
// byte  : 2  3  0  1  6  7  4  5 10 11  8  9 ...etc...
// dword : 0           1           2          ...etc...
// Thanks to https://github.com/paulscottrobson for the new macro. Before was: (row[((X) & 0xFFFC) + ((2 + (X)) & 3)])
#define PIXELINROW(row, X) (row[(X) ^ 2])

// requires variables: m_viewPort
#define PIXEL(X, Y) PIXELINROW(m_viewPort[(Y)], X)



/** \ingroup Enumerations
 * @brief Represents one of the four blocks of horizontal or vertical line
 */
enum ScreenBlock {
  FrontPorch,   /**< Horizontal line sequence is: FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA */
  Sync,         /**< Horizontal line sequence is: SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH */
  BackPorch,    /**< Horizontal line sequence is: BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC */
  VisibleArea   /**< Horizontal line sequence is: VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH */
};


/** @brief Specifies the VGA timings. This is a modeline decoded. */
struct Timings {
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
  ScreenBlock   HStartingBlock;  /**< Horizontal starting block. DetermineshHorizontal order of signals */
};



/**
* @brief Represents the VGA controller
*
* Use this class to set screen resolution and to associate VGA signals to ESP32 GPIO outputs.
*
* This example initializes VGA Controller with 64 colors at 640x350:
*
*     fabgl::VGAController VGAController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     VGAController.begin();
*     VGAController.setResolution(VGA_640x350_70Hz);
*
* This example initializes VGA Controller with 8 colors (5 GPIOs used) and 640x350 resolution:
*
*     // Assign GPIO22 to Red, GPIO21 to Green, GPIO19 to Blue, GPIO18 to HSync and GPIO5 to VSync
*     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
*
*     // Set 640x350@70Hz resolution
*     VGAController.setResolution(VGA_640x350_70Hz);
*
* This example initializes VGA Controller with 64 colors (8 GPIOs used) and 640x350 resolution:
*
*     // Assign GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
*
*     // Set 640x350@70Hz resolution
*     VGAController.setResolution(VGA_640x350_70Hz);
*/
class VGAController : public DisplayController {

public:

  VGAController();

  // unwanted methods
  VGAController(VGAController const&)   = delete;
  void operator=(VGAController const&)  = delete;


  /**
   * @brief Returns the singleton instance of VGAController class
   *
   * @return A pointer to VGAController singleton object
   */
  static VGAController * instance() { return s_instance; }


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

  /**
   * @brief Gets number of bits allocated for each channel.
   *
   * Number of bits depends by which begin() initializer has been called.
   *
   * @return 1 (8 colors) or 2 (64 colors).
   */
  uint8_t getBitsPerChannel() { return m_bitsPerChannel; }

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

  void setResolution(Timings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  Timings * getResolutionTimings() { return &m_timings; }

  /**
   * @brief Determines the screen width in pixels.
   *
   * @return Screen width in pixels.
   */
  int getScreenWidth()    { return m_timings.HVisibleArea; }

  /**
   * @brief Determines the screen height in pixels.
   *
   * @return Screen height in pixels.
   */
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

  /**
   * @brief Determines horizontal size of the viewport.
   *
   * @return Horizontal size of the viewport.
   */
  int getViewPortWidth()  { return m_viewPortWidth; }

  /**
   * @brief Determines vertical size of the viewport.
   *
   * @return Vertical size of the viewport.
   */
  int getViewPortHeight() { return m_viewPortHeight; }

  void addPrimitive(Primitive const & primitive);

  void primitivesExecutionWait();

  /**
   * @brief Enables or disables drawings inside vertical retracing time.
   *
   * When vertical retracing occurs (on Vertical Sync) an interrupt is trigged. Inside this interrupt primitives
   * like line, circles, glyphs, etc.. are painted.<br>
   * This method can disable (or reenable) this behavior, making drawing instantaneous. Flickering may occur when
   * drawings are executed out of retracing time.<br>
   * When background executing is disabled the queue is emptied executing all pending primitives.
   *
   * @param value When true drawings are done during vertical retracing, when false drawings are executed instantly.
   */
  void enableBackgroundPrimitiveExecution(bool value);

  /**
   * @brief Enables or disables execution time limitation inside vertical retracing interrupt
   *
   * Disabling interrupt execution timeout may generate flickering but speedup drawing operations.
   *
   * @param value True enables timeout (default), False disables timeout
   */
  void enableBackgroundPrimitiveTimeout(bool value) { m_backgroundPrimitiveTimeoutEnabled = value; }

  bool backgroundPrimitiveTimeoutEnabled()          { return m_backgroundPrimitiveTimeoutEnabled; }

  /**
   * @brief Suspends drawings.
   *
   * Suspends drawings disabling vertical sync interrupt.<br>
   * After call to suspendBackgroundPrimitiveExecution() adding new primitives may cause a deadlock.<br>
   * To avoid it a call to "processPrimitives()" should be performed very often.<br>
   * This method maintains a counter so can be nested.
   */
  void suspendBackgroundPrimitiveExecution();

  /**
   * @brief Resumes drawings after suspendBackgroundPrimitiveExecution().
   *
   * Resumes drawings enabling vertical sync interrupt.
   */
  void resumeBackgroundPrimitiveExecution();

  /**
   * @brief Draws immediately all primitives in the queue.
   *
   * Draws all primitives before they are processed in the vertical sync interrupt.<br>
   * May generate flickering because don't care of vertical sync.
   */
  void processPrimitives();

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

  /**
   * @brief Sets the list of active sprites.
   *
   * A sprite is an image that keeps background unchanged.<br>
   * There is no limit to the number of active sprites, but flickering and slow
   * refresh happens when a lot of sprites (or large sprites) are visible.<br>
   * To empty the list of active sprites call VGAController.removeSprites().
   *
   * @param sprites The list of sprites to make currently active.
   * @param count Number of sprites in the list.
   *
   * Example:
   *
   *     // define a sprite with user data (velX and velY)
   *     struct MySprite : Sprite {
   *       int  velX;
   *       int  velY;
   *     };
   *
   *     static MySprite sprites[10];
   *
   *     VGAController.setSprites(sprites, 10);
   */
  template <typename T>
  void setSprites(T * sprites, int count) {
    setSprites(sprites, count, sizeof(T));
  }

  /**
   * @brief Empties the list of active sprites.
   *
   * Call this method when you don't need active sprites anymore.
   */
  void removeSprites() { setSprites(nullptr, 0, 0); }

  /**
   * @brief Forces the sprites to be updated.
   *
   * Screen is automatically updated whenever a primitive is painted (look at Canvas).<br>
   * When a sprite updates its image or its position (or any other property) it is required
   * to force a refresh using this method.<br>
   * VGAController.refreshSprites() is required also when using the double buffered mode, to paint sprites.
   */
  void refreshSprites();

  /**
   * @brief Determines whether VGAController is on double buffered mode.
   *
   * @return True if VGAController is on double buffered mode.
   */
  bool isDoubleBuffered() { return m_doubleBuffered; }

  /**
   * @brief Sets mouse cursor and make it visible.
   *
   * @param cursor Cursor to use when mouse pointer need to be painted. nullptr = disable mouse pointer.
   */
  void setMouseCursor(Cursor const * cursor);

  /**
   * @brief Sets mouse cursor from a set of predefined cursors.
   *
   * @param cursorName Name (enum) of predefined cursor.
   *
   * Example:
   *
   *     VGAController.setMouseCursor(CursorName::CursorPointerShadowed);
   */
  void setMouseCursor(CursorName cursorName);

  /**
   * @brief Sets mouse cursor position.
   *
   * @param X Mouse cursor horizontal position.
   * @param Y Mouse cursor vertical position.
   */
  void setMouseCursorPos(int X, int Y);

  /**
   * @brief Reads pixels inside the specified rectangle.
   *
   * Screen reading may occur while other drawings are in progress, so the result may be not updated.
   *
   * @param rect Screen rectangle to read. To improve performance rectangle is not checked.
   * @param destBuf Destination buffer. Buffer size must be at least rect.width() * rect.height.
   *
   * Example:
   *
   *     // paint a red rectangle
   *     Canvas.setBrushColor(RGB(3, 0, 0));
   *     Rect rect = Rect(10, 10, 100, 100);
   *     Canvas.fillRectangle(rect);
   *
   *     // wait for vsync (hence actual drawing)
   *     VGAController.processPrimitives();
   *
   *     // read rectangle pixels into "buf"
   *     auto buf = new RGB[rect.width() * rect.height()];
   *     VGAController.readScreen(rect, buf);
   *
   *     // write buf 110 pixels to the reight
   *     VGAController.writeScreen(rect.translate(110, 0), buf);
   *     delete buf;
   */
  void readScreen(Rect const & rect, RGB * destBuf);

  /**
   * @brief Writes pixels inside the specified rectangle.
   *
   * Screen writing may occur while other drawings are in progress, so written pixels may be overlapped or mixed.
   *
   * @param rect Screen rectangle to write. To improve performance rectangle is not checked.
   * @param srcBuf Source buffer. Buffer size must be at least rect.width() * rect.height.
   *
   * Example:
   *
   *     // paint a red rectangle
   *     Canvas.setBrushColor(RGB(3, 0, 0));
   *     Rect rect = Rect(10, 10, 100, 100);
   *     Canvas.fillRectangle(rect);
   *
   *     // wait for vsync (hence actual drawing)
   *     VGAController.processPrimitives();
   *
   *     // read rectangle pixels into "buf"
   *     auto buf = new RGB[rect.width() * rect.height()];
   *     VGAController.readScreen(rect, buf);
   *
   *     // write buf 110 pixels to the reight
   *     VGAController.writeScreen(rect.translate(110, 0), buf);
   *     delete buf;
   */
  void writeScreen(Rect const & rect, RGB * srcBuf);

  /**
   * @brief Creates a raw pixel to use with VGAController.setRawPixel
   *
   * A raw pixel (or raw color) is a byte (uint8_t) that contains color information and synchronization signals.
   *
   * @param rgb Pixel RGB color
   *
   * Example:
   *
   *     // Set color of pixel at 100, 100
   *     VGAController.setRawPixel(100, 100, VGAController.createRawPixel(RGB(3, 0, 0));
   */
  uint8_t createRawPixel(RGB rgb)             { return preparePixel(rgb); }

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
   *     VGAController.setRawPixel(100, 100, VGAController.createRawPixel(RGB(3, 0, 0));
   */
  void setRawPixel(int x, int y, uint8_t rgb) { PIXEL(x, y) = rgb; }

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
  uint8_t * getScanline(int y)                { return (uint8_t*) m_viewPort[y]; }

private:

  void init(gpio_num_t VSyncGPIO);

  uint8_t packHVSync(bool HSync = false, bool VSync = false);
  uint8_t preparePixel(RGB rgb, bool HSync = false, bool VSync = false);

  void freeBuffers();
  void fillHorizBuffers(int offsetX);
  void fillVertBuffers(int offsetY);
  int fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool hsync, bool vsync);
  void allocateViewPort();
  void freeViewPort();
  int calcRequiredDMABuffersCount(int viewPortHeight);

  void execPrimitive(Primitive const & prim);

  void execSetPixel(Point const & position);
  void execSetPixelAt(PixelDesc const & pixelDesc);
  void execLineTo(Point const & position);
  void execFillRect(Rect const & rect);
  void execDrawRect(Rect const & rect);
  void execFillEllipse(Size const & size);
  void execDrawEllipse(Size const & size);
  void execClear();
  void execVScroll(int scroll);
  void execHScroll(int scroll);
  void execDrawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB penColor, RGB brushColor);
  void execDrawGlyph_full(Glyph const & glyph, GlyphOptions glyphOptions, RGB penColor, RGB brushColor);
  void execDrawGlyph_light(Glyph const & glyph, GlyphOptions glyphOptions, RGB penColor, RGB brushColor);
  void execInvertRect(Rect const & rect);
  void execCopyRect(Rect const & source);
  void execSwapFGBG(Rect const & rect);
  void execRenderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo);
  void execDrawBitmap(BitmapDrawingInfo const & bitmapDrawingInfo);
  void execSwapBuffers();
  void execDrawPath(Path const & path);
  void execFillPath(Path const & path);

  void updateAbsoluteClippingRect();

  void drawBitmap(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, bool ignoreClippingRect);

  void fillRow(int y, int x1, int x2, uint8_t pattern);
  void swapRows(int yA, int yB, int x1, int x2);

  void drawLine(int X1, int Y1, int X2, int Y2, uint8_t pattern);

  void hideSprites();
  void showSprites();

  void setSprites(Sprite * sprites, int count, int spriteSize);

  static void VSyncInterrupt();

  static void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode);

  // DMA related methods
  bool setDMABuffersCount(int buffersCount);
  void setDMABufferBlank(int index, void volatile * address, int length);
  void setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA);
  void setDMABufferView(int index, int row, int scan);
  void volatile * getDMABuffer(int index, int * length);


  static VGAController * s_instance;

  GPIOStream             m_GPIOStream;

  int                    m_bitsPerChannel;  // 1 = 8 colors, 2 = 64 colors, set by begin()
  Timings                m_timings;
  int16_t                m_linesCount;
  volatile int16_t       m_maxVSyncISRTime; // Maximum us VSync interrupt routine can run

  // These buffers contains a full line, with FrontPorch, Sync, BackPorch and blank visible area, in the
  // order specified by timings.HStartingBlock
  volatile uint8_t *     m_HBlankLine_withVSync;
  volatile uint8_t *     m_HBlankLine;
  int16_t                m_HLineSize;

  bool                   m_doubleBuffered;

  volatile int16_t       m_viewPortCol;
  volatile int16_t       m_viewPortRow;
  volatile int16_t       m_viewPortWidth;
  volatile int16_t       m_viewPortHeight;

  // when double buffer is enabled the "drawing" view port is always m_viewPort, while the "visible" view port is always m_viewPortVisible
  // when double buffer is not enabled then m_viewPort = m_viewPortVisible
  volatile uint8_t * *   m_viewPort;
  volatile uint8_t * *   m_viewPortVisible;

  uint8_t *              m_viewPortMemoryPool[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT + 1];  // last allocated pool is nullptr

  volatile QueueHandle_t m_execQueue;
  PaintState             m_paintState;

  // when double buffer is enabled the running DMA buffer is always m_DMABuffersRunning
  // when double buffer is not enabled then m_DMABuffers = m_DMABuffersRunning
  lldesc_t volatile *    m_DMABuffersHead;
  lldesc_t volatile *    m_DMABuffers;
  lldesc_t volatile *    m_DMABuffersVisible;

  int                    m_DMABuffersCount;

  gpio_num_t             m_VSyncGPIO;
  int                    m_VSyncInterruptSuspended;             // 0 = enabled, >0 suspended
  bool                   m_backgroundPrimitiveExecutionEnabled; // when False primitives are execute immediately
  volatile bool          m_backgroundPrimitiveTimeoutEnabled;   // when False VSyncInterrupt() has not timeout

  void *                 m_sprites;       // pointer to array of sprite structures
  int                    m_spriteSize;    // size of sprite structure
  int                    m_spritesCount;  // number of sprites in m_sprites array

  bool                   m_spritesHidden; // true between hideSprites() and showSprites()

  // mouse cursor (mouse pointer) support
  Sprite                 m_mouseCursor;
  int16_t                m_mouseHotspotX;
  int16_t                m_mouseHotspotY;

};



} // end of namespace







