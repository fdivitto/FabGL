/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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

#include "esp_spi_flash.h"

#include "fabutils.h"
#include "cvbs16controller.h"



#pragma GCC optimize ("O2")




namespace fabgl {



// high nibble is pixel 0, low nibble is pixel 1

static inline __attribute__((always_inline)) void CVBS16_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

static inline __attribute__((always_inline)) int CVBS16_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 1;
  return ((x & 1) ? (row[brow] & 0x0f) : ((row[brow] & 0xf0) >> 4));
}

#define CVBS16_INVERTPIXELINROW(row, x)       (row)[(x) >> 1] ^= (0xf0 >> (((x) & 1) << 2))

static inline __attribute__((always_inline)) void CVBS16_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) CVBS16Controller::sgetScanline(y);
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

#define CVBS16_GETPIXEL(x, y)                 CVBS16_GETPIXELINROW((uint8_t*)CVBS16Controller::s_viewPort[(y)], (x))

#define CVBS16_INVERT_PIXEL(x, y)             CVBS16_INVERTPIXELINROW((uint8_t*)CVBS16Controller::s_viewPort[(y)], (x))


#define CVBS16_COLUMNSQUANTUM 16


/*************************************************************************************/
/* CVBS16Controller definitions */


CVBS16Controller *    CVBS16Controller::s_instance = nullptr;
volatile uint16_t * * CVBS16Controller::s_paletteToRawPixel[2];


CVBS16Controller::CVBS16Controller()
  : CVBSPalettedController(CVBS16_COLUMNSQUANTUM, NativePixelFormat::PALETTE16, 2, 1),
    m_monochrome(false)
{
  s_instance = this;
  s_paletteToRawPixel[0] = s_paletteToRawPixel[1] = nullptr;
}


void CVBS16Controller::allocateViewPort()
{
  CVBSPalettedController::allocateViewPort();
  
  for (int line = 0; line < 2; ++line) {
    s_paletteToRawPixel[line] = (volatile uint16_t * *) heap_caps_malloc(sizeof(uint16_t *) * 16, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    for (int index = 0; index < 16; ++index)
      s_paletteToRawPixel[line][index] = (uint16_t *) heap_caps_malloc(sizeof(uint16_t) * CVBS_SUBCARRIERPHASES * 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  }
  
  switch (horizontalRate()) {
    case 1:
      setDrawScanlineCallback(drawScanlineX1);
      break;
    case 2:
      setDrawScanlineCallback(drawScanlineX2);
      break;
    default:
      setDrawScanlineCallback(drawScanlineX3);
      break;
  }

  //printf("m_viewPortWidth = %d  m_viewPortHeight = %d\n", m_viewPortWidth, m_viewPortHeight);
  //printf("C. Free Memory   : %d bytes\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
}


void CVBS16Controller::checkViewPortSize()
{
  CVBSPalettedController::checkViewPortSize();
}


void CVBS16Controller::setupDefaultPalette()
{
  for (int colorIndex = 0; colorIndex < 16; ++colorIndex) {
    RGB888 rgb888((Color)colorIndex);
    setPaletteItem(colorIndex, rgb888);
  }
}


void CVBS16Controller::setMonochrome(bool value)
{
  m_monochrome = value;
  setupDefaultPalette();
}


void CVBS16Controller::setPaletteItem(int index, RGB888 const & color)
{
  if (s_paletteToRawPixel[0]) {
    index %= 16;
    m_palette[index] = color;
    
    double range = params()->whiteLevel - params()->blackLevel + 1;
    
    double r = color.R / 255.;
    double g = color.G / 255.;
    double b = color.B / 255.;
    
    for (int line = 0; line < 2; ++line) {
      for (int sample = 0; sample < CVBS_SUBCARRIERPHASES * 2; ++sample) {
      
        double phase = 2. * M_PI * sample / CVBS_SUBCARRIERPHASES;
        
        double Y;
        double chroma = params()->getComposite(line == 0, phase, r, g, b, &Y);
        
        // black/white?
        if (m_monochrome)
          chroma = 0;
        
        s_paletteToRawPixel[line][index][sample] = (uint16_t)(params()->blackLevel + (Y + chroma) * range) << 8;
      }
    }
  }
}


void CVBS16Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); },
                    CVBS16_SETPIXEL
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void CVBS16Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                      { return RGB888toPaletteIndex(color); },
                     [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); },
                     [&] (int Y, int X1, int X2)                     { rawInvertRow(Y, X1, X2); },
                     CVBS16_SETPIXEL,
                     [&] (int X, int Y)                              { CVBS16_INVERT_PIXEL(X, Y); }
                     );
}


