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


// calc and alloc required save-background space
// Display controller requires to call using Single Buffering display (Double Buffering doesn't require this)
void Sprite::allocRequiredBackgroundBuffer()
{
  int reqBackBufferSize = 0;
  for (int i = 0; i < framesCount; ++i)
    reqBackBufferSize = tmax(reqBackBufferSize, frames[i]->width * frames[i]->height);
  savedBackground = (uint8_t*) realloc(savedBackground, reqBackBufferSize);
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
  m_sprites = nullptr;
  m_spritesCount = 0;
  m_doubleBuffered = false;
  m_mouseCursor.visible = false;
  m_backgroundPrimitiveTimeoutEnabled = true;
}


DisplayController::~DisplayController()
{
  vQueueDelete(m_execQueue);
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


// call this only inside an ISR
void IRAM_ATTR DisplayController::insertPrimitiveISR(Primitive * primitive)
{
  xQueueSendToFrontFromISR(m_execQueue, primitive, nullptr);
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
  if (!isDoubleBuffered()) {
    uint8_t * spritePtr = (uint8_t*)m_sprites;
    for (int i = 0; i < m_spritesCount; ++i, spritePtr += m_spriteSize) {
      Sprite * sprite = (Sprite*) spritePtr;
      sprite->allocRequiredBackgroundBuffer();
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
        m_mouseCursor.allocRequiredBackgroundBuffer();
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




} // end of namespace
