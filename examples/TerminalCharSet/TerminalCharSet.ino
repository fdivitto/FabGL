/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019 Fabrizio Di Vittorio.
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


#include "fabgl.h"




/* * * *  C O N F I G U R A T I O N  * * * */

// select one color configuration
#define USE_8_COLORS  0
#define USE_64_COLORS 1

// indicate VGA GPIOs to use for selected color configuration
#if USE_8_COLORS
  #define VGA_RED    GPIO_NUM_22
  #define VGA_GREEN  GPIO_NUM_21
  #define VGA_BLUE   GPIO_NUM_19
  #define VGA_HSYNC  GPIO_NUM_18
  #define VGA_VSYNC  GPIO_NUM_5
#elif USE_64_COLORS
  #define VGA_RED1   GPIO_NUM_22
  #define VGA_RED0   GPIO_NUM_21
  #define VGA_GREEN1 GPIO_NUM_19
  #define VGA_GREEN0 GPIO_NUM_18
  #define VGA_BLUE1  GPIO_NUM_5
  #define VGA_BLUE0  GPIO_NUM_4
  #define VGA_HSYNC  GPIO_NUM_23
  #define VGA_VSYNC  GPIO_NUM_15
#endif

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */



TerminalClass Terminal;


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);
  //VGAController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  Terminal.begin();
}


void slowPrintf(const char * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(NULL, 0, format, ap) + 1;
  if (size > 0) {
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
  slowPrintf("\e[34m* * 2019 by Fabrizio Di Vittorio - www.fabgl.com\e[32m\r\n\n");

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
