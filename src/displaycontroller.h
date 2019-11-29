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
 * @brief This file contains fabgl::DisplayController definition.
 */



#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"




namespace fabgl {



/*
  Notes:
    - all positions can have negative and outofbound coordinates. Shapes are always clipped correctly.
*/
enum PrimitiveCmd {

  // Refresh display. Some displays (ie SSD1306) aren't repainted if there aren't primitives
  // so posting Refresh allows to resend the screenbuffer
  Refresh,

  // Set current pen color
  // params: color
  SetPenColor,

  // Set current brush color
  // params: color
  SetBrushColor,

  // Paint a pixel at specified coordinates, using current pen color
  // params: color
  SetPixel,

  // Paint a pixel at specified coordinates using the specified color
  // params: pixelDesc
  SetPixelAt,

  // Move current position to the specified one
  // params: point
  MoveTo,

  // Draw a line from current position to the specified one, using current pen color. Update current position.
  // params: point
  LineTo,

  // Fill a rectangle using current brush color
  // params: rect
  FillRect,

  // Draw a rectangle using current pen color
  // params: rect
  DrawRect,

  // Fill an ellipse, current position is the center, using current brush color
  // params: size
  FillEllipse,

  // Draw an ellipse, current position is the center, using current pen color
  // params: size
  DrawEllipse,

  // Fill viewport with brush color
  // params: none
  Clear,

  // Scroll vertically without copying buffers
  // params: ivalue (scroll amount, can be negative)
  VScroll,

  // Scroll horizontally (time consuming operation!)
  // params: ivalue (scroll amount, can be negative)
  HScroll,

  // Draw a glyph (BW image)
  // params: glyph
  DrawGlyph,

  // Set paint options
  // params: glyphOptions
  SetGlyphOptions,

  // Set gluph options
  // params: paintOptions
  SetPaintOptions,

  // Invert a rectangle
  // params: rect
  InvertRect,

  // Copy (overlapping) rectangle to current position
  // params: rect (source rectangle)
  CopyRect,

  // Set scrolling region
  // params: rect
  SetScrollingRegion,

  // Swap foreground (pen) and background (brush) colors of all pixels inside the specified rectangles. Other colors remain untaltered.
  // params: rect
  SwapFGBG,

  // Render glyphs buffer
  // params: glyphsBufferRenderInfo
  RenderGlyphsBuffer,

  // Draw a bitmap
  // params: bitmapDrawingInfo
  DrawBitmap,

  // Refresh sprites
  // no params
  RefreshSprites,

  // Swap buffers (m_doubleBuffered must be True)
  SwapBuffers,

  // Fill a path, using current brush color
  // params: path
  FillPath,

  // Draw a path, using current pen color
  // params: path
  DrawPath,

  // Set axis origin
  // params: point
  SetOrigin,

  // Set clipping rectangle
  // params: rect
  SetClippingRect,
};



/** \ingroup Enumerations
 * @brief This enum defines named colors.
 *
 * First eight full implement all available colors when 1 bit per channel mode is used (having 8 colors).
 */
enum Color {
  Black,          /**< Equivalent to RGB222(0,0,0) and RGB888(0,0,0) */
  Red,            /**< Equivalent to RGB222(2,0,0) and RGB888(128,0,0) */
  Green,          /**< Equivalent to RGB222(0,2,0) and RGB888(0,128,0) */
  Yellow,         /**< Equivalent to RGB222(2,2,0) and RGB888(128,128,0) */
  Blue,           /**< Equivalent to RGB222(0,0,2) and RGB888(0,0,128) */
  Magenta,        /**< Equivalent to RGB222(2,0,2) and RGB888(128,0,128) */
  Cyan,           /**< Equivalent to RGB222(0,2,2) and RGB888(0,128,128) */
  White,          /**< Equivalent to RGB222(2,2,2) and RGB888(128,128,128) */
  BrightBlack,    /**< Equivalent to RGB222(1,1,1) and RGB888(64,64,64) */
  BrightRed,      /**< Equivalent to RGB222(3,0,0) and RGB888(255,0,0) */
  BrightGreen,    /**< Equivalent to RGB222(0,3,0) and RGB888(0,255,0) */
  BrightYellow,   /**< Equivalent to RGB222(3,3,0) and RGB888(255,255,0) */
  BrightBlue,     /**< Equivalent to RGB222(0,0,3) and RGB888(0,0,255) */
  BrightMagenta,  /**< Equivalent to RGB222(3,0,3) and RGB888(255,0,255) */
  BrightCyan,     /**< Equivalent to RGB222(0,3,3) and RGB888(0,255,255) */
  BrightWhite,    /**< Equivalent to RGB222(3,3,3) and RGB888(255,255,255) */
};



/**
 * @brief Represents a 24 bit RGB color.
 *
 * For each channel minimum value is 0, maximum value is 255.
 */
struct RGB888 {
  uint8_t R;  /**< The Red channel   */
  uint8_t G;  /**< The Green channel */
  uint8_t B;  /**< The Blue channel  */

