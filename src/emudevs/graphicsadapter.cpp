/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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


#include "graphicsadapter.h"




#pragma GCC optimize ("O2")


namespace fabgl {



static const RGB222 CGAPalette[16] = {
  RGB222(0, 0, 0),    // black
  RGB222(0, 0, 2),    // blue
  RGB222(0, 2, 0),    // green
  RGB222(0, 2, 2),    // cyan
  RGB222(2, 0, 0),    // red
  RGB222(2, 0, 2),    // magenta
  RGB222(2, 1, 0),    // brown
  RGB222(2, 2, 2),    // light gray
  RGB222(1, 1, 1),    // dark gray
  RGB222(1, 1, 3),    // light blue
  RGB222(1, 3, 1),    // light green
  RGB222(1, 3, 3),    // light cyan
  RGB222(3, 1, 1),    // light red
  RGB222(3, 1, 3),    // light magenta
  RGB222(3, 3, 1),    // yellow
  RGB222(3, 3, 3),    // white
};



static const RGB222 CGAGraphics4ColorsPalette[4][4] = {
  // low intensity PC graphics 4 colors palette
  {
    RGB222(0, 0, 0),    // background (not used)
    RGB222(0, 2, 0),    // green
    RGB222(2, 0, 0),    // red
    RGB222(2, 1, 0),    // brown
  },
  // high intensity PC graphics 4 colors palette
  {
    RGB222(0, 0, 0),    // background (not used)
    RGB222(1, 3, 1),    // light green
    RGB222(3, 1, 1),    // light red
    RGB222(3, 3, 1),    // yellow
  },
  // low intensity PC graphics 4 colors alternate palette
  {
    RGB222(0, 0, 0),    // background (not used)
    RGB222(0, 2, 2),    // cyan
    RGB222(2, 0, 2),    // magenta
    RGB222(2, 2, 2),    // light gray
  },
  // low intensity PC graphics 4 colors alternate palette
  {
    RGB222(0, 0, 0),    // background (not used)
    RGB222(1, 3, 3),    // light cyan
    RGB222(3, 1, 3),    // light magenta
    RGB222(3, 3, 3),    // white
  },
};




GraphicsAdapter::GraphicsAdapter()
  : m_VGADCtrl(false),
    m_emulation(Emulation::None),
    m_videoBuffer(nullptr),
    m_rawLUT(nullptr),
    m_cursorRow(0),
    m_cursorCol(0),
    m_cursorStart(0),
    m_cursorEnd(0),
    m_cursorVisible(false),
    m_cursorGlyph(nullptr),
    m_bit7blink(true),
    m_PCGraphicsBackgroundColorIndex(0),
    m_PCGraphicsForegroundColorIndex(15),
    m_PCGraphicsPaletteInUse(0)
{
  m_font.data = nullptr;
  m_VGADCtrl.begin();
}


GraphicsAdapter::~GraphicsAdapter()
{
  cleanupFont();
  freeLUT();
  if (m_cursorGlyph)
    heap_caps_free(m_cursorGlyph);
}


void GraphicsAdapter::setEmulation(Emulation emulation)
{
  if (m_emulation != emulation) {
    m_emulation = emulation;

    m_VGADCtrl.end();
    freeLUT();

    switch (m_emulation) {

      case Emulation::None:
        //printf("Emulation::None\n");
        break;

      case Emulation::PC_Text_40x25_16Colors:
        //printf("Emulation::PC_Text_40x25_16Colors\n");
        setFont(&FONT_8x8);
        setCursorShape(5, 7);
        m_VGADCtrl.setDrawScanlineCallback(drawScanline_PC_Text_40x25_16Colors, this);
        m_VGADCtrl.setScanlinesPerCallBack(4);
        m_VGADCtrl.setResolution(VGA_320x200_70Hz);
        m_columns = m_VGADCtrl.getViewPortWidth() / m_font.width;
        m_rows    = m_VGADCtrl.getViewPortHeight() / m_font.height;
        break;

      case Emulation::PC_Text_80x25_16Colors:
        //printf("Emulation::PC_Text_80x25_16Colors\n");
        setFont(&FONT_8x16);
        setCursorShape(13, 15);
        m_VGADCtrl.setDrawScanlineCallback(drawScanline_PC_Text_80x25_16Colors, this);
        m_VGADCtrl.setScanlinesPerCallBack(8);
        m_VGADCtrl.setResolution(VGA_640x400_70Hz);
        m_columns = m_VGADCtrl.getViewPortWidth() / m_font.width;
        m_rows    = m_VGADCtrl.getViewPortHeight() / m_font.height;
        break;

      case Emulation::PC_Graphics_320x200_4Colors:
        //printf("Emulation::PC_Graphics_320x200_4Colors\n");
        m_VGADCtrl.setDrawScanlineCallback(drawScanline_PC_Graphics_320x200_4Colors, this);
        m_VGADCtrl.setScanlinesPerCallBack(1);
        m_VGADCtrl.setResolution(VGA_320x200_70Hz);
        break;

      case Emulation::PC_Graphics_640x200_2Colors:
        //printf("Emulation::PC_Graphics_640x200_2Colors\n");
        m_VGADCtrl.setDrawScanlineCallback(drawScanline_PC_Graphics_640x200_2Colors, this);
        m_VGADCtrl.setScanlinesPerCallBack(1);
        m_VGADCtrl.setResolution(VGA_640x200_70Hz);
        break;

      case Emulation::PC_Graphics_HGC_720x348:
        //printf("Emulation::PC_Graphics_HGC_720x348\n");
        m_VGADCtrl.setDrawScanlineCallback(drawScanline_PC_Graphics_HGC_720x348, this);
        m_VGADCtrl.setScanlinesPerCallBack(2);
        m_VGADCtrl.setResolution(VGA_720x348_73Hz);
        break;

    }

    if (m_emulation != Emulation::None) {
      setupLUT();
      m_VGADCtrl.run();
    }
  }
}


void GraphicsAdapter::freeLUT()
{
  if (m_rawLUT)
    heap_caps_free(m_rawLUT);
  m_rawLUT = nullptr;
}


void GraphicsAdapter::setupLUT()
{
  switch (m_emulation) {

    case Emulation::None:
      break;

    case Emulation::PC_Text_80x25_16Colors:
    case Emulation::PC_Text_40x25_16Colors:
      // each LUT item contains half pixel (an index to 16 colors palette)
      if (!m_rawLUT)
        m_rawLUT = (uint8_t*) heap_caps_malloc(16, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      for (int i = 0; i < 16; ++i)
        m_rawLUT[i]  = m_VGADCtrl.createRawPixel(CGAPalette[i]);
      break;

    case Emulation::PC_Graphics_320x200_4Colors:
      // each LUT item contains four pixels (decodes as four raw bytes)
      if (!m_rawLUT)
        m_rawLUT = (uint8_t*) heap_caps_malloc(256 * 4, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 4; ++j) {
          int pixel = (i >> (6 - j * 2)) & 0b11;
          uint8_t rawPixel = m_VGADCtrl.createRawPixel(pixel == 0 ? CGAPalette[m_PCGraphicsBackgroundColorIndex] : CGAGraphics4ColorsPalette[m_PCGraphicsPaletteInUse][pixel]);
          m_rawLUT[(i * 4) + (j ^ 2)] = rawPixel;
        }
      }
      break;

    case Emulation::PC_Graphics_640x200_2Colors:
      // each LUT item contains eight pixels (decodes as eight raw bytes)
      if (!m_rawLUT)
        m_rawLUT = (uint8_t*) heap_caps_malloc(256 * 8, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 8; ++j) {
          bool pixel = (i >> (7 - j)) & 1;
          uint8_t rawPixel = m_VGADCtrl.createRawPixel(pixel ? CGAPalette[m_PCGraphicsForegroundColorIndex] : RGB222(0, 0, 0));
          m_rawLUT[(i * 8) + (j ^ 2)] = rawPixel;
        }
      }
      break;

    case Emulation::PC_Graphics_HGC_720x348:
      // each LUT item contains eight pixels (decodes as eight raw bytes)
      if (!m_rawLUT)
        m_rawLUT = (uint8_t*) heap_caps_malloc(256 * 8, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 8; ++j) {
          bool pixel = (i >> (7 - j)) & 1;
          uint8_t rawPixel = m_VGADCtrl.createRawPixel(RGB222(pixel * 3, pixel * 3, pixel * 3));
          m_rawLUT[(i * 8) + (j ^ 2)] = rawPixel;
        }
      }
      break;

  }
}


