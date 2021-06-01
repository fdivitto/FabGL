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
 * Optional SD Card connections:
 *   MISO => GPIO 16  (2 for PICO-D4)
 *   MOSI => GPIO 17  (12 for PICO-D4)
 *   CLK  => GPIO 14
 *   CS   => GPIO 13
 *
 * To change above assignment fill other paramaters of FileBrowser::mountSDCard().
 */





#include "fabgl.h"
#include "src/supervisor.h"
#include "src/programs.h"



// Flash and SDCard configuration
#define FORMAT_ON_FAIL     true
#define SPIFFS_MOUNT_PATH  "/flash"
#define SDCARD_MOUNT_PATH  "/SD"
#define MAXFILES           6     // SDCARD: each file takes about 4K of RAM!



// globals

fabgl::VGA16Controller   DisplayController;
//fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;

Supervisor supervisor(&DisplayController);



// base path (can be SPIFFS_MOUNT_PATH or SDCARD_MOUNT_PATH depending from what was successfully mounted first)
char const * basepath = nullptr;


String driveA_path;
String driveB_path;



void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\r\nReset\r\n");

  disableCore0WDT();
  disableCore1WDT();

  // Reduces some defaults to save RAM...
  fabgl::VGAController::queueSize                    = 128;
  fabgl::Terminal::inputQueueSize                    = 32;
  fabgl::Terminal::inputConsumerTaskStackSize        = 1300;
  fabgl::Terminal::keyboardReaderTaskStackSize       = 800;
  fabgl::Keyboard::scancodeToVirtualKeyTaskStackSize = 1500;

  // because mouse is optional, don't re-try if it is not found (to speed-up boot)
  fabgl::Mouse::quickCheckHardware();

  // keyboard configured on port 0, and optionally mouse on port 1
  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);

  // setup VGA (default configuration with 64 colors)

  DisplayController.begin();
  DisplayController.setResolution(VGA_640x480_60Hz);      // good when using VGA16Controller

  auto term = new fabgl::Terminal;
  term->begin(&DisplayController);
  term->connectLocally();      // to use Terminal.read(), available(), etc..
  term->clear();
  term->write("Initializing...\r");
  term->flush();

  if (FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH, MAXFILES))
    basepath = SDCARD_MOUNT_PATH;
  else if (FileBrowser::mountSPIFFS(FORMAT_ON_FAIL, SPIFFS_MOUNT_PATH, MAXFILES))
    basepath = SPIFFS_MOUNT_PATH;

  // uncomment to format SPIFFS or SD card
  //FileBrowser::format(fabgl::DriveType::SPIFFS, 0);
  //FileBrowser::format(fabgl::DriveType::SDCard, 0);

  driveA_path = String(basepath) + String("/driveA");
  driveB_path = String(basepath) + String("/driveB");

  FileBrowser fb;

  fb.setDirectory(basepath);
  if (!fb.exists("driveA", false)) {

    fb.makeDirectory("driveA");

    // create default programs
    for (int i = 0; i < sizeof(programs) / sizeof(Program); ++i) {
      term->printf("Creating %s\\%s\r\n", programs[i].path, programs[i].filename);
      term->flush();
      // check directory
      fb.setDirectory(driveA_path.c_str());
      if (!fb.exists(programs[i].path, false))
        fb.makeDirectory(programs[i].path);
      fb.changeDirectory(programs[i].path);
      // copy file
      FILE * f = fb.openFile(programs[i].filename, "wb");
      if (f) {
        fwrite(programs[i].data, 1, programs[i].size, f);
        fclose(f);
      } else {
        term->write("  FAILED!\r\n");
      }
    }

  }

  fb.setDirectory(basepath);
  if (!fb.exists("driveB", false))
    fb.makeDirectory("driveB");

  delete term;

  supervisor.onNewSession = [&](HAL * hal) {

    hal->mountDrive(0, driveA_path.c_str());
    hal->mountDrive(1, driveB_path.c_str());

    // Serial (USB) ->  CP/M AUX1
    hal->setSerial(0, &Serial);
  };

  // change terminal when pressing F1..F12
  PS2Controller.keyboard()->onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk >= VirtualKey::VK_F1 && *vk <= VirtualKey::VK_F12) {
      if (keyDown) {
        supervisor.activateSession((int)*vk - VirtualKey::VK_F1);
      }
      *vk = VirtualKey::VK_NONE;
    }
  };



}


void loop()
{
  supervisor.activateSession(0);
  vTaskDelete(NULL);
}






