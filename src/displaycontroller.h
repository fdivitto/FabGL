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
 * @brief This file contains fabgl::BitmappedDisplayController definition.
 */



#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "fabglconf.h"
#include "fabutils.h"




namespace fabgl {



/*
  Notes:
    - all positions can have negative and outofbound coordinates. Shapes are always clipped correctly.
*/
enum PrimitiveCmd : uint8_t {

  // Needed to send the updated area of screen buffer on some displays (ie SSD1306)
  Flush,

  // Refresh display. Some displays (ie SSD1306) aren't repainted if there aren't primitives
  // so posting Refresh allows to resend the screenbuffer
  // params: rect (rectangle to refresh)
  Refresh,

  // Reset paint state
  // params: none
  Reset,

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
  // params: notifyTask
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

  // Set pen width
  // params: ivalue
  SetPenWidth,

  // Set line ends
  // params: lineEnds
  SetLineEnds,
};



/** \ingroup Enumerations
 * @brief This enum defines named colors.
 *
 * First eight full implement all available colors when 1 bit per channel mode is used (having 8 colors).
 */
enum Color {
  Black,          /**< Equivalent to RGB888(0,0,0) */
  Red,            /**< Equivalent to RGB888(128,0,0) */
  Green,          /**< Equivalent to RGB888(0,128,0) */
  Yellow,         /**< Equivalent to RGB888(128,128,0) */
  Blue,           /**< Equivalent to RGB888(0,0,128) */
  Magenta,        /**< Equivalent to RGB888(128,0,128) */
  Cyan,           /**< Equivalent to RGB888(0,128,128) */
  White,          /**< Equivalent to RGB888(128,128,128) */
  BrightBlack,    /**< Equivalent to RGB888(64,64,64) */
  BrightRed,      /**< Equivalent to RGB888(255,0,0) */
  BrightGreen,    /**< Equivalent to RGB888(0,255,0) */
  BrightYellow,   /**< Equivalent to RGB888(255,255,0) */
  BrightBlue,     /**< Equivalent to RGB888(0,0,255) */
  BrightMagenta,  /**< Equivalent to RGB888(255,0,255) */
  BrightCyan,     /**< Equivalent to RGB888(0,255,255) */
  BrightWhite,    /**< Equivalent to RGB888(255,255,255) */
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
} __attribute__ ((packed));


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

  RGBA8888() : R(0), G(0), B(0), A(0) { }
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
  RGB222(uint8_t red, uint8_t green, uint8_t blue) : R(red), G(green), B(blue) { }
  RGB222(RGB888 const & value);

  static bool lowBitOnly;  // true= 8 colors, false 64 colors
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


//   0 .. 63  => 0
//  64 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
uint8_t RGB888toPackedRGB222(RGB888 const & rgb);


/**
 * @brief Represents a glyph position, size and binary data.
 *
 * A glyph is a bitmap (1 bit per pixel). The fabgl::Terminal uses glyphs to render characters.
 */
struct Glyph {
  int16_t         X;      /**< Horizontal glyph coordinate */
  int16_t         Y;      /**< Vertical glyph coordinate */
  uint8_t         width;  /**< Glyph horizontal size */
  uint8_t         height; /**< Glyph vertical size */
  uint8_t const * data;   /**< Byte aligned binary data of the glyph. A 0 represents background or a transparent pixel. A 1 represents foreground. */

  Glyph() : X(0), Y(0), width(0), height(0), data(nullptr) { }
  Glyph(int X_, int Y_, int width_, int height_, uint8_t const * data_) : X(X_), Y(Y_), width(width_), height(height_), data(data_) { }
}  __attribute__ ((packed));



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

  /** @brief Helper method to set or reset foreground and background swapping */
  GlyphOptions & Blank(uint8_t value) { blank = value; return *this; }
} __attribute__ ((packed));



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
inline uint8_t glyphMapItem_getIndex(uint32_t const & mapItem)          { return mapItem >> GLYPHMAP_INDEX_BIT & 0xFF; }

inline Color glyphMapItem_getBGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_BGCOLOR_BIT & 0x0F); }
inline Color glyphMapItem_getBGColor(uint32_t const & mapItem)          { return (Color)(mapItem >> GLYPHMAP_BGCOLOR_BIT & 0x0F); }

inline Color glyphMapItem_getFGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_FGCOLOR_BIT & 0x0F); }
inline Color glyphMapItem_getFGColor(uint32_t const & mapItem)          { return (Color)(mapItem >> GLYPHMAP_FGCOLOR_BIT & 0x0F); }

inline GlyphOptions glyphMapItem_getOptions(uint32_t const volatile * mapItem) { return (GlyphOptions){.value = (uint16_t)(*mapItem >> GLYPHMAP_OPTIONS_BIT & 0xFFFF)}; }
inline GlyphOptions glyphMapItem_getOptions(uint32_t const & mapItem)          { return (GlyphOptions){.value = (uint16_t)(mapItem >> GLYPHMAP_OPTIONS_BIT & 0xFFFF)}; }

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
} __attribute__ ((packed));


