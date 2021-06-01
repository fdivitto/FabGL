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


/*

  Credits:
     - Guido Lehwalder, https://github.com/guidol70 : using USB-serial Port for ANSI-Terminal (https://github.com/fdivitto/FabGL/issues/110)

*/

#include "fabgl.h"


fabgl::BitmappedDisplayController * DisplayController;
fabgl::PS2Controller                PS2Controller;
fabgl::Terminal                     Terminal;



// notes about GPIO 2 and 12
//    - GPIO2:  may cause problem on programming. GPIO2 must also be either left unconnected/floating, or driven Low, in order to enter the serial bootloader.
//              In normal boot mode (GPIO0 high), GPIO2 is ignored.
//    - GPIO12: should be avoided. It selects flash voltage. To use it disable GPIO12 detection setting efuses with:
//                    python espefuse.py --port /dev/cu.SLAB_USBtoUART set_flash_voltage 3.3V
//                       WARN!! Good for ESP32 with 3.3V voltage (ESP-WROOM-32). This will BRICK your ESP32 if the flash isn't 3.3V
//                       NOTE1: replace "/dev/cu.SLAB_USBtoUART" with your serial port
//                       NOTE2: espefuse.py is downloadable from https://github.com/espressif/esptool

// UART Pins for USB serial
#define UART_URX 3
#define UART_UTX 1

// UART Pins for normal serial Port
#define UART_SRX 34
#define UART_STX 2


#define RESET_PIN 12


#include "confdialog.h"


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\nReset\n\n"); // DEBUG ONLY

  // more stack is required for the UI (used inside Terminal.onVirtualKey)
  Terminal.keyboardReaderTaskStackSize = 3000;

  Terminal.inputQueueSize = 2048; // 2048 good to pass vttest

  preferences.begin("AnsiTerminal", false);

  ConfDialogApp::checkVersion();

  pinMode(RESET_PIN, INPUT);
  if (digitalRead(RESET_PIN) == 1)
    preferences.clear();

  // because mouse is optional, don't re-try if it is not found (to speed-up boot)
  fabgl::Mouse::quickCheckHardware();

  // keyboard configured on port 0, and optionally mouse on port 1
  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);

  ConfDialogApp::setupDisplay();

  ConfDialogApp::loadConfiguration();  

  //Terminal.setLogStream(Serial);  // debug only

  Terminal.clear();
  Terminal.enableCursor(true);

  if (ConfDialogApp::getBootInfo() == BOOTINFO_ENABLED) {
    Terminal.write("* *  FabGL - Serial Terminal                            * *\r\n");
    Terminal.write("* *  2019-2021 by Fabrizio Di Vittorio - www.fabgl.com  * *\r\n\n");
    Terminal.printf("Version            : %d.%d\r\n", TERMVERSION_MAJ, TERMVERSION_MIN);
    Terminal.printf("Screen Size        : %d x %d\r\n", DisplayController->getScreenWidth(), DisplayController->getScreenHeight());
    Terminal.printf("Terminal Size      : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
    Terminal.printf("Keyboard Layout    : %s\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? SupportedLayouts::names()[ConfDialogApp::getKbdLayoutIndex()] : "No Keyboard");
    //Terminal.printf("Mouse              : %s\r\n", PS2Controller.mouse()->isMouseAvailable() ? "Yes" : "No");
    Terminal.printf("Terminal Type      : %s\r\n", SupportedTerminals::names()[(int)ConfDialogApp::getTermType()]);
    //Terminal.printf("Free Memory        : %d bytes\r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    if (ConfDialogApp::getSerCtl() == SERCTL_ENABLED)
      Terminal.printf("Serial Port        : USB RX-Pin[%d] TX-Pin[%d]\r\n", UART_URX, UART_UTX);
    else
      Terminal.printf("Serial Port        : Serial RX-Pin[%d] TX-Pin[%d]\r\n", UART_SRX, UART_STX);
    Terminal.printf("Serial Parameters  : %s\r\n", ConfDialogApp::getSerParamStr());

    Terminal.write("\r\nPress F12 to change terminal configuration and CTRL-ALT-F12 to reset settings\r\n\n");
  } else if (ConfDialogApp::getBootInfo() == BOOTINFO_TEMPDISABLED) {
    preferences.putInt("BootInfo", BOOTINFO_ENABLED);
  }

  // onVirtualKey is triggered whenever a key is pressed or released
  Terminal.onVirtualKeyItem = [&](VirtualKeyItem * vkItem) {
    if (vkItem->vk == VirtualKey::VK_F12) {
      if (vkItem->CTRL && (vkItem->LALT || vkItem->RALT)) {
        Terminal.deactivate();
        preferences.clear();
        // show reboot dialog
        auto rebootApp = new RebootDialogApp;
        rebootApp->run(DisplayController);
      } else if (!vkItem->CTRL && !vkItem->LALT && !vkItem->RALT && !vkItem->down) {
        // releasing F12 key to open configuration dialog
        Terminal.deactivate();
        PS2Controller.mouse()->emptyQueue();  // avoid previous mouse movements to be showed on UI
        auto dlgApp = new ConfDialogApp;
        dlgApp->run(DisplayController);
        auto progToInstall = dlgApp->progToInstall;
        delete dlgApp;
        Terminal.keyboard()->emptyVirtualKeyQueue();
        Terminal.activate();
        // it has been requested to install a demo program?
        if (progToInstall > -1)
          installProgram(progToInstall);
        vkItem->vk = VirtualKey::VK_NONE;
      }
    }
  };

  // onUserSequence is triggered whenever a User Sequence has been received (ESC + '_#' ... '$'), where '...' is sent here
  Terminal.onUserSequence = [&](char const * seq) {
    // 'R': change resolution (for example: ESC + "_#R512x384x64$")
    for (int i = 0; i < RESOLUTIONS_COUNT; ++i)
      if (strcmp(RESOLUTIONS_CMDSTR[i], seq) == 0) {
        // found resolution string
        preferences.putInt("TempResolution", i);
        if (ConfDialogApp::getBootInfo() == BOOTINFO_ENABLED)
          preferences.putInt("BootInfo", BOOTINFO_TEMPDISABLED);
        ESP.restart();
      }
  };
}


void loop()
{
  // the job is done using UART interrupts
  vTaskDelete(NULL);
}