  RGB888() : R(0), G(0), B(0) { }
  RGB888(Color color);
  RGB888(uint8_t red, uint8_t green, uint8_t blue) : R(red), G(green), B(blue) { }
};


inline bool operator==(RGB888 const& lhs, RGB888 const& rhs)
{
  return lhs.R == rhs.R && lhs.G == rhs.G && lhs.B == rhs.B;
}


inline bool operator!=(RGB888 const& lhs, RGB888 const& rhs)
{
  return lhs.R != rhs.R || lhs.G != rhs.G || lhs.B == rhs.B;
}



/**
 * @brief Represents a 32 bit RGBA color.
 *
 * For each channel minimum value is 0, maximum value is 255.
 */
struct RGBA8888 {
  uint8_t R;  /**< The Red channel   */
  uint8_t G;  /**< The Green channel */
  uint8_t B;  /**< The Blue channel  */
  uint8_t A;  /**< The Alpha channel */

  RGBA8888(int red, int green, int blue, int alpha) : R(red), G(green), B(blue), A(alpha) { }
};



/**
 * @brief Represents a 6 bit RGB color.
 *
 * When 1 bit per channel (8 colors) is used then the maximum value (white) is 1 (R = 1, G = 1, B = 1).
 * When 2 bits per channel (64 colors) are used then the maximum value (white) is 3 (R = 3, G = 3, B = 3).
 */
struct RGB222 {
  uint8_t R : 2;  /**< The Red channel   */
  uint8_t G : 2;  /**< The Green channel */
  uint8_t B : 2;  /**< The Blue channel  */

  RGB222() : R(0), G(0), B(0) { }
  RGB222(Color color);
  RGB222(uint8_t red, uint8_t green, uint8_t blue) : R(red), G(green), B(blue) { }
  RGB222(RGB888 const & value);

  static void optimizeFor64Colors();
  uint8_t pack() { return R | (G << 2) | (B << 4); }
};


inline bool operator==(RGB222 const& lhs, RGB222 const& rhs)
{
  return lhs.R == rhs.R && lhs.G == rhs.G && lhs.B == rhs.B;
}


inline bool operator!=(RGB222 const& lhs, RGB222 const& rhs)
{
  return lhs.R != rhs.R || lhs.G != rhs.G || lhs.B == rhs.B;
}


/**
 * @brief Represents an 8 bit ABGR color
 *
 * For each channel minimum value is 0, maximum value is 3.
 */
struct RGBA2222 {
  uint8_t R : 2;  /**< The Red channel (LSB) */
  uint8_t G : 2;  /**< The Green channel */
  uint8_t B : 2;  /**< The Blue channel  */
  uint8_t A : 2;  /**< The Alpha channel (MSB) */

  RGBA2222(int red, int green, int blue, int alpha) : R(red), G(green), B(blue), A(alpha) { }
};


/**
 * @brief Represents a glyph position, size and binary data.
 *
 * A glyph is a bitmap (1 bit per pixel). The fabgl::Terminal uses glyphs to render characters.
 */
struct Glyph {
  int16_t         X;      /**< Horizontal glyph coordinate */
  int16_t         Y;      /**< Vertical glyph coordinate */
  int16_t         width;  /**< Glyph horizontal size */
  int16_t         height; /**< Glyph vertical size */
  uint8_t const * data;   /**< Byte aligned binary data of the glyph. A 0 represents background or a transparent pixel. A 1 represents foreground. */

