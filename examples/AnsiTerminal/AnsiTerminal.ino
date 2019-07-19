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

// indicate PS/2 GPIOs for Keyboard port
#define PS2_PORT0_CLK GPIO_NUM_33
#define PS2_PORT0_DAT GPIO_NUM_32

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */


TerminalClass Terminal;


void setup()
{
  // for ESP32-PICO-D4 use GPIO 13 (RX) and 9 (TX)
  Serial2.begin(115200, SERIAL_8N1, 12, 2);

  // other boards
  //Serial2.begin(115200);

  //Serial.begin(115200); // debug only

  // only keyboard configured on port 0
  Keyboard.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);

  // this speed-up display but may generate flickering
  VGAController.enableBackgroundPrimitiveExecution(false);

  Terminal.begin();
  Terminal.connectSerialPort(Serial2);
  //Terminal.setLogStream(Serial);  // debug only

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::Green);
  Terminal.clear();
  Terminal.enableCursor(true);

  Terminal.write("* * FabGL - Serial VT/ANSI Terminal\r\n");
  Terminal.write("* * 2019 by Fabrizio Di Vittorio - www.fabgl.com\r\n\n");
  Terminal.printf("Screen Size        : %d x %d\r\n", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  Terminal.printf("Viewport Size      : %d x %d\r\n", Canvas.getWidth(), Canvas.getHeight());
  Terminal.printf("Terminal Size      : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("Keyboard           : %s\r\n\r\n", Keyboard.isKeyboardAvailable() ? "OK" : "Error");
  Terminal.printf("Free DMA Memory    : %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("Free 32 bit Memory : %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("Connect server to UART2 - 8N1 - 115200 baud\r\n\n");

  Terminal.setForegroundColor(Color::BrightGreen);
}


void loop()
{
  Terminal.pollSerialPort();
}
