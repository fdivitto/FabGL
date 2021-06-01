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
 * @brief This file contains fabgl::Canvas definition.
 */


#include <stdint.h>
#include <stddef.h>

#include "displaycontroller.h"


namespace fabgl {



/**
* @brief A class with a set of drawing methods.
*
* This class interfaces directly to the display controller and provides
* a set of primitives to paint lines, circles, etc. and to scroll regions, copy
* rectangles and draw glyphs.<br>
* For default origin is at the top left, starting from (0, 0) up to (Canvas Width - 1, Canvas Height - 1).
*
* Example:
*
*     // Setup pins and resolution (5 GPIOs hence we have up to 8 colors)
*     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
*     VGAController.setResolution(VGA_640x350_70Hz);
*
*     // Paint a green rectangle with red border
*     fabgl::Canvas cv(&VGAController);
*     cv.setPenColor(Color::BrightRed);
*     cv.setBrushColor(Color::BrightGreen);
*     cv.fillRectangle(0, 0, cv.getWidth() - 1, cv.getHeight() - 1);
*     cv.drawRectangle(0, 0, cv.getWidth() - 1, cv.getHeight() - 1);
*
*/
class Canvas {

public:

  Canvas(BitmappedDisplayController * displayController);

  /**
   * @brief Determines the canvas width in pixels.
   *
   * This is equivalent to VGA Controller viewport width.
   *
   * @return The canvas width in pixels.
   */
  int getWidth() { return m_displayController->getViewPortWidth(); }

  /**
   * @brief Determines the canvas height in pixels.
   *
   * This is equivalent to VGA Controller viewport height.
   *
   * @return The canvas height in pixels.
   */
  int getHeight() { return m_displayController->getViewPortHeight(); }

  /**
   * @brief Waits for drawing queue to become empty.
   *
   * @param waitVSync If true drawings are done on vertical retracing (slow), if false drawings are perfomed immediately (fast with flickering).
   */
  void waitCompletion(bool waitVSync = true);

  /**
   * @brief Suspends drawings.
   *
   * After call to beginUpdate() adding new primitives may cause a deadlock.<br>
   * To avoid this a call to "Canvas.waitCompletion(false)" should be performed very often.
   */
  void beginUpdate();

  /**
   * @brief Resumes drawings after beginUpdate().
   */
  void endUpdate();

  /**
   * @brief Swaps screen buffer when double buffering is enabled.
   *
   * Double buffering is enabled calling VGAController.setResolution() with doubleBuffered = true.<br>
   * Buffers swap is always executed in vertical retracing (at VSync pulse).
   */
  void swapBuffers();

  /**
   * @brief Sets the axes origin.
   *
   * Setting axes origin will translate every coordinate by the specified value (except for sprites).
   *
   * @param X Horizontal translation (0 = no translation).
   * @param Y Vertical translation (0 = no translation).
   */
  void setOrigin(int X, int Y);

  /**
   * @brief Sets the axes origin.
   *
   * Setting axes origin will translate every coordinate by the specified value (except for sprites).
   *
   * @param origin Origin coordinates.
   */
  void setOrigin(Point const & origin);

  /**
   * @brief Gets last origin set using setOrigin().
   *
   * @return Last origin set.
   */
  Point getOrigin() { return m_origin; }

  /**
   * @brief Sets clipping rectangle relative to the origin.
   *
   * The default clipping rectangle covers the entire canvas area.
   *
   * @param rect Clipping rectangle relative to the origin.
   */
  void setClippingRect(Rect const & rect);

  /**
   * @brief Gets last clipping rectangle set using setClippingRect().
   *
   * @return Last clipping rectangle set.
   */
  Rect getClippingRect();


  /**
   * @brief Fills the entire canvas with the brush color.
   */
  void clear();

  /**
   * @brief Resets paint state and other display controller settings.
   */
  void reset();

