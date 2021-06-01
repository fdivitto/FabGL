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



#include "displaycontroller.h"

#include <string.h>
#include <limits.h>
#include <math.h>

#include "freertos/task.h"

#include "fabutils.h"
#include "images/cursors.h"


#pragma GCC optimize ("O2")


namespace fabgl {




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



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// RGB222 implementation

bool RGB222::lowBitOnly = false;


//   0 ..  63 => 0
//  64 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
RGB222::RGB222(RGB888 const & value)
{
  if (lowBitOnly) {
    R = value.R ? 3 : 0;
    G = value.G ? 3 : 0;
    B = value.B ? 3 : 0;
  } else {
    R = value.R >> 6;
    G = value.G >> 6;
    B = value.B >> 6;
  }
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
// RGB888toPackedRGB222()

uint8_t RGB888toPackedRGB222(RGB888 const & rgb)
{
  // 64 colors
  static const int CONVR64[4] = { 0 << 0,    // 00XXXXXX (0..63)
                                  1 << 0,    // 01XXXXXX (64..127)
                                  2 << 0,    // 10XXXXXX (128..191)
                                  3 << 0, }; // 11XXXXXX (192..255)
  static const int CONVG64[4] = { 0 << 2,    // 00XXXXXX (0..63)
                                  1 << 2,    // 01XXXXXX (64..127)
                                  2 << 2,    // 10XXXXXX (128..191)
                                  3 << 2, }; // 11XXXXXX (192..255)
  static const int CONVB64[4] = { 0 << 4,    // 00XXXXXX (0..63)
                                  1 << 4,    // 01XXXXXX (64..127)
                                  2 << 4,    // 10XXXXXX (128..191)
                                  3 << 4, }; // 11XXXXXX (192..255)
  // 8 colors
  static const int CONVR8[4] = { 0 << 0,    // 00XXXXXX (0..63)
                                 3 << 0,    // 01XXXXXX (64..127)
                                 3 << 0,    // 10XXXXXX (128..191)
                                 3 << 0, }; // 11XXXXXX (192..255)
  static const int CONVG8[4] = { 0 << 2,    // 00XXXXXX (0..63)
                                 3 << 2,    // 01XXXXXX (64..127)
                                 3 << 2,    // 10XXXXXX (128..191)
                                 3 << 2, }; // 11XXXXXX (192..255)
  static const int CONVB8[4] = { 0 << 4,    // 00XXXXXX (0..63)
                                 3 << 4,    // 01XXXXXX (64..127)
                                 3 << 4,    // 10XXXXXX (128..191)
                                 3 << 4, }; // 11XXXXXX (192..255)

  if (RGB222::lowBitOnly)
    return (CONVR8[rgb.R >> 6]) | (CONVG8[rgb.G >> 6]) | (CONVB8[rgb.B >> 6]);
  else
    return (CONVR64[rgb.R >> 6]) | (CONVG64[rgb.G >> 6]) | (CONVB64[rgb.B >> 6]);
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
  savedX                  = 0;
  savedY                  = 0;
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
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
      data = (uint8_t*) heap_caps_malloc((width + 7) / 8 * height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
    case PixelFormat::RGBA2222:
      data = (uint8_t*) heap_caps_malloc(width * height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
    case PixelFormat::RGBA8888:
      data = (uint8_t*) heap_caps_malloc(width * height * 4, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
  }
}


// data must have the same pixel format
void Bitmap::copyFrom(void const * srcData)
{
  switch (format) {
    case PixelFormat::Undefined:
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
      memcpy(data, srcData, (width + 7) / 8 * height);
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
    case PixelFormat::Native:
      r = 0xff;
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
    heap_caps_free((void*)data);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// BitmappedDisplayController implementation


int BitmappedDisplayController::queueSize = FABGLIB_DEFAULT_DISPLAYCONTROLLER_QUEUE_SIZE;


BitmappedDisplayController::BitmappedDisplayController()
  : m_primDynMemPool(FABGLIB_PRIMITIVES_DYNBUFFERS_SIZE)
{
  m_execQueue                           = nullptr;
  m_backgroundPrimitiveExecutionEnabled = true;
  m_sprites                             = nullptr;
  m_spritesCount                        = 0;
  m_doubleBuffered                      = false;
  m_mouseCursor.visible                 = false;
  m_backgroundPrimitiveTimeoutEnabled   = true;
  m_spritesHidden                       = true;
}


BitmappedDisplayController::~BitmappedDisplayController()
{
  vQueueDelete(m_execQueue);
}


void BitmappedDisplayController::setDoubleBuffered(bool value)
{
  m_doubleBuffered = value;
  if (m_execQueue)
    vQueueDelete(m_execQueue);
  // on double buffering a queue of single element is enough and necessary (see addPrimitive() for details)
  m_execQueue = xQueueCreate(value ? 1 : BitmappedDisplayController::queueSize, sizeof(Primitive));
}


void IRAM_ATTR BitmappedDisplayController::resetPaintState()
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
  m_paintState.penWidth              = 1;
  m_paintState.lineEnds              = LineEnds::None;
}


void BitmappedDisplayController::addPrimitive(Primitive & primitive)
{
  if ((m_backgroundPrimitiveExecutionEnabled && m_doubleBuffered == false) || primitive.cmd == PrimitiveCmd::SwapBuffers) {
    primitiveReplaceDynamicBuffers(primitive);
    xQueueSendToBack(m_execQueue, &primitive, portMAX_DELAY);

    if (m_doubleBuffered) {
      // wait notufy from PrimitiveCmd::SwapBuffers executor
      ulTaskNotifyTake(true, portMAX_DELAY);
    }

  } else {
    Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
    execPrimitive(primitive, updateRect, false);
    showSprites(updateRect);
  }
}


// some primitives require additional buffers (like drawPath and fillPath).
// this function copies primitive data into an allocated buffer (using LightMemoryPool allocator) that
// will be released inside primitive drawing code.
void BitmappedDisplayController::primitiveReplaceDynamicBuffers(Primitive & primitive)
{
  switch (primitive.cmd) {
    case PrimitiveCmd::DrawPath:
    case PrimitiveCmd::FillPath:
    {
      int sz = primitive.path.pointsCount * sizeof(Point);
      if (sz < FABGLIB_PRIMITIVES_DYNBUFFERS_SIZE) {
        void * newbuf = nullptr;
        // wait until we have enough free space
        while ((newbuf = m_primDynMemPool.alloc(sz)) == nullptr)
          taskYIELD();
        memcpy(newbuf, primitive.path.points, sz);
        primitive.path.points = (Point*)newbuf;
        primitive.path.freePoints = true;
      }
      break;
    }

    default:
      break;
  }
}


// call this only inside an ISR
bool IRAM_ATTR BitmappedDisplayController::getPrimitiveISR(Primitive * primitive)
{
  return xQueueReceiveFromISR(m_execQueue, primitive, nullptr);
}


bool BitmappedDisplayController::getPrimitive(Primitive * primitive, int timeOutMS)
{
  return xQueueReceive(m_execQueue, primitive, msToTicks(timeOutMS));
}


// cannot be called inside an ISR
void BitmappedDisplayController::waitForPrimitives()
{
  Primitive p;
  xQueuePeek(m_execQueue, &p, portMAX_DELAY);
}


void BitmappedDisplayController::primitivesExecutionWait()
{
  if (m_backgroundPrimitiveExecutionEnabled) {
    while (uxQueueMessagesWaiting(m_execQueue) > 0)
      ;
  }
}


// When false primitives are executed immediately, otherwise they are added to the primitive queue
// When set to false the queue is emptied executing all pending primitives
// Cannot be nested
void BitmappedDisplayController::enableBackgroundPrimitiveExecution(bool value)
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
void IRAM_ATTR BitmappedDisplayController::processPrimitives()
{
  suspendBackgroundPrimitiveExecution();
  Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
  Primitive prim;
  while (xQueueReceive(m_execQueue, &prim, 0) == pdTRUE)
    execPrimitive(prim, updateRect, false);
  showSprites(updateRect);
  resumeBackgroundPrimitiveExecution();
  Primitive p(PrimitiveCmd::Refresh, updateRect);
  addPrimitive(p);
}


void BitmappedDisplayController::setSprites(Sprite * sprites, int count, int spriteSize)
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
        reqBackBufferSize = tmax(reqBackBufferSize, sprite->frames[i]->width * getBitmapSavePixelSize() * sprite->frames[i]->height);
      if (reqBackBufferSize > 0)
        sprite->savedBackground = (uint8_t*) realloc(sprite->savedBackground, reqBackBufferSize);
    }
  }
}


Sprite * IRAM_ATTR BitmappedDisplayController::getSprite(int index)
{
  return (Sprite*) ((uint8_t*)m_sprites + index * m_spriteSize);
}


void BitmappedDisplayController::refreshSprites()
{
  Primitive p(PrimitiveCmd::RefreshSprites);
  addPrimitive(p);
}


void IRAM_ATTR BitmappedDisplayController::hideSprites(Rect & updateRect)
{
  if (!m_spritesHidden) {
    m_spritesHidden = true;

    // normal sprites
    if (spritesCount() > 0 && !isDoubleBuffered()) {
      // restore saved backgrounds
      for (int i = spritesCount() - 1; i >= 0; --i) {
        Sprite * sprite = getSprite(i);
        if (sprite->allowDraw && sprite->savedBackgroundWidth > 0) {
          int savedX = sprite->savedX;
          int savedY = sprite->savedY;
          int savedWidth  = sprite->savedBackgroundWidth;
          int savedHeight = sprite->savedBackgroundHeight;
          Bitmap bitmap(savedWidth, savedHeight, sprite->savedBackground, PixelFormat::Native);
          absDrawBitmap(savedX, savedY, &bitmap, nullptr, true);
          updateRect = updateRect.merge(Rect(savedX, savedY, savedX + savedWidth - 1, savedY + savedHeight - 1));
          sprite->savedBackgroundWidth = sprite->savedBackgroundHeight = 0;
        }
      }
    }

    // mouse cursor sprite
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->savedBackgroundWidth > 0) {
      int savedX = mouseSprite->savedX;
      int savedY = mouseSprite->savedY;
      int savedWidth  = mouseSprite->savedBackgroundWidth;
      int savedHeight = mouseSprite->savedBackgroundHeight;
      Bitmap bitmap(savedWidth, savedHeight, mouseSprite->savedBackground, PixelFormat::Native);
      absDrawBitmap(savedX, savedY, &bitmap, nullptr, true);
      updateRect = updateRect.merge(Rect(savedX, savedY, savedX + savedWidth - 1, savedY + savedHeight - 1));
      mouseSprite->savedBackgroundWidth = mouseSprite->savedBackgroundHeight = 0;
    }

  }
}


void IRAM_ATTR BitmappedDisplayController::showSprites(Rect & updateRect)
{
  if (m_spritesHidden) {
    m_spritesHidden = false;

    // normal sprites
    // save backgrounds and draw sprites
    for (int i = 0; i < spritesCount(); ++i) {
      Sprite * sprite = getSprite(i);
      if (sprite->visible && sprite->allowDraw && sprite->getFrame()) {
        // save sprite X and Y so other threads can change them without interferring
        int spriteX = sprite->x;
        int spriteY = sprite->y;
        Bitmap const * bitmap = sprite->getFrame();
        int bitmapWidth  = bitmap->width;
        int bitmapHeight = bitmap->height;
        absDrawBitmap(spriteX, spriteY, bitmap, sprite->savedBackground, true);
        sprite->savedX = spriteX;
        sprite->savedY = spriteY;
        sprite->savedBackgroundWidth  = bitmapWidth;
        sprite->savedBackgroundHeight = bitmapHeight;
        if (sprite->isStatic)
          sprite->allowDraw = false;
        updateRect = updateRect.merge(Rect(spriteX, spriteY, spriteX + bitmapWidth - 1, spriteY + bitmapHeight - 1));
      }
    }

    // mouse cursor sprite
    // save backgrounds and draw mouse cursor
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->visible && mouseSprite->getFrame()) {
      // save sprite X and Y so other threads can change them without interferring
      int spriteX = mouseSprite->x;
      int spriteY = mouseSprite->y;
      Bitmap const * bitmap = mouseSprite->getFrame();
      int bitmapWidth  = bitmap->width;
      int bitmapHeight = bitmap->height;
      absDrawBitmap(spriteX, spriteY, bitmap, mouseSprite->savedBackground, true);
      mouseSprite->savedX = spriteX;
      mouseSprite->savedY = spriteY;
      mouseSprite->savedBackgroundWidth  = bitmapWidth;
      mouseSprite->savedBackgroundHeight = bitmapHeight;
      updateRect = updateRect.merge(Rect(spriteX, spriteY, spriteX + bitmapWidth - 1, spriteY + bitmapHeight - 1));
    }

  }
}


