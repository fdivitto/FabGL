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
#include "vga2controller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")




namespace fabgl {



static inline __attribute__((always_inline)) void VGA2_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 3;
  row[brow] ^= (-value ^ row[brow]) & (0x80 >> (x & 7));
}

static inline __attribute__((always_inline)) int VGA2_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 3;
  return (row[brow] & (0x80 >> (x & 7))) != 0;
}

#define VGA2_INVERTPIXELINROW(row, x)       (row)[(x) >> 3] ^= (0x80 >> ((x) & 7))

static inline __attribute__((always_inline)) void VGA2_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) VGA2Controller::sgetScanline(y);
  int brow = x >> 3;
  row[brow] ^= (-value ^ row[brow]) & (0x80 >> (x & 7));
}

#define VGA2_GETPIXEL(x, y)                 VGA2_GETPIXELINROW((uint8_t*)VGA2Controller::s_viewPort[(y)], (x))

#define VGA2_INVERT_PIXEL(x, y)             VGA2_INVERTPIXELINROW((uint8_t*)VGA2Controller::s_viewPort[(y)], (x))




/*************************************************************************************/
/* VGA2Controller definitions */


VGA2Controller * VGA2Controller::s_instance = nullptr;



VGA2Controller::VGA2Controller()
  : VGAPalettedController(VGA2_LinesCount, NativePixelFormat::PALETTE2, 8, 1, ISRHandler)
{
  s_instance = this;
  m_packedPaletteIndexOctet_to_signals = (uint64_t *) heap_caps_malloc(256 * sizeof(uint64_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


VGA2Controller::~VGA2Controller()
{
  heap_caps_free((void *)m_packedPaletteIndexOctet_to_signals);
}


void VGA2Controller::setupDefaultPalette()
{
  setPaletteItem(0, RGB888(0, 0, 0));       // 0: black
  setPaletteItem(1, RGB888(255, 255, 255)); // 1: white
}


void VGA2Controller::setPaletteItem(int index, RGB888 const & color)
{
  index %= 2;
  m_palette[index] = color;
  auto packed222 = RGB888toPackedRGB222(color);
  for (int i = 0; i < 256; ++i) {
    auto b = (uint8_t *) (m_packedPaletteIndexOctet_to_signals + i);
    for (int j = 0; j < 8; ++j) {
      auto aj = 7 - j;
      if ((index == 0 && ((1 << aj) & i) == 0) || (index == 1 && ((1 << aj) & i) != 0)) {
        b[j ^ 2] = m_HVSync | packed222;
      }
    }
  }
}


void VGA2Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); },
                    VGA2_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void VGA2Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     VGA2_SETPIXEL,
                     [&] (int X, int Y)                              { VGA2_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void VGA2Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void VGA2Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 7) != 0; ++x) {
    VGA2_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 8 bits words (8 pixels)
  if (x <= x2) {
    int sz = (x2 & ~7) - x;
    memset((void*)(row + x / 8), colorIndex ? 0xFF : 0x00, sz / 8);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA2_SETPIXELINROW(row, x, colorIndex);
  }
}


// parameters not checked
void VGA2Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA2_INVERTPIXELINROW(row, x);
}


void VGA2Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 7) != 0; ++x) {
    VGA2_SETPIXELINROW(dstRow, x, VGA2_GETPIXELINROW(srcRow, x));
  }
  // copy whole 8 bits words (8 pixels)
  auto src = (uint8_t*)(srcRow + x / 8);
  auto dst = (uint8_t*)(dstRow + x / 8);
  for (int right = (x2 & ~7); x < right; x += 8)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~7); x <= x2; ++x) {
    VGA2_SETPIXELINROW(dstRow, x, VGA2_GETPIXELINROW(srcRow, x));
  }
}