// parameters not checked
void CVBS16Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, RGB888toPaletteIndex(color));
}


// parameters not checked
void CVBS16Controller::rawFillRow(int y, int x1, int x2, uint8_t colorIndex)
{
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    CVBS16_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 16 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 2), colorIndex | (colorIndex << 4), sz / 2);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    CVBS16_SETPIXELINROW(row, x, colorIndex);
  }
}


// parameters not checked
void CVBS16Controller::rawInvertRow(int y, int x1, int x2)
{
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    CVBS16_INVERTPIXELINROW(row, x);
}


void CVBS16Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    CVBS16_SETPIXELINROW(dstRow, x, CVBS16_GETPIXELINROW(srcRow, x));
  }
  // copy whole 16 bits words (4 pixels)
  auto src = (uint16_t*)(srcRow + x / 2);
  auto dst = (uint16_t*)(dstRow + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    CVBS16_SETPIXELINROW(dstRow, x, CVBS16_GETPIXELINROW(srcRow, x));
  }
}


void CVBS16Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = CVBS16_GETPIXELINROW(rowA, x);
    uint8_t b = CVBS16_GETPIXELINROW(rowB, x);
    CVBS16_SETPIXELINROW(rowA, x, b);
    CVBS16_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 16 bits words (4 pixels)
  auto a = (uint16_t*)(rowA + x / 2);
  auto b = (uint16_t*)(rowB + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = CVBS16_GETPIXELINROW(rowA, x);
    uint8_t b = CVBS16_GETPIXELINROW(rowB, x);
    CVBS16_SETPIXELINROW(rowA, x, b);
    CVBS16_SETPIXELINROW(rowB, x, a);
  }
}


void CVBS16Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)  { return RGB888toPaletteIndex(color); },
                     CVBS16_SETPIXEL
                    );
}


void CVBS16Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern = paletteIndex | (paletteIndex << 4);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern, m_viewPortWidth / 2);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void CVBS16Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, color); }           // rawFillRow
                );
}


void CVBS16Controller::HScroll(int scroll, Rect & updateRect)
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
          CVBS16_SETPIXELINROW(row, x, CVBS16_GETPIXELINROW(row, x - scroll));
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
          CVBS16_SETPIXELINROW(row, x + scroll, CVBS16_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back4);
      }
    }

  }
}


void CVBS16Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                   [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                   CVBS16_SETPIXELINROW
                  );
}


void CVBS16Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void CVBS16Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  CVBS16_GETPIXELINROW,
                  CVBS16_SETPIXELINROW
                 );
}


// Slow operation!
// supports overlapping of source and dest rectangles
void CVBS16Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  CVBS16_GETPIXELINROW,
                  CVBS16_SETPIXELINROW
                 );
}


// no bounds check is done!
void CVBS16Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[CVBS16_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}


void CVBS16Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              CVBS16_SETPIXELINROW
                             );
}


void CVBS16Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundColorIndex = RGB888toPaletteIndex(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },                    // rawGetRow
                            CVBS16_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { CVBS16_SETPIXELINROW(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}


void CVBS16Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                               // rawGetRow
                                CVBS16_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { CVBS16_SETPIXELINROW(row, x, RGB2222toPaletteIndex(src)); }       // rawSetPixelInRow
                               );
}


void CVBS16Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },                           // rawGetRow
                                 [&] (uint8_t * row, int x)                       { return CVBS16_GETPIXELINROW(row, x); },                       // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { CVBS16_SETPIXELINROW(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}