  Glyph() : X(0), Y(0), width(0), height(0), data(nullptr) { }
  Glyph(int X_, int Y_, int width_, int height_, uint8_t const * data_) : X(X_), Y(Y_), width(width_), height(height_), data(data_) { }
};



/**
 * @brief Specifies various glyph painting options.
 */
union GlyphOptions {
  struct {
    uint16_t fillBackground   : 1;  /**< If enabled glyph background is filled with current background color. */
    uint16_t bold             : 1;  /**< If enabled produces a bold-like style. */
    uint16_t reduceLuminosity : 1;  /**< If enabled reduces luminosity. To implement characters faint. */
    uint16_t italic           : 1;  /**< If enabled skews the glyph on the right. To implement characters italic. */
    uint16_t invert           : 1;  /**< If enabled swaps foreground and background colors. To implement characters inverse (XORed with PaintOptions.swapFGBG) */
    uint16_t blank            : 1;  /**< If enabled the glyph is filled with the background color. To implement characters invisible or blink. */
    uint16_t underline        : 1;  /**< If enabled the glyph is underlined. To implement characters underline. */
    uint16_t doubleWidth      : 2;  /**< If enabled the glyph is doubled. To implement characters double width. 0 = normal, 1 = double width, 2 = double width - double height top, 3 = double width - double height bottom. */
    uint16_t userOpt1         : 1;  /**< User defined option */
    uint16_t userOpt2         : 1;  /**< User defined option */
  };
  uint16_t value;

  /** @brief Helper method to set or reset fillBackground */
  GlyphOptions & FillBackground(bool value) { fillBackground = value; return *this; }

  /** @brief Helper method to set or reset bold */
  GlyphOptions & Bold(bool value) { bold = value; return *this; }

  /** @brief Helper method to set or reset italic */
  GlyphOptions & Italic(bool value) { italic = value; return *this; }

  /** @brief Helper method to set or reset underlined */
  GlyphOptions & Underline(bool value) { underline = value; return *this; }

  /** @brief Helper method to set or reset doubleWidth */
  GlyphOptions & DoubleWidth(uint8_t value) { doubleWidth = value; return *this; }

  /** @brief Helper method to set or reset foreground and background swapping */
  GlyphOptions & Invert(uint8_t value) { invert = value; return *this; }
};



// GlyphsBuffer.map support functions
//  0 ..  7 : index
//  8 .. 11 : BG color (Color)
// 12 .. 15 : FG color (Color)
// 16 .. 31 : options (GlyphOptions)
// note: volatile pointer to avoid optimizer to get less than 32 bit from 32 bit access only memory
#define GLYPHMAP_INDEX_BIT    0
#define GLYPHMAP_BGCOLOR_BIT  8
#define GLYPHMAP_FGCOLOR_BIT 12
#define GLYPHMAP_OPTIONS_BIT 16
#define GLYPHMAP_ITEM_MAKE(index, bgColor, fgColor, options) (((uint32_t)(index) << GLYPHMAP_INDEX_BIT) | ((uint32_t)(bgColor) << GLYPHMAP_BGCOLOR_BIT) | ((uint32_t)(fgColor) << GLYPHMAP_FGCOLOR_BIT) | ((uint32_t)((options).value) << GLYPHMAP_OPTIONS_BIT))
inline uint8_t glyphMapItem_getIndex(uint32_t const volatile * mapItem) { return *mapItem >> GLYPHMAP_INDEX_BIT & 0xFF; }
inline Color glyphMapItem_getBGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_BGCOLOR_BIT & 0x0F); }
inline Color glyphMapItem_getFGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_FGCOLOR_BIT & 0x0F); }
inline GlyphOptions glyphMapItem_getOptions(uint32_t const volatile * mapItem) { return (GlyphOptions){.value = (uint16_t)(*mapItem >> GLYPHMAP_OPTIONS_BIT & 0xFFFF)}; }
inline void glyphMapItem_setOptions(uint32_t volatile * mapItem, GlyphOptions const & options) { *mapItem = (*mapItem & ~((uint32_t)0xFFFF << GLYPHMAP_OPTIONS_BIT)) | ((uint32_t)(options.value) << GLYPHMAP_OPTIONS_BIT); }

struct GlyphsBuffer {
  int16_t         glyphsWidth;
  int16_t         glyphsHeight;
  uint8_t const * glyphsData;
  int16_t         columns;
  int16_t         rows;
  uint32_t *      map;  // look at glyphMapItem_... inlined functions
};


struct GlyphsBufferRenderInfo {
  int16_t              itemX;  // starts from 0
  int16_t              itemY;  // starts from 0
  GlyphsBuffer const * glyphsBuffer;

  GlyphsBufferRenderInfo(int itemX_, int itemY_, GlyphsBuffer const * glyphsBuffer_) : itemX(itemX_), itemY(itemY_), glyphsBuffer(glyphsBuffer_) { }
};


/** \ingroup Enumerations
 * @brief This enum defines the display controller native pixel format
 */
