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

#include "esp_task_wdt.h"

#include "fabgl.h"
#include "fabutils.h"

#include "machine.h"


Machine machine;

void setup()
{
  Serial.begin(115200);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

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

  while (true) {
    int cycles = machine.run();

    if (restart) {
      scycles = cycles;
      restart = false;
    }
    else
      scycles += cycles;
  }
}

// 138000-140000
// 167000-170000
// senza raddoppio pixel:
// 384000-386000
// 404000-407000
// 408000-411000
// 420000-425000
// 434000-438000
// 438000-443000

