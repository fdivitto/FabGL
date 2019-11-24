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


fabgl::VGAController VGAController;
fabgl::PS2Controller PS2Controller;
fabgl::Terminal      Terminal;


void setup()
{
  // for ESP32-PICO-D4 use GPIO 12 (RX) and 2 (TX)
  Serial2.begin(115200, SERIAL_8N1, 12, 2);

  // other boards
  //Serial2.begin(115200);

  //Serial.begin(115200); // debug only

  // only keyboard configured on port 0
  PS2Controller.begin(PS2Preset::KeyboardPort0);

  VGAController.begin();
  VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);

  Terminal.begin(&VGAController);
  Terminal.connectSerialPort(Serial2);
  //Terminal.setLogStream(Serial);  // debug only

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::Green);
  Terminal.clear();
  Terminal.enableCursor(true);

  Terminal.write("* * FabGL - Serial VT/ANSI Terminal\r\n");
  Terminal.write("* * 2019 by Fabrizio Di Vittorio - www.fabgl.com\r\n\n");
  Terminal.printf("Screen Size        : %d x %d\r\n", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  Terminal.printf("Viewport Size      : %d x %d\r\n", VGAController.getViewPortWidth(), VGAController.getViewPortHeight());
  Terminal.printf("Terminal Size      : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("Keyboard           : %s\r\n\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "Error");
  Terminal.printf("Free DMA Memory    : %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("Free 32 bit Memory : %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("Connect server to UART2 - 8N1 - 115200 baud\r\n\n");

  Terminal.setForegroundColor(Color::BrightGreen);
}


void loop()
{
  Terminal.pollSerialPort();
}