void IRAM_ATTR CVBS16Controller::drawScanlineX1(void * arg, uint16_t * dest, int destSample, int scanLine)
{
  auto ctrl = (CVBS16Controller *) arg;

  auto const width  = ctrl->m_viewPortWidth;

  auto src    = (uint8_t const *) s_viewPortVisible[scanLine];
  auto dest32 = (uint32_t*) (dest + destSample);
  
  int  subCarrierPhaseSam = CVBSGenerator::subCarrierPhase();
  auto paletteToRaw       = (uint16_t * *)  s_paletteToRawPixel[CVBSGenerator::lineSwitch()];
  auto sampleLUT          = CVBSGenerator::lineSampleToSubCarrierSample() + destSample;
  
  // optimization warn: horizontal resolution must be a multiple of 16!
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
    
    *dest32++ = (paletteToRaw[src1 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src1 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src2 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src2 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src3 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src3 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src4 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src4 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src5 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src5 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src6 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src6 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src7 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src7 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;
    *dest32++ = (paletteToRaw[src8 >> 4][*sampleLUT + subCarrierPhaseSam] << 16) | (paletteToRaw[src8 & 0x0f][*(sampleLUT + 1) + subCarrierPhaseSam]);
    sampleLUT += 2;

    src    += 8;  // advance by 8x2=16 pixels

  }

  if (CVBSGenerator::VSync() && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
    // vertical sync, unlock primitive execution task
    // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
    vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
  }
}


void IRAM_ATTR CVBS16Controller::drawScanlineX2(void * arg, uint16_t * dest, int destSample, int scanLine)
{
  auto ctrl = (CVBS16Controller *) arg;

  auto const width  = ctrl->m_viewPortWidth * 2;

  auto src    = (uint8_t const *) s_viewPortVisible[scanLine];
  auto dest32 = (uint32_t*) (dest + destSample);
  
  int  subCarrierPhaseSam = CVBSGenerator::subCarrierPhase();
  auto paletteToRaw       = (uint16_t * *) s_paletteToRawPixel[CVBSGenerator::lineSwitch()];
  auto sampleLUT          = CVBSGenerator::lineSampleToSubCarrierSample() + destSample;
  
  /*
  if (CVBSGenerator::field() == 0) {
    dest += destSample;
    for (int i = 0; i < width; ++i)
      *dest++ = 0x1900;
  } else
  //*/
  
  // optimization warn: horizontal resolution must be a multiple of 8!
  for (int col = 0; col < width; col += 16) {

    auto src1 = *(src + 0);
    auto src2 = *(src + 1);
    auto src3 = *(src + 2);
    auto src4 = *(src + 3);
    
    PSRAM_HACK;

    auto praw = paletteToRaw[src1 >> 4] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src1 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src2 >> 4] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src2 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src3 >> 4] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src3 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src4 >> 4] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    praw = paletteToRaw[src4 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (praw[*sampleLUT] << 16) | (praw[*(sampleLUT + 1)]);
    sampleLUT += 2;

    src += 4;  // advance by 4x2=8 pixels

  }

  if (CVBSGenerator::VSync() && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
    // vertical sync, unlock primitive execution task
    // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
    vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
  }

}


void IRAM_ATTR CVBS16Controller::drawScanlineX3(void * arg, uint16_t * dest, int destSample, int scanLine)
{
  auto ctrl = (CVBS16Controller *) arg;

  auto const width  = ctrl->m_viewPortWidth * 3;

  auto src    = (uint8_t const *) s_viewPortVisible[scanLine];
  auto dest32 = (uint32_t*) (dest + destSample);
  
  int  subCarrierPhaseSam = CVBSGenerator::subCarrierPhase();
  auto paletteToRaw       = (uint16_t * *) s_paletteToRawPixel[CVBSGenerator::lineSwitch()];
  auto sampleLUT          = CVBSGenerator::lineSampleToSubCarrierSample() + destSample;
  
  // optimization warn: horizontal resolution must be a multiple of 8!
  for (int col = 0; col < width; col += 24) {

    auto src1 = *(src + 0);
    auto src2 = *(src + 1);
    auto src3 = *(src + 2);
    auto src4 = *(src + 3);
    
    PSRAM_HACK;
    
    auto prawl = paletteToRaw[src1 >> 4] + subCarrierPhaseSam;
    auto prawr = paletteToRaw[src1 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawl[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawr[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    prawl = paletteToRaw[src2 >> 4] + subCarrierPhaseSam;
    prawr = paletteToRaw[src2 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawl[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawr[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    
    prawl = paletteToRaw[src3 >> 4] + subCarrierPhaseSam;
    prawr = paletteToRaw[src3 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawl[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawr[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;

    prawl = paletteToRaw[src4 >> 4] + subCarrierPhaseSam;
    prawr = paletteToRaw[src4 & 0x0f] + subCarrierPhaseSam;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawl[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawl[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;
    *dest32++ = (prawr[*sampleLUT] << 16) | (prawr[*(sampleLUT + 1)]);
    sampleLUT += 2;

    src    += 4;  // advance by 4x2=8 pixels

  }

  if (CVBSGenerator::VSync() && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
    // vertical sync, unlock primitive execution task
    // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
    vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
  }
}





} // end of namespace
