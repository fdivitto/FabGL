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



#include "displaycontroller.h"

#include <string.h>
#include <limits.h>

#include "fabutils.h"
#include "images/cursors.h"



namespace fabgl {




// Array to convert Color enum to RGB222 struct
// First eight maximum value is '1' to make them visible also when 8 colors are used.
// From Red to Cyan are changed (1=>2) when 64 color mode is used.
RGB222 COLOR2RGB222[16] = {
  { 0, 0, 0 }, // Black
  { 1, 0, 0 }, // Red
  { 0, 1, 0 }, // Green
  { 1, 1, 0 }, // Yellow
  { 0, 0, 1 }, // Blue
  { 1, 0, 1 }, // Magenta
  { 0, 1, 1 }, // Cyan
  { 1, 1, 1 }, // White
  { 1, 1, 1 }, // BrightBlack
  { 3, 0, 0 }, // BrightRed
  { 0, 3, 0 }, // BrightGreen
  { 3, 3, 0 }, // BrightYellow
  { 0, 0, 3 }, // BrightBlue
  { 3, 0, 3 }, // BrightMagenta
  { 0, 3, 3 }, // BrightCyan
  { 3, 3, 3 }, // BrightWhite
};


// Array to convert Color enum to RGB888 struct
const RGB888 COLOR2RGB888[16] = {
  {   0,   0,   0 }, // Black
  { 128,   0,   0 }, // Red
  {   0, 128,   0 }, // Green
  { 128, 128,   0 }, // Yellow
  {   0,   0, 128 }, // Blue
  { 128,   0, 128 }, // Magenta
  {   0, 128, 128 }, // Cyan
  { 128, 128, 128 }, // White
  {  64,  64,  64 }, // BrightBlack
  { 255,   0,   0 }, // BrightRed
  {   0, 255,   0 }, // BrightGreen
  { 255, 255,   0 }, // BrightYellow
  {   0,   0, 255 }, // BrightBlue
  { 255,   0, 255 }, // BrightMagenta
  {   0, 255, 255 }, // BrightCyan
  { 255, 255, 255 }, // BrightWhite
};



int getRowLength(int width, PixelFormat format)
{
  switch (format) {
    case PixelFormat::Mask:
      return (width + 7) / 8;
    case PixelFormat::RGBA2222:
      return width * sizeof(RGBA2222);
    case PixelFormat::RGBA8888:
      return width * sizeof(RGBA8888);
    case PixelFormat::Undefined:
      return 0;
  }
  return 0; // just to avoid compiler complaint
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// RGB222 implementation


RGB222::RGB222(Color color)
{
  *this = COLOR2RGB222[(int)color];
}


// static member
void RGB222::optimizeFor64Colors()
{
  COLOR2RGB222[1] = RGB222(2, 0, 0); // Red
  COLOR2RGB222[2] = RGB222(0, 2, 0); // Green
  COLOR2RGB222[3] = RGB222(2, 2, 0); // Yellow
  COLOR2RGB222[4] = RGB222(0, 0, 2); // Blue
  COLOR2RGB222[5] = RGB222(2, 0, 2); // Magenta
  COLOR2RGB222[6] = RGB222(0, 2, 2); // Cyan
  COLOR2RGB222[7] = RGB222(2, 2, 2); // White
}


//   0 ..  63 => 0
//  64 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
RGB222::RGB222(RGB888 const & value)
{
  R = value.R >> 6;
  G = value.G >> 6;
  B = value.B >> 6;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// RGB888 implementation


RGB888::RGB888(Color color)
{
  *this = COLOR2RGB888[(int)color];
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Sprite implementation


Sprite::Sprite()
{
  x                       = 0;
  y                       = 0;
  currentFrame            = 0;
  frames                  = nullptr;
  framesCount             = 0;
  savedBackgroundWidth    = 0;
  savedBackgroundHeight   = 0;
  savedBackground         = nullptr; // allocated or reallocated when bitmaps are added
  collisionDetectorObject = nullptr;
  visible                 = true;
  isStatic                = false;
  allowDraw               = true;
}


Sprite::~Sprite()
{
  free(frames);
  free(savedBackground);
}


void Sprite::clearBitmaps()
{
  free(frames);
  frames = nullptr;
  framesCount = 0;
}


Sprite * Sprite::addBitmap(Bitmap * bitmap)
{
  ++framesCount;
  frames = (Bitmap**) realloc(frames, sizeof(Bitmap*) * framesCount);
  frames[framesCount - 1] = bitmap;
  return this;
}


Sprite * Sprite::addBitmap(Bitmap * bitmap[], int count)
{
  frames = (Bitmap**) realloc(frames, sizeof(Bitmap*) * (framesCount + count));
  for (int i = 0; i < count; ++i)
    frames[framesCount + i] = bitmap[i];
  framesCount += count;
  return this;
}


Sprite * Sprite::moveBy(int offsetX, int offsetY)
{
  x += offsetX;
  y += offsetY;
  return this;
}


Sprite * Sprite::moveBy(int offsetX, int offsetY, int wrapAroundWidth, int wrapAroundHeight)
{
  x += offsetX;
  y += offsetY;
  if (x > wrapAroundWidth)
    x = - (int) getWidth();
  if (x < - (int) getWidth())
    x = wrapAroundWidth;
  if (y > wrapAroundHeight)
    y = - (int) getHeight();
  if (y < - (int) getHeight())
    y = wrapAroundHeight;
  return this;
}


Sprite * Sprite::moveTo(int x, int y)
{
  this->x = x;
  this->y = y;
  return this;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Bitmap implementation



Bitmap::Bitmap(int width_, int height_, void const * data_, PixelFormat format_, RGB888 foregroundColor_, bool copy)
  : width(width_),
    height(height_),
    format(format_),
    foregroundColor(foregroundColor_),
    data((uint8_t*)data_),
    dataAllocated(false)
{
  if (copy) {
    allocate();
    copyFrom(data_);
  }
}


Bitmap::Bitmap(int width_, int height_, void const * data_, PixelFormat format_, bool copy)
  : Bitmap(width_, height_, data_, format_, RGB888(255, 255, 255), copy)
{
}


void Bitmap::allocate()
{
  if (dataAllocated) {
    free((void*)data);
    data = nullptr;
  }
  dataAllocated = true;
  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Mask:
      data = (uint8_t*) malloc((width + 7) * height / 8);
      break;
    case PixelFormat::RGBA2222:
      data = (uint8_t*) malloc(width * height);
      break;
    case PixelFormat::RGBA8888:
      data = (uint8_t*) malloc(width * height * 4);
      break;
  }
}


// data must have the same pixel format
void Bitmap::copyFrom(void const * srcData)
{
  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Mask:
      memcpy(data, srcData, (width + 7) * height / 8);
      break;
    case PixelFormat::RGBA2222:
      memcpy(data, srcData, width * height);
      break;
    case PixelFormat::RGBA8888:
      memcpy(data, srcData, width * height * 4);
      break;
  }
}


void Bitmap::setPixel(int x, int y, int value)
{
  int rowlen = (width + 7) / 8;
  uint8_t * rowptr = data + y * rowlen;
  if (value)
    rowptr[x >> 3] |= 0x80 >> (x & 7);
  else
    rowptr[x >> 3] &= ~(0x80 >> (x & 7));
}


void Bitmap::setPixel(int x, int y, RGBA2222 value)
{
  ((RGBA2222*)data)[y * width + x] = value;
}


void Bitmap::setPixel(int x, int y, RGBA8888 value)
{
  ((RGBA8888*)data)[y * width + x] = value;
}


int Bitmap::getAlpha(int x, int y)
{
  int r = 0;
  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Mask:
    {
      int rowlen = (width + 7) / 8;
      uint8_t * rowptr = data + y * rowlen;
      r = (rowptr[x >> 3] >> (7 - (x & 7))) & 1;
      break;
    }
    case PixelFormat::RGBA2222:
      r = ((RGBA2222*)data)[y * height + x].A;
      break;
    case PixelFormat::RGBA8888:
      r = ((RGBA8888*)data)[y * height + x].A;
      break;
  }
  return r;
}


Bitmap::~Bitmap()
{
  if (dataAllocated)
    free((void*)data);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// DisplayController implementation


DisplayController::DisplayController()
{
  m_execQueue = xQueueCreate(FABGLIB_EXEC_QUEUE_SIZE, sizeof(Primitive));

  m_backgroundPrimitiveExecutionEnabled = true;
  m_sprites                             = nullptr;
  m_spritesCount                        = 0;
  m_doubleBuffered                      = false;
  m_mouseCursor.visible                 = false;
  m_backgroundPrimitiveTimeoutEnabled   = true;
  m_spritesHidden                       = true;
}


DisplayController::~DisplayController()
{
  vQueueDelete(m_execQueue);
}


void DisplayController::resetPaintState()
{
  m_paintState.penColor              = RGB888(255, 255, 255);
  m_paintState.brushColor            = RGB888(0, 0, 0);
  m_paintState.position              = Point(0, 0);
  m_paintState.glyphOptions.value    = 0;  // all options: 0
  m_paintState.paintOptions          = PaintOptions();
  m_paintState.scrollingRegion       = Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1);
  m_paintState.origin                = Point(0, 0);
  m_paintState.clippingRect          = Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1);
  m_paintState.absClippingRect       = m_paintState.clippingRect;
}


void DisplayController::addPrimitive(Primitive const & primitive)
{
  if ((m_backgroundPrimitiveExecutionEnabled && m_doubleBuffered == false) || primitive.cmd == PrimitiveCmd::SwapBuffers)
    xQueueSendToBack(m_execQueue, &primitive, portMAX_DELAY);
  else {
    execPrimitive(primitive);
    showSprites();
  }
}


// call this only inside an ISR
bool IRAM_ATTR DisplayController::getPrimitiveISR(Primitive * primitive)
{
  return xQueueReceiveFromISR(m_execQueue, primitive, nullptr);
}


bool DisplayController::getPrimitive(Primitive * primitive, int timeOutMS)
{
  return xQueueReceive(m_execQueue, primitive, timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS));
}