// cursor = nullptr -> disable mouse
void BitmappedDisplayController::setMouseCursor(Cursor * cursor)
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
        m_mouseCursor.savedBackground = (uint8_t*) realloc(m_mouseCursor.savedBackground, cursor->bitmap.width * getBitmapSavePixelSize() * cursor->bitmap.height);
    }
    refreshSprites();
  }
}


void BitmappedDisplayController::setMouseCursor(CursorName cursorName)
{
  setMouseCursor(&CURSORS[(int)cursorName]);
}


void BitmappedDisplayController::setMouseCursorPos(int X, int Y)
{
  m_mouseCursor.moveTo(X - m_mouseHotspotX, Y - m_mouseHotspotY);
  refreshSprites();
}


void IRAM_ATTR BitmappedDisplayController::execPrimitive(Primitive const & prim, Rect & updateRect, bool insideISR)
{
  switch (prim.cmd) {
    case PrimitiveCmd::Flush:
      break;
    case PrimitiveCmd::Refresh:
      updateRect = updateRect.merge(prim.rect);
      break;
    case PrimitiveCmd::Reset:
      resetPaintState();
      break;
    case PrimitiveCmd::SetPenColor:
      paintState().penColor = prim.color;
      break;
    case PrimitiveCmd::SetBrushColor:
      paintState().brushColor = prim.color;
      break;
    case PrimitiveCmd::SetPixel:
      setPixelAt( (PixelDesc) { prim.position, getActualPenColor() }, updateRect );
      break;
    case PrimitiveCmd::SetPixelAt:
      setPixelAt(prim.pixelDesc, updateRect);
      break;
    case PrimitiveCmd::MoveTo:
      paintState().position = Point(prim.position.X + paintState().origin.X, prim.position.Y + paintState().origin.Y);
      break;
    case PrimitiveCmd::LineTo:
      lineTo(prim.position, updateRect);
      break;
    case PrimitiveCmd::FillRect:
      fillRect(prim.rect, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::DrawRect:
      drawRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::FillEllipse:
      fillEllipse(paintState().position.X, paintState().position.Y, prim.size, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::DrawEllipse:
      drawEllipse(prim.size, updateRect);
      break;
    case PrimitiveCmd::Clear:
      updateRect = updateRect.merge(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
      clear(updateRect);
      break;
    case PrimitiveCmd::VScroll:
      updateRect = updateRect.merge(Rect(paintState().scrollingRegion.X1, paintState().scrollingRegion.Y1, paintState().scrollingRegion.X2, paintState().scrollingRegion.Y2));
      VScroll(prim.ivalue, updateRect);
      break;
    case PrimitiveCmd::HScroll:
      updateRect = updateRect.merge(Rect(paintState().scrollingRegion.X1, paintState().scrollingRegion.Y1, paintState().scrollingRegion.X2, paintState().scrollingRegion.Y2));
      HScroll(prim.ivalue, updateRect);
      break;
    case PrimitiveCmd::DrawGlyph:
      drawGlyph(prim.glyph, paintState().glyphOptions, paintState().penColor, paintState().brushColor, updateRect);
      break;
    case PrimitiveCmd::SetGlyphOptions:
      paintState().glyphOptions = prim.glyphOptions;
      break;
    case PrimitiveCmd::SetPaintOptions:
      paintState().paintOptions = prim.paintOptions;
      break;
    case PrimitiveCmd::InvertRect:
      invertRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::CopyRect:
      copyRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::SetScrollingRegion:
      paintState().scrollingRegion = prim.rect;
      break;
    case PrimitiveCmd::SwapFGBG:
      swapFGBG(prim.rect, updateRect);
      break;
    case PrimitiveCmd::RenderGlyphsBuffer:
      renderGlyphsBuffer(prim.glyphsBufferRenderInfo, updateRect);
      break;
    case PrimitiveCmd::DrawBitmap:
      drawBitmap(prim.bitmapDrawingInfo, updateRect);
      break;
    case PrimitiveCmd::RefreshSprites:
      hideSprites(updateRect);
      showSprites(updateRect);
      break;
    case PrimitiveCmd::SwapBuffers:
      swapBuffers();
      updateRect = updateRect.merge(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
      if (insideISR)
        vTaskNotifyGiveFromISR(prim.notifyTask, nullptr);
      else
        xTaskNotifyGive(prim.notifyTask);
      break;
    case PrimitiveCmd::DrawPath:
      drawPath(prim.path, updateRect);
      break;
    case PrimitiveCmd::FillPath:
      fillPath(prim.path, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::SetOrigin:
      paintState().origin = prim.position;
      updateAbsoluteClippingRect();
      break;
    case PrimitiveCmd::SetClippingRect:
      paintState().clippingRect = prim.rect;
      updateAbsoluteClippingRect();
      break;
    case PrimitiveCmd::SetPenWidth:
      paintState().penWidth = imax(1, prim.ivalue);
      break;
    case PrimitiveCmd::SetLineEnds:
      paintState().lineEnds = prim.lineEnds;
      break;
  }
}


RGB888 IRAM_ATTR BitmappedDisplayController::getActualBrushColor()
{
  return paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;
}


RGB888 IRAM_ATTR BitmappedDisplayController::getActualPenColor()
{
  return paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor;
}


void IRAM_ATTR BitmappedDisplayController::lineTo(Point const & position, Rect & updateRect)
{
  RGB888 color = getActualPenColor();

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;
  int x1 = paintState().position.X;
  int y1 = paintState().position.Y;
  int x2 = position.X + origX;
  int y2 = position.Y + origY;

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(imin(x1, x2) - hw, imin(y1, y2) - hw, imax(x1, x2) + hw, imax(y1, y2) + hw));
  hideSprites(updateRect);
  absDrawLine(x1, y1, x2, y2, color);

  paintState().position = Point(x2, y2);
}


void IRAM_ATTR BitmappedDisplayController::updateAbsoluteClippingRect()
{
  int X1 = iclamp(paintState().origin.X + paintState().clippingRect.X1, 0, getViewPortWidth() - 1);
  int Y1 = iclamp(paintState().origin.Y + paintState().clippingRect.Y1, 0, getViewPortHeight() - 1);
  int X2 = iclamp(paintState().origin.X + paintState().clippingRect.X2, 0, getViewPortWidth() - 1);
  int Y2 = iclamp(paintState().origin.Y + paintState().clippingRect.Y2, 0, getViewPortHeight() - 1);
  paintState().absClippingRect = Rect(X1, Y1, X2, Y2);
}


void IRAM_ATTR BitmappedDisplayController::drawRect(Rect const & rect, Rect & updateRect)
{
  int x1 = (rect.X1 < rect.X2 ? rect.X1 : rect.X2) + paintState().origin.X;
  int y1 = (rect.Y1 < rect.Y2 ? rect.Y1 : rect.Y2) + paintState().origin.Y;
  int x2 = (rect.X1 < rect.X2 ? rect.X2 : rect.X1) + paintState().origin.X;
  int y2 = (rect.Y1 < rect.Y2 ? rect.Y2 : rect.Y1) + paintState().origin.Y;

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(x1 - hw, y1 - hw, x2 + hw, y2 + hw));
  hideSprites(updateRect);
  RGB888 color = getActualPenColor();

  absDrawLine(x1 + 1, y1,     x2, y1, color);
  absDrawLine(x2,     y1 + 1, x2, y2, color);
  absDrawLine(x2 - 1, y2,     x1, y2, color);
  absDrawLine(x1,     y2 - 1, x1, y1, color);
}


void IRAM_ATTR BitmappedDisplayController::fillRect(Rect const & rect, RGB888 const & color, Rect & updateRect)
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

  updateRect = updateRect.merge(Rect(x1, y1, x2, y2));
  hideSprites(updateRect);

  for (int y = y1; y <= y2; ++y)
    rawFillRow(y, x1, x2, color);
}


// McIlroy's algorithm
void IRAM_ATTR BitmappedDisplayController::fillEllipse(int centerX, int centerY, Size const & size, RGB888 const & color, Rect & updateRect)
{
  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

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
  int width = 1;
  int t = -a2 * y;
  int dxt = 2 * b2 * x;
  int dyt = -2 * a2 * y;

  while (y >= 0 && x <= halfWidth) {
    if (t + b2 * x <= crit1 || t + a2 * y <= crit3) {
      x++;
      dxt += d2xt;
      t += dxt;
      width += 2;
    } else {
      int col1 = centerX - x;
      int col2 = centerX - x + width - 1;
      if (col1 <= clipX2 && col2 >= clipX1) {
        col1 = iclamp(col1, clipX1, clipX2);
        col2 = iclamp(col2, clipX1, clipX2);
        int row1 = centerY - y;
        int row2 = centerY + y;
        if (row1 >= clipY1 && row1 <= clipY2)
          rawFillRow(row1, col1, col2, color);
        if (y != 0 && row2 >= clipY1 && row2 <= clipY2)
          rawFillRow(row2, col1, col2, color);
      }
      if (t - a2 * y <= crit2) {
        x++;
        dxt += d2xt;
        t += dxt;
        width += 2;
      }
      y--;
      dyt += d2yt;
      t += dyt;
    }
  }
  // one line horizontal ellipse case
  if (halfHeight == 0 && centerY >= clipY1 && centerY <= clipY2)
    rawFillRow(centerY, iclamp(centerX - halfWidth, clipX1, clipX2), iclamp(centerX - halfWidth + 2 * halfWidth + 1, clipX1, clipX2), color);
}


void IRAM_ATTR BitmappedDisplayController::renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo, Rect & updateRect)
{
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

  drawGlyph(glyph, glyphOptions, fgColor, bgColor, updateRect);
}


void IRAM_ATTR BitmappedDisplayController::drawPath(Path const & path, Rect & updateRect)
{
  RGB888 color = getActualPenColor();

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;

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

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(minX - hw, minY - hw, maxX + hw, maxY + hw));
  hideSprites(updateRect);

  int i = 0;
  for (; i < path.pointsCount - 1; ++i) {
    const int x1 = path.points[i].X + origX;
    const int y1 = path.points[i].Y + origY;
    const int x2 = path.points[i + 1].X + origX;
    const int y2 = path.points[i + 1].Y + origY;
    absDrawLine(x1, y1, x2, y2, color);
  }
  const int x1 = path.points[i].X + origX;
  const int y1 = path.points[i].Y + origY;
  const int x2 = path.points[0].X + origX;
  const int y2 = path.points[0].Y + origY;
  absDrawLine(x1, y1, x2, y2, color);

  if (path.freePoints)
    m_primDynMemPool.free((void*)path.points);
}