/** \ingroup Enumerations
 * @brief This enum defines the display controller native pixel format
 */
enum class NativePixelFormat : uint8_t {
  Mono,       /**< 1 bit per pixel. 0 = black, 1 = white */
  SBGR2222,   /**< 8 bit per pixel: VHBBGGRR (bit 7=VSync 6=HSync 5=B 4=B 3=G 2=G 1=R 0=R). Each color channel can have values from 0 to 3 (maxmum intensity). */
  RGB565BE,   /**< 16 bit per pixel: RGB565 big endian. */
  PALETTE2,   /**< 1 bit palette (2 colors), packed as 8 pixels per byte. */
  PALETTE4,   /**< 2 bit palette (4 colors), packed as 4 pixels per byte. */
  PALETTE8,   /**< 3 bit palette (8 colors), packed as 8 pixels every 3 bytes. */
  PALETTE16,  /**< 4 bit palette (16 colors), packed as two pixels per byte. */
};


/** \ingroup Enumerations
 * @brief This enum defines a pixel format
 */
enum class PixelFormat : uint8_t {
  Undefined,  /**< Undefined pixel format */
  Native,     /**< Native device pixel format */
  Mask,       /**< 1 bit per pixel. 0 = transparent, 1 = opaque (color must be specified apart) */
  RGBA2222,   /**< 8 bit per pixel: AABBGGRR (bit 7=A 6=A 5=B 4=B 3=G 2=G 1=R 0=R). AA = 0 fully transparent, AA = 3 fully opaque. Each color channel can have values from 0 to 3 (maxmum intensity). */
  RGBA8888    /**< 32 bits per pixel: RGBA (R=byte 0, G=byte1, B=byte2, A=byte3). Minimum value for each channel is 0, maximum is 255. */
};


/** \ingroup Enumerations
* @brief This enum defines line ends when pen width is greater than 1
*/
enum class LineEnds : uint8_t {
  None,    /**< No line ends */
  Circle,  /**< Circle line ends */
};


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
} __attribute__ ((packed));


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
  bool          freePoints; // deallocate points after drawing
} __attribute__ ((packed));


/**
 * @brief Specifies general paint options.
 */
struct PaintOptions {
  uint8_t swapFGBG : 1;  /**< If enabled swaps foreground and background colors */
  uint8_t NOT      : 1;  /**< If enabled performs NOT logical operator on destination. Implemented only for straight lines and non-filled rectangles. */

  PaintOptions() : swapFGBG(false), NOT(false) { }
} __attribute__ ((packed));


struct PixelDesc {
  Point  pos;
  RGB888 color;
} __attribute__ ((packed));


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
    LineEnds               lineEnds;
    TaskHandle_t           notifyTask;
  } __attribute__ ((packed));

  Primitive() { }
  Primitive(PrimitiveCmd cmd_) : cmd(cmd_) { }
  Primitive(PrimitiveCmd cmd_, Rect const & rect_) : cmd(cmd_), rect(rect_) { }
} __attribute__ ((packed));


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
  int16_t      penWidth;
  LineEnds     lineEnds;
};



/** \ingroup Enumerations
 * @brief This enum defines types of display controllers
 */
enum class DisplayControllerType {
  Textual,      /**< The display controller can represents text only */
  Bitmapped,    /**< The display controller can represents text and bitmapped graphics */
};



/**
 * @brief Represents the base abstract class for all display controllers
 */
class BaseDisplayController {

public:

  virtual void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false) = 0;

  virtual void begin() = 0;

  /**
   * @brief Determines the display controller type
   *
   * @return Display controller type.
   */
  virtual DisplayControllerType controllerType() = 0;

  /**
   * @brief Determines the screen width in pixels.
   *
   * @return Screen width in pixels.
   */
  int getScreenWidth()                         { return m_screenWidth; }

  /**
   * @brief Determines the screen height in pixels.
   *
   * @return Screen height in pixels.
   */
  int getScreenHeight()                        { return m_screenHeight; }

protected:

  // inherited classes should call setScreenSize once display size is known
  void setScreenSize(int width, int height)    { m_screenWidth = width; m_screenHeight = height; }

private:

  // we store here these info to avoid to have virtual methods (due the -vtables in flash- problem)
  int16_t m_screenWidth;
  int16_t m_screenHeight;
};



/**
 * @brief Represents the base abstract class for textual display controllers
 */
class TextualDisplayController : public BaseDisplayController {

public:

    DisplayControllerType controllerType() { return DisplayControllerType::Textual; }

    virtual int getColumns() = 0;
    virtual int getRows() = 0;
    virtual void adjustMapSize(int * columns, int * rows) = 0;
    virtual void setTextMap(uint32_t const * map, int rows) = 0;
    virtual void enableCursor(bool value) = 0;
    virtual void setCursorPos(int row, int col) = 0;      // row and col starts from 0
    virtual void setCursorForeground(Color value) = 0;
    virtual void setCursorBackground(Color value) = 0;
    virtual FontInfo const * getFont() = 0;
};