// cannot be called inside an ISR
void DisplayController::waitForPrimitives()
{
  Primitive p;
  xQueuePeek(m_execQueue, &p, portMAX_DELAY);
}


// call this only inside an ISR
void IRAM_ATTR DisplayController::insertPrimitiveISR(Primitive * primitive)
{
  xQueueSendToFrontFromISR(m_execQueue, primitive, nullptr);
}


void DisplayController::insertPrimitive(Primitive * primitive, int timeOutMS)
{
  xQueueSendToFront(m_execQueue, primitive, timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS));
}


void DisplayController::primitivesExecutionWait()
{
  while (uxQueueMessagesWaiting(m_execQueue) > 0)
    ;
}


// When false primitives are executed immediately, otherwise they are added to the primitive queue
// When set to false the queue is emptied executing all pending primitives
// Cannot be nested
void DisplayController::enableBackgroundPrimitiveExecution(bool value)
{
  if (value != m_backgroundPrimitiveExecutionEnabled) {
    if (value) {
      resumeBackgroundPrimitiveExecution();
    } else {
      suspendBackgroundPrimitiveExecution();
      processPrimitives();
    }
    m_backgroundPrimitiveExecutionEnabled = value;
  }
}


// Use for fast queue processing. Warning, may generate flickering because don't care of vertical sync
// Do not call inside ISR
void IRAM_ATTR DisplayController::processPrimitives()
{
  suspendBackgroundPrimitiveExecution();
  Primitive prim;
  while (xQueueReceive(m_execQueue, &prim, 0) == pdTRUE)
    execPrimitive(prim);
  showSprites();
  resumeBackgroundPrimitiveExecution();
}