enum class NativePixelFormat : uint8_t {
  Mono,       /**< 1 bit per pixel. 0 = black, 1 = white */
  SBGR2222,   /**< 8 bit per pixel: VHBBGGRR (bit 7=VSync 6=HSync 5=B 4=B 3=G 2=G 1=R 0=R). Each color channel can have values from 0 to 3 (maxmum intensity). */
};


/** \ingroup Enumerations
 * @brief This enum defines a pixel format
 */
enum class PixelFormat : uint8_t {
  Undefined,  /**< Undefined pixel format */
  Mask,       /**< 1 bit per pixel. 0 = transparent, 1 = opaque (color must be specified apart) */
  RGBA2222,   /**< 8 bit per pixel: AABBGGRR (bit 7=A 6=A 5=B 4=B 3=G 2=G 1=R 0=R). AA = 0 fully transparent, AA = 3 fully opaque. Each color channel can have values from 0 to 3 (maxmum intensity). */
  RGBA8888    /**< 32 bits per pixel: RGBA (R=byte 0, G=byte1, B=byte2, A=byte3). Minimum value for each channel is 0, maximum is 255. */
};


// returns row length using the specified pixel format, in bytes
int getRowLength(int width, PixelFormat format);


/**
 * @brief Represents an image
 */
struct Bitmap {
  int16_t         width;           /**< Bitmap horizontal size */
  int16_t         height;          /**< Bitmap vertical size */
  PixelFormat     format;          /**< Bitmap pixel format */
  RGB888          foregroundColor; /**< Foreground color when format is PixelFormat::Mask */
  uint8_t *       data;            /**< Bitmap binary data */
  bool            dataAllocated;   /**< If true data is released when bitmap is destroyed */

  Bitmap() : width(0), height(0), format(PixelFormat::Undefined), foregroundColor(RGB888(255, 255, 255)), data(nullptr), dataAllocated(false) { }
  Bitmap(int width_, int height_, void const * data_, PixelFormat format_, bool copy = false);
  Bitmap(int width_, int height_, void const * data_, PixelFormat format_, RGB888 foregroundColor_, bool copy = false);
  ~Bitmap();

  void setPixel(int x, int y, int value);       // use with PixelFormat::Mask. value can be 0 or not 0
  void setPixel(int x, int y, RGBA2222 value);  // use with PixelFormat::RGBA2222
  void setPixel(int x, int y, RGBA8888 value);  // use with PixelFormat::RGBA8888

  int getAlpha(int x, int y);

private:
  void allocate();
  void copyFrom(void const * srcData);
};


struct BitmapDrawingInfo {
  int16_t        X;
  int16_t        Y;
  Bitmap const * bitmap;

  BitmapDrawingInfo(int X_, int Y_, Bitmap const * bitmap_) : X(X_), Y(Y_), bitmap(bitmap_) { }
};


/** \ingroup Enumerations
 * @brief This enum defines a set of predefined mouse cursors.
 */
enum CursorName : uint8_t {
  CursorPointerAmigaLike,     /**< 11x11 Amiga like colored mouse pointer */
  CursorPointerSimpleReduced, /**< 10x15 mouse pointer */
  CursorPointerSimple,        /**< 11x19 mouse pointer */
  CursorPointerShadowed,      /**< 11x19 shadowed mouse pointer */
  CursorPointer,              /**< 12x17 mouse pointer */
  CursorPen,                  /**< 16x16 pen */
  CursorCross1,               /**< 9x9 cross */
  CursorCross2,               /**< 11x11 cross */
  CursorPoint,                /**< 5x5 point */
  CursorLeftArrow,            /**< 11x11 left arrow */
  CursorRightArrow,           /**< 11x11 right arrow */
  CursorDownArrow,            /**< 11x11 down arrow */
  CursorUpArrow,              /**< 11x11 up arrow */
  CursorMove,                 /**< 19x19 move */
  CursorResize1,              /**< 12x12 resize orientation 1 */
  CursorResize2,              /**< 12x12 resize orientation 2 */
  CursorResize3,              /**< 11x17 resize orientation 3 */
  CursorResize4,              /**< 17x11 resize orientation 4 */
  CursorTextInput,            /**< 7x15 text input */
};


/**
 * @brief Defines a cursor.
 */
struct Cursor {
  int16_t hotspotX;           /**< Cursor horizontal hotspot (0 = left bitmap side) */
  int16_t hotspotY;           /**< Cursor vertical hotspot (0 = upper bitmap side) */
  Bitmap  bitmap;             /**< Cursor bitmap */
};


