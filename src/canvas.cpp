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


#include <stdarg.h>

#include "canvas.h"

// Embedded fonts
#include "fonts/font_4x6.h"
#include "fonts/font_8x8.h"
#include "fonts/font_8x9.h"
#include "fonts/font_8x14.h"
#include "fonts/font_std_12.h"
#include "fonts/font_std_14.h"
#include "fonts/font_std_15.h"
#include "fonts/font_std_16.h"
#include "fonts/font_std_17.h"
#if FABGLIB_EMBEDS_ADDITIONAL_FONTS
#include "fonts/font_std_18.h"
#include "fonts/font_std_22.h"
#include "fonts/font_std_24.h"
#endif


fabgl::CanvasClass Canvas;


namespace fabgl {


#define INVALIDRECT Rect(-32768, -32768, -32768, -32768)


CanvasClass::CanvasClass()
  : m_fontInfo(nullptr), m_textHorizRate(1), m_origin(Point(0, 0)), m_clippingRect(INVALIDRECT)
{
}


void CanvasClass::setOrigin(int X, int Y)
{
  setOrigin(Point(X, Y));
}


void CanvasClass::setOrigin(Point const & origin)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::SetOrigin;
  p.position = m_origin = origin;
  VGAController.addPrimitive(p);
}


void CanvasClass::setClippingRect(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SetClippingRect;
  p.rect = m_clippingRect = rect;
  VGAController.addPrimitive(p);
}


Rect CanvasClass::getClippingRect()
{
  if (m_clippingRect == INVALIDRECT)
    m_clippingRect = Rect(0, 0, getWidth() - 1, getHeight() - 1);
  return m_clippingRect;
}


void CanvasClass::waitCompletion(bool waitVSync)
{
  if (waitVSync)
    VGAController.primitivesExecutionWait();   // wait on VSync normal processing
  else
    VGAController.processPrimitives();         // process right now!
}


void CanvasClass::clear()
{
  Primitive p;
  p.cmd = PrimitiveCmd::Clear;
  p.ivalue = 0;
  VGAController.addPrimitive(p);
}


void CanvasClass::scroll(int offsetX, int offsetY)
{
  Primitive p;
  if (offsetY != 0) {
    p.cmd    = PrimitiveCmd::VScroll;
    p.ivalue = offsetY;
    VGAController.addPrimitive(p);
  }
  if (offsetX != 0) {
    p.cmd    = PrimitiveCmd::HScroll;
    p.ivalue = offsetX;
    VGAController.addPrimitive(p);
  }
}


void CanvasClass::setScrollingRegion(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SetScrollingRegion;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}


void CanvasClass::setPixel(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::SetPixel;
  p.position = Point(X, Y);
  VGAController.addPrimitive(p);
}


void CanvasClass::setPixel(int X, int Y, RGB const & color)
{
  setPixel(Point(X, Y), color);
}


void CanvasClass::setPixel(Point const & pos, RGB const & color)
{
  Primitive p;
  p.cmd       = PrimitiveCmd::SetPixelAt;
  p.pixelDesc = { pos, color };
  VGAController.addPrimitive(p);
}


void CanvasClass::moveTo(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::MoveTo;
  p.position = Point(X, Y);
  VGAController.addPrimitive(p);
}


void CanvasClass::setPenColor(Color color)
{
  setPenColor(RGB(color));
}


void CanvasClass::setPenColor(uint8_t red, uint8_t green, uint8_t blue)
{
  setPenColor(RGB(red, green, blue));
}


void CanvasClass::setPenColor(RGB const & color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPenColor;
  p.color = color;
  VGAController.addPrimitive(p);
}


void CanvasClass::setBrushColor(Color color)
{
  setBrushColor(RGB(color));
}


void CanvasClass::setBrushColor(uint8_t red, uint8_t green, uint8_t blue)
{
  setBrushColor(RGB(red, green, blue));
}


void CanvasClass::setBrushColor(RGB const & color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetBrushColor;
  p.color = color;
  VGAController.addPrimitive(p);
}


void CanvasClass::lineTo(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::LineTo;
  p.position = Point(X, Y);
  VGAController.addPrimitive(p);
}


void CanvasClass::drawLine(int X1, int Y1, int X2, int Y2)
{
  moveTo(X1, Y1);
  lineTo(X2, Y2);
}


void CanvasClass::drawRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::DrawRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}


void CanvasClass::drawRectangle(Rect const & rect)
{
  drawRectangle(rect.X1, rect.Y1, rect.X2, rect.Y2);
}


void CanvasClass::fillRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::FillRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}


