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


#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"

#include "fabgl.h"

#include "altair.h"
#include "disks/cpm22_63k.h"



/* * * *  C O N F I G U R A T I O N  * * * */

// select one color configuration
#define USE_8_COLORS  0
#define USE_64_COLORS 1

// indicate VGA GPIOs to use for selected color configuration
#if USE_8_COLORS
  #define VGA_RED    GPIO_NUM_22
  #define VGA_GREEN  GPIO_NUM_21
  #define VGA_BLUE   GPIO_NUM_19
  #define VGA_HSYNC  GPIO_NUM_18
  #define VGA_VSYNC  GPIO_NUM_5
#elif USE_64_COLORS
  #define VGA_RED1   GPIO_NUM_22
  #define VGA_RED0   GPIO_NUM_21
  #define VGA_GREEN1 GPIO_NUM_19
  #define VGA_GREEN0 GPIO_NUM_18
  #define VGA_BLUE1  GPIO_NUM_5
  #define VGA_BLUE0  GPIO_NUM_4
  #define VGA_HSYNC  GPIO_NUM_23
  #define VGA_VSYNC  GPIO_NUM_15
#endif

// indicate PS/2 Keyboard GPIOs
#define PS2_PORT0_CLK GPIO_NUM_33
#define PS2_PORT0_DAT GPIO_NUM_32

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */



TerminalClass Terminal;


void setup()
{
  Serial.begin(115200); // DEBUG ONLY

  // GPIOs for keyboard CLK and DATA
  Keyboard.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);
  //Keyboard.setLayout(&fabgl::ItalianLayout);    // DEBUG ONLY

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  VGAController.setResolution(VGA_640x200_70Hz, 640, 200);
  //VGAController.shrinkScreen(5, 0);
  //VGAController.moveScreen(-1, 0);
  //VGAController.enableBackgroundPrimitiveExecution(false);

  // this speed-up display but may generate flickering
  VGAController.enableBackgroundPrimitiveExecution(false);

  Terminal.begin();
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..
  //Terminal.setLogStream(Serial);  // DEBUG ONLY

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();

  Terminal.enableCursor(true);

}


void loop()
{
  Terminal.write("Initializing...\r");

  /*
  // setup SPIFFS
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
  };
  esp_err_t FS_OK = esp_vfs_spiffs_register(&conf);
  //esp_spiffs_format(nullptr);
  */
  esp_vfs_fat_mount_config_t mount_config;
  mount_config.max_files = 4;
  mount_config.format_if_mount_failed = true;
  mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;
  static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
  esp_err_t FS_OK = esp_vfs_fat_spiflash_mount("/spiflash", nullptr, &mount_config, &s_wl_handle);


  Machine altair;

  // setup disk drives
  Mits88Disk diskDrive(&altair);
  //diskDrive.attachFile(0, "/Users/fdivitto/Sviluppo/TestEmuAltair/disks/CPM2.2-2.2b/cpm22_63k.dsk", true);
  //diskDrive.attachFile(1, "/Users/fdivitto/Sviluppo/TestEmuAltair/disks/CPM2.2-2.2b/cpm22_empty.dsk", false);
  diskDrive.attachReadOnlyBuffer(0, cpm22_63k_dsk);
  //diskDrive.attachFile(1, "/spiffs/empty1.dsk");
  diskDrive.attachFile(1, "/spiflash/empty1.dsk");

  // setup SIOs
  SIO SIO0(&altair, 0x00), SIO1(&altair, 0x10), SIO2(&altair, 0x12);
  //SIO0.attachTerminal(&Terminal);
  SIO1.attachTerminal(&Terminal);
  //SIO2.attachTerminal(&Terminal);

  // boot ROM
  altair.load(0xff00, AltairBootROM, 256);

  Terminal.write("\e[33m");
  Terminal.write("                        *******************************\r\n");
  Terminal.write("                        *    A L T A I R   8 8 0 0    *\r\n");
  Terminal.write("                        *******************************\r\n\n");
  Terminal.printf("\e[33mFree DMA Memory       :\e[32m %d bytes\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("\e[33mFree 32 bit Memory    :\e[32m %d bytes\r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  if (FS_OK == ESP_OK) {
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    Terminal.printf("\e[33mSPI Flash File System :\e[32m %d bytes used, %d bytes free\r\n", used, total);
  }

  altair.run(0xff00);
}