/**
 * @brief Represents the base abstract class for bitmapped display controllers
 */
class BitmappedDisplayController : public BaseDisplayController {

public:

  BitmappedDisplayController();
  virtual ~BitmappedDisplayController();

  DisplayControllerType controllerType() { return DisplayControllerType::Bitmapped; }

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
   * @brief Represents the native pixel format used by this display.
   *
   * @return Display native pixel format
   */
  virtual NativePixelFormat nativePixelFormat() = 0;

  PaintState & paintState() { return m_paintState; }

  void addPrimitive(Primitive & primitive);

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
   * To empty the list of active sprites call BitmappedDisplayController.removeSprites().
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
   * BitmappedDisplayController.refreshSprites() is required also when using the double buffered mode, to paint sprites.
   */
  void refreshSprites();

  /**
   * @brief Determines whether BitmappedDisplayController is on double buffered mode.
   *
   * @return True if BitmappedDisplayController is on double buffered mode.
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


  // statics (used for common default properties)

  /**
   * @brief Size of display controller primitives queue
   *
   * Application should change before begin() method.
   *
   * Default value is FABGLIB_DEFAULT_DISPLAYCONTROLLER_QUEUE_SIZE (defined in fabglconf.h)
   */
  static int queueSize;


protected:

  //// abstract methods

  virtual void setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect) = 0;

  virtual void absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color) = 0;

  virtual void rawFillRow(int y, int x1, int x2, RGB888 color) = 0;

  virtual void drawEllipse(Size const & size, Rect & updateRect) = 0;

  virtual void clear(Rect & updateRect) = 0;

  virtual void VScroll(int scroll, Rect & updateRect) = 0;

  virtual void HScroll(int scroll, Rect & updateRect) = 0;

  virtual void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect) = 0;

  virtual void invertRect(Rect const & rect, Rect & updateRect) = 0;

  virtual void swapFGBG(Rect const & rect, Rect & updateRect) = 0;

  virtual void copyRect(Rect const & source, Rect & updateRect) = 0;

  virtual void swapBuffers() = 0;

  virtual int getBitmapSavePixelSize() = 0;

  virtual void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount) = 0;

  virtual void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  virtual void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  virtual void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) = 0;

  //// implemented methods

  void execPrimitive(Primitive const & prim, Rect & updateRect, bool insideISR);

  void updateAbsoluteClippingRect();

  RGB888 getActualPenColor();

  RGB888 getActualBrushColor();

  void lineTo(Point const & position, Rect & updateRect);

  void drawRect(Rect const & rect, Rect & updateRect);

  void drawPath(Path const & path, Rect & updateRect);

  void absDrawThickLine(int X1, int Y1, int X2, int Y2, int penWidth, RGB888 const & color);

  void fillRect(Rect const & rect, RGB888 const & color, Rect & updateRect);

  void fillEllipse(int centerX, int centerY, Size const & size, RGB888 const & color, Rect & updateRect);

  void fillPath(Path const & path, RGB888 const & color, Rect & updateRect);

  void renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo, Rect & updateRect);

  void setSprites(Sprite * sprites, int count, int spriteSize);

  Sprite * getSprite(int index);

  int spritesCount() { return m_spritesCount; }

  void hideSprites(Rect & updateRect);

  void showSprites(Rect & updateRect);

  void drawBitmap(BitmapDrawingInfo const & bitmapDrawingInfo, Rect & updateRect);

  void absDrawBitmap(int destX, int destY, Bitmap const * bitmap, void * saveBackground, bool ignoreClippingRect);

  void setDoubleBuffered(bool value);

  bool getPrimitive(Primitive * primitive, int timeOutMS = 0);

  bool getPrimitiveISR(Primitive * primitive);

  void waitForPrimitives();

  Sprite * mouseCursor() { return &m_mouseCursor; }

  void resetPaintState();

private:

  void primitiveReplaceDynamicBuffers(Primitive & primitive);


  PaintState             m_paintState;

  volatile bool          m_doubleBuffered;
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

  // memory pool used to allocate buffers of primitives
  LightMemoryPool        m_primDynMemPool;

};




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenericBitmappedDisplayController


class GenericBitmappedDisplayController : public BitmappedDisplayController {

protected:


  template <typename TPreparePixel, typename TRawSetPixel>
  void genericSetPixelAt(PixelDesc const & pixelDesc, Rect & updateRect, TPreparePixel preparePixel, TRawSetPixel rawSetPixel)
  {
    const int x = pixelDesc.pos.X + paintState().origin.X;
    const int y = pixelDesc.pos.Y + paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2) {
      updateRect = updateRect.merge(Rect(x, y, x, y));
      hideSprites(updateRect);
      rawSetPixel(x, y, preparePixel(pixelDesc.color));
    }
  }


  // coordinates are absolute values (not relative to origin)
  // line clipped on current absolute clipping rectangle
  template <typename TPreparePixel, typename TRawFillRow, typename TRawInvertRow, typename TRawSetPixel, typename TRawInvertPixel>
  void genericAbsDrawLine(int X1, int Y1, int X2, int Y2, RGB888 const & color, TPreparePixel preparePixel, TRawFillRow rawFillRow, TRawInvertRow rawInvertRow, TRawSetPixel rawSetPixel, TRawInvertPixel rawInvertPixel)
  {
    if (paintState().penWidth > 1) {
      absDrawThickLine(X1, Y1, X2, Y2, paintState().penWidth, color);
      return;
    }
    auto pattern = preparePixel(color);
    if (Y1 == Y2) {
      // horizontal line
      if (Y1 < paintState().absClippingRect.Y1 || Y1 > paintState().absClippingRect.Y2)
        return;
      if (X1 > X2)
        tswap(X1, X2);
      if (X1 > paintState().absClippingRect.X2 || X2 < paintState().absClippingRect.X1)
        return;
      X1 = iclamp(X1, paintState().absClippingRect.X1, paintState().absClippingRect.X2);
      X2 = iclamp(X2, paintState().absClippingRect.X1, paintState().absClippingRect.X2);
      if (paintState().paintOptions.NOT)
        rawInvertRow(Y1, X1, X2);
      else
        rawFillRow(Y1, X1, X2, pattern);
    } else if (X1 == X2) {
      // vertical line
      if (X1 < paintState().absClippingRect.X1 || X1 > paintState().absClippingRect.X2)
        return;
      if (Y1 > Y2)
        tswap(Y1, Y2);
      if (Y1 > paintState().absClippingRect.Y2 || Y2 < paintState().absClippingRect.Y1)
        return;
      Y1 = iclamp(Y1, paintState().absClippingRect.Y1, paintState().absClippingRect.Y2);
      Y2 = iclamp(Y2, paintState().absClippingRect.Y1, paintState().absClippingRect.Y2);
      if (paintState().paintOptions.NOT) {
        for (int y = Y1; y <= Y2; ++y)
          rawInvertPixel(X1, y);
      } else {
        for (int y = Y1; y <= Y2; ++y)
          rawSetPixel(X1, y, pattern);
      }
    } else {
      // other cases (Bresenham's algorithm)
      // TODO: to optimize
      //   Unfortunately here we cannot clip exactly using Sutherland-Cohen algorithm (as done before)
      //   because the starting line (got from clipping algorithm) may not be the same of Bresenham's
      //   line (think to continuing an existing line).
      //   Possible solutions:
      //      - "Yevgeny P. Kuzmin" algorithm:
      //               https://stackoverflow.com/questions/40884680/how-to-use-bresenhams-line-drawing-algorithm-with-clipping
      //               https://github.com/ktfh/ClippedLine/blob/master/clip.hpp
      // For now Sutherland-Cohen algorithm is only used to check the line is actually visible,
      // then test for every point inside the main Bresenham's loop.
      if (!clipLine(X1, Y1, X2, Y2, paintState().absClippingRect, true))  // true = do not change line coordinates!
        return;
      const int dx = abs(X2 - X1);
      const int dy = abs(Y2 - Y1);
      const int sx = X1 < X2 ? 1 : -1;
      const int sy = Y1 < Y2 ? 1 : -1;
      int err = (dx > dy ? dx : -dy) / 2;
      while (true) {
        if (paintState().absClippingRect.contains(X1, Y1)) {
          if (paintState().paintOptions.NOT)
            rawInvertPixel(X1, Y1);
          else
            rawSetPixel(X1, Y1, pattern);
        }
        if (X1 == X2 && Y1 == Y2)
          break;
        int e2 = err;
        if (e2 > -dx) {
          err -= dy;
          X1 += sx;
        }
        if (e2 < dy) {
          err += dx;
          Y1 += sy;
        }
      }
    }
  }


  // McIlroy's algorithm
  template <typename TPreparePixel, typename TRawSetPixel>
  void genericDrawEllipse(Size const & size, Rect & updateRect, TPreparePixel preparePixel, TRawSetPixel rawSetPixel)
  {
    auto pattern = preparePixel(getActualPenColor());

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int centerX = paintState().position.X;
    const int centerY = paintState().position.Y;

    const int halfWidth  = size.width / 2;
    const int halfHeight = size.height / 2;

    updateRect = updateRect.merge(Rect(centerX - halfWidth, centerY - halfHeight, centerX + halfWidth, centerY + halfHeight));
    hideSprites(updateRect);

    const int a2 = halfWidth * halfWidth;
    const int b2 = halfHeight * halfHeight;
    const int crit1 = -(a2 / 4 + halfWidth % 2 + b2);
    const int crit2 = -(b2 / 4 + halfHeight % 2 + a2);
    const int crit3 = -(b2 / 4 + halfHeight % 2);
    const int d2xt = 2 * b2;
    const int d2yt = 2 * a2;
    int x = 0;          // travels from 0 up to halfWidth
    int y = halfHeight; // travels from halfHeight down to 0
    int t = -a2 * y;
    int dxt = 2 * b2 * x;
    int dyt = -2 * a2 * y;

    while (y >= 0 && x <= halfWidth) {
      const int col1 = centerX - x;
      const int col2 = centerX + x;
      const int row1 = centerY - y;
      const int row2 = centerY + y;

      if (col1 >= clipX1 && col1 <= clipX2) {
        if (row1 >= clipY1 && row1 <= clipY2)
          rawSetPixel(col1, row1, pattern);
        if (row2 >= clipY1 && row2 <= clipY2)
          rawSetPixel(col1, row2, pattern);
      }
      if (col2 >= clipX1 && col2 <= clipX2) {
        if (row1 >= clipY1 && row1 <= clipY2)
          rawSetPixel(col2, row1, pattern);
        if (row2 >= clipY1 && row2 <= clipY2)
          rawSetPixel(col2, row2, pattern);
      }

      if (t + b2 * x <= crit1 || t + a2 * y <= crit3) {
        x++;
        dxt += d2xt;
        t += dxt;
      } else if (t - a2 * y > crit2) {
        y--;
        dyt += d2yt;
        t += dyt;
      } else {
        x++;
        dxt += d2xt;
        t += dxt;
        y--;
        dyt += d2yt;
        t += dyt;
      }
    }
  }


  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    if (!glyphOptions.bold && !glyphOptions.italic && !glyphOptions.blank && !glyphOptions.underline && !glyphOptions.doubleWidth && glyph.width <= 32)
      genericDrawGlyph_light(glyph, glyphOptions, penColor, brushColor, updateRect, preparePixel, rawGetRow, rawSetPixelInRow);
    else
      genericDrawGlyph_full(glyph, glyphOptions, penColor, brushColor, updateRect, preparePixel, rawGetRow, rawSetPixelInRow);
  }


  // TODO: Italic doesn't work well when clipping rect is specified
  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph_full(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int glyphX = glyph.X + origX;
    const int glyphY = glyph.Y + origY;

    if (glyphX > clipX2 || glyphY > clipY2)
      return;

    int16_t glyphWidth        = glyph.width;
    int16_t glyphHeight       = glyph.height;
    uint8_t const * glyphData = glyph.data;
    int16_t glyphWidthByte    = (glyphWidth + 7) / 8;
    int16_t glyphSize         = glyphHeight * glyphWidthByte;

    bool fillBackground = glyphOptions.fillBackground;
    bool bold           = glyphOptions.bold;
    bool italic         = glyphOptions.italic;
    bool blank          = glyphOptions.blank;
    bool underline      = glyphOptions.underline;
    int doubleWidth     = glyphOptions.doubleWidth;

    // modify glyph to handle top half and bottom half double height
    // doubleWidth = 1 is handled directly inside drawing routine
    if (doubleWidth > 1) {
      uint8_t * newGlyphData = (uint8_t*) alloca(glyphSize);
      // doubling top-half or doubling bottom-half?
      int offset = (doubleWidth == 2 ? 0 : (glyphHeight >> 1));
      for (int y = 0; y < glyphHeight ; ++y)
        for (int x = 0; x < glyphWidthByte; ++x)
          newGlyphData[x + y * glyphWidthByte] = glyphData[x + (offset + (y >> 1)) * glyphWidthByte];
      glyphData = newGlyphData;
    }

    // a very simple and ugly skew (italic) implementation!
    int skewAdder = 0, skewH1 = 0, skewH2 = 0;
    if (italic) {
      skewAdder = 2;
      skewH1 = glyphHeight / 3;
      skewH2 = skewH1 * 2;
    }

    int16_t X1 = 0;
    int16_t XCount = glyphWidth;
    int16_t destX = glyphX;

    if (destX < clipX1) {
      X1 = (clipX1 - destX) / (doubleWidth ? 2 : 1);
      destX = clipX1;
    }
    if (X1 >= glyphWidth)
      return;

    if (destX + XCount + skewAdder > clipX2 + 1)
      XCount = clipX2 + 1 - destX - skewAdder;
    if (X1 + XCount > glyphWidth)
      XCount = glyphWidth - X1;

    int16_t Y1 = 0;
    int16_t YCount = glyphHeight;
    int destY = glyphY;

    if (destY < clipY1) {
      Y1 = clipY1 - destY;
      destY = clipY1;
    }
    if (Y1 >= glyphHeight)
      return;

    if (destY + YCount > clipY2 + 1)
      YCount = clipY2 + 1 - destY;
    if (Y1 + YCount > glyphHeight)
      YCount = glyphHeight - Y1;

    updateRect = updateRect.merge(Rect(destX, destY, destX + XCount + skewAdder - 1, destY + YCount - 1));
    hideSprites(updateRect);

    if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
      tswap(penColor, brushColor);

    // a very simple and ugly reduce luminosity (faint) implementation!
    if (glyphOptions.reduceLuminosity) {
      if (penColor.R > 128) penColor.R = 128;
      if (penColor.G > 128) penColor.G = 128;
      if (penColor.B > 128) penColor.B = 128;
    }

    auto penPattern   = preparePixel(penColor);
    auto brushPattern = preparePixel(brushColor);
    auto boldPattern  = bold ? preparePixel(RGB888(penColor.R / 2 + 1,
                                                   penColor.G / 2 + 1,
                                                   penColor.B / 2 + 1))
                             : preparePixel(RGB888(0, 0, 0));

    for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {

      // true if previous pixel has been set
      bool prevSet = false;

      auto dstrow = rawGetRow(destY);
      auto srcrow = glyphData + y * glyphWidthByte;

      if (underline && y == glyphHeight - FABGLIB_UNDERLINE_POSITION - 1) {

        for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
          rawSetPixelInRow(dstrow, adestX, blank ? brushPattern : penPattern);
          if (doubleWidth) {
            ++adestX;
            if (adestX > clipX2)
              break;
            rawSetPixelInRow(dstrow, adestX, blank ? brushPattern : penPattern);
          }
        }

      } else {

        for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
          if ((srcrow[x >> 3] << (x & 7)) & 0x80 && !blank) {
            rawSetPixelInRow(dstrow, adestX, penPattern);
            prevSet = true;
          } else if (bold && prevSet) {
            rawSetPixelInRow(dstrow, adestX, boldPattern);
            prevSet = false;
          } else if (fillBackground) {
            rawSetPixelInRow(dstrow, adestX, brushPattern);
            prevSet = false;
          } else {
            prevSet = false;
          }
          if (doubleWidth) {
            ++adestX;
            if (adestX > clipX2)
              break;
            if (fillBackground)
              rawSetPixelInRow(dstrow, adestX, prevSet ? penPattern : brushPattern);
            else if (prevSet)
              rawSetPixelInRow(dstrow, adestX, penPattern);
          }
        }

      }

      if (italic && (y == skewH1 || y == skewH2))
        --skewAdder;

    }
  }


  // assume:
  //   glyph.width <= 32
  //   glyphOptions.fillBackground = 0 or 1
  //   glyphOptions.invert : 0 or 1
  //   glyphOptions.reduceLuminosity: 0 or 1
  //   glyphOptions.... others = 0
  //   paintState().paintOptions.swapFGBG: 0 or 1
  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph_light(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int glyphX = glyph.X + origX;
    const int glyphY = glyph.Y + origY;

    if (glyphX > clipX2 || glyphY > clipY2)
      return;

    int16_t glyphWidth        = glyph.width;
    int16_t glyphHeight       = glyph.height;
    uint8_t const * glyphData = glyph.data;
    int16_t glyphWidthByte    = (glyphWidth + 7) / 8;

    int16_t X1 = 0;
    int16_t XCount = glyphWidth;
    int16_t destX = glyphX;

    int16_t Y1 = 0;
    int16_t YCount = glyphHeight;
    int destY = glyphY;

    if (destX < clipX1) {
      X1 = clipX1 - destX;
      destX = clipX1;
    }
    if (X1 >= glyphWidth)
      return;

    if (destX + XCount > clipX2 + 1)
      XCount = clipX2 + 1 - destX;
    if (X1 + XCount > glyphWidth)
      XCount = glyphWidth - X1;

    if (destY < clipY1) {
      Y1 = clipY1 - destY;
      destY = clipY1;
    }
    if (Y1 >= glyphHeight)
      return;

    if (destY + YCount > clipY2 + 1)
      YCount = clipY2 + 1 - destY;
    if (Y1 + YCount > glyphHeight)
      YCount = glyphHeight - Y1;

    updateRect = updateRect.merge(Rect(destX, destY, destX + XCount - 1, destY + YCount - 1));
    hideSprites(updateRect);

    if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
      tswap(penColor, brushColor);

    // a very simple and ugly reduce luminosity (faint) implementation!
    if (glyphOptions.reduceLuminosity) {
      if (penColor.R > 128) penColor.R = 128;
      if (penColor.G > 128) penColor.G = 128;
      if (penColor.B > 128) penColor.B = 128;
    }

    bool fillBackground = glyphOptions.fillBackground;

    auto penPattern   = preparePixel(penColor);
    auto brushPattern = preparePixel(brushColor);

    for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {
      auto dstrow = rawGetRow(destY);
      uint8_t const * srcrow = glyphData + y * glyphWidthByte;

      uint32_t src = (srcrow[0] << 24) | (srcrow[1] << 16) | (srcrow[2] << 8) | (srcrow[3]);
      src <<= X1;
      if (fillBackground) {
        // filled background
        for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1)
          rawSetPixelInRow(dstrow, adestX, src & 0x80000000 ? penPattern : brushPattern);
      } else {
        // transparent background
        for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1)
          if (src & 0x80000000)
            rawSetPixelInRow(dstrow, adestX, penPattern);
      }
    }
  }


  template <typename TRawInvertRow>
  void genericInvertRect(Rect const & rect, Rect & updateRect, TRawInvertRow rawInvertRow)
  {
    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int x1 = iclamp(rect.X1 + origX, clipX1, clipX2);
    const int y1 = iclamp(rect.Y1 + origY, clipY1, clipY2);
    const int x2 = iclamp(rect.X2 + origX, clipX1, clipX2);
    const int y2 = iclamp(rect.Y2 + origY, clipY1, clipY2);

    updateRect = updateRect.merge(Rect(x1, y1, x2, y2));
    hideSprites(updateRect);

    for (int y = y1; y <= y2; ++y)
      rawInvertRow(y, x1, x2);
  }


  template <typename TPreparePixel, typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericSwapFGBG(Rect const & rect, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    auto penPattern   = preparePixel(paintState().penColor);
    auto brushPattern = preparePixel(paintState().brushColor);

    int origX = paintState().origin.X;
    int origY = paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int x1 = iclamp(rect.X1 + origX, clipX1, clipX2);
    const int y1 = iclamp(rect.Y1 + origY, clipY1, clipY2);
    const int x2 = iclamp(rect.X2 + origX, clipX1, clipX2);
    const int y2 = iclamp(rect.Y2 + origY, clipY1, clipY2);

    updateRect = updateRect.merge(Rect(x1, y1, x2, y2));
    hideSprites(updateRect);

    for (int y = y1; y <= y2; ++y) {
      auto row = rawGetRow(y);
      for (int x = x1; x <= x2; ++x) {
        auto px = rawGetPixelInRow(row, x);
        if (px == penPattern)
          rawSetPixelInRow(row, x, brushPattern);
        else if (px == brushPattern)
          rawSetPixelInRow(row, x, penPattern);
      }
    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericCopyRect(Rect const & source, Rect & updateRect, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    int origX = paintState().origin.X;
    int origY = paintState().origin.Y;

    int srcX = source.X1 + origX;
    int srcY = source.Y1 + origY;
    int width  = source.X2 - source.X1 + 1;
    int height = source.Y2 - source.Y1 + 1;
    int destX = paintState().position.X;
    int destY = paintState().position.Y;
    int deltaX = destX - srcX;
    int deltaY = destY - srcY;

    int incX = deltaX < 0 ? 1 : -1;
    int incY = deltaY < 0 ? 1 : -1;

    int startX = deltaX < 0 ? destX : destX + width - 1;
    int startY = deltaY < 0 ? destY : destY + height - 1;

    updateRect = updateRect.merge(Rect(srcX, srcY, srcX + width - 1, srcY + height - 1));
    updateRect = updateRect.merge(Rect(destX, destY, destX + width - 1, destY + height - 1));
    hideSprites(updateRect);

    for (int y = startY, i = 0; i < height; y += incY, ++i) {
      if (y >= clipY1 && y <= clipY2) {
        auto srcRow = rawGetRow(y - deltaY);
        auto dstRow = rawGetRow(y);
        for (int x = startX, j = 0; j < width; x += incX, ++j) {
          if (x >= clipX1 && x <= clipX2)
            rawSetPixelInRow(dstRow, x, rawGetPixelInRow(srcRow, x - deltaX));
        }
      }
    }
  }


  template <typename TRawGetRow, typename TRawSetPixelInRow, typename TDataType>
  void genericRawDrawBitmap_Native(int destX, int destY, TDataType * data, int width, int X1, int Y1, int XCount, int YCount,
                                   TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int yEnd = Y1 + YCount;
    const int xEnd = X1 + XCount;
    for (int y = Y1; y < yEnd; ++y, ++destY) {
      auto dstrow = rawGetRow(destY);
      auto src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src)
        rawSetPixelInRow(dstrow, adestX, *src);
    }
  }



  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                 TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width = bitmap->width;
    const int yEnd  = Y1 + YCount;
    const int xEnd  = X1 + XCount;
    auto data = bitmap->data;
    const int rowlen = (bitmap->width + 7) / 8;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * rowlen;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if ((src[x >> 3] << (x & 7)) & 0x80)
            rawSetPixelInRow(dstrow, adestX);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * rowlen;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX) {
          if ((src[x >> 3] << (x & 7)) & 0x80)
            rawSetPixelInRow(dstrow, adestX);
        }
      }

    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                     TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width  = bitmap->width;
    const int yEnd   = Y1 + YCount;
    const int xEnd   = X1 + XCount;
    auto data = bitmap->data;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx, ++src) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if (*src & 0xc0)  // alpha > 0 ?
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src) {
          if (*src & 0xc0)  // alpha > 0 ?
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                     TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width = bitmap->width;
    const int yEnd  = Y1 + YCount;
    const int xEnd  = X1 + XCount;
    auto data = (RGBA8888 const *) bitmap->data;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx, ++src) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if (src->A)
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src) {
          if (src->A)
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    }
  }


  // Scroll is done copying and filling rows
  // scroll < 0 -> scroll UP
  // scroll > 0 -> scroll DOWN
  template <typename TRawCopyRow, typename TRawFillRow>
  void genericVScroll(int scroll, Rect & updateRect,
                      TRawCopyRow rawCopyRow, TRawFillRow rawFillRow)
  {
    hideSprites(updateRect);
    RGB888 color = getActualBrushColor();
    int Y1 = paintState().scrollingRegion.Y1;
    int Y2 = paintState().scrollingRegion.Y2;
    int X1 = paintState().scrollingRegion.X1;
    int X2 = paintState().scrollingRegion.X2;
    int height = Y2 - Y1 + 1;

    if (scroll < 0) {

      // scroll UP

      for (int i = 0; i < height + scroll; ++i) {
        // copy X1..X2 of (Y1 + i - scroll) to (Y1 + i)
        rawCopyRow(X1, X2, (Y1 + i - scroll), (Y1 + i));
      }
      // fill lower area with brush color
      for (int i = height + scroll; i < height; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    } else if (scroll > 0) {

      // scroll DOWN
      for (int i = height - scroll - 1; i >= 0; --i) {
        // copy X1..X2 of (Y1 + i) to (Y1 + i + scroll)
        rawCopyRow(X1, X2, (Y1 + i), (Y1 + i + scroll));
      }

      // fill upper area with brush color
      for (int i = 0; i < scroll; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    }
  }


  // scroll is done swapping rows and rows pointers
  // scroll < 0 -> scroll UP
  // scroll > 0 -> scroll DOWN
  template <typename TSwapRowsCopying, typename TSwapRowsPointers, typename TRawFillRow>
  void genericVScroll(int scroll, Rect & updateRect,
                      TSwapRowsCopying swapRowsCopying, TSwapRowsPointers swapRowsPointers, TRawFillRow rawFillRow)
  {
    hideSprites(updateRect);
    RGB888 color = getActualBrushColor();
    const int Y1 = paintState().scrollingRegion.Y1;
    const int Y2 = paintState().scrollingRegion.Y2;
    const int X1 = paintState().scrollingRegion.X1;
    const int X2 = paintState().scrollingRegion.X2;
    const int height = Y2 - Y1 + 1;

    const int viewPortWidth = getViewPortWidth();

    if (scroll < 0) {

      // scroll UP

      for (int i = 0; i < height + scroll; ++i) {

        // these are necessary to maintain invariate out of scrolling regions
        if (X1 > 0)
          swapRowsCopying(Y1 + i, Y1 + i - scroll, 0, X1 - 1);
        if (X2 < viewPortWidth - 1)
          swapRowsCopying(Y1 + i, Y1 + i - scroll, X2 + 1, viewPortWidth - 1);

        // swap scan lines
        swapRowsPointers(Y1 + i, Y1 + i - scroll);
      }

      // fill lower area with brush color
      for (int i = height + scroll; i < height; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    } else if (scroll > 0) {

      // scroll DOWN
      for (int i = height - scroll - 1; i >= 0; --i) {

        // these are necessary to maintain invariate out of scrolling regions
        if (X1 > 0)
          swapRowsCopying(Y1 + i, Y1 + i + scroll, 0, X1 - 1);
        if (X2 < viewPortWidth - 1)
          swapRowsCopying(Y1 + i, Y1 + i + scroll, X2 + 1, viewPortWidth - 1);

        // swap scan lines
        swapRowsPointers(Y1 + i, Y1 + i + scroll);
      }

      // fill upper area with brush color
      for (int i = 0; i < scroll; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    }
  }



  // Scroll is done copying and filling columns
  // scroll < 0 -> scroll LEFT
  // scroll > 0 -> scroll RIGHT
  template <typename TPreparePixel, typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericHScroll(int scroll, Rect & updateRect,
                      TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    hideSprites(updateRect);
    auto pattern = preparePixel(getActualBrushColor());

    int Y1 = paintState().scrollingRegion.Y1;
    int Y2 = paintState().scrollingRegion.Y2;
    int X1 = paintState().scrollingRegion.X1;
    int X2 = paintState().scrollingRegion.X2;

    if (scroll < 0) {
      // scroll left
      for (int y = Y1; y <= Y2; ++y) {
        auto row = rawGetRow(y);
        for (int x = X1; x <= X2 + scroll; ++x) {
          auto c = rawGetPixelInRow(row, x - scroll);
          rawSetPixelInRow(row, x, c);
        }
        // fill right area with brush color
        for (int x = X2 + 1 + scroll; x <= X2; ++x)
          rawSetPixelInRow(row, x, pattern);
      }
    } else if (scroll > 0) {
      // scroll right
      for (int y = Y1; y <= Y2; ++y) {
        auto row = rawGetRow(y);
        for (int x = X2 - scroll; x >= X1; --x) {
          auto c = rawGetPixelInRow(row, x);
          rawSetPixelInRow(row, x + scroll, c);
        }
        // fill left area with brush color
        for (int x = X1; x < X1 + scroll; ++x)
          rawSetPixelInRow(row, x, pattern);
      }
    }
  }




};



} // end of namespace