void CanvasClass::fillRectangle(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::FillRect;
  p.rect = rect;
  VGAController.addPrimitive(p);
}


void CanvasClass::invertRectangle(int X1, int Y1, int X2, int Y2)
{
  invertRectangle(Rect(X1, Y1, X2, Y2));
}


void CanvasClass::invertRectangle(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::InvertRect;
  p.rect = rect;
  VGAController.addPrimitive(p);
}


void CanvasClass::swapRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SwapFGBG;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}


void CanvasClass::fillEllipse(int X, int Y, int width, int height)
{
  moveTo(X, Y);
  Primitive p;
  p.cmd  = PrimitiveCmd::FillEllipse;
  p.size = Size(width, height);
  VGAController.addPrimitive(p);
}


void CanvasClass::drawEllipse(int X, int Y, int width, int height)
{
  moveTo(X, Y);
  Primitive p;
  p.cmd  = PrimitiveCmd::DrawEllipse;
  p.size = Size(width, height);
  VGAController.addPrimitive(p);
}


void CanvasClass::drawGlyph(int X, int Y, int width, int height, uint8_t const * data, int index)
{
  Primitive p;
  p.cmd   = PrimitiveCmd::DrawGlyph;
  p.glyph = Glyph(X, Y, width, height, data + index * height * ((width + 7) / 8));
  VGAController.addPrimitive(p);
}


void CanvasClass::renderGlyphsBuffer(int itemX, int itemY, GlyphsBuffer const * glyphsBuffer)
{
  Primitive p;
  p.cmd                    = PrimitiveCmd::RenderGlyphsBuffer;
  p.glyphsBufferRenderInfo = GlyphsBufferRenderInfo(itemX, itemY, glyphsBuffer);
  VGAController.addPrimitive(p);
}


void CanvasClass::setGlyphOptions(GlyphOptions options)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetGlyphOptions;
  p.glyphOptions = options;
  VGAController.addPrimitive(p);
  m_textHorizRate = options.doubleWidth > 0 ? 2 : 1;
}


void CanvasClass::resetGlyphOptions()
{
  setGlyphOptions(GlyphOptions());
}


void CanvasClass::setPaintOptions(PaintOptions options)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPaintOptions;
  p.paintOptions = options;
  VGAController.addPrimitive(p);
}


void CanvasClass::resetPaintOptions()
{
  setPaintOptions(PaintOptions());
}


void CanvasClass::selectFont(FontInfo const * fontInfo)
{
  m_fontInfo = fontInfo;
}


void CanvasClass::drawChar(int X, int Y, char c)
{
  drawGlyph(X, Y, m_fontInfo->width, m_fontInfo->height, m_fontInfo->data, c);
}


void CanvasClass::drawText(int X, int Y, char const * text, bool wrap)
{
  if (m_fontInfo == nullptr)
    selectFont(getPresetFontInfo(80, 25));
  drawText(m_fontInfo, X, Y, text, wrap);
}


void CanvasClass::drawText(FontInfo const * fontInfo, int X, int Y, char const * text, bool wrap)
{
  int fontWidth = fontInfo->width;
  for (; *text; ++text, X += fontWidth * m_textHorizRate) {
    if (wrap && X >= getWidth()) {    // TODO: clipX2 instead of getWidth()?
      X = 0;
      Y += fontInfo->height;
    }
    if (fontInfo->chptr) {
      // variable width
      uint8_t const * chptr = fontInfo->data + fontInfo->chptr[(int)(*text)];
      fontWidth = *chptr++;
      drawGlyph(X, Y, fontWidth, fontInfo->height, chptr, 0);
    } else {
      // fixed width
      drawGlyph(X, Y, fontInfo->width, fontInfo->height, fontInfo->data, *text);
    }
  }
}


void CanvasClass::drawTextWithEllipsis(FontInfo const * fontInfo, int X, int Y, char const * text, int maxX)
{
  int fontWidth  = fontInfo->width;
  int fontHeight = fontInfo->height;
  for (; *text; ++text, X += fontWidth) {
    if (X >= maxX - fontHeight) {
      // draw ellipsis and exit
      drawText(fontInfo, X, Y, "...");
      break;
    }
    if (fontInfo->chptr) {
      // variable width
      uint8_t const * chptr = fontInfo->data + fontInfo->chptr[(int)(*text)];
      fontWidth = *chptr++;
      drawGlyph(X, Y, fontWidth, fontInfo->height, chptr, 0);
    } else {
      // fixed width
      drawGlyph(X, Y, fontInfo->width, fontInfo->height, fontInfo->data, *text);
    }
  }
}


