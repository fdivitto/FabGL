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
#include "font_4x6.h"
#include "font_8x8.h"
#include "font_8x14.h"


fabgl::CanvasClass Canvas;


namespace fabgl {


CanvasClass::CanvasClass()
  : m_fontInfo(NULL), m_textHorizRate(1)
{
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


void CanvasClass::moveTo(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::MoveTo;
  p.position = Point(X, Y);
  VGAController.addPrimitive(p);
}


void CanvasClass::setPenColor(Color color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPenColor;
  p.color = RGB(color);
  VGAController.addPrimitive(p);
}


void CanvasClass::setPenColor(uint8_t red, uint8_t green, uint8_t blue)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPenColor;
  p.color = (RGB) {red, green, blue};
  VGAController.addPrimitive(p);
}


void CanvasClass::setBrushColor(Color color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetBrushColor;
  p.color = RGB(color);
  VGAController.addPrimitive(p);
}


void CanvasClass::setBrushColor(uint8_t red, uint8_t green, uint8_t blue)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetBrushColor;
  p.color = (RGB) {red, green, blue};
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
  moveTo(X1, Y1);
  lineTo(X2, Y1);
  lineTo(X2, Y2);
  lineTo(X1, Y2);
  lineTo(X1, Y1);
}


void CanvasClass::fillRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::FillRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}


#if FABGLIB_HAS_INVERTRECT
void CanvasClass::invertRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::InvertRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  VGAController.addPrimitive(p);
}
#endif


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


void CanvasClass::setPaintOptions(PaintOptions options)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPaintOptions;
  p.paintOptions = options;
  VGAController.addPrimitive(p);
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
  drawText(m_fontInfo, X, Y, text, wrap);
}


void CanvasClass::drawText(FontInfo const * fontInfo, int X, int Y, char const * text, bool wrap)
{
  for (; *text; ++text, X += fontInfo->width * m_textHorizRate) {
    if (wrap && X >= getWidth()) {
      X = 0;
      Y += fontInfo->height;
    }
    drawGlyph(X, Y, fontInfo->width, fontInfo->height, fontInfo->data, *text);
  }
}


void CanvasClass::drawTextFmt(int X, int Y, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(NULL, 0, format, ap) + 1;
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


#if FABGLIB_HAS_READWRITE_RAW_DATA
void CanvasClass::readRawData(int sourceX, int sourceY, int width, int height, uint8_t * dest)
{
  Primitive p;
  p.cmd     = PrimitiveCmd::ReadRawData;
  p.rawData = RawData(sourceX, sourceY, width, height, dest);
  VGAController.addPrimitive(p);
}


void CanvasClass::writeRawData(uint8_t * source, int destX, int destY, int width, int height)
{
  Primitive p;
  p.cmd     = PrimitiveCmd::WriteRawData;
  p.rawData = RawData(destX, destY, width, height, source);
  VGAController.addPrimitive(p);
}
#endif


FontInfo const * CanvasClass::getPresetFontInfo(int columns, int rows)
{
  static const FontInfo * EMBEDDED_FONTS[] = {
    // please, bigger fonts first!
    &FONT_8x14,
    &FONT_8x8,
    &FONT_4x6,
  };

  FontInfo const * * fontInfo = &EMBEDDED_FONTS[0];

  for (int i = 0; i < sizeof(EMBEDDED_FONTS) / sizeof(FontInfo*); ++i, ++fontInfo)
    if (Canvas.getWidth() / (*fontInfo)->width >= columns && Canvas.getHeight() / (*fontInfo)->height >= rows)
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



} // end of namespace