struct QuadTreeObject;


/**
 * @brief Represents a sprite.
 *
 * A sprite contains one o more bitmaps (fabgl::Bitmap object) and has
 * a position in a scene (fabgl::Scane class). Only one bitmap is displayed at the time.<br>
 * It can be included in a collision detection group (fabgl::CollisionDetector class).
 * Bitmaps can have different sizes.
 */
struct Sprite {
  volatile int16_t   x;
  volatile int16_t   y;
  Bitmap * *         frames;  // array of pointer to Bitmap
  int16_t            framesCount;
  int16_t            currentFrame;
  int16_t            savedX;
  int16_t            savedY;
  int16_t            savedBackgroundWidth;
  int16_t            savedBackgroundHeight;
  uint8_t *          savedBackground;
  QuadTreeObject *   collisionDetectorObject;
  struct {
    uint8_t visible:  1;
    // A static sprite should be positioned before dynamic sprites.
    // It is never re-rendered unless allowDraw is 1. Static sprites always sets allowDraw=0 after drawings.
    uint8_t isStatic:  1;
    // This is always '1' for dynamic sprites and always '0' for static sprites.
    uint8_t allowDraw: 1;
  };

  Sprite();
  ~Sprite();
  Bitmap * getFrame() { return frames ? frames[currentFrame] : nullptr; }
  int getFrameIndex() { return currentFrame; }
  void nextFrame() { ++currentFrame; if (currentFrame >= framesCount) currentFrame = 0; }
  Sprite * setFrame(int frame) { currentFrame = frame; return this; }
  Sprite * addBitmap(Bitmap * bitmap);
  Sprite * addBitmap(Bitmap * bitmap[], int count);
  void clearBitmaps();
  int getWidth()  { return frames[currentFrame]->width; }
  int getHeight() { return frames[currentFrame]->height; }
  Sprite * moveBy(int offsetX, int offsetY);
  Sprite * moveBy(int offsetX, int offsetY, int wrapAroundWidth, int wrapAroundHeight);
  Sprite * moveTo(int x, int y);
};


struct Path {
  Point const * points;
  int           pointsCount;
};


/**
 * @brief Specifies general paint options.
 */
struct PaintOptions {
  uint8_t swapFGBG : 1;  /**< If enabled swaps foreground and background colors */
  uint8_t NOT      : 1;  /**< If enabled performs NOT logical operator on destination. Implemented only for straight lines and non-filled rectangles. */

  PaintOptions() : swapFGBG(false), NOT(false) { }
};


struct PixelDesc {
  Point  pos;
  RGB888 color;
};


struct Primitive {
  PrimitiveCmd cmd;
  union {
    int16_t                ivalue;
    RGB888                 color;
    Point                  position;
    Size                   size;
    Glyph                  glyph;
    Rect                   rect;
    GlyphOptions           glyphOptions;
    PaintOptions           paintOptions;
    GlyphsBufferRenderInfo glyphsBufferRenderInfo;
    BitmapDrawingInfo      bitmapDrawingInfo;
    Path                   path;
    PixelDesc              pixelDesc;
  };

  Primitive() { }
  Primitive(PrimitiveCmd cmd_) : cmd(cmd_) { }
};


struct PaintState {
  RGB888       penColor;
  RGB888       brushColor;
  Point        position;        // value already traslated to "origin"
  GlyphOptions glyphOptions;
  PaintOptions paintOptions;
  Rect         scrollingRegion;
  Point        origin;
  Rect         clippingRect;    // relative clipping rectangle
  Rect         absClippingRect; // actual absolute clipping rectangle (calculated when setting "origin" or "clippingRect")
};




/**
 * @brief Represents the base abstract class for display controllers
 */
class DisplayController {

public:

  DisplayController();

  ~DisplayController();

  /**
   * @brief Determines horizontal size of the viewport.
   *
   * @return Horizontal size of the viewport.
   */
  virtual int getViewPortWidth() = 0;

  /**
   * @brief Determines vertical size of the viewport.
   *
   * @return Vertical size of the viewport.
   */
  virtual int getViewPortHeight() = 0;

  /**
   * @brief Determines the screen width in pixels.
   *
   * @return Screen width in pixels.
   */
  virtual int getScreenWidth() = 0;

  /**
   * @brief Determines the screen height in pixels.
   *
   * @return Screen height in pixels.
   */
  virtual int getScreenHeight() = 0;