  /**
   * @brief Defines the scrolling region.
   *
   * A scrolling region is the rectangle area where Canvas.scroll() method can operate.
   *
   * @param X1 Top left horizontal coordinate.
   * @param Y1 Top left vertical coordiante.
   * @param X2 Bottom right horizontal coordinate.
   * @param Y2 Bottom right vertical coordiante.
   */
  void setScrollingRegion(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Scrolls pixels horizontally and/or vertically.
   *
   * Scrolling occurs inside the scrolling region defined by Canvas.setScrollingRegion().<br>
   * Vertical scroll is done moving line pointers, so it is very fast to perform.<br>
   * Horizontal scroll is done moving pixel by pixel (CPU intensive task).
   *
   * @param offsetX Number of pixels to scroll left (offsetX < 0) or right (offsetX > 0).
   * @param offsetY Number of pixels to scroll up (offsetY < 0) or down (offsetY > 0).
   */
  void scroll(int offsetX, int offsetY);

  /**
   * @brief Moves current pen position to the spcified coordinates.
   *
   * Some methods expect initial pen position to be specified, like Canvas.lineTo().
   *
   * @param X Horizontal pen position.
   * @param Y Vertical pen position.
   *
   * Example:
   *
   *     // Paint a triangle
   *     Canvas.moveTo(10, 12);
   *     Canvas.lineTo(30, 40);
   *     Canvas.lineTo(10, 12);
   */
  void moveTo(int X, int Y);

  /**
   * @brief Sets pen (foreground) color specifying color components.
   *
   * @param red Red color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   * @param green Green color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   * @param blue Blue color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   *
   * Example:
   *
   *     // Set white pen
   *     Canvas.setPenColor(255, 255, 255);
   */
  void setPenColor(uint8_t red, uint8_t green, uint8_t blue);

  /**
   * @brief Sets pen (foreground) color using a color name.
   *
   * @param color Color name.
   *
   * Example:
   *
   *      // Set white pen
   *      Canvas.setPenColor(Color::BrightWhite);
   */
  void setPenColor(Color color);

  /**
   * @brief Sets pen (foreground) color specifying color components.
   *
   * @param color Color RGB888 components.
   *
   * Example:
   *
   *      // Set white pen
   *      Canvas.setPenColor(Color::BrightWhite);
   */
  void setPenColor(RGB888 const & color);

  /**
   * @brief Sets brush (background) color specifying color components.
   *
   * @param red Red color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   * @param green Green color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   * @param blue Blue color component. Minimum value is 0, maximum value is 1 with 8 colors and 3 with 64 colors.
   *
   * Example:
   *
   *     // Set blue brush with 8 colors setup
   *     Canvas.setBrushColor(0, 0, 1);
   *
   *     // Set blue brush with 64 colors setup
   *     Canvas.setBrushColor(0, 0, 3);
   */
  void setBrushColor(uint8_t red, uint8_t green, uint8_t blue);

  /**
   * @brief Sets brush (background) color using a color name.
   *
   * @param color The color name.
   *
   * Example:
   *
   *      // Set blue brush
   *      Canvas.setBrushColor(Color::BrightBlue);
   */
  void setBrushColor(Color color);

  /**
   * @brief Sets brush (background) color specifying color components.
   *
   * @param color The color RGB888 components.
   *
   * Example:
   *
   *      // Set blue brush
   *      Canvas.setBrushColor(Color::BrightBlue);
   */
  void setBrushColor(RGB888 const & color);

  /**
   * @brief Sets pen width for lines, rectangles and paths
   *
   * @param value Pen width (minimum is 1).
   *
   * Example:
   * 
   *     Canvas.setPenWidth(4);
   *     Canvas.drawLine(10, 10, 100, 10);
   */
  void setPenWidth(int value);

  /**
   * @brief Sets line ends shape
   *
   * @param value Line ends shape.
   *
   * Example:
   *
   *     Canvas.setPenWidth(4);
   *     Canvas.setLineEnds(LineEnds::Circle);
   *     Canvas.drawLine(10, 10, 100, 10);
   */
  void setLineEnds(LineEnds value);

  /**
   * @brief Fills a single pixel with the pen color.
   *
   * @param X Horizontal pixel position.
   * @param Y Vertical pixel position.
   */
  void setPixel(int X, int Y);

  /**
   * @brief Fills a single pixel with the specified color.
   *
   * @param X Horizontal pixel position.
   * @param Y Vertical pixel position.
   * @param color Pixe color.
   */
  void setPixel(int X, int Y, RGB888 const & color);

  /**
   * @brief Fills a single pixel with the specified color.
   *
   * @param pos Pixel position.
   * @param color Pixe color.
   */
  void setPixel(Point const & pos, RGB888 const & color);

  /**
   * @brief Draws a line starting from current pen position.
   *
   * Starting pen position is specified using Canvas.moveTo() method or
   * from ending position of the last call to Canvas.lineTo().<br>
   * The line has the current pen color.
   *
   * @param X Horizontal ending line position.
   * @param Y Vertical ending line position.
   *
   * Example:
   *
   *     // Paint white a triangle
   *     Canvas.setPenColor(Color::BrightWhite);
   *     Canvas.moveTo(10, 12);
   *     Canvas.lineTo(30, 40);
   *     Canvas.lineTo(10, 12);
   */
  void lineTo(int X, int Y);

  /**
   * @brief Draws a line specifying initial and ending coordinates.
   *
   * The line has the current pen color.
   *
   * @param X1 Starting horizontal coordinate.
   * @param Y1 Starting vertical coordinate.
   * @param X2 Ending horizontal coordinate.
   * @param Y2 Ending vertical coordinate.
   *
   * Example:
   *
   *     // Paint a blue X over the whole canvas
   *     Canvas.setPenColor(Color::BrightBlue);
   *     Canvas.drawLine(0, 0, Canvas.getWidth() - 1, Canvas.getHeight() - 1);
   *     Canvas.drawLine(Canvas.getWidth() - 1, 0, 0, Canvas.getHeight() - 1);
   */
  void drawLine(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Draws a rectangle using the current pen color.
   *
   * @param X1 Top left horizontal coordinate.
   * @param Y1 Top left vertical coordiante.
   * @param X2 Bottom right horizontal coordinate.
   * @param Y2 Bottom right vertical coordiante.
   *
   * Example:
   *
   *     // Paint a yellow rectangle
   *     Canvas.setPenColor(Color::BrightYellow);
   *     Canvas.drawRectangle(10, 10, 100, 100);
   */
  void drawRectangle(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Draws a rectangle using the current pen color.
   *
   * @param rect Rectangle coordinates.
   */
  void drawRectangle(Rect const & rect);

  /**
   * @brief Fills a rectangle using the current brush color.
   *
   * @param X1 Top left horizontal coordinate.
   * @param Y1 Top left vertical coordiante.
   * @param X2 Bottom right horizontal coordinate.
   * @param Y2 Bottom right vertical coordiante.
   *
   * Example:
   *
   *     // Paint a filled yellow rectangle
   *     Canvas.setBrushColor(Color::BrightYellow);
   *     Canvas.fillRectangle(10, 10, 100, 100);
   *
   *     // Paint a yellow rectangle with blue border
   *     Canvas.setPenColor(Color::BrightBlue);
   *     Canvas.setBrushColor(Color::BrightYellow);
   *     Canvas.fillRectangle(10, 10, 100, 100);
   *     Canvas.drawRectangle(10, 10, 100, 100);
   */
  void fillRectangle(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Fills a rectangle using the current brush color.
   *
   * @param rect Rectangle coordinates.
   *
   * Example:
   *
   *     // Paint a filled yellow rectangle
   *     Canvas.setBrushColor(Color::BrightYellow);
   *     Canvas.fillRectangle(Rect(10, 10, 100, 100));
   */
  void fillRectangle(Rect const & rect);

  /**
   * @brief Inverts a rectangle.
   *
   * The Not logic operator is applied to pixels inside the rectangle.
   *
   * @param X1 Top left horizontal coordinate.
   * @param Y1 Top left vertical coordiante.
   * @param X2 Bottom right horizontal coordinate.
   * @param Y2 Bottom right vertical coordiante.
   */
  void invertRectangle(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Inverts a rectangle.
   *
   * The Not logic operator is applied to pixels inside the rectangle.
   *
   * @param rect Rectangle coordinates.
   */
  void invertRectangle(Rect const & rect);

  /**
   * @brief Swaps pen and brush colors of the specified rectangle.
   *
   * @param X1 Top left horizontal coordinate.
   * @param Y1 Top left vertical coordiante.
   * @param X2 Bottom right horizontal coordinate.
   * @param Y2 Bottom right vertical coordiante.
   */
  void swapRectangle(int X1, int Y1, int X2, int Y2);

  /**
   * @brief Draws an ellipse specifying center and size, using current pen color.
   *
   * @param X Horizontal coordinate of the ellipse center.
   * @param Y Vertical coordinate of the ellipse center.
   * @param width Ellipse width.
   * @param height Ellipse height.
   *
   * Example:
   *
   *     // Paint a yellow ellipse
   *     Canvas.setPenColor(Color::BrightYellow);
   *     Canvas.drawEllipse(100, 100, 40, 40);
   */
  void drawEllipse(int X, int Y, int width, int height);

  /**
   * @brief Fills an ellipse specifying center and size, using current brush color.
   *
   * @param X Horizontal coordinate of the ellipse center.
   * @param Y Vertical coordinate of the ellipse center.
   * @param width Ellipse width.
   * @param height Ellipse height.
   *
   * Example:
   *
   *     // Paint a filled yellow ellipse
   *     Canvas.setBrushColor(Color::BrightYellow);
   *     Canvas.fillEllipse(100, 100, 40, 40);
   *
   *     // Paint a yellow ellipse with blue border
   *     Canvas.setPenColor(Color::BrightBlue);
   *     Canvas.setBrushColor(Color::BrightYellow);
   *     Canvas.fillEllipse(100, 100, 40, 40);
   *     Canvas.drawEllipse(100, 100, 40, 40);
   */
  void fillEllipse(int X, int Y, int width, int height);

  /**
   * @brief Draws a glyph at specified position.
   *
   * A Glyph is a monochrome bitmap (1 bit per pixel) that can be painted using pen (foreground) and brush (background) colors.<br>
   * Various drawing options can be set using Canvas.setGlyphOptions() method.<br>
   * Glyphs are used by Terminal to render characters.
   *
   * @param X Horizontal coordinate where to draw the glyph.
   * @param Y Vertical coordinate where to draw the glyph.
   * @param width Horizontal size of the glyph.
   * @param height Vertical size of the glyph.
   * @param data Memory buffer containing the glyph. Each line is byte aligned. The size must be "(width + 7) / 8 * height" (integer division!).
   * @param index Optional index inside data. Use when the buffer contains multiple glyphs.
   *
   * Example:
   *
   *     // draw an 'A' using the predefined 8x8 font
   *     Canvas.setPenColor(Color::BrightGreen);
   *     const fabgl::FontInfo * f = &fabgl::FONT_8x8;
   *     Canvas.drawGlyph(0, 0, f->width, f->height, f->data, 0x41);
   *
   *     // draw a 12x8 sprite
   *     const uint8_t enemy[] = {
   *       0x0f, 0x00, 0x7f, 0xe0, 0xff, 0xf0, 0xe6, 0x70, 0xff,
   *       0xf0, 0x39, 0xc0, 0x66, 0x60, 0x30, 0xc0,
   *     };
   *     Canvas.setPenColor(Color::BrightYellow);
   *     Canvas.drawGlyph(50, 80, 12, 8, enemy);
   *
   */
  void drawGlyph(int X, int Y, int width, int height, uint8_t const * data, int index = 0);

  /**
   * @brief Sets drawing options for the next glyphs.
   *
   * Setting glyph options allows to slightly change how a glyph is rendered, applying
   * effects like Bold, Italic, Inversion, double width or height and so on.<br>
   * Because Canvas draws text using glyphs these options affects
   * also how text is rendered.
   *
   * @param options A bit field to specify multiple options
   *
   * Example:
   *
   *     // Draw "Hello World!"
   *     Canvas.setGlyphOptions(GlyphOptions().FillBackground(true).DoubleWidth(1));
   *     Canvas.drawText(20, 20, "Hello World!");
   */
  void setGlyphOptions(GlyphOptions options);

  /**
   * @brief Resets glyph options.
   */
  void resetGlyphOptions();

  void renderGlyphsBuffer(int itemX, int itemY, GlyphsBuffer const * glyphsBuffer);

  /**
   * @brief Sets paint options.
   */
  void setPaintOptions(PaintOptions options);

  /**
   * @brief Resets paint options.
   */
  void resetPaintOptions();

  /**
   * @brief Gets info about currently selected font.
   *
   * @return FontInfo structure representing font info and data.
   */
  FontInfo const * getFontInfo() { return m_fontInfo; }

  /**
   * @brief Selects a font to use for the next text drawings.
   *
   * @param fontInfo Pointer to font structure containing font info and glyphs data.
   *
   * Examples:
   *
   *     // Set a font for about 40x14 text screen
   *     Canvas.selectFont(fabgl::getPresetFontInfo(Canvas.getWidth(), Canvas.getHeight(), 40, 14));
   *
   *     // Set the 8x8 predefined font
   *     Canvas.selectFont(&fabgl::FONT_8x8);
   */
  void selectFont(FontInfo const * fontInfo);

  /**
   * @brief Draws a character at specified position.
   *
   * drawChar() uses currently selected font (selectFont() method) and currently selected glyph options (setGlyphOptions() method).
   *
   * @param X Horizontal position of character left side.
   * @param Y Vertical position of character top side.
   * @param c Character to draw (an index in the character font glyphs set).
   *
   * Example:
   *
   *     // Draw a 'C' at position 100, 100
   *     Canvas.drawChar(100, 100, 'C');
   */
  void drawChar(int X, int Y, char c);

  /**
   * @brief Draws a string at specified position.
   *
   * drawText() uses currently selected font (selectFont() method) and currently selected glyph options (setGlyphOptions() method).
   *
   * @param X Horizontal position of first character left side.
   * @param Y Vertical position of first character top side.
   * @param text String to draw (indexes in the character font glyphs set).
   * @param wrap If true text is wrapped at the end of line.
   *
   * Example:
   *
   *     // Draw a 'Hello World!' at position 100, 100
   *     Canvas.drawText(100, 100, "Hellow World!");
   */
  void drawText(int X, int Y, char const * text, bool wrap = false);

  /**
   * @brief Draws a string at specified position.
   *
   * drawText() uses the specified font and currently selected glyph options (setGlyphOptions() method).
   *
   * @param fontInfo Pointer to font structure containing font info and glyphs data.
   * @param X Horizontal position of first character left side.
   * @param Y Vertical position of first character top side.
   * @param text String to draw (indexes in the character font glyphs set).
   * @param wrap If true text is wrapped at the end of line.
   *
   * Example:
   *
   *     // Draw a 'Hello World!' at position 100, 100
   *     Canvas.drawText(&fabgl::FONT_8x8, 100, 100, "Hellow World!");
   */
  void drawText(FontInfo const * fontInfo, int X, int Y, char const * text, bool wrap = false);

  /**
   * @brief Draws a string at specified position. Add ellipses before truncation.
   *
   * @param fontInfo Pointer to font structure containing font info and glyphs data.
   * @param X Horizontal position of first character left side.
   * @param Y Vertical position of first character top side.
   * @param text String to draw (indexes in the character font glyphs set).
   * @param maxX Maximum horizontal coordinate allowed.
   */
  void drawTextWithEllipsis(FontInfo const * fontInfo, int X, int Y, char const * text, int maxX);

  /**
   * @brief Calculates text extension in pixels.
   *
   * @param fontInfo Pointer to font structure containing font info and glyphs data.
   * @param text String to calculate length (indexes in the character font glyphs set).
   */
  int textExtent(FontInfo const * fontInfo, char const * text);

  /**
   * @brief Calculates text extension in pixels.
   *
   * @param text String to calculate length (indexes in the character font glyphs set).
   */
  int textExtent(char const * text);

  /**
   * @brief Draws formatted text at specified position.
   *
   * drawTextFmt() uses currently selected font (selectFont() method) and currently selected glyph options (setGlyphOptions() method).
   *
   * @param X Horizontal position of first character left side.
   * @param Y Vertical position of first character top side.
   * @param format Format specifier like printf.
   *
   * Example:
   *
   *     Canvas.drawTextFmt(100, 100, "Free DMA memory: %d", heap_caps_get_free_size(MALLOC_CAP_DMA));
   */
  void drawTextFmt(int X, int Y, const char *format, ...);

  /**
   * @brief Copies a screen rectangle to the specified position.
   *
   * Source and destination rectangles can overlap. The copy is performed pixel by pixel and it is a slow operation.
   *
   * @param sourceX Left source rectangle horizontal position.
   * @param sourceY Top source rectangle vertical position.
   * @param destX Left destination rectangle horizontal position.
   * @param destY Top destination rectangle vertical position.
   * @param width Rectangle horizontal size.
   * @param height Rectangle vertical size.
   */
  void copyRect(int sourceX, int sourceY, int destX, int destY, int width, int height);

  /**
   * @brief Draws a bitmap at specified position.
   *
   * A bitmap is an rectangular image with one byte per pixel. Each pixel has up to 64 colors (2 bits per channel) and
   * can have 4 level of transparency. At the moment only level 0 (full transparent) and level 3 (full opaque)
   * is supported.
   *
   * @param X Horizontal position of bitmap left side.
   * @param Y Vertical position of bitmap top side.
   * @param bitmap Pointer to bitmap structure.
   */
  void drawBitmap(int X, int Y, Bitmap const * bitmap);

  /**
   * @brief Draws a sequence of lines.
   *
   * @param points A pointer to an array of Point objects. Points array is copied to a temporary buffer.
   * @param pointsCount Number of points in the array.
   *
   * Example:
   *
   *     Point points[3] = { {10, 10}, {20, 10}, {15, 20} };
   *     Canvas.setPenColor(Color::Red);
   *     Canvas.drawPath(points, 3);
   */
  void drawPath(Point const * points, int pointsCount);

  /**
   * @brief Fills the polygon enclosed in a sequence of lines.
   *
   * @param points A pointer to an array of Point objects. Points array is copied to a temporary buffer.
   * @param pointsCount Number of points in the array.
   *
   * Example:
   *
   *     Point points[3] = { {10, 10}, {20, 10}, {15, 20} };
   *     Canvas.setBrushColor(Color::Red);
   *     Canvas.fillPath(points, 3);
   */
  void fillPath(Point const * points, int pointsCount);

  /**
   * @brief Reads the pixel at specified position.
   *
   * Screen reading may occur while other drawings are in progress, so the result may be not updated. To avoid it, call getPixel() after
   * waitCompletion() or disable background drawings.
   *
   * @param X Horizontal coordinate.
   * @param Y Vertical coordinate.
   *
   * @return Pixel color as RGB888 structure.
   */
  RGB888 getPixel(int X, int Y);

private:

  BitmappedDisplayController * m_displayController;

  FontInfo const *    m_fontInfo;
  uint8_t             m_textHorizRate; // specify character size: 1 = m_fontInfo.width, 2 = m_fontInfo.width * 2, etc...

  Point               m_origin;
  Rect                m_clippingRect;
};


} // end of namespace







