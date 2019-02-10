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


void printHelp()
{
  Serial.printf("\n\nPS/2 Keyboard Studio\n");
  Serial.printf("Chip Revision: %d   Chip Frequency: %d MHz\n", ESP.getChipRevision(), ESP.getCpuFreqMHz());

  printInfo();

  Serial.printf("Commands:\n");
  Serial.printf("  1 = US Layout  2 = UK Layout     3 = DE Layout   4 = IT Layout\n");
  Serial.printf("  r = Reset      s = Scancode Mode a = VirtualKey/ASCII Mode\n");
  Serial.printf("  l = Test LEDs\n");
  Serial.printf("Various:\n");
  Serial.printf("  h = Print This help\n\n");
}


void printInfo()
{
  if (Keyboard.isKeyboardAvailable()) {
    Serial.write("Device Id = ");
    switch (Keyboard.identify()) {
      case fabgl::OldATKeyboard:
        Serial.write("\"Old AT Keyboard\"");
        break;
      case fabgl::MouseStandard:
        Serial.write("\"Standard Mouse\"");
        break;
      case fabgl::MouseWithScrollWheel:
        Serial.write("\"Mouse with scroll wheel\"");
        break;
      case fabgl::Mouse5Buttons:
        Serial.write("\"5 Buttons Mouse\"");
        break;
      case fabgl::MF2KeyboardWithTranslation:
        Serial.write("\"MF2 Keyboard with translation\"");
        break;
      case fabgl::M2Keyboard:
        Serial.write("\"MF2 keyboard\"");
        break;
      default:
        Serial.write("\"Unknown\"");
        break;
    }
    Serial.printf("  Keyboard Layout: \"%s\"\n", Keyboard.getLayout()->name);
  } else
    Serial.write("Keyboard Error!\n");
}



void setup()
{
  Serial.begin(115200);
  delay(500);  // avoid garbage into the UART
  Serial.write("\n\nReset\n");

  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);  // clk, dat

  printHelp();
}




void loop()
{
  static char mode = 'a';
  //static fabgl::VirtualKey lastvk = fabgl::VK_NONE; // avoid to repeat last vk

  if (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        printHelp();
        break;
      case '1':
        Keyboard.setLayout(&fabgl::USLayout);
        printInfo();
        break;
      case '2':
        Keyboard.setLayout(&fabgl::UKLayout);
        printInfo();
        break;
      case '3':
        Keyboard.setLayout(&fabgl::GermanLayout);
        printInfo();
        break;
      case '4':
        Keyboard.setLayout(&fabgl::ItalianLayout);
        printInfo();
        break;
      case 'r':
        Keyboard.reset();
        printInfo();
        break;
      case 's':
      case 'a':
        mode = c;
        Keyboard.suspendVirtualKeyGeneration(c == 's');
        break;
      case 'l':
      {
        for (int i = 0; i < 8; ++i) {
          Keyboard.setLEDs(i & 1, i & 2, i & 4);
          delay(1000);
        }
        delay(2000);
        if (Keyboard.setLEDs(0, 0, 0))
          Serial.write("OK\n");
        break;
      }
    }
  }

  if (mode == 's' && Keyboard.scancodeAvailable()) {
    // scancode mode (show scancodes)
    Serial.printf("Scancode = 0x%02X\n", Keyboard.getNextScancode());
  } else if (Keyboard.virtualKeyAvailable()) {
    // ascii mode (show ASCII and VirtualKeys)
    bool down;
    auto vk = Keyboard.getNextVirtualKey(&down);
    //if (vk != lastvk) {
      Serial.printf("VirtualKey = %s", Keyboard.virtualKeyToString(vk));
      int c = Keyboard.virtualKeyToASCII(vk);
      if (c > -1) {
        Serial.printf("  ASCII = 0x%02X   ", c);
        if (c >= ' ')
          Serial.write(c);
      }
      if (!down)
        Serial.write(" UP");
      Serial.write('\n');
      //lastvk = down ? vk : fabgl::VK_NONE;
    //}
  }

}