int CanvasClass::textExtent(FontInfo const * fontInfo, char const * text)
{
  int fontWidth  = fontInfo->width;
  int extent = 0;
  for (; *text; ++text, extent += fontWidth) {
    if (fontInfo->chptr) {
      // variable width
      uint8_t const * chptr = fontInfo->data + fontInfo->chptr[(int)(*text)];
      fontWidth = *chptr;
    }
  }
  return extent;
}


int CanvasClass::textExtent(char const * text)
{
  return textExtent(m_fontInfo, text);
}


void CanvasClass::drawTextFmt(int X, int Y, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    drawText(X, Y, buf, false);
  }
  va_end(ap);
}


void CanvasClass::copyRect(int sourceX, int sourceY, int destX, int destY, int width, int height)
{
  moveTo(destX, destY);
  int sourceX2 = sourceX + width - 1;
  int sourceY2 = sourceY + height - 1;
  Primitive p;
  p.cmd  = PrimitiveCmd::CopyRect;
  p.rect = Rect(sourceX, sourceY, sourceX2, sourceY2);
  VGAController.addPrimitive(p);
}



static const FontInfo * FIXED_WIDTH_EMBEDDED_FONTS[] = {
  // please, bigger fonts first!
  &FONT_8x14,
  &FONT_8x8,
  &FONT_8x9,
  &FONT_4x6,
};


static const FontInfo * VAR_WIDTH_EMBEDDED_FONTS[] = {
  // please, bigger fonts first!
#if FABGLIB_EMBEDS_ADDITIONAL_FONTS
  &FONT_std_24,
  &FONT_std_22,
  &FONT_std_18,
#endif
  &FONT_std_17,
  &FONT_std_16,
  &FONT_std_15,
  &FONT_std_14,
  &FONT_std_12,
};


FontInfo const * CanvasClass::getPresetFontInfo(int columns, int rows)
{
  FontInfo const * * fontInfo = &FIXED_WIDTH_EMBEDDED_FONTS[0];

  for (int i = 0; i < sizeof(FIXED_WIDTH_EMBEDDED_FONTS) / sizeof(FontInfo*) - 1; ++i, ++fontInfo)  // -1, so the smallest is always selected
    if (Canvas.getWidth() / (*fontInfo)->width >= columns && Canvas.getHeight() / (*fontInfo)->height >= rows)
      break;

  return *fontInfo;
}


FontInfo const * CanvasClass::getPresetFontInfoFromHeight(int height, bool fixedWidth)
{
  FontInfo const * * fontInfo = fixedWidth ? &FIXED_WIDTH_EMBEDDED_FONTS[0] : &VAR_WIDTH_EMBEDDED_FONTS[0];
  int count = (fixedWidth ? sizeof(FIXED_WIDTH_EMBEDDED_FONTS) : sizeof(VAR_WIDTH_EMBEDDED_FONTS)) / sizeof(FontInfo*);

  for (int i = 0; i < count - 1; ++i, ++fontInfo) // -1, so the smallest is always selected
    if (height >= (*fontInfo)->height)
      break;

  return *fontInfo;
}


void CanvasClass::drawBitmap(int X, int Y, Bitmap const * bitmap)
{
  Primitive p;
  p.cmd               = PrimitiveCmd::DrawBitmap;
  p.bitmapDrawingInfo = BitmapDrawingInfo(X, Y, bitmap);
  VGAController.addPrimitive(p);
}


void CanvasClass::swapBuffers()
{
  Primitive p;
  p.cmd = PrimitiveCmd::SwapBuffers;
  VGAController.addPrimitive(p);
  VGAController.primitivesExecutionWait();
}


// warn: points memory must survive until next vsync interrupt when primitive is not executed immediately
void CanvasClass::drawPath(Point const * points, int pointsCount)
{
  Primitive p;
  p.cmd = PrimitiveCmd::DrawPath;
  p.path.points = points;
  p.path.pointsCount = pointsCount;
  VGAController.addPrimitive(p);
}

// warn: points memory must survive until next vsync interrupt when primitive is not executed immediately
void CanvasClass::fillPath(Point const * points, int pointsCount)
{
  Primitive p;
  p.cmd = PrimitiveCmd::FillPath;
  p.path.points = points;
  p.path.pointsCount = pointsCount;
  VGAController.addPrimitive(p);
}


RGB CanvasClass::getPixel(int X, int Y)
{
  RGB rgb;
  VGAController.readScreen(Rect(X, Y, X, Y), &rgb);
  return rgb;
}


} // end of namespace
