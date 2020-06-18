 /*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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


fabgl::VGAController DisplayController;
fabgl::PS2Controller PS2Controller;
fabgl::Terminal      Terminal;


// notes about GPIO 2 and 12
//    - GPIO2:  may cause problem on programming. GPIO2 must also be either left unconnected/floating, or driven Low, in order to enter the serial bootloader.
//              In normal boot mode (GPIO0 high), GPIO2 is ignored.
//    - GPIO12: should be avoided. It selects flash voltage. To use it disable GPIO12 detection setting efuses with:
//                    python espefuse.py --port /dev/cu.SLAB_USBtoUART set_flash_voltage 3.3V
//                       WARN!! Good for ESP32 with 3.3V voltage (ESP-WROOM-32). This will BRICK your ESP32 if the flash isn't 3.3V
//                       NOTE1: replace "/dev/cu.SLAB_USBtoUART" with your serial port
//                       NOTE2: espefuse.py is downloadable from https://github.com/espressif/esptool
#define UART_RX 34
#define UART_TX 2

#define UART_BAUD     115200
#define UART_CONF     SERIAL_8N1
#define UART_FLOWCTRL FlowControl::Software



void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\nReset\n\n"); // DEBUG ONLY

  // only keyboard configured on port 0
  PS2Controller.begin(PS2Preset::KeyboardPort0);

  DisplayController.begin();
  DisplayController.setResolution(VGA_640x350_70HzAlt1, 640, 350);

  Terminal.begin(&DisplayController);
  Terminal.connectSerialPort(UART_BAUD, UART_CONF, UART_RX, UART_TX, UART_FLOWCTRL);
  //Terminal.setLogStream(Serial);  // debug only

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::Green);
  Terminal.clear();
  Terminal.enableCursor(true);

  Terminal.write("* * FabGL - Serial VT/ANSI Terminal\r\n");
  Terminal.write("* * 2019-2020 by Fabrizio Di Vittorio - www.fabgl.com\r\n\n");
  Terminal.printf("Screen Size        : %d x %d\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
  Terminal.printf("Viewport Size      : %d x %d\r\n", DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight());
  Terminal.printf("Terminal Size      : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("Keyboard           : %s\r\n\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "Error");
  Terminal.printf("Free DMA Memory    : %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("Free 32 bit Memory : %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("Connect server to UART2 - 8N1 - 115200 baud\r\n\n");

  Terminal.setForegroundColor(Color::BrightGreen);

  // uncomment to specify terminal emulation
  //Terminal.setTerminalType(TermType::ANSILegacy);
}


void loop()
{
  // the job is done using UART interrupts
  vTaskSuspend(nullptr);
}
