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


#include "fabgl.h"





fabgl::VGAController DisplayController;
fabgl::Terminal      Terminal;


void setup()
{
  DisplayController.begin();
  DisplayController.setResolution(VGA_640x350_70HzAlt1);
  //DisplayController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  Terminal.begin(&DisplayController);
}


void slowPrintf(const char * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    for (int i = 0; i < size; ++i) {
      Terminal.write(buf[i]);
      delay(25);
    }
  }
  va_end(ap);
}


void loop()
{
  Terminal.write("\e[40;32m"); // background: black, foreground: green
  Terminal.write("\e[2J");     // clear screen
  Terminal.write("\e[1;1H");   // move cursor to 1,1

  slowPrintf("\e[37m* * FabGL - Character set show\r\n");
  slowPrintf("\e[34m* * 2019-2021 by Fabrizio Di Vittorio - www.fabgl.com\e[32m\r\n\n");

  for (int i = 32; i <= 255; ++i) {
    Terminal.setForegroundColor(Color::Green);
    slowPrintf("$%2x (%3d) = ", i, i);
    Terminal.setForegroundColor(Color::Yellow);
    slowPrintf("%c\t", (char)i);
    if ((i - 31) % 5 == 0)
      Terminal.write("\r\n");
  }

  delay(10000);
}
