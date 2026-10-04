/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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
 * FabGL as plain ESP-IDF component (no Arduino core).
 *
 * VGA terminal with PS/2 keyboard echo and a startup beep. Free memory is reported on the serial console
 * and on screen, so it is easy to compare different IDF versions.
 *
 * Wiring is the default one of FabGL VGA boards (see "fabgl GPIOs assignment.txt"):
 *   VGA      : R = 22, 21  G = 19, 18  B = 5, 4  HSync = 23  VSync = 15
 *   Keyboard : CLK = 33  DAT = 32
 *   Audio    : 25
 */


#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#include "fabgl.h"


fabgl::VGA16Controller DisplayController;
fabgl::PS2Controller   PS2Controller;
fabgl::Terminal        Terminal;
fabgl::SoundGenerator  soundGenerator;


static void printMemory(char const * stage)
{
  printf("[%5d ms, %s] 8bit: %u (largest %u)  DMA: %u (largest %u)  32bit: %u (largest %u)\n", (int) (esp_timer_get_time() / 1000), stage,
         (unsigned) heap_caps_get_free_size(MALLOC_CAP_8BIT),  (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
         (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA),   (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
         (unsigned) heap_caps_get_free_size(MALLOC_CAP_32BIT), (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
}


extern "C" void app_main()
{
  printMemory("boot");

  PS2Controller.begin(PS2Preset::KeyboardPort0);
  printMemory("after PS/2");
  printf("Keyboard: %s\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "not found");

  DisplayController.begin();
  DisplayController.setResolution(VGA_640x480_60Hz);
  printMemory("after VGA 640x480x16");

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..
  Terminal.enableCursor(true);
  printMemory("after terminal");

  Terminal.write("\e[40;92m\e[2J\e[1;1H");
  Terminal.printf("FabGL on ESP-IDF %s\r\n\n", esp_get_idf_version());
  Terminal.printf("\e[97mScreen size      :\e[92m %d x %d\r\n", DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight());
  Terminal.printf("\e[97mTerminal size    :\e[92m %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  Terminal.printf("\e[97mKeyboard         :\e[92m %s\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "not found");
  Terminal.printf("\e[97mFree DMA memory  :\e[92m %d\r\n", (int) heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("\e[97mFree 32bit memory:\e[92m %d\r\n\n", (int) heap_caps_get_free_size(MALLOC_CAP_32BIT));
  Terminal.write("\e[93mType something (echoed here and on the serial console):\e[97m\r\n");

  // startup beep
  soundGenerator.playSound(SineWaveformGenerator(), 880, 200);

  for (;;) {
    if (Terminal.available()) {
      char c = Terminal.read();
      if (c == '\r')
        Terminal.write("\r\n");
      else
        Terminal.write(c);
      printf("key 0x%02X\n", c);
    } else
      vTaskDelay(pdMS_TO_TICKS(10));
  }
}
