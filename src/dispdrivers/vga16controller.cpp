/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
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



#include <alloca.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"
#include "esp_spi_flash.h"
#include "esp_heap_caps.h"

#include "fabutils.h"
#include "vga16controller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")




namespace fabgl {



// high nibble is pixel 0, low nibble is pixel 1

static inline __attribute__((always_inline)) void VGA16_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

static inline __attribute__((always_inline)) int VGA16_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 1;
  return ((x & 1) ? (row[brow] & 0x0f) : ((row[brow] & 0xf0) >> 4));
}

#define VGA16_INVERTPIXELINROW(row, x)       (row)[(x) >> 1] ^= (0xf0 >> (((x) & 1) << 2))

static inline __attribute__((always_inline)) void VGA16_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) VGA16Controller::sgetScanline(y);
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

#define VGA16_GETPIXEL(x, y)                 VGA16_GETPIXELINROW((uint8_t*)VGA16Controller::s_viewPort[(y)], (x))

#define VGA16_INVERT_PIXEL(x, y)             VGA16_INVERTPIXELINROW((uint8_t*)VGA16Controller::s_viewPort[(y)], (x))




/*************************************************************************************/
/* VGA16Controller definitions */


VGA16Controller * VGA16Controller::s_instance = nullptr;



VGA16Controller::VGA16Controller()
  : VGAPalettedController(VGA16_LinesCount, NativePixelFormat::PALETTE16, 2, 1, ISRHandler)
{
  s_instance = this;
}


void VGA16Controller::setupDefaultPalette()
{
  for (int colorIndex = 0; colorIndex < 16; ++colorIndex) {
    RGB888 rgb888((Color)colorIndex);
    setPaletteItem(colorIndex, rgb888);
  }
}


void VGA16Controller::setPaletteItem(int index, RGB888 const & color)
{
  index %= 16;
  m_palette[index] = color;
  auto packed222 = RGB888toPackedRGB222(color);
  for (int i = 0; i < 16; ++i) {
    m_packedPaletteIndexPair_to_signals[(index << 4) | i] &= 0xFF00;
    m_packedPaletteIndexPair_to_signals[(index << 4) | i] |= (m_HVSync | packed222);
    m_packedPaletteIndexPair_to_signals[(i << 4) | index] &= 0x00FF;
    m_packedPaletteIndexPair_to_signals[(i << 4) | index] |= (m_HVSync | packed222) << 8;
  }
}


void VGA16Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); },
                    VGA16_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void VGA16Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     VGA16_SETPIXEL,
                     [&] (int X, int Y)                              { VGA16_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void VGA16Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void VGA16Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 16 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 2), colorIndex | (colorIndex << 4), sz / 2);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
}


// parameters not checked
void VGA16Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA16_INVERTPIXELINROW(row, x);
}


void VGA16Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
  // copy whole 16 bits words (4 pixels)
  auto src = (uint16_t*)(srcRow + x / 2);
  auto dst = (uint16_t*)(dstRow + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
}


void VGA16Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 16 bits words (4 pixels)
  auto a = (uint16_t*)(rowA + x / 2);
  auto b = (uint16_t*)(rowB + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
}


void VGA16Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)  { return RGB888toPaletteIndex(color); },
                     VGA16_SETPIXEL
                    );
}


void VGA16Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern = paletteIndex | (paletteIndex << 4);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern, m_viewPortWidth / 2);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void VGA16Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


void VGA16Controller::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t back4 = RGB888toPaletteIndex(getActualBrushColor());

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 3) == 0 && (width & 3) == 0);  // 4 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 2;
        for (int s = -scroll; s > 0;) {
          if (s > 1) {
            // scroll left by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row, row + sc / 2, (sz - sc) / 2);
            rawFillRow(y, X2 - sc + 1, X2, back4);
            s -= sc;
          } else if (s & 1) {
            // scroll left 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> 12 34 56 7P
            // word (little endian CPU) : 2301 6745  ->  3412 7P56
            auto prev = back4;
            auto w = (uint16_t *) (row + width / 2) - 1;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w-- = (p4 << 4 & 0xf000) | (prev << 8 & 0x0f00) | (p4 << 4 & 0x00f0) | (p4 >> 12 & 0x000f);
              prev = p4 >> 4 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back4);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 2;
        for (int s = scroll; s > 0;) {
          if (s > 1) {
            // scroll right by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row + sc / 2, row, (sz - sc) / 2);
            rawFillRow(y, X1, X1 + sc - 1, back4);
            s -= sc;
          } else if (s & 1) {
            // scroll right 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> P0 12 34 56 7...
            // word (little endian CPU) : 2301 6745  ->  12P0 5634 ...
            auto prev = back4;
            auto w = (uint16_t *) row;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w++ = (p4 << 12 & 0xf000) | (p4 >> 4 & 0x0f00) | (prev << 4) | (p4 >> 4 & 0x000f);
              prev = p4 >> 8 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA16_SETPIXELINROW(row, x + scroll, VGA16_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back4);
      }
    }

  }
}


void VGA16Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                   VGA16_SETPIXELINROW
                  );
}


void VGA16Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void VGA16Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void VGA16Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}


// no bounds check is done!
void VGA16Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA16_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void VGA16Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              VGA16_SETPIXELINROW
                             );
}


void VGA16Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },                   // rawGetRow
                            VGA16_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { VGA16_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void VGA16Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },       // rawGetRow
                                VGA16_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { VGA16_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void VGA16Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },     // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA16_GETPIXELINROW(row, x); },  // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA16_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR VGA16Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA16Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    auto const packedPaletteIndexPair_to_signals = (uint16_t const *) ctrl->m_packedPaletteIndexPair_to_signals;
    auto const lines  = ctrl->m_lines;

    int scanLine = (s_scanLine + VGA16_LinesCount / 2) % height;

    auto lineIndex = scanLine & (VGA16_LinesCount - 1);

    for (int i = 0; i < VGA16_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) lines[lineIndex];

      // optimizazion warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);
        auto src3 = *(src + 2);
        auto src4 = *(src + 3);
        auto src5 = *(src + 4);
        auto src6 = *(src + 5);
        auto src7 = *(src + 6);
        auto src8 = *(src + 7);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexPair_to_signals[src1];
        auto v2 = packedPaletteIndexPair_to_signals[src2];
        auto v3 = packedPaletteIndexPair_to_signals[src3];
        auto v4 = packedPaletteIndexPair_to_signals[src4];
        auto v5 = packedPaletteIndexPair_to_signals[src5];
        auto v6 = packedPaletteIndexPair_to_signals[src6];
        auto v7 = packedPaletteIndexPair_to_signals[src7];
        auto v8 = packedPaletteIndexPair_to_signals[src8];

        *(dest + 1) = v1;
        *(dest    ) = v2;
        *(dest + 3) = v3;
        *(dest + 2) = v4;
        *(dest + 5) = v5;
        *(dest + 4) = v6;
        *(dest + 7) = v7;
        *(dest + 6) = v8;

        dest += 8;
        src += 8;

      }

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA16_LinesCount / 2;

    if (scanLine >= height && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
      // vertical sync, unlock primitive execution task
      // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
      vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
    }

  }

  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  s_vgapalctrlcycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}



} // end of namespace