void DisplayController::setSprites(Sprite * sprites, int count, int spriteSize)
{
  processPrimitives();
  primitivesExecutionWait();
  m_sprites      = sprites;
  m_spriteSize   = spriteSize;
  m_spritesCount = count;

  // allocates background buffer
  if (!isDoubleBuffered()) {
    uint8_t * spritePtr = (uint8_t*)m_sprites;
    for (int i = 0; i < m_spritesCount; ++i, spritePtr += m_spriteSize) {
      Sprite * sprite = (Sprite*) spritePtr;
      int reqBackBufferSize = 0;
      for (int i = 0; i < sprite->framesCount; ++i)
        reqBackBufferSize = tmax(reqBackBufferSize, getRowLength(sprite->frames[i]->width, getBitmapSavePixelFormat()) * sprite->frames[i]->height);
      sprite->savedBackground = (uint8_t*) realloc(sprite->savedBackground, reqBackBufferSize);
    }
  }
}


Sprite * IRAM_ATTR DisplayController::getSprite(int index)
{
  return (Sprite*) ((uint8_t*)m_sprites + index * m_spriteSize);
}


void DisplayController::refreshSprites()
{
  Primitive p;
  p.cmd = PrimitiveCmd::RefreshSprites;
  addPrimitive(p);
}


