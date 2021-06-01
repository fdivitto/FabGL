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



#define FABGL_FONT_INCLUDE_DEFINITION

#include "fabfonts.h"



namespace fabgl {


// do not include all fonts in "font" folder to avoid waste of flash
// for apps that uses getPresetFontInfo() (used in Terminal class), getPresetFontInfoFromHeight()
// and getPresetFixedFont().
static const FontInfo * FIXED_WIDTH_EMBEDDED_FONTS[] = {
  // please, bigger fonts first!
  &FONT_8x19,
  &FONT_8x16,
  &FONT_8x14,
  &FONT_8x8,
  &FONT_8x9,
  &FONT_6x8,
  &FONT_5x7,
  &FONT_4x6,
};


static const FontInfo * VAR_WIDTH_EMBEDDED_FONTS[] = {
  // please, bigger fonts first!
  &FONT_std_24,
  &FONT_std_22,
  &FONT_std_18,
  &FONT_std_17,
  &FONT_std_16,
  &FONT_std_15,
  &FONT_std_14,
  &FONT_std_12,
};



FontInfo const * getFixedWidthFont(int index)
{
  return FIXED_WIDTH_EMBEDDED_FONTS[index];
}


int getFixedWidthFontCount()
{
  return sizeof(FIXED_WIDTH_EMBEDDED_FONTS) / sizeof(FontInfo*);
}


FontInfo const * getVarWidthFont(int index)
{
  return VAR_WIDTH_EMBEDDED_FONTS[index];
}


int getVarWidthFontCount()
{
  return sizeof(VAR_WIDTH_EMBEDDED_FONTS) / sizeof(FontInfo*);
}


FontInfo const * getPresetFontInfo(int viewPortWidth, int viewPortHeight, int columns, int rows)
{
  FontInfo const * fontInfo = nullptr;
  for (int i = 0; i < getFixedWidthFontCount(); ++i) {
    fontInfo = getFixedWidthFont(i);
    if (viewPortWidth / fontInfo->width >= columns && viewPortHeight / fontInfo->height >= rows)
      return fontInfo;
  }
  // not found, return the smallest
  return fontInfo;
}


FontInfo const * getPresetFontInfoFromHeight(int height, bool fixedWidth)
{
  FontInfo const * fontInfo = nullptr;
  if (fixedWidth) {
    for (int i = 0; i < getFixedWidthFontCount(); ++i) {
      fontInfo = getFixedWidthFont(i);
      if (height >= fontInfo->height)
        return fontInfo;
    }
  } else {
    for (int i = 0; i < getVarWidthFontCount(); ++i) {
      fontInfo = getVarWidthFont(i);
      if (height >= fontInfo->height)
        return fontInfo;
    }
  }
  // not found, return the smallest
  return fontInfo;
}


FontInfo const * getPresetFixedFont(int width, int height)
{
  FontInfo const * fontInfo = nullptr;
  for (int i = 0; i < getFixedWidthFontCount(); ++i) {
    fontInfo = getFixedWidthFont(i);
    if (height == fontInfo->height && width == fontInfo->width)
      return fontInfo;
  }
  // not found, return the smallest (TODO: find a way to get meaningful result)
  return fontInfo;
}



}
