/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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


 /* Instructions:

    - to run this application you need an ESP32 with PSRAM installed and an SD-CARD slot (ie TTGO VGA32 v1.4)
    - open this with Arduino and make sure PSRAM is DISABLED
    - copy the boot floppy disk image (fd.img) into the SD-CARD (https://github.com/fdivitto/FabGL/blob/master/examples/VGA/PCEmulator/fd.img)
    - compile and upload the sketch


  An hard disk may be added. It must be named "hd.img".


  Notes about software compatibility:
    - Windows 3.0a, set with "Hercules" video adapter, keyboard type must be "Enhanced 101 or 102 key US and Non US"

 */


#include "esp32-hal-psram.h"
extern "C" {
#include "esp_spiram.h"
}


#include "fabgl.h"

#include "machine.h"


Machine machine;


void setup()
{
  Serial.begin(115200); delay(500); printf("\n\n\nReset\n\n");// DEBUG ONLY

  disableCore0WDT();
  delay(100); // experienced crashes without this delay!
  disableCore1WDT();

  // we need PSRAM for this app, but we will handle it manually, so please DO NOT enable PSRAM on your development env
  #ifdef BOARD_HAS_PSRAM
  printf("Please disable PSRAM!\n");
  while (1);
  #endif

  // note: we use just 2MB of PSRAM so the infamous PSRAM bug should not happen. But to avoid gcc compiler hack (-mfix-esp32-psram-cache-issue)
  // we enable PSRAM at runtime, otherwise the hack slows down CPU too much (PSRAM_HACK is no more required).
  if (esp_spiram_init() != ESP_OK) {
    printf("This app requires PSRAM!!\n");
    while (1);
  }
  esp_spiram_init_cache();

  if (!FileBrowser::mountSDCard(false, "/SD", 8)) {
    printf("This app requires an SD-CARD\n");
    while (1);
  }

  // setup time (@TODO: use WiFi and sntp)
  auto tm = (struct tm){ .tm_sec  = 0, .tm_min  = 0, .tm_hour = 8, .tm_mday = 14, .tm_mon  = 7, .tm_year = 84 };
  auto now = (timeval){ .tv_sec = mktime(&tm) };
  settimeofday(&now, nullptr);

  machine.run();
}



#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
namespace fabgl {
  extern volatile uint64_t s_vgapalctrlcycles;
}
using fabgl::s_vgapalctrlcycles;
#endif



void loop()
{

#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  static uint32_t tcpu = 0, s1 = 0, count = 0;
  tcpu = machine.ticksCounter();
  s_vgapalctrlcycles = 0;
  s1 = fabgl::getCycleCount();
  delay(1000);
  printf("%d\tCPU: %d", count, machine.ticksCounter() - tcpu);
  printf("   Graph: %lld / %d   (%d%%)\n", s_vgapalctrlcycles, fabgl::getCycleCount() - s1, (int)((double)s_vgapalctrlcycles/240000000*100));
  ++count;
#else

  vTaskDelete(NULL);

#endif
}


