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



namespace fabgl {




// Array to convert Color enum to RGB222 struct
// First eight maximum value is '1' to make them visible also when 8 colors are used.
// From Red to Cyan are changed (1=>2) when 64 color mode is used.
RGB222 COLOR2RGB222[16] = {
  {0, 0, 0}, // Black
  {1, 0, 0}, // Red
  {0, 1, 0}, // Green
  {1, 1, 0}, // Yellow
  {0, 0, 1}, // Blue
  {1, 0, 1}, // Magenta
  {0, 1, 1}, // Cyan
  {1, 1, 1}, // White
  {1, 1, 1}, // BrightBlack
  {3, 0, 0}, // BrightRed
  {0, 3, 0}, // BrightGreen
  {3, 3, 0}, // BrightYellow
  {0, 0, 3}, // BrightBlue
  {3, 0, 3}, // BrightMagenta
  {0, 3, 3}, // BrightCyan
  {3, 3, 3}, // BrightWhite
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


Sprite * Sprite::addBitmap(Bitmap const * bitmap)
{
  ++framesCount;
  frames = (Bitmap const **) realloc(frames, sizeof(Bitmap*) * framesCount);
  frames[framesCount - 1] = bitmap;
  return this;
}


Sprite * Sprite::addBitmap(Bitmap const * bitmap[], int count)
{
  frames = (Bitmap const **) realloc(frames, sizeof(Bitmap*) * (framesCount + count));
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



Bitmap::Bitmap(int width_, int height_, void const * data_, bool copy)
  : width(width_), height(height_), data((uint8_t const*)data_), dataAllocated(false)
{
  if (copy) {
    dataAllocated = true;
    data = (uint8_t const*) malloc(width * height);
    memcpy((void*)data, data_, width * height);
  }
}

// bitsPerPixel:
//    1 : 1 bit per pixel, 0 = transparent, 1 = foregroundColor
//    8 : 8 bits per pixel: AABBGGRR
Bitmap::Bitmap(int width_, int height_, void const * data_, int bitsPerPixel, RGB222 foregroundColor, bool copy)
  : width(width_), height(height_)
{
  width  = width_;
  height = height_;

  switch (bitsPerPixel) {

    case 1:
    {
      // convert to 8 bit
      uint8_t * dstdata = (uint8_t*) malloc(width * height);
      data = dstdata;
      int rowlen = (width + 7) / 8;
      for (int y = 0; y < height; ++y) {
        uint8_t const * srcrow = (uint8_t const*)data_ + y * rowlen;
        uint8_t * dstrow = dstdata + y * width;
        for (int x = 0; x < width; ++x) {
          if ((srcrow[x >> 3] << (x & 7)) & 0x80)
            dstrow[x] = foregroundColor.R | (foregroundColor.G << 2) | (foregroundColor.B << 4) | (3 << 6);
          else
            dstrow[x] = 0;
        }
      }
      dataAllocated = true;
      break;
    }

    case 8:
      if (copy) {
        data = (uint8_t const*) malloc(width * height);
        memcpy((void*)data, data_, width * height);
        dataAllocated = true;
      } else {
        data = (uint8_t const*) data_;
        dataAllocated = false;
      }
      break;

  }
}


Bitmap::~Bitmap()
{
  if (dataAllocated)
    free((void*) data);
}










} // end of namespace
