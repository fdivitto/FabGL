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
#include "vga4controller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")




namespace fabgl {



static inline __attribute__((always_inline)) void VGA4_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] ^= ((value << shift) ^ row[brow]) & (3 << shift);
}

static inline __attribute__((always_inline)) int VGA4_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  return (row[brow] >> shift) & 3;
}

#define VGA4_INVERTPIXELINROW(row, x)       (row)[(x) >> 2] ^= 3 << (6 - ((x) & 3) * 2)

static inline __attribute__((always_inline)) void VGA4_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) VGA4Controller::sgetScanline(y);
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] ^= ((value << shift) ^ row[brow]) & (3 << shift);
}

#define VGA4_GETPIXEL(x, y)                 VGA4_GETPIXELINROW((uint8_t*)VGA4Controller::s_viewPort[(y)], (x))

#define VGA4_INVERT_PIXEL(x, y)             VGA4_INVERTPIXELINROW((uint8_t*)VGA4Controller::s_viewPort[(y)], (x))




/*************************************************************************************/
/* VGA4Controller definitions */


VGA4Controller * VGA4Controller::s_instance = nullptr;



VGA4Controller::VGA4Controller()
  : VGAPalettedController(VGA4_LinesCount, NativePixelFormat::PALETTE4, 4, 1, ISRHandler)
{
  s_instance = this;
  m_packedPaletteIndexQuad_to_signals = (uint32_t *) heap_caps_malloc(256 * sizeof(uint32_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


VGA4Controller::~VGA4Controller()
{
  heap_caps_free((void *)m_packedPaletteIndexQuad_to_signals);
}


void VGA4Controller::setupDefaultPalette()
{
  setPaletteItem(0, RGB888(0, 0, 0));       // 0: black
  setPaletteItem(1, RGB888(0, 0, 255));     // 1: blue
  setPaletteItem(2, RGB888(0, 255, 0));     // 2: green
  setPaletteItem(3, RGB888(255, 255, 255)); // 3: white
}


void VGA4Controller::setPaletteItem(int index, RGB888 const & color)
{
  index %= 4;
  m_palette[index] = color;
  auto packed222 = RGB888toPackedRGB222(color);
  for (int i = 0; i < 256; ++i) {
    auto b = (uint8_t *) (m_packedPaletteIndexQuad_to_signals + i);
    for (int j = 0; j < 4; ++j) {
      auto aj = 6 - j * 2;
      if (((i >> aj) & 3) == index) {
        b[j ^ 2] = m_HVSync | packed222;
      }
    }
  }
}


void VGA4Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); },
                    VGA4_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void VGA4Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     VGA4_SETPIXEL,
                     [&] (int X, int Y)                              { VGA4_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void VGA4Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void VGA4Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA4_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 8 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 4), colorIndex | (colorIndex << 2) | (colorIndex << 4) | (colorIndex << 6), sz / 4);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA4_SETPIXELINROW(row, x, colorIndex);
  }
}


// parameters not checked
void VGA4Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA4_INVERTPIXELINROW(row, x);
}


void VGA4Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA4_SETPIXELINROW(dstRow, x, VGA4_GETPIXELINROW(srcRow, x));
  }
  // copy whole 8 bits words (4 pixels)
  auto src = (uint8_t*)(srcRow + x / 4);
  auto dst = (uint8_t*)(dstRow + x / 4);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    VGA4_SETPIXELINROW(dstRow, x, VGA4_GETPIXELINROW(srcRow, x));
  }
}


void VGA4Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = VGA4_GETPIXELINROW(rowA, x);
    uint8_t b = VGA4_GETPIXELINROW(rowB, x);
    VGA4_SETPIXELINROW(rowA, x, b);
    VGA4_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 8 bits words (4 pixels)
  auto a = (uint8_t*)(rowA + x / 4);
  auto b = (uint8_t*)(rowB + x / 4);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = VGA4_GETPIXELINROW(rowA, x);
    uint8_t b = VGA4_GETPIXELINROW(rowB, x);
    VGA4_SETPIXELINROW(rowA, x, b);
    VGA4_SETPIXELINROW(rowB, x, a);
  }
}


void VGA4Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)  { return RGB888toPaletteIndex(color); },
                     VGA4_SETPIXEL
                    );
}


void VGA4Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern4 = paletteIndex | (paletteIndex << 2) | (paletteIndex << 4) | (paletteIndex << 6);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern4, m_viewPortWidth / 4);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void VGA4Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


void VGA4Controller::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t back  = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t back4 = back | (back << 2) | (back << 4) | (back << 6);

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
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 4;
        for (int s = -scroll; s > 0;) {
          if (s < 4) {
            // scroll left by 1..3
            int sz = width / 4;
            uint8_t prev = back4;
            for (int i = sz - 1; i >= 0; --i) {
              uint8_t lowbits = prev >> (8 - s * 2);
              prev = row[i];
              row[i] = (row[i] << (s * 2)) | lowbits;
            }
            s = 0;
          } else {
            // scroll left by multiplies of 4
            auto sc = s & ~3;
            auto sz = width & ~3;
            memmove(row, row + sc / 4, (sz - sc) / 4);
            rawFillRow(y, X2 - sc + 1, X2, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 4;
        for (int s = scroll; s > 0;) {
          if (s < 4) {
            // scroll right by 1..3
            int sz = width / 4;
            uint8_t prev = back4;
            for (int i = 0; i < sz; ++i) {
              uint8_t highbits = prev << (8 - s * 2);
              prev = row[i];
              row[i] = (row[i] >> (s * 2)) | highbits;
            }
            s = 0;
          } else {
            // scroll right by multiplies of 4
            auto sc = s & ~3;
            auto sz = width & ~3;
            memmove(row + sc / 4, row, (sz - sc) / 4);
            rawFillRow(y, X1, X1 + sc - 1, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA4_SETPIXELINROW(row, x + scroll, VGA4_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back);
      }
    }

  }
}


void VGA4Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                   VGA4_SETPIXELINROW
                  );
}


void VGA4Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void VGA4Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA4_GETPIXELINROW,
                  VGA4_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void VGA4Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA4_GETPIXELINROW,
                  VGA4_SETPIXELINROW
                 );
}


// no bounds check is done!
void VGA4Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA4_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void VGA4Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              VGA4_SETPIXELINROW
                             );
}


void VGA4Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },                  // rawGetRow
                            VGA4_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { VGA4_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void VGA4Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                             // rawGetRow
                                VGA4_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { VGA4_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void VGA4Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },                         // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA4_GETPIXELINROW(row, x); },                       // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA4_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR VGA4Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA4Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    auto const packedPaletteIndexQuad_to_signals = (uint32_t const *) ctrl->m_packedPaletteIndexQuad_to_signals;
    auto const lines  = ctrl->m_lines;

    int scanLine = (s_scanLine + VGA4_LinesCount / 2) % height;

    auto lineIndex = scanLine & (VGA4_LinesCount - 1);

    for (int i = 0; i < VGA4_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint32_t*) lines[lineIndex];

      // optimizazion warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);
        auto src3 = *(src + 2);
        auto src4 = *(src + 3);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexQuad_to_signals[src1];
        auto v2 = packedPaletteIndexQuad_to_signals[src2];
        auto v3 = packedPaletteIndexQuad_to_signals[src3];
        auto v4 = packedPaletteIndexQuad_to_signals[src4];

        *(dest + 0) = v1;
        *(dest + 1) = v2;
        *(dest + 2) = v3;
        *(dest + 3) = v4;

        dest += 4;
        src += 4;
      }

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA4_LinesCount / 2;

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