void IRAM_ATTR BitmappedDisplayController::fillPath(Path const & path, RGB888 const & color, Rect & updateRect)
{
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

  updateRect = updateRect.merge(Rect(minX, minY, maxX, maxY));
  hideSprites(updateRect);

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
        rawFillRow(pixelY, nodeX[i], nodeX[i + 1] - 1, color);
      }
    }
  }

  if (path.freePoints)
    m_primDynMemPool.free((void*)path.points);
}


void IRAM_ATTR BitmappedDisplayController::absDrawThickLine(int X1, int Y1, int X2, int Y2, int penWidth, RGB888 const & color)
{
  // just to "de-absolutize"
  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;
  X1 -= origX;
  Y1 -= origY;
  X2 -= origX;
  Y2 -= origY;

  Point pts[4];

  const double angle = atan2(Y2 - Y1, X2 - X1);
  const double pw = (double)penWidth / 2.0;
  const int ofs1 = lround(pw * cos(angle + M_PI_2));
  const int ofs2 = lround(pw * sin(angle + M_PI_2));
  const int ofs3 = lround(pw * cos(angle - M_PI_2));
  const int ofs4 = lround(pw * sin(angle - M_PI_2));
  pts[0].X = X1 + ofs1;
  pts[0].Y = Y1 + ofs2;
  pts[1].X = X1 + ofs3;
  pts[1].Y = Y1 + ofs4;
  pts[2].X = X2 + ofs3;
  pts[2].Y = Y2 + ofs4;
  pts[3].X = X2 + ofs1;
  pts[3].Y = Y2 + ofs2;

  Rect updateRect;
  Path path = { pts, 4, false };
  fillPath(path, color, updateRect);

  switch (paintState().lineEnds) {
    case LineEnds::Circle:
      if ((penWidth & 1) == 0)
        --penWidth;
      fillEllipse(X1, Y1, Size(penWidth, penWidth), color, updateRect);
      fillEllipse(X2, Y2, Size(penWidth, penWidth), color, updateRect);
      break;
    default:
      break;
  }
}


void IRAM_ATTR BitmappedDisplayController::drawBitmap(BitmapDrawingInfo const & bitmapDrawingInfo, Rect & updateRect)
{
  int x = bitmapDrawingInfo.X + paintState().origin.X;
  int y = bitmapDrawingInfo.Y + paintState().origin.Y;
  updateRect = updateRect.merge(Rect(x, y, x + bitmapDrawingInfo.bitmap->width - 1, y + bitmapDrawingInfo.bitmap->height - 1));
  hideSprites(updateRect);
  absDrawBitmap(x, y, bitmapDrawingInfo.bitmap, nullptr, false);
}


void IRAM_ATTR BitmappedDisplayController::absDrawBitmap(int destX, int destY, Bitmap const * bitmap, void * saveBackground, bool ignoreClippingRect)
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

    case PixelFormat::Native:
      rawDrawBitmap_Native(destX, destY, bitmap, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::Mask:
      rawDrawBitmap_Mask(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA2222:
      rawDrawBitmap_RGBA2222(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA8888:
      rawDrawBitmap_RGBA8888(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

  }

}



} // end of namespace