void IRAM_ATTR DisplayController::hideSprites()
{
  if (!m_spritesHidden) {
    m_spritesHidden = true;

    // normal sprites
    if (spritesCount() > 0 && !isDoubleBuffered()) {
      // restore saved backgrounds
      for (int i = spritesCount() - 1; i >= 0; --i) {
        Sprite * sprite = getSprite(i);
        if (sprite->allowDraw && sprite->savedBackgroundWidth > 0) {
          Bitmap bitmap(sprite->savedBackgroundWidth, sprite->savedBackgroundHeight, sprite->savedBackground, getBitmapSavePixelFormat());
          drawBitmap(sprite->savedX, sprite->savedY, &bitmap, nullptr, true);
          sprite->savedBackgroundWidth = sprite->savedBackgroundHeight = 0;
        }
      }
    }

    // mouse cursor sprite
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->savedBackgroundWidth > 0) {
      Bitmap bitmap(mouseSprite->savedBackgroundWidth, mouseSprite->savedBackgroundHeight, mouseSprite->savedBackground, getBitmapSavePixelFormat());
      drawBitmap(mouseSprite->savedX, mouseSprite->savedY, &bitmap, nullptr, true);
      mouseSprite->savedBackgroundWidth = mouseSprite->savedBackgroundHeight = 0;
    }

  }
}


void IRAM_ATTR DisplayController::showSprites()
{
  if (m_spritesHidden) {
    m_spritesHidden = false;

    // normal sprites
    // save backgrounds and draw sprites
    for (int i = 0; i < spritesCount(); ++i) {
      Sprite * sprite = getSprite(i);
      if (sprite->visible && sprite->allowDraw && sprite->getFrame()) {
        // save sprite X and Y so other threads can change them without interferring
        int16_t spriteX = sprite->x;
        int16_t spriteY = sprite->y;
        Bitmap const * bitmap = sprite->getFrame();
        drawBitmap(spriteX, spriteY, bitmap, sprite->savedBackground, true);
        sprite->savedX = spriteX;
        sprite->savedY = spriteY;
        sprite->savedBackgroundWidth  = bitmap->width;
        sprite->savedBackgroundHeight = bitmap->height;
        if (sprite->isStatic)
          sprite->allowDraw = false;
      }
    }

    // mouse cursor sprite
    // save backgrounds and draw mouse cursor
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->visible && mouseSprite->getFrame()) {
      // save sprite X and Y so other threads can change them without interferring
      int16_t spriteX = mouseSprite->x;
      int16_t spriteY = mouseSprite->y;
      Bitmap const * bitmap = mouseSprite->getFrame();
      drawBitmap(spriteX, spriteY, bitmap, mouseSprite->savedBackground, true);
      mouseSprite->savedX = spriteX;
      mouseSprite->savedY = spriteY;
      mouseSprite->savedBackgroundWidth  = bitmap->width;
      mouseSprite->savedBackgroundHeight = bitmap->height;
    }

  }
}


