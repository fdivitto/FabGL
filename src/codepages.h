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


/**
 * @file
 *
 * @brief This file contains codepages declarations
 *
 */


#include "fabutils.h"


namespace fabgl {



// associates virtual key to ASCII code
struct VirtualKeyToASCII {
  VirtualKey vk;
  uint8_t    ASCII;
};


struct CodePage {
  uint16_t                  codepage;
  const VirtualKeyToASCII * convTable;  // last item has vk = VK_NONE (ending marker)
};


extern const CodePage CodePage437;
extern const CodePage CodePage1252;


struct CodePages {
  static int count() { return 2; }
  static CodePage const * get(uint16_t codepage, CodePage const * defaultValue = &CodePage437) {
    static const CodePage * codepages[] = { &CodePage437, &CodePage1252 };
    for (int i = 0; i < sizeof(CodePage) / sizeof(CodePage*); ++i)
      if (codepages[i]->codepage == codepage)
        return codepages[i];
    return defaultValue;
  }
};



/**
 * @brief Converts virtual key item to ASCII.
 *
 * This method converts the specified virtual key to ASCII, if possible.<br>
 * For example VK_A is converted to 'A' (ASCII 0x41), CTRL  + VK_SPACE produces ASCII NUL (0x00), CTRL + letter produces
 * ASCII control codes from SOH (0x01) to SUB (0x1A), CTRL + VK_BACKSLASH produces ASCII FS (0x1C), CTRL + VK_QUESTION produces
 * ASCII US (0x1F), CTRL + VK_LEFTBRACKET produces ASCII ESC (0x1B), CTRL + VK_RIGHTBRACKET produces ASCII GS (0x1D),
 * CTRL + VK_TILDE produces ASCII RS (0x1E) and VK_SCROLLLOCK produces XON or XOFF.
 *
 * @param virtualKey The virtual key to convert.
 * @param codepage Codepage used to convert language specific characters.
 *
 * @return The ASCII code of virtual key or -1 if virtual key cannot be translated to ASCII.
 */
int virtualKeyToASCII(VirtualKeyItem const & item, CodePage const * codepage);



} // fabgl namespace

