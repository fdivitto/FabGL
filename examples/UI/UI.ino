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
#include "fabui.h"

#include "App.h"



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

// select one Keyboard and Mouse configuration
#define KEYBOARD_ON_PORT0                0
#define MOUSE_ON_PORT0                   0
#define KEYBOARD_ON_PORT0_MOUSE_ON_PORT1 1

// indicate PS/2 GPIOs for each port
#define PS2_PORT0_CLK GPIO_NUM_33
#define PS2_PORT0_DAT GPIO_NUM_32
#define PS2_PORT1_CLK GPIO_NUM_26
#define PS2_PORT1_DAT GPIO_NUM_27

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */




void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  #if KEYBOARD_ON_PORT0_MOUSE_ON_PORT1
  // both keyboard (port 0) and mouse configured (port 1)
  PS2Controller.begin(PS2_PORT0_CLK, PS2_PORT0_DAT, PS2_PORT1_CLK, PS2_PORT1_DAT);
  Keyboard.begin(true, false, 0);
  Mouse.begin(1);
  #elif KEYBOARD_ON_PORT0
  // only keyboard configured on port 0
  Keyboard.begin(PS2_PORT0_CLK, PS2_PORT0_DAT, true, false);
  #elif MOUSE_ON_PORT0
  // only mouse configured on port 0
  Mouse.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);
  #endif

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  //VGAController.setResolution(VGA_320x200_75Hz);
  VGAController.setResolution(VGA_640x350_70HzAlt1);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(-6, 0);
}




void loop()
{
  MyApp().run();
}






