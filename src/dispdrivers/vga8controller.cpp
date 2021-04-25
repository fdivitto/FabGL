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
#include "vga8controller.h"
#include "devdrivers/swgenerator.h"




#pragma GCC optimize ("O2")



namespace fabgl {

/*
To improve drawing and rendering speed pixels order is a bit oddy because we want to pack pixels (3 bits) into a uint32_t and ESP32 is little-endian.
8 pixels (24 bits) are packed in 3 bytes:
bytes:      0        1        2    ...
bits:   76543210 76543210 76543210 ...
pixels: 55666777 23334445 00011122 ...
bits24: 0                          1...
*/

static inline __attribute__((always_inline)) void VGA8_SETPIXELINROW(uint8_t * row, int x, int value) {
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 ^= ((value << shift) ^ *bits24) & (7 << shift);
}

static inline __attribute__((always_inline)) int VGA8_GETPIXELINROW(uint8_t * row, int x) {
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  return (*bits24 >> shift) & 7;
}

#define VGA8_INVERTPIXELINROW(row, x)       *((uint32_t*)(row + ((x) >> 3) * 3)) ^= 7 << (21 - ((x) & 7) * 3)

static inline __attribute__((always_inline)) void VGA8_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) VGA8Controller::sgetScanline(y);
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 ^= ((value << shift) ^ *bits24) & (7 << shift);
}

#define VGA8_GETPIXEL(x, y)                 VGA8_GETPIXELINROW((uint8_t*)VGA8Controller::s_viewPort[(y)], (x))

#define VGA8_INVERT_PIXEL(x, y)             VGA8_INVERTPIXELINROW((uint8_t*)VGA8Controller::s_viewPort[(y)], (x))




/*************************************************************************************/
/* VGA8Controller definitions */


VGA8Controller * VGA8Controller::s_instance = nullptr;




VGA8Controller::VGA8Controller()
  : VGAPalettedController(VGA8_LinesCount, NativePixelFormat::PALETTE8, 8, 3, ISRHandler)
{
  s_instance = this;
  m_packedPaletteIndexPair_to_signals = (uint16_t *) heap_caps_malloc(256 * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


VGA8Controller::~VGA8Controller()
{
  heap_caps_free((void *)m_packedPaletteIndexPair_to_signals);
}


void VGA8Controller::setupDefaultPalette()
{
  setPaletteItem(0, RGB888(0, 0, 0));       // 0: black
  setPaletteItem(1, RGB888(128, 0, 0));     // 1: red
  setPaletteItem(2, RGB888(0, 128, 0));     // 2: green
  setPaletteItem(3, RGB888(0, 0, 128));     // 3: blue
  setPaletteItem(4, RGB888(255, 0, 0));     // 4: bright red
  setPaletteItem(5, RGB888(0, 255, 0));     // 5: bright green
  setPaletteItem(6, RGB888(0, 0, 255));     // 6: bright blue
  setPaletteItem(7, RGB888(255, 255, 255)); // 7: white
}


void VGA8Controller::setPaletteItem(int index, RGB888 const & color)
{
  index %= 8;
  m_palette[index] = color;
  auto packed222 = RGB888toPackedRGB222(color);
  for (int i = 0; i < 8; ++i) {
    m_packedPaletteIndexPair_to_signals[(index << 3) | i] &= 0xFF00;
    m_packedPaletteIndexPair_to_signals[(index << 3) | i] |= (m_HVSync | packed222);
    m_packedPaletteIndexPair_to_signals[(i << 3) | index] &= 0x00FF;
    m_packedPaletteIndexPair_to_signals[(i << 3) | index] |= (m_HVSync | packed222) << 8;
  }
}


void VGA8Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); },
                    VGA8_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void VGA8Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     VGA8_SETPIXEL,
                     [&] (int X, int Y)                              { VGA8_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void VGA8Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void VGA8Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) m_viewPort[y];
  for (; x1 <= x2; ++x1)
    VGA8_SETPIXELINROW(row, x1, colorIndex);
}


// parameters not checked
void VGA8Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA8_INVERTPIXELINROW(row, x);
}


void VGA8Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  for (; x1 <= x2; ++x1)
    VGA8_SETPIXELINROW(dstRow, x1, VGA8_GETPIXELINROW(srcRow, x1));
}


void VGA8Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  for (; x1 <= x2; ++x1) {
    uint8_t a = VGA8_GETPIXELINROW(rowA, x1);
    uint8_t b = VGA8_GETPIXELINROW(rowB, x1);
    VGA8_SETPIXELINROW(rowA, x1, b);
    VGA8_SETPIXELINROW(rowB, x1, a);
  }
}


void VGA8Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)  { return RGB888toPaletteIndex(color); },
                     VGA8_SETPIXEL
                    );
}


