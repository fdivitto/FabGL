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


#include <stdarg.h>

#include "canvas.h"
#include "fabfonts.h"


#pragma GCC optimize ("O2")


namespace fabgl {


#define INVALIDRECT Rect(-32768, -32768, -32768, -32768)


Canvas::Canvas(BitmappedDisplayController * displayController)
  : m_displayController(displayController),
    m_fontInfo(nullptr),
    m_textHorizRate(1),
    m_origin(Point(0, 0)),
    m_clippingRect(INVALIDRECT)
{
}


void Canvas::setOrigin(int X, int Y)
{
  setOrigin(Point(X, Y));
}


void Canvas::setOrigin(Point const & origin)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::SetOrigin;
  p.position = m_origin = origin;
  m_displayController->addPrimitive(p);
}


void Canvas::setClippingRect(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SetClippingRect;
  p.rect = m_clippingRect = rect;
  m_displayController->addPrimitive(p);
}


Rect Canvas::getClippingRect()
{
  if (m_clippingRect == INVALIDRECT)
    m_clippingRect = Rect(0, 0, getWidth() - 1, getHeight() - 1);
  return m_clippingRect;
}


void Canvas::waitCompletion(bool waitVSync)
{
  if (waitVSync)
    m_displayController->primitivesExecutionWait();   // wait on VSync normal processing
  else
    m_displayController->processPrimitives();         // process right now!
}


// Warning: beginUpdate() disables vertical sync interrupts. This means that
// the BitmappedDisplayController primitives queue is not processed, and adding primitives may
// cause a deadlock. To avoid this a call to "Canvas.waitCompletion(false)"
// should be performed very often.
void Canvas::beginUpdate()
{
  m_displayController->suspendBackgroundPrimitiveExecution();
}


void Canvas::endUpdate()
{
  m_displayController->resumeBackgroundPrimitiveExecution();
}


void Canvas::clear()
{
  Primitive p;
  p.cmd = PrimitiveCmd::Clear;
  p.ivalue = 0;
  m_displayController->addPrimitive(p);
}


void Canvas::reset()
{
  Primitive p;
  p.cmd = PrimitiveCmd::Reset;
  m_displayController->addPrimitive(p);
  m_origin = Point(0, 0);
  m_clippingRect = INVALIDRECT;
  m_textHorizRate = 1;
}


void Canvas::scroll(int offsetX, int offsetY)
{
  Primitive p;
  if (offsetY != 0) {
    p.cmd    = PrimitiveCmd::VScroll;
    p.ivalue = offsetY;
    m_displayController->addPrimitive(p);
  }
  if (offsetX != 0) {
    p.cmd    = PrimitiveCmd::HScroll;
    p.ivalue = offsetX;
    m_displayController->addPrimitive(p);
  }
}


void Canvas::setScrollingRegion(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SetScrollingRegion;
  p.rect = Rect(X1, Y1, X2, Y2);
  m_displayController->addPrimitive(p);
}


void Canvas::setPixel(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::SetPixel;
  p.position = Point(X, Y);
  m_displayController->addPrimitive(p);
}


void Canvas::setPixel(int X, int Y, RGB888 const & color)
{
  setPixel(Point(X, Y), color);
}


void Canvas::setPixel(Point const & pos, RGB888 const & color)
{
  Primitive p;
  p.cmd       = PrimitiveCmd::SetPixelAt;
  p.pixelDesc = { pos, color };
  m_displayController->addPrimitive(p);
}


void Canvas::moveTo(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::MoveTo;
  p.position = Point(X, Y);
  m_displayController->addPrimitive(p);
}


void Canvas::setPenColor(Color color)
{
  setPenColor(RGB888(color));
}


void Canvas::setPenColor(uint8_t red, uint8_t green, uint8_t blue)
{
  setPenColor(RGB888(red, green, blue));
}


void Canvas::setPenColor(RGB888 const & color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPenColor;
  p.color = color;
  m_displayController->addPrimitive(p);
}


void Canvas::setBrushColor(Color color)
{
  setBrushColor(RGB888(color));
}


void Canvas::setBrushColor(uint8_t red, uint8_t green, uint8_t blue)
{
  setBrushColor(RGB888(red, green, blue));
}


void Canvas::setPenWidth(int value)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPenWidth;
  p.ivalue = value;
  m_displayController->addPrimitive(p);
}


void Canvas::setLineEnds(LineEnds value)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetLineEnds;
  p.lineEnds = value;
  m_displayController->addPrimitive(p);
}


void Canvas::setBrushColor(RGB888 const & color)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetBrushColor;
  p.color = color;
  m_displayController->addPrimitive(p);
}


void Canvas::lineTo(int X, int Y)
{
  Primitive p;
  p.cmd      = PrimitiveCmd::LineTo;
  p.position = Point(X, Y);
  m_displayController->addPrimitive(p);
}


void Canvas::drawLine(int X1, int Y1, int X2, int Y2)
{
  moveTo(X1, Y1);
  lineTo(X2, Y2);
}


void Canvas::drawRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::DrawRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  m_displayController->addPrimitive(p);
}


void Canvas::drawRectangle(Rect const & rect)
{
  drawRectangle(rect.X1, rect.Y1, rect.X2, rect.Y2);
}


void Canvas::fillRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::FillRect;
  p.rect = Rect(X1, Y1, X2, Y2);
  m_displayController->addPrimitive(p);
}


void Canvas::fillRectangle(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::FillRect;
  p.rect = rect;
  m_displayController->addPrimitive(p);
}