// cursor = nullptr -> disable mouse
void DisplayController::setMouseCursor(Cursor * cursor)
{
  if (cursor == nullptr || &cursor->bitmap != m_mouseCursor.getFrame()) {
    m_mouseCursor.visible = false;
    m_mouseCursor.clearBitmaps();

    refreshSprites();
    processPrimitives();
    primitivesExecutionWait();

    if (cursor) {
      m_mouseCursor.moveBy(+m_mouseHotspotX, +m_mouseHotspotY);
      m_mouseHotspotX = cursor->hotspotX;
      m_mouseHotspotY = cursor->hotspotY;
      m_mouseCursor.addBitmap(&cursor->bitmap);
      m_mouseCursor.visible = true;
      m_mouseCursor.moveBy(-m_mouseHotspotX, -m_mouseHotspotY);
      if (!isDoubleBuffered())
        m_mouseCursor.savedBackground = (uint8_t*) realloc(m_mouseCursor.savedBackground, getRowLength(cursor->bitmap.width, getBitmapSavePixelFormat()) * cursor->bitmap.height);
    }
    refreshSprites();
  }
}


void DisplayController::setMouseCursor(CursorName cursorName)
{
  setMouseCursor(&CURSORS[(int)cursorName]);
}


void DisplayController::setMouseCursorPos(int X, int Y)
{
  m_mouseCursor.moveTo(X - m_mouseHotspotX, Y - m_mouseHotspotY);
  refreshSprites();
}


void IRAM_ATTR DisplayController::execPrimitive(Primitive const & prim)
{
  switch (prim.cmd) {
    case PrimitiveCmd::Refresh:
      break;
    case PrimitiveCmd::SetPenColor:
      paintState().penColor = prim.color;
      break;
    case PrimitiveCmd::SetBrushColor:
      paintState().brushColor = prim.color;
      break;
    case PrimitiveCmd::SetPixel:
      setPixelAt( (PixelDesc) { prim.position, paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor } );
      break;
    case PrimitiveCmd::SetPixelAt:
      setPixelAt(prim.pixelDesc);
      break;
    case PrimitiveCmd::MoveTo:
      paintState().position = Point(prim.position.X + paintState().origin.X, prim.position.Y + paintState().origin.Y);
      break;
    case PrimitiveCmd::LineTo:
      lineTo(prim.position);
      break;
    case PrimitiveCmd::FillRect:
      fillRect(prim.rect);
      break;
    case PrimitiveCmd::DrawRect:
      drawRect(prim.rect);
      break;
    case PrimitiveCmd::FillEllipse:
      fillEllipse(prim.size);
      break;
    case PrimitiveCmd::DrawEllipse:
      drawEllipse(prim.size);
      break;
    case PrimitiveCmd::Clear:
      clear();
      break;
    case PrimitiveCmd::VScroll:
      VScroll(prim.ivalue);
      break;
    case PrimitiveCmd::HScroll:
      HScroll(prim.ivalue);
      break;
    case PrimitiveCmd::DrawGlyph:
      drawGlyph(prim.glyph, paintState().glyphOptions, paintState().penColor, paintState().brushColor);
      break;
    case PrimitiveCmd::SetGlyphOptions:
      paintState().glyphOptions = prim.glyphOptions;
      break;
    case PrimitiveCmd::SetPaintOptions:
      paintState().paintOptions = prim.paintOptions;
      break;
    case PrimitiveCmd::InvertRect:
      invertRect(prim.rect);
      break;
    case PrimitiveCmd::CopyRect:
      copyRect(prim.rect);
      break;
    case PrimitiveCmd::SetScrollingRegion:
      paintState().scrollingRegion = prim.rect;
      break;
    case PrimitiveCmd::SwapFGBG:
      swapFGBG(prim.rect);
      break;
    case PrimitiveCmd::RenderGlyphsBuffer:
      renderGlyphsBuffer(prim.glyphsBufferRenderInfo);
      break;
    case PrimitiveCmd::DrawBitmap:
      hideSprites();
      drawBitmap(prim.bitmapDrawingInfo.X + paintState().origin.X, prim.bitmapDrawingInfo.Y + paintState().origin.Y, prim.bitmapDrawingInfo.bitmap, nullptr, false);
      break;
    case PrimitiveCmd::RefreshSprites:
      hideSprites();
      showSprites();
      break;
    case PrimitiveCmd::SwapBuffers:
      swapBuffers();
      break;
    case PrimitiveCmd::DrawPath:
      drawPath(prim.path);
      break;
    case PrimitiveCmd::FillPath:
      fillPath(prim.path);
      break;
    case PrimitiveCmd::SetOrigin:
      paintState().origin = prim.position;
      updateAbsoluteClippingRect();
      break;
    case PrimitiveCmd::SetClippingRect:
      paintState().clippingRect = prim.rect;
      updateAbsoluteClippingRect();
      break;
  }
}


