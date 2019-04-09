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


/** @example LoopbackTerminal/LoopbackTerminal.ino
 * Loopback VT/ANSI Terminal
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

// indicate PS/2 Keyboard GPIOs
#define PS2_PORT0_CLK GPIO_NUM_33
#define PS2_PORT0_DAT GPIO_NUM_32

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */



TerminalClass Terminal;


void print_info()
{
  Terminal.write("\e[37m* * FabGL - Loopback VT/ANSI Terminal\r\n");
  Terminal.write("\e[34m* * 2019 by Fabrizio Di Vittorio - www.fabgl.com\e[32m\r\n\n");
  Terminal.printf("\e[32mScreen Size        :\e[33m %d x %d\r\n", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  Terminal.printf("\e[32mTerminal Size      :\e[33m %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("\e[32mKeyboard           :\e[33m %s\r\n", Keyboard.isKeyboardAvailable() ? "OK" : "Error");
  Terminal.printf("\e[32mFree DMA Memory    :\e[33m %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("\e[32mFree 32 bit Memory :\e[33m %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("\e[32mFree typing test - press ESC to introduce escape VT/ANSI codes\r\n\n");
}


void setup()
{
  // GPIOs for keyboard CLK and DATA
  Keyboard.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);

  // adjust screen position and size
  //VGAController.shrinkScreen(5, 0);
  //VGAController.moveScreen(-1, 0);

  Terminal.begin();
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();

  print_info();

  Terminal.enableCursor(true);
}


void loop()
{
  if (Terminal.available()) {
    char c = Terminal.read();
    switch (c) {
     case 0x7F:       // DEL -> backspace + ESC[K
       Terminal.write("\b\e[K");
       break;
     case 0x0D:       // CR  -> CR + LF
       Terminal.write("\r\n");
       break;
    default:
       Terminal.write(c);
       break;
    }
  }
}