  /**
   * @brief Represents the native pixel format used by this display.
   *
   * @return Display native pixel format
   */
  virtual NativePixelFormat nativePixelFormat() = 0;

  PaintState & paintState() { return m_paintState; }

  void addPrimitive(Primitive const & primitive);

  void primitivesExecutionWait();

  /**
   * @brief Enables or disables drawings inside vertical retracing time.
   *
   * Primitives are processed in background (for example in vertical retracing for VGA or in a separated task for SPI/I2C displays).
   * This method can disable (or reenable) this behavior, making drawing instantaneous. Flickering may occur when
   * drawings are executed out of retracing time.<br>
   * When background executing is disabled the queue is emptied executing all pending primitives.
   * Some displays (SPI/I2C) may be not updated at all when enableBackgroundPrimitiveExecution is False.
   *
   * @param value When true drawings are done on background, when false drawings are executed instantly.
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
  virtual void suspendBackgroundPrimitiveExecution() = 0;

  /**
   * @brief Resumes drawings after suspendBackgroundPrimitiveExecution().
   *
   * Resumes drawings enabling vertical sync interrupt.
   */
  virtual void resumeBackgroundPrimitiveExecution() = 0;

  /**
   * @brief Draws immediately all primitives in the queue.
   *
   * Draws all primitives before they are processed in the vertical sync interrupt.<br>
   * May generate flickering because don't care of vertical sync.
   */
  void processPrimitives();

  /**
   * @brief Sets the list of active sprites.
   *
   * A sprite is an image that keeps background unchanged.<br>
   * There is no limit to the number of active sprites, but flickering and slow
   * refresh happens when a lot of sprites (or large sprites) are visible.<br>
   * To empty the list of active sprites call DisplayController.removeSprites().
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
   * DisplayController.refreshSprites() is required also when using the double buffered mode, to paint sprites.
   */
  void refreshSprites();

  /**
   * @brief Determines whether DisplayController is on double buffered mode.
   *
   * @return True if DisplayController is on double buffered mode.
   */
  bool isDoubleBuffered() { return m_doubleBuffered; }

  /**
   * @brief Sets mouse cursor and make it visible.
   *
   * @param cursor Cursor to use when mouse pointer need to be painted. nullptr = disable mouse pointer.
   */
  void setMouseCursor(Cursor * cursor);

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

  virtual void readScreen(Rect const & rect, RGB888 * destBuf) = 0;

protected:

  //// abstract methods

  virtual void setPixelAt(PixelDesc const & pixelDesc) = 0;

  virtual void drawLine(int X1, int Y1, int X2, int Y2, RGB888 color) = 0;

  virtual void fillRow(int y, int x1, int x2, RGB888 color) = 0;

  virtual void drawEllipse(Size const & size) = 0;

  virtual void clear() = 0;

  virtual void VScroll(int scroll) = 0;

  virtual void HScroll(int scroll) = 0;

  virtual void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor) = 0;

  virtual void invertRect(Rect const & rect) = 0;

  virtual void swapFGBG(Rect const & rect) = 0;

  virtual void copyRect(Rect const & source) = 0;

  virtual void swapBuffers() = 0;

  virtual PixelFormat getBitmapSavePixelFormat() = 0;

  virtual void drawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  virtual void drawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  virtual void drawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  //// implemented methods

  void execPrimitive(Primitive const & prim);

  void updateAbsoluteClippingRect();

  void lineTo(Point const & position);

  void drawRect(Rect const & rect);

  void drawPath(Path const & path);

  void fillRect(Rect const & rect);

  void fillEllipse(Size const & size);

  void fillPath(Path const & path);

  void renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo);

  void setSprites(Sprite * sprites, int count, int spriteSize);

  Sprite * getSprite(int index);

  int spritesCount() { return m_spritesCount; }

  void hideSprites();

  void showSprites();

  void drawBitmap(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, bool ignoreClippingRect);

  void setDoubleBuffered(bool value) { m_doubleBuffered = value; }

  bool getPrimitive(Primitive * primitive, int timeOutMS = 0);

  bool getPrimitiveISR(Primitive * primitive);

  void waitForPrimitives();

  void insertPrimitiveISR(Primitive * primitive);

  void insertPrimitive(Primitive * primitive, int timeOutMS = -1);

  Sprite * mouseCursor() { return &m_mouseCursor; }

  void resetPaintState();

private:

  PaintState             m_paintState;

  bool                   m_doubleBuffered;
  volatile QueueHandle_t m_execQueue;

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



