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



#pragma once


#include "fabutils.h"
#include "fabglconf.h"


// Embedded fonts

// fixed width
#include "fonts/font_4x6.h"
#include "fonts/font_5x7.h"
#include "fonts/font_5x8.h"
#include "fonts/font_6x8.h"
#include "fonts/font_6x9.h"
#include "fonts/font_6x10.h"
#include "fonts/font_6x12.h"
#include "fonts/font_6x13.h"
#include "fonts/font_7x13.h"
#include "fonts/font_7x14.h"
#include "fonts/font_8x13.h"
#include "fonts/font_8x8.h"
#include "fonts/font_8x9.h"
#include "fonts/font_8x14.h"
#include "fonts/font_8x16.h"
#include "fonts/font_8x19.h"
#include "fonts/font_9x15.h"
#include "fonts/font_9x18.h"
#include "fonts/font_10x20.h"

#include "fonts/font_slant_8x14.h"
#include "fonts/font_sanserif_8x16.h"
#include "fonts/font_sanserif_8x14.h"
#include "fonts/font_lcd_8x14.h"
#include "fonts/font_courier_8x14.h"
#include "fonts/font_computer_8x14.h"
#include "fonts/font_bigserif_8x14.h"
#include "fonts/font_bigserif_8x16.h"
#include "fonts/font_block_8x14.h"
#include "fonts/font_broadway_8x14.h"
#include "fonts/font_oldengl_8x16.h"
#include "fonts/font_wiggly_8x16.h"



// variable width
#include "fonts/font_std_12.h"
#include "fonts/font_std_14.h"
#include "fonts/font_std_15.h"
#include "fonts/font_std_16.h"
#include "fonts/font_std_17.h"
#include "fonts/font_std_18.h"
#include "fonts/font_std_22.h"
#include "fonts/font_std_24.h"


namespace fabgl {



/**
 * @brief Gets the font info that best fits the specified number of columns and rows.
 *
 * This method returns only fixed width fonts.
 *
 * @param viewPortWidth Viewport width where to fit specified columns
 * @param viewPortHeight Viewport height where to fit specified rows
 * @param columns Minimum number of required columns.
 * @param rows Minimum number of required rows.
 *
 * @return The font info found.
 */
FontInfo const * getPresetFontInfo(int viewPortWidth, int viewPortHeight, int columns, int rows);


/**
 * @brief Gets the font info that best fits the specified height.
 *
 * @param height Required font height in pixels.
 * @param fixedWidth If True returns only fixed width fonts, if False returns only variable width fonts.
 *
 * @return The font info found.
 */
FontInfo const * getPresetFontInfoFromHeight(int height, bool fixedWidth);


/**
 * @brief Gets the fixed width font info with the specified sizes
 *
 * @param width Font width
 * @param height Font height
 *
 * @return The font info found.
 */
FontInfo const * getPresetFixedFont(int width, int height);


};