void IRAM_ATTR DisplayController::lineTo(Point const & position)
{
  hideSprites();
  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor;

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;

  drawLine(paintState().position.X, paintState().position.Y, position.X + origX, position.Y + origY, color);

  paintState().position = Point(position.X + origX, position.Y + origY);
}


void IRAM_ATTR DisplayController::updateAbsoluteClippingRect()
{
  int X1 = iclamp(paintState().origin.X + paintState().clippingRect.X1, 0, getViewPortWidth() - 1);
  int Y1 = iclamp(paintState().origin.Y + paintState().clippingRect.Y1, 0, getViewPortHeight() - 1);
  int X2 = iclamp(paintState().origin.X + paintState().clippingRect.X2, 0, getViewPortWidth() - 1);
  int Y2 = iclamp(paintState().origin.Y + paintState().clippingRect.Y2, 0, getViewPortHeight() - 1);
  paintState().absClippingRect = Rect(X1, Y1, X2, Y2);
}


void IRAM_ATTR DisplayController::drawRect(Rect const & rect)
{
  int x1 = (rect.X1 < rect.X2 ? rect.X1 : rect.X2) + paintState().origin.X;
  int y1 = (rect.Y1 < rect.Y2 ? rect.Y1 : rect.Y2) + paintState().origin.Y;
  int x2 = (rect.X1 < rect.X2 ? rect.X2 : rect.X1) + paintState().origin.X;
  int y2 = (rect.Y1 < rect.Y2 ? rect.Y2 : rect.Y1) + paintState().origin.Y;

  hideSprites();
  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor;

  drawLine(x1 + 1, y1,     x2, y1, color);
  drawLine(x2,     y1 + 1, x2, y2, color);
  drawLine(x2 - 1, y2,     x1, y2, color);
  drawLine(x1,     y2 - 1, x1, y1, color);
}


