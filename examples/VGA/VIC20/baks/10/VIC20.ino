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
#include "fabutils.h"

#include "machine.h"

#include "progs/16k/tomthumbs.h"



void setup()
{
  Serial.begin(115200);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);
  Keyboard.setLayout(&fabgl::UKLayout);

  VGAController.begin();
  VGAController.setResolution(VGA_400x300_60Hz);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(20, -2);
}


static volatile int scycles = 0;
static volatile bool restart = false;


void vTimerCallback1SecExpired(xTimerHandle pxTimer)
{
  Serial.printf("%d\n", scycles / 5);
  restart = true;
}


void loop()
{
  TimerHandle_t xtimer = xTimerCreate("", pdMS_TO_TICKS(5000), pdTRUE, (void*)0, vTimerCallback1SecExpired);
  xTimerStart(xtimer, 0);

  Machine machine;
  machine.setRAMExpansion(RAM_16K);

  while (true) {
    int cycles = machine.run();

    if (restart) {
      scycles = cycles;
      restart = false;
    }
    else
      scycles += cycles;

    if (Keyboard.virtualKeyAvailable()) {
      bool keyDown;
      VirtualKey vk = Keyboard.getNextVirtualKey(&keyDown);
      switch (vk) {
        case VirtualKey::VK_F11:
          if (!keyDown)
            machine.reset();
          break;
        case VirtualKey::VK_F12:
          if (!keyDown)
            machine.loadPRG(tomthumbs, sizeof(tomthumbs), true);
          break;
        default:
          machine.setKeyboard(vk, keyDown);
          break;
      }
    }
  }
}

//  138000 -  140000
//  167000 -  170000
//  senza raddoppio pixel:
//  384000 -  386000
//  404000 -  407000
//  408000 -  411000
//  420000 -  425000
//  434000 -  438000
//  438000 -  443000
//  447000 -  451000
//  460000 -  465000
//  465000 -  469000
//  500000 -  505000 (fine del 15/08/2019)
//  650000 -  655000
//  673000 -  673000
//  677000 -  682000
//  704000 -  704000
//  713000 -  717000 (fine del 16/08/2019)
//  726000 -  726000
//  726000 -  731000
//  757000 -  757000 (fine del 17/08/2019)
//  762000 -  766000
//  801000 -  801000
//  806000 -  810000
// 1076000 - 1072000
// 1240000 - 1245000
// 1253000 - 1258000 (fine del 18/08/2019)
// 1258000 - 1262000
// 1284000 - 1289000
// 1298000 - 1302000 (game 952000-957000)