void VGA2Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 7) != 0; ++x) {
    uint8_t a = VGA2_GETPIXELINROW(rowA, x);
    uint8_t b = VGA2_GETPIXELINROW(rowB, x);
    VGA2_SETPIXELINROW(rowA, x, b);
    VGA2_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 8 bits words (8 pixels)
  auto a = (uint8_t*)(rowA + x / 8);
  auto b = (uint8_t*)(rowB + x / 8);
  for (int right = (x2 & ~7); x < right; x += 8)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~7); x <= x2; ++x) {
    uint8_t a = VGA2_GETPIXELINROW(rowA, x);
    uint8_t b = VGA2_GETPIXELINROW(rowB, x);
    VGA2_SETPIXELINROW(rowA, x, b);
    VGA2_SETPIXELINROW(rowB, x, a);
  }
}


void VGA2Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)  { return RGB888toPaletteIndex(color); },
                     VGA2_SETPIXEL
                    );
}


void VGA2Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern8 = paletteIndex ? 0xFF : 0x00;
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern8, m_viewPortWidth / 8);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void VGA2Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


void VGA2Controller::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t back  = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t back8 = back ? 0xFF : 0x00;

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 7) == 0 && (width & 7) == 0);  // 8 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8;
        for (int s = -scroll; s > 0;) {
          if (s < 8) {
            // scroll left by 1..7
            int sz = width / 8;
            uint8_t prev = back8;
            for (int i = sz - 1; i >= 0; --i) {
              uint8_t lowbits = prev >> (8 - s);
              prev = row[i];
              row[i] = (row[i] << s) | lowbits;
            }
            s = 0;
          } else {
            // scroll left by multiplies of 8
            auto sc = s & ~7;
            auto sz = width & ~7;
            memmove(row, row + sc / 8, (sz - sc) / 8);
            rawFillRow(y, X2 - sc + 1, X2, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA2_SETPIXELINROW(row, x, VGA2_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8;
        for (int s = scroll; s > 0;) {
          if (s < 8) {
            // scroll right by 1..7
            int sz = width / 8;
            uint8_t prev = back8;
            for (int i = 0; i < sz; ++i) {
              uint8_t highbits = prev << (8 - s);
              prev = row[i];
              row[i] = (row[i] >> s) | highbits;
            }
            s = 0;
          } else {
            // scroll right by multiplies of 8
            auto sc = s & ~7;
            auto sz = width & ~7;
            memmove(row + sc / 8, row, (sz - sc) / 8);
            rawFillRow(y, X1, X1 + sc - 1, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA2_SETPIXELINROW(row, x + scroll, VGA2_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back);
      }
    }

  }
}


void VGA2Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                   VGA2_SETPIXELINROW
                  );
}


void VGA2Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void VGA2Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA2_GETPIXELINROW,
                  VGA2_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void VGA2Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA2_GETPIXELINROW,
                  VGA2_SETPIXELINROW
                 );
}


// no bounds check is done!
void VGA2Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA2_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void VGA2Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              VGA2_SETPIXELINROW
                             );
}


void VGA2Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },                   // rawGetRow
                            VGA2_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { VGA2_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void VGA2Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },       // rawGetRow
                                VGA2_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { VGA2_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void VGA2Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },     // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA2_GETPIXELINROW(row, x); },  // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA2_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR VGA2Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA2Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    auto const packedPaletteIndexOctet_to_signals = (uint64_t const *) ctrl->m_packedPaletteIndexOctet_to_signals;
    auto const lines  = ctrl->m_lines;

    int scanLine = (s_scanLine + VGA2_LinesCount / 2) % height;

    auto lineIndex = scanLine & (VGA2_LinesCount - 1);

    for (int i = 0; i < VGA2_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint64_t*) lines[lineIndex];

      // optimizazion warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexOctet_to_signals[src1];
        auto v2 = packedPaletteIndexOctet_to_signals[src2];

        *(dest + 0) = v1;
        *(dest + 1) = v2;

        dest += 2;
        src += 2;
        
      }

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA2_LinesCount / 2;

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