void IRAM_ATTR DisplayController::fillRect(Rect const & rect)
{
  int x1 = (rect.X1 < rect.X2 ? rect.X1 : rect.X2) + paintState().origin.X;
  int y1 = (rect.Y1 < rect.Y2 ? rect.Y1 : rect.Y2) + paintState().origin.Y;
  int x2 = (rect.X1 < rect.X2 ? rect.X2 : rect.X1) + paintState().origin.X;
  int y2 = (rect.Y1 < rect.Y2 ? rect.Y2 : rect.Y1) + paintState().origin.Y;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  if (x1 > clipX2 || x2 < clipX1 || y1 > clipY2 || y2 < clipY1)
    return;

  x1 = iclamp(x1, clipX1, clipX2);
  y1 = iclamp(y1, clipY1, clipY2);
  x2 = iclamp(x2, clipX1, clipX2);
  y2 = iclamp(y2, clipY1, clipY2);

  hideSprites();
  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;

  for (int y = y1; y <= y2; ++y)
    fillRow(y, x1, x2, color);
}


void IRAM_ATTR DisplayController::fillEllipse(Size const & size)
{
  hideSprites();
  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int halfWidth  = size.width / 2;
  const int halfHeight = size.height / 2;
  const int hh = halfHeight * halfHeight;
  const int ww = halfWidth * halfWidth;
  const int hhww = hh * ww;

  int x0 = halfWidth;
  int dx = 0;

  int centerX = paintState().position.X;
  int centerY = paintState().position.Y;

  if (centerY >= clipY1 && centerY <= clipY2) {
    int col1 = centerX - halfWidth;
    int col2 = centerX + halfWidth;
    if (col1 <= clipX2 && col2 >= clipX1) {
      col1 = iclamp(col1, clipX1, clipX2);
      col2 = iclamp(col2, clipX1, clipX2);
      fillRow(centerY, col1, col2, color);
    }
  }

  for (int y = 1; y <= halfHeight; ++y)
  {
    int x1 = x0 - (dx - 1);
    for ( ; x1 > 0; x1--)
      if (x1 * x1 * hh + y * y * ww <= hhww)
        break;
    dx = x0 - x1;
    x0 = x1;

    int col1 = centerX - x0;
    int col2 = centerX + x0;

    if (col1 <= clipX2 && col2 >= clipX1) {

      col1 = iclamp(col1, clipX1, clipX2);
      col2 = iclamp(col2, clipX1, clipX2);

      int y1 = centerY - y;
      if (y1 >= clipY1 && y1 <= clipY2)
        fillRow(y1, col1, col2, color);

      int y2 = centerY + y;
      if (y2 >= clipY1 && y2 <= clipY2)
        fillRow(y2, col1, col2, color);

    }
  }
}


void IRAM_ATTR DisplayController::renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo)
{
  hideSprites();
  int itemX = glyphsBufferRenderInfo.itemX;
  int itemY = glyphsBufferRenderInfo.itemY;

  int glyphsWidth  = glyphsBufferRenderInfo.glyphsBuffer->glyphsWidth;
  int glyphsHeight = glyphsBufferRenderInfo.glyphsBuffer->glyphsHeight;

  uint32_t const * mapItem = glyphsBufferRenderInfo.glyphsBuffer->map + itemX + itemY * glyphsBufferRenderInfo.glyphsBuffer->columns;

  GlyphOptions glyphOptions = glyphMapItem_getOptions(mapItem);
  auto fgColor = glyphMapItem_getFGColor(mapItem);
  auto bgColor = glyphMapItem_getBGColor(mapItem);

  Glyph glyph;
  glyph.X      = (int16_t) (itemX * glyphsWidth * (glyphOptions.doubleWidth ? 2 : 1));
  glyph.Y      = (int16_t) (itemY * glyphsHeight);
  glyph.width  = glyphsWidth;
  glyph.height = glyphsHeight;
  glyph.data   = glyphsBufferRenderInfo.glyphsBuffer->glyphsData + glyphMapItem_getIndex(mapItem) * glyphsHeight * ((glyphsWidth + 7) / 8);;

  drawGlyph(glyph, glyphOptions, fgColor, bgColor);
}