void Canvas::invertRectangle(int X1, int Y1, int X2, int Y2)
{
  invertRectangle(Rect(X1, Y1, X2, Y2));
}


void Canvas::invertRectangle(Rect const & rect)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::InvertRect;
  p.rect = rect;
  m_displayController->addPrimitive(p);
}


void Canvas::swapRectangle(int X1, int Y1, int X2, int Y2)
{
  Primitive p;
  p.cmd  = PrimitiveCmd::SwapFGBG;
  p.rect = Rect(X1, Y1, X2, Y2);
  m_displayController->addPrimitive(p);
}


void Canvas::fillEllipse(int X, int Y, int width, int height)
{
  moveTo(X, Y);
  Primitive p;
  p.cmd  = PrimitiveCmd::FillEllipse;
  p.size = Size(width, height);
  m_displayController->addPrimitive(p);
}


void Canvas::drawEllipse(int X, int Y, int width, int height)
{
  moveTo(X, Y);
  Primitive p;
  p.cmd  = PrimitiveCmd::DrawEllipse;
  p.size = Size(width, height);
  m_displayController->addPrimitive(p);
}


void Canvas::drawGlyph(int X, int Y, int width, int height, uint8_t const * data, int index)
{
  Primitive p;
  p.cmd   = PrimitiveCmd::DrawGlyph;
  p.glyph = Glyph(X, Y, width, height, data + index * height * ((width + 7) / 8));
  m_displayController->addPrimitive(p);
}


void Canvas::renderGlyphsBuffer(int itemX, int itemY, GlyphsBuffer const * glyphsBuffer)
{
  Primitive p;
  p.cmd                    = PrimitiveCmd::RenderGlyphsBuffer;
  p.glyphsBufferRenderInfo = GlyphsBufferRenderInfo(itemX, itemY, glyphsBuffer);
  m_displayController->addPrimitive(p);
}


void Canvas::setGlyphOptions(GlyphOptions options)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetGlyphOptions;
  p.glyphOptions = options;
  m_displayController->addPrimitive(p);
  m_textHorizRate = options.doubleWidth > 0 ? 2 : 1;
}


void Canvas::resetGlyphOptions()
{
  setGlyphOptions(GlyphOptions());
}


void Canvas::setPaintOptions(PaintOptions options)
{
  Primitive p;
  p.cmd = PrimitiveCmd::SetPaintOptions;
  p.paintOptions = options;
  m_displayController->addPrimitive(p);
}


void Canvas::resetPaintOptions()
{
  setPaintOptions(PaintOptions());
}


void Canvas::selectFont(FontInfo const * fontInfo)
{
  m_fontInfo = fontInfo;
}


void Canvas::drawChar(int X, int Y, char c)
{
  drawGlyph(X, Y, m_fontInfo->width, m_fontInfo->height, m_fontInfo->data, c);
}


void Canvas::drawText(int X, int Y, char const * text, bool wrap)
{
  if (m_fontInfo == nullptr)
    selectFont(&FONT_8x8);
  drawText(m_fontInfo, X, Y, text, wrap);
}


void Canvas::drawText(FontInfo const * fontInfo, int X, int Y, char const * text, bool wrap)
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


void Canvas::drawTextWithEllipsis(FontInfo const * fontInfo, int X, int Y, char const * text, int maxX)
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


int Canvas::textExtent(FontInfo const * fontInfo, char const * text)
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


int Canvas::textExtent(char const * text)
{
  return textExtent(m_fontInfo, text);
}


void Canvas::drawTextFmt(int X, int Y, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    drawText(X, Y, buf, false);
  }
  va_end(ap);
}


void Canvas::copyRect(int sourceX, int sourceY, int destX, int destY, int width, int height)
{
  moveTo(destX, destY);
  int sourceX2 = sourceX + width - 1;
  int sourceY2 = sourceY + height - 1;
  Primitive p;
  p.cmd  = PrimitiveCmd::CopyRect;
  p.rect = Rect(sourceX, sourceY, sourceX2, sourceY2);
  m_displayController->addPrimitive(p);
}


void Canvas::drawBitmap(int X, int Y, Bitmap const * bitmap)
{
  Primitive p;
  p.cmd               = PrimitiveCmd::DrawBitmap;
  p.bitmapDrawingInfo = BitmapDrawingInfo(X, Y, bitmap);
  m_displayController->addPrimitive(p);
}


void Canvas::swapBuffers()
{
  Primitive p;
  p.cmd = PrimitiveCmd::SwapBuffers;
  p.notifyTask = xTaskGetCurrentTaskHandle();
  m_displayController->addPrimitive(p);
  m_displayController->primitivesExecutionWait();
}


void Canvas::drawPath(Point const * points, int pointsCount)
{
  Primitive p;
  p.cmd = PrimitiveCmd::DrawPath;
  p.path.points = points;
  p.path.pointsCount = pointsCount;
  p.path.freePoints = false;
  m_displayController->addPrimitive(p);
}


void Canvas::fillPath(Point const * points, int pointsCount)
{
  Primitive p;
  p.cmd = PrimitiveCmd::FillPath;
  p.path.points = points;
  p.path.pointsCount = pointsCount;
  p.path.freePoints = false;
  m_displayController->addPrimitive(p);
}


RGB888 Canvas::getPixel(int X, int Y)
{
  RGB888 rgb;
  m_displayController->readScreen(Rect(X, Y, X, Y), &rgb);
  return rgb;
}


} // end of namespace