void VGA8Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint32_t pattern8 = (paletteIndex) | (paletteIndex << 3) | (paletteIndex << 6) | (paletteIndex << 9) | (paletteIndex << 12) | (paletteIndex << 15) | (paletteIndex << 18) | (paletteIndex << 21);
  for (int y = 0; y < m_viewPortHeight; ++y) {
    auto dest = (uint8_t*) m_viewPort[y];
    for (int x = 0; x < m_viewPortWidth; x += 8, dest += 3)
      *((uint32_t*)dest) = (*((uint32_t*)dest) & 0xFF000000) | pattern8;
  }
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void VGA8Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


// todo: optimize!
void VGA8Controller::HScroll(int scroll, Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t back = RGB888toPaletteIndex(getActualBrushColor());

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 7) == 0 && (width & 7) == 0);  // 8 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      for (int s = -scroll; s > 0;) {
        if (HScrolllingRegionAligned && s >= 8) {
          // scroll left by multiplies of 8
          uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8 * 3;
          auto sc = s & ~7;
          auto sz = width & ~7;
          memmove(row, row + sc / 8 * 3, (sz - sc) / 8 * 3);
          rawFillRow(y, X2 - sc + 1, X2, back);
          s -= sc;
        } else {
          // unaligned horizontal scrolling region, fallback to slow version
          auto row = (uint8_t*) m_viewPort[y];
          for (int x = X1; x <= X2 - s; ++x)
            VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x + s));
          // fill right area with brush color
          rawFillRow(y, X2 - s + 1, X2, back);
          s = 0;
        }
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      for (int s = scroll; s > 0;) {
        if (HScrolllingRegionAligned && s >= 8) {
          // scroll right by multiplies of 8
          uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8 * 3;
          auto sc = s & ~7;
          auto sz = width & ~7;
          memmove(row + sc / 8 * 3, row, (sz - sc) / 8 * 3);
          rawFillRow(y, X1, X1 + sc - 1, back);
          s -= sc;
        } else {
          // unaligned horizontal scrolling region, fallback to slow version
          auto row = (uint8_t*) m_viewPort[y];
          for (int x = X2 - s; x >= X1; --x)
            VGA8_SETPIXELINROW(row, x + s, VGA8_GETPIXELINROW(row, x));
          // fill left area with brush color
          rawFillRow(y, X1, X1 + s - 1, back);
          s = 0;
        }
      }
    }
  }
}


void VGA8Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                   VGA8_SETPIXELINROW
                  );
}


void VGA8Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void VGA8Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA8_GETPIXELINROW,
                  VGA8_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void VGA8Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA8_GETPIXELINROW,
                  VGA8_SETPIXELINROW
                 );
}


// no bounds check is done!
void VGA8Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA8_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void VGA8Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              VGA8_SETPIXELINROW
                             );
}


void VGA8Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },                  // rawGetRow
                            VGA8_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { VGA8_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void VGA8Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                             // rawGetRow
                                VGA8_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { VGA8_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void VGA8Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },                         // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return VGA8_GETPIXELINROW(row, x); },                       // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { VGA8_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR VGA8Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA8Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    auto const packedPaletteIndexPair_to_signals = (uint16_t const *) ctrl->m_packedPaletteIndexPair_to_signals;
    auto const lines  = ctrl->m_lines;

    int scanLine = (s_scanLine + VGA8_LinesCount / 2) % height;

    auto lineIndex = scanLine & (VGA8_LinesCount - 1);

    for (int i = 0; i < VGA8_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) lines[lineIndex];

      // optimizazion warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto w1 = *((uint16_t*)(src    ));  // hi A:23334445, lo A:55666777
        auto w2 = *((uint16_t*)(src + 2));  // hi B:55666777, lo A:00011122
        auto w3 = *((uint16_t*)(src + 4));  // hi B:00011122, lo B:23334445

        PSRAM_HACK;

        auto src1 = w1 | (w2 << 16);
        auto src2 = (w2 >> 8) | (w3 << 8);

        auto v1 = packedPaletteIndexPair_to_signals[(src1      ) & 0x3f];  // pixels 0, 1
        auto v2 = packedPaletteIndexPair_to_signals[(src1 >>  6) & 0x3f];  // pixels 2, 3
        auto v3 = packedPaletteIndexPair_to_signals[(src1 >> 12) & 0x3f];  // pixels 4, 5
        auto v4 = packedPaletteIndexPair_to_signals[(src1 >> 18) & 0x3f];  // pixels 6, 7
        auto v5 = packedPaletteIndexPair_to_signals[(src2      ) & 0x3f];  // pixels 8, 9
        auto v6 = packedPaletteIndexPair_to_signals[(src2 >>  6) & 0x3f];  // pixels 10, 11
        auto v7 = packedPaletteIndexPair_to_signals[(src2 >> 12) & 0x3f];  // pixels 12, 13
        auto v8 = packedPaletteIndexPair_to_signals[(src2 >> 18) & 0x3f];  // pixels 14, 15

        *(dest + 2) = v1;
        *(dest + 3) = v2;
        *(dest + 0) = v3;
        *(dest + 1) = v4;
        *(dest + 6) = v5;
        *(dest + 7) = v6;
        *(dest + 4) = v7;
        *(dest + 5) = v8;

        dest += 8;
        src += 6;

      }

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA8_LinesCount / 2;

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
