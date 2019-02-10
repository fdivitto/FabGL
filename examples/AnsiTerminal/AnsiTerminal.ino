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



void setup()
{
  Serial2.begin(115200);

  //Serial.begin(115200); // debug only

  // GPIOs for keyboard CLK and DATA
  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);

  // 8 colors
  //VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);

  // 64 colors
  VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);

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