void IRAM_ATTR DisplayController::drawPath(Path const & path)
{
  hideSprites();

  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor;

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;

  int i = 0;
  for (; i < path.pointsCount - 1; ++i)
    drawLine(path.points[i].X + origX, path.points[i].Y + origY, path.points[i + 1].X + origX, path.points[i + 1].Y + origY, color);
  drawLine(path.points[i].X + origX, path.points[i].Y + origY, path.points[0].X + origX, path.points[0].Y + origY, color);
}


void IRAM_ATTR DisplayController::fillPath(Path const & path)
{
  hideSprites();

  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;

  int minX = clipX1;
  int maxX = clipX2 + 1;

  int minY = INT_MAX;
  int maxY = 0;
  for (int i = 0; i < path.pointsCount; ++i) {
    int py = path.points[i].Y + origY;
    if (py < minY)
      minY = py;
    if (py > maxY)
      maxY = py;
  }
  minY = tmax(clipY1, minY);
  maxY = tmin(clipY2, maxY);

  int16_t nodeX[path.pointsCount];

  for (int pixelY = minY; pixelY <= maxY; ++pixelY) {

    int nodes = 0;
    int j = path.pointsCount - 1;
    for (int i = 0; i < path.pointsCount; ++i) {
      int piy = path.points[i].Y + origY;
      int pjy = path.points[j].Y + origY;
      if ((piy < pixelY && pjy >= pixelY) || (pjy < pixelY && piy >= pixelY)) {
        int pjx = path.points[j].X + origX;
        int pix = path.points[i].X + origX;
        int a = (pixelY - piy) * (pjx - pix);
        int b = (pjy - piy);
        nodeX[nodes++] = pix + a / b + (((a < 0) ^ (b > 0)) && (a % b));
      }
      j = i;
    }

    int i = 0;
    while (i < nodes - 1) {
      if (nodeX[i] > nodeX[i + 1]) {
        tswap(nodeX[i], nodeX[i + 1]);
        if (i)
          --i;
      } else
        ++i;
    }

    for (int i = 0; i < nodes; i += 2) {
      if (nodeX[i] >= maxX)
        break;
      if (nodeX[i + 1] > minX) {
        if (nodeX[i] < minX)
          nodeX[i] = minX;
        if (nodeX[i + 1] > maxX)
          nodeX[i + 1] = maxX;
        fillRow(pixelY, nodeX[i], nodeX[i + 1] - 1, color);
      }
    }
  }
}


void IRAM_ATTR DisplayController::drawBitmap(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, bool ignoreClippingRect)
{
  const int clipX1 = ignoreClippingRect ? 0 : paintState().absClippingRect.X1;
  const int clipY1 = ignoreClippingRect ? 0 : paintState().absClippingRect.Y1;
  const int clipX2 = ignoreClippingRect ? getViewPortWidth() - 1 : paintState().absClippingRect.X2;
  const int clipY2 = ignoreClippingRect ? getViewPortHeight() - 1 : paintState().absClippingRect.Y2;

  if (destX > clipX2 || destY > clipY2)
    return;

  int width  = bitmap->width;
  int height = bitmap->height;

  int X1 = 0;
  int XCount = width;

  if (destX < clipX1) {
    X1 = clipX1 - destX;
    destX = clipX1;
  }
  if (X1 >= width)
    return;

  if (destX + XCount > clipX2 + 1)
    XCount = clipX2 + 1 - destX;
  if (X1 + XCount > width)
    XCount = width - X1;

  int Y1 = 0;
  int YCount = height;

  if (destY < clipY1) {
    Y1 = clipY1 - destY;
    destY = clipY1;
  }
  if (Y1 >= height)
    return;

  if (destY + YCount > clipY2 + 1)
    YCount = clipY2 + 1 - destY;
  if (Y1 + YCount > height)
    YCount = height - Y1;

  switch (bitmap->format) {

    case PixelFormat::Undefined:
      break;

    case PixelFormat::Mask:
      drawBitmap_Mask(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA2222:
      drawBitmap_RGBA2222(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA8888:
      drawBitmap_RGBA8888(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

  }

}



} // end of namespace
