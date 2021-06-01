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


/** @example VGA/LoopbackTerminal/LoopbackTerminal.ino
 * Loopback VT/ANSI Terminal
 */


#include "fabgl.h"




fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;


void print_info()
{
  Terminal.write("\e[37m* * FabGL - Loopback VT/ANSI Terminal\r\n");
  Terminal.write("\e[34m* * 2019-2021 by Fabrizio Di Vittorio - www.fabgl.com\e[32m\r\n\n");
  Terminal.printf("\e[32mScreen Size        :\e[33m %d x %d\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
  Terminal.printf("\e[32mTerminal Size      :\e[33m %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("\e[32mKeyboard           :\e[33m %s\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "Error");
  Terminal.printf("\e[32mFree DMA Memory    :\e[33m %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("\e[32mFree 32 bit Memory :\e[33m %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("\e[32mFree typing test - press ESC to introduce escape VT/ANSI codes\r\n\n");
}


void setup()
{
  Serial.begin(115200);

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  DisplayController.begin();
  DisplayController.setResolution();

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();

  print_info();

  Terminal.enableCursor(true);

  /*
  // do you want sound on key press? ;-)
  Terminal.onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (keyDown)
      Terminal.soundGenerator()->playSound(VICNoiseGenerator(), (*vk == VirtualKey::VK_RETURN ? 90 : 120), 10);;
      Terminal.soundGenerator()->playSound(SquareWaveformGenerator(), (*vk == VirtualKey::VK_RETURN ? 1000 : 2000), 4);;
  };
  */
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