void GraphicsAdapter::setPCGraphicsBackgroundColorIndex(int colorIndex)
{
  m_PCGraphicsBackgroundColorIndex = colorIndex;
  setupLUT();
}


void GraphicsAdapter::setPCGraphicsForegroundColorIndex(int colorIndex)
{
  m_PCGraphicsForegroundColorIndex = colorIndex;
  setupLUT();
}


void GraphicsAdapter::setPCGraphicsPaletteInUse(int paletteIndex)
{
  m_PCGraphicsPaletteInUse = paletteIndex;
  setupLUT();
}


void GraphicsAdapter::setVideoBuffer(void const * videoBuffer)
{
  m_videoBuffer = (uint8_t*) videoBuffer;
}


void GraphicsAdapter::cleanupFont()
{
  if (m_font.data) {
    heap_caps_free((void*)m_font.data);
    m_font.data = nullptr;
  }
}


void GraphicsAdapter::setFont(FontInfo const * font)
{
  cleanupFont();
  if (font) {
    m_font = *font;
    // copy font data into internal RAM
    auto size = 256 * ((m_font.width + 7) / 8) * m_font.height;
    m_font.data = (uint8_t const *) heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    memcpy((void*)m_font.data, font->data, size);
  }
}


void GraphicsAdapter::setCursorShape(int start, int end)
{
  if (start != m_cursorStart || end != m_cursorEnd) {
    m_cursorStart = start;
    m_cursorEnd   = end;

    // readapt start->end to the actual font height to make sure the cursor is always visible
    if (start <= end && end >= m_font.height) {
      int h = end - start;
      end   = m_font.height - 1;
      start = end - h;
    }

    int charWidthInBytes = (m_font.width + 7) / 8;
    if (!m_cursorGlyph)
      m_cursorGlyph = (uint8_t*) heap_caps_malloc(charWidthInBytes * m_font.height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    memset(m_cursorGlyph, 0, charWidthInBytes * m_font.height);
    if (end >= start && start >= 0 && start < m_font.height && end < m_font.height)
      memset(m_cursorGlyph + (start * charWidthInBytes), 0xff, (end - start + 1) * charWidthInBytes);
  }
}


void GraphicsAdapter::setCursorPos(int row, int column)
{
  m_cursorRow = row;
  m_cursorCol = column;
}


void IRAM_ATTR GraphicsAdapter::drawScanline_PC_Text_40x25_16Colors(void * arg, uint8_t * dest, int scanLine)
{
  auto ga = (GraphicsAdapter*) arg;

  constexpr int CHARWIDTH        = 8;
  constexpr int CHARHEIGHT       = 8;
  constexpr int CHARWIDTHINBYTES = (CHARWIDTH + 7) / 8;
  constexpr int CHARSIZEINBYTES  = CHARWIDTHINBYTES * CHARHEIGHT;
  constexpr int COLUMNS          = 40;
  constexpr int SCREENWIDTH      = 320;
  constexpr int LINES            = 8;

  if (scanLine == 0)
    ++ga->m_frameCounter;

  int charScanline = scanLine & (CHARHEIGHT - 1);
  int textRow      = scanLine / CHARHEIGHT;

  uint8_t const * fontData = ga->m_font.data + (charScanline * CHARWIDTHINBYTES);

  uint8_t * rawLUT = ga->m_rawLUT;

  uint8_t const * curItem = ga->m_videoBuffer + (textRow * COLUMNS * 2);

  bool showCursor = ga->m_cursorVisible && ga->m_cursorRow == textRow && ((ga->m_frameCounter & 0x1f) < 0xf);
  int cursorCol = ga->m_cursorCol;

  bool bit7blink = ga->m_bit7blink;
  bool blinktime = bit7blink && !((ga->m_frameCounter & 0x3f) < 0x1f);

  for (int textCol = 0; textCol < COLUMNS; ++textCol) {

    int charIdx  = *curItem++;
    int charAttr = *curItem++;

    bool blink = false;
    if (bit7blink) {
      blink = blinktime && (charAttr & 0x80);
      charAttr &= 0x7f;
    }

    uint8_t bg = rawLUT[charAttr >> 4];
    uint8_t fg = blink ? bg : rawLUT[charAttr & 0xf];

    const uint8_t colors[2] = { bg, fg };

    uint8_t const * charBitmapPtr = fontData + charIdx * CHARSIZEINBYTES;

    auto destptr = dest;

    if (showCursor && textCol == cursorCol) {

      uint8_t const * cursorBitmapPtr = ga->m_cursorGlyph + (charScanline * CHARWIDTHINBYTES);

      for (int charRow = 0; charRow < LINES / 2; ++charRow) {

        uint32_t charBitmap = *charBitmapPtr | *cursorBitmapPtr;

        *(destptr + 0) = colors[(bool)(charBitmap & 0x20)];
        *(destptr + 1) = colors[(bool)(charBitmap & 0x10)];
        *(destptr + 2) = colors[(bool)(charBitmap & 0x80)];
        *(destptr + 3) = colors[(bool)(charBitmap & 0x40)];
        *(destptr + 4) = colors[(bool)(charBitmap & 0x02)];
        *(destptr + 5) = colors[(bool)(charBitmap & 0x01)];
        *(destptr + 6) = colors[(bool)(charBitmap & 0x08)];
        *(destptr + 7) = colors[(bool)(charBitmap & 0x04)];

        destptr += SCREENWIDTH;
        charBitmapPtr += CHARWIDTHINBYTES;
        cursorBitmapPtr += CHARWIDTHINBYTES;
      }

    } else {

      for (int charRow = 0; charRow < LINES / 2; ++charRow) {

        uint32_t charBitmap = *charBitmapPtr;

        *(destptr + 0) = colors[(bool)(charBitmap & 0x20)];
        *(destptr + 1) = colors[(bool)(charBitmap & 0x10)];
        *(destptr + 2) = colors[(bool)(charBitmap & 0x80)];
        *(destptr + 3) = colors[(bool)(charBitmap & 0x40)];
        *(destptr + 4) = colors[(bool)(charBitmap & 0x02)];
        *(destptr + 5) = colors[(bool)(charBitmap & 0x01)];
        *(destptr + 6) = colors[(bool)(charBitmap & 0x08)];
        *(destptr + 7) = colors[(bool)(charBitmap & 0x04)];

        destptr += SCREENWIDTH;
        charBitmapPtr += CHARWIDTHINBYTES;
      }

    }

    dest += 8;

  }
}


void IRAM_ATTR GraphicsAdapter::drawScanline_PC_Text_80x25_16Colors(void * arg, uint8_t * dest, int scanLine)
{
  auto ga = (GraphicsAdapter*) arg;

  constexpr int CHARWIDTH        = 8;
  constexpr int CHARHEIGHT       = 16;
  constexpr int CHARWIDTHINBYTES = (CHARWIDTH + 7) / 8;
  constexpr int CHARSIZEINBYTES  = CHARWIDTHINBYTES * CHARHEIGHT;
  constexpr int COLUMNS          = 80;
  constexpr int SCREENWIDTH      = 640;
  constexpr int LINES            = 16;

  if (scanLine == 0)
    ++ga->m_frameCounter;

  int charScanline = scanLine & (CHARHEIGHT - 1);
  int textRow      = scanLine / CHARHEIGHT;

  uint8_t const * fontData = ga->m_font.data + (charScanline * CHARWIDTHINBYTES);

  uint8_t * rawLUT = ga->m_rawLUT;

  uint8_t const * curItem = ga->m_videoBuffer + (textRow * COLUMNS * 2);

  bool showCursor = ga->m_cursorVisible && ga->m_cursorRow == textRow && ((ga->m_frameCounter & 0x1f) < 0xf);
  int cursorCol = ga->m_cursorCol;

  bool bit7blink = ga->m_bit7blink;
  bool blinktime = bit7blink && !((ga->m_frameCounter & 0x3f) < 0x1f);

  for (int textCol = 0; textCol < COLUMNS; ++textCol) {

    int charIdx  = *curItem++;
    int charAttr = *curItem++;

    bool blink = false;
    if (bit7blink) {
      blink = blinktime && (charAttr & 0x80);
      charAttr &= 0x7f;
    }

    uint8_t bg = rawLUT[charAttr >> 4];
    uint8_t fg = blink ? bg : rawLUT[charAttr & 0xf];

    const uint8_t colors[2] = { bg, fg };

    uint8_t const * charBitmapPtr = fontData + charIdx * CHARSIZEINBYTES;

    auto destptr = dest;

    if (showCursor && textCol == cursorCol) {

      uint8_t const * cursorBitmapPtr = ga->m_cursorGlyph + (charScanline * CHARWIDTHINBYTES);

      for (int charRow = 0; charRow < LINES / 2; ++charRow) {

        uint32_t charBitmap = *charBitmapPtr | *cursorBitmapPtr;

        *(destptr + 0) = colors[(bool)(charBitmap & 0x20)];
        *(destptr + 1) = colors[(bool)(charBitmap & 0x10)];
        *(destptr + 2) = colors[(bool)(charBitmap & 0x80)];
        *(destptr + 3) = colors[(bool)(charBitmap & 0x40)];
        *(destptr + 4) = colors[(bool)(charBitmap & 0x02)];
        *(destptr + 5) = colors[(bool)(charBitmap & 0x01)];
        *(destptr + 6) = colors[(bool)(charBitmap & 0x08)];
        *(destptr + 7) = colors[(bool)(charBitmap & 0x04)];

        destptr += SCREENWIDTH;
        charBitmapPtr += CHARWIDTHINBYTES;
        cursorBitmapPtr += CHARWIDTHINBYTES;
      }

    } else {

      for (int charRow = 0; charRow < LINES / 2; ++charRow) {

        uint32_t charBitmap = *charBitmapPtr;

        *(destptr + 0) = colors[(bool)(charBitmap & 0x20)];
        *(destptr + 1) = colors[(bool)(charBitmap & 0x10)];
        *(destptr + 2) = colors[(bool)(charBitmap & 0x80)];
        *(destptr + 3) = colors[(bool)(charBitmap & 0x40)];
        *(destptr + 4) = colors[(bool)(charBitmap & 0x02)];
        *(destptr + 5) = colors[(bool)(charBitmap & 0x01)];
        *(destptr + 6) = colors[(bool)(charBitmap & 0x08)];
        *(destptr + 7) = colors[(bool)(charBitmap & 0x04)];

        destptr += SCREENWIDTH;
        charBitmapPtr += CHARWIDTHINBYTES;
      }

    }

    dest += 8;

  }
}


// offset 0x0000 for even scan lines (bit 0 = 0), lines 0, 2, 4...
// offset 0x2000 for odd scan lines  (bit 0 = 1), lines 1, 3, 5...
void IRAM_ATTR GraphicsAdapter::drawScanline_PC_Graphics_320x200_4Colors(void * arg, uint8_t * dest, int scanLine)
{
  constexpr int WIDTH         = 320;
  constexpr int PIXELSPERBYTE = 4;
  constexpr int WIDTHINBYTES  = WIDTH / PIXELSPERBYTE;

  auto ga = (GraphicsAdapter*) arg;

  auto src = ga->m_videoBuffer + ((scanLine & 1) << 13) + WIDTHINBYTES * (scanLine >> 1);

  auto dest32 = (uint32_t*) dest;

  auto LUT32 = (uint32_t*) ga->m_rawLUT;

  for (int col = 0; col < WIDTH; col += PIXELSPERBYTE) {
    *dest32++ = LUT32[*src++];
  }
}


// offset 0x0000 for even scan lines (bit 0 = 0), lines 0, 2, 4...
// offset 0x2000 for odd scan lines  (bit 0 = 1), lines 1, 3, 5...
void IRAM_ATTR GraphicsAdapter::drawScanline_PC_Graphics_640x200_2Colors(void * arg, uint8_t * dest, int scanLine)
{
  constexpr int WIDTH         = 640;
  constexpr int PIXELSPERBYTE = 8;
  constexpr int WIDTHINBYTES  = WIDTH / PIXELSPERBYTE;

  auto ga = (GraphicsAdapter*) arg;

  auto src = ga->m_videoBuffer + ((scanLine & 1) << 13) + WIDTHINBYTES * (scanLine >> 1);

  auto dest64 = (uint64_t*) dest;

  auto LUT64 = (uint64_t*) ga->m_rawLUT;

  for (int col = 0; col < WIDTH; col += PIXELSPERBYTE) {
    *dest64++ = LUT64[*src++];
  }
}


// offset 0x0000 for lines 0, 4, 8, 12...
// offset 0x2000 for lines 1, 5, 9, 13...
// offset 0x4000 for lines 2, 6, 10, 14...
// offset 0x6000 for lines 3, 7, 11, 15...
void IRAM_ATTR GraphicsAdapter::drawScanline_PC_Graphics_HGC_720x348(void * arg, uint8_t * dest, int scanLine)
{
  constexpr int WIDTH         = 720;
  constexpr int PIXELSPERBYTE = 8;
  constexpr int WIDTHINBYTES  = WIDTH / PIXELSPERBYTE;

  auto ga = (GraphicsAdapter*) arg;

  auto src = ga->m_videoBuffer + ((scanLine & 0b11) << 13) + WIDTHINBYTES * (scanLine >> 2);

  auto dest64 = (uint64_t*) dest;

  auto LUT64 = (uint64_t*) ga->m_rawLUT;

  for (int col = 0; col < WIDTH; col += PIXELSPERBYTE)
    *dest64++ = LUT64[*src++];

  ++scanLine;
  src = ga->m_videoBuffer + ((scanLine & 0b11) << 13) + WIDTHINBYTES * (scanLine >> 2);

  for (int col = 0; col < WIDTH; col += PIXELSPERBYTE)
    *dest64++ = LUT64[*src++];
}





};  // fabgl namespace
