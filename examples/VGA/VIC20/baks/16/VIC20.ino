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

#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_spiffs.h"
#include <stdio.h>

#include "fabgl.h"
#include "fabutils.h"

#include "machine.h"



// embedded (free) games
#include "progs/freegames/commodoredemo3.h"
#include "progs/freegames/pu239demo1.h"
#include "progs/freegames/tetrisdeluxe.h"
#include "progs/freegames/vic20revivaldemo.h"
#include "progs/freegames/blue_star.h"
#include "progs/freegames/omega_fury.h"
#include "progs/freegames/quikman.h"
#include "progs/freegames/splatform.h"
#include "progs/freegames/demo_birthday.h"
#include "progs/freegames/arukanoido.h"
#include "progs/freegames/demo_bad_scene_poetry.h"
#include "progs/freegames/demo_bah_bah_final.h"
#include "progs/freegames/demo_digit.h"
#include "progs/freegames/demo_frozen.h"
#include "progs/freegames/demo_happyhour.h"
#include "progs/freegames/demo_intro_2.h"
#include "progs/freegames/demo_plotter.h"
#include "progs/freegames/demo_rasterbar.h"
#include "progs/freegames/demopart.h"
#include "progs/freegames/dragonwing.h"
#include "progs/freegames/kikstart.h"
#include "progs/freegames/kweepoutmc.h"
#include "progs/freegames/lunar_blitz.h"
#include "progs/freegames/nibbler.h"
#include "progs/freegames/pitfall.h"
#include "progs/freegames/pong.h"
#include "progs/freegames/pooyan.h"
#include "progs/freegames/popeye.h"
#include "progs/freegames/pulse.h"
#include "progs/freegames/screen_demo.h"
#include "progs/freegames/spikes.h"
#include "progs/freegames/tank_battalion.h"
#include "progs/freegames/tetris_plus.h"
#include "progs/freegames/vicmine.h"




struct EmbeddedProgDef {
  char const *    filename;
  uint8_t const * data;
  int             size;
};


const EmbeddedProgDef embeddedProgs[] = {
  // to test, some dont work!
  { arukanoido_filename, arukanoido_prg, sizeof(arukanoido_prg) },
  { demo_bad_scene_poetry_filename, demo_bad_scene_poetry_prg, sizeof(demo_bad_scene_poetry_prg) },
  { demo_bah_bah_final_filename, demo_bah_bah_final_prg, sizeof(demo_bah_bah_final_prg) },
  { demo_digit_filename, demo_digit_prg, sizeof(demo_digit_prg) },
  { demo_frozen_filename, demo_frozen_prg, sizeof(demo_frozen_prg) },
  { demo_happyhour_filename, demo_happyhour_prg, sizeof(demo_happyhour_prg) },
  { demo_intro_2_filename, demo_intro_2_prg, sizeof(demo_intro_2_prg) },
  { demo_plotter_filename, demo_plotter_prg, sizeof(demo_plotter_prg) },
  { demo_rasterbar_filename, demo_rasterbar_prg, sizeof(demo_rasterbar_prg) },
  { demopart_filename, demopart_prg, sizeof(demopart_prg) },
  { dragonwing_filename, dragonwing_prg, sizeof(dragonwing_prg) },
  { kikstart_filename, kikstart_prg, sizeof(kikstart_prg) },
  { kweepoutmc_filename, kweepoutmc_prg, sizeof(kweepoutmc_prg) },
  { lunar_blitz_filename, lunar_blitz_prg, sizeof(lunar_blitz_prg) },
  { nibbler_filename, nibbler_prg, sizeof(nibbler_prg) },
  { pitfall_filename, pitfall_prg, sizeof(pitfall_prg) },
  { pong_filename, pong_prg, sizeof(pong_prg) },
  { pooyan_filename, pooyan_prg, sizeof(pooyan_prg) },
  { popeye_filename, popeye_prg, sizeof(popeye_prg) },
  { pulse_filename, pulse_prg, sizeof(pulse_prg) },
  { screen_demo_filename, screen_demo_prg, sizeof(screen_demo_prg) },
  { spikes_filename, spikes_prg, sizeof(spikes_prg) },
  { tank_battalion_filename, tank_battalion_prg, sizeof(tank_battalion_prg) },
  { tetris_plus_filename, tetris_plus_prg, sizeof(tetris_plus_prg) },
  { vicmine_filename, vicmine_prg, sizeof(vicmine_prg) },
  { blue_star_filename, blue_star_prg, sizeof(blue_star_prg) },
  { commodoredemo3_filename, commodoredemo3_prg, sizeof(commodoredemo3_prg) },
  { demo_birthday_filename, demo_birthday_prg, sizeof(demo_birthday_prg) },
  { omega_fury_filename, omega_fury_prg, sizeof(omega_fury_prg) },
  { pu239demo1_filename, pu239demo1_prg, sizeof(pu239demo1_prg) },
  { splatform_filename, splatform_prg, sizeof(splatform_prg) },
  { tetrisdeluxe_filename, tetrisdeluxe_prg, sizeof(tetrisdeluxe_prg) },
  { vic20revivaldemo_filename, vic20revivaldemo_prg, sizeof(vic20revivaldemo_prg) },
  { quikman_filename, quikman_prg, sizeof(quikman_prg) },

};



// DEBUG
static volatile int scycles = 0;
static volatile bool restart = false;


// DEBUG
void vTimerCallback1SecExpired(xTimerHandle pxTimer)
{
  Serial.printf("%d - ", scycles / 5);
  Serial.printf("Free memory (total, min, largest): %d, %d, %d\n", heap_caps_get_free_size(0),
                                                                   heap_caps_get_minimum_free_size(0),
                                                                   heap_caps_get_largest_free_block(0));
  restart = true;
}


// Main machine
Machine machine;


void initSPIFFS()
{
  static bool initDone = false;
  if (!initDone) {
    // setup SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,
        .max_files              = 4,
        .format_if_mount_failed = true
    };
    fabgl::suspendInterrupts();
    esp_vfs_spiffs_register(&conf);
    fabgl::resumeInterrupts();
  }
}


// copies embedded programs into SPIFFS. TODO: in future just copy a reference to avoid data duplication!
// configuration given by filename:
//    PROGNAME[_EXP][_ADDRESS].EXT
//      PROGNAME : program name (not included extension)
//      EXP      : expansion ("3K", "8K", "16K", "24K", "27K", "32K", "35K")
//      ADDRESS  : hex address ("A000", "2000", "6000"...), A0000 is optional
//      EXT      : "CRT" or "PRG"
void copyEmbeddedPrograms()
{
  char const * EMBDIR = "Free Games";  // name of embedded programs directory

  auto dir = FileBrowser();
  dir.setDirectory("/spiffs");
  if (!dir.exists(EMBDIR)) {
    // there isn't a EMBDIR folder, let's create and populate it
    dir.makeDirectory(EMBDIR);
    dir.changeDirectory(EMBDIR);
    for (int i = 0; i < sizeof(embeddedProgs) / sizeof(EmbeddedProgDef); ++i) {
      int fullpathlen = dir.getFullPath(embeddedProgs[i].filename);
      char fullpath[fullpathlen];
      dir.getFullPath(embeddedProgs[i].filename, fullpath, fullpathlen);
      FILE * f = fopen(fullpath, "wb");
      if (f) {
        fwrite(embeddedProgs[i].data, embeddedProgs[i].size, 1, f);
        fclose(f);
      }
    }
  }
}


// this is the UI app that shows the emulator menu
class Menu : public uiApp {

  uiFileBrowser * fileBrowser;
  uiComboBox * RAMExpComboBox;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB(3, 3, 3);

    // programs list
    fileBrowser = new uiFileBrowser(rootWindow(), Point(5, 10), Size(140, 310));
    fileBrowser->listBoxStyle().backgroundColor = RGB(2, 2, 0);
    fileBrowser->listBoxStyle().focusedBackgroundColor = RGB(3, 3, 0);
    fileBrowser->setDirectory("/spiffs");
    /*
    // these trigger "run" pressing ENTER or DBL CLICK on the listbox
    fileBrowser->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_RETURN)
        quit(0);
    };
    fileBrowser->onDblClick = [&]() {
      quit(0);
    };
    */

    // "run" button - run selected item
    auto runButton = new uiButton(rootWindow(), "RUN", Point(150, 10), Size(45, 25));
    runButton->onClick = [&]() { quit(0); };

    // "close" button - quit without action
    auto cancelButton = new uiButton(rootWindow(), "Close", Point(205, 10), Size(45, 25));
    cancelButton->onClick = [&]() { quit(-1); };

    // "reset" button - unload and reset
    auto resetButton = new uiButton(rootWindow(), "Reset", Point(150, 40), Size(45, 25));
    resetButton->onClick = [&]() { quit(-2); };

    // joystick emulation options
    int y = 80;
    new uiLabel(rootWindow(), "Joystick:", Point(150, y));
    new uiLabel(rootWindow(), "None", Point(185, y + 20));
    auto radioJNone = new uiCheckBox(rootWindow(), Point(160, y + 20), Size(16, 16), uiCheckBoxKind::RadioButton);
    new uiLabel(rootWindow(), "Cursor Keys", Point(185, y + 40));
    auto radioJCurs = new uiCheckBox(rootWindow(), Point(160, y + 40), Size(16, 16), uiCheckBoxKind::RadioButton);
    new uiLabel(rootWindow(), "Mouse", Point(185, y + 60));
    auto radioJMous = new uiCheckBox(rootWindow(), Point(160, y + 60), Size(16, 16), uiCheckBoxKind::RadioButton);
    radioJNone->setGroupIndex(1);
    radioJCurs->setGroupIndex(1);
    radioJMous->setGroupIndex(1);
    radioJNone->setChecked(machine.joyEmu() == JE_None);
    radioJCurs->setChecked(machine.joyEmu() == JE_CursorKeys);
    radioJMous->setChecked(machine.joyEmu() == JE_Mouse);
    radioJNone->onChange = [&]() { machine.setJoyEmu(JE_None); };
    radioJCurs->onChange = [&]() { machine.setJoyEmu(JE_CursorKeys); };
    radioJMous->onChange = [&]() { machine.setJoyEmu(JE_Mouse); };

    // RAM expansion options
    y = 160;
    new uiLabel(rootWindow(), "RAM Expansion:", Point(150, y));
    RAMExpComboBox = new uiComboBox(rootWindow(), Point(160, y + 20), Size(90, 25), 130);
    RAMExpComboBox->items().append("Unexpanded");
    RAMExpComboBox->items().append("3K");
    RAMExpComboBox->items().append("8K");
    RAMExpComboBox->items().append("16K");
    RAMExpComboBox->items().append("24K");
    RAMExpComboBox->items().append("27K (24K+3K)");
    RAMExpComboBox->items().append("32K");
    RAMExpComboBox->items().append("35K (32K+3K)");
    RAMExpComboBox->selectItem((int)machine.RAMExpansion());
    RAMExpComboBox->onChange = [&]() {
      machine.setRAMExpansion((RAMExpansionOption)(RAMExpComboBox->selectedItem()));
    };

    // process ESC and F12: boths quit without action
    rootWindow()->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_ESCAPE || key.VK == VirtualKey::VK_F12)
        quit(-1);
    };

    // focus on programs list
    setFocusedWindow(fileBrowser);
  }

};



///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup()
{
  Serial.begin(115200); // for debug purposes

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);
  Keyboard.setLayout(&fabgl::UKLayout);

  VGAController.begin();
  VGAController.setResolution(VGA_256x384_60Hz);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(20, -2);

  Canvas.selectFont(Canvas.getPresetFontInfo(32, 48));

  Canvas.clear();
  Canvas.drawText(25, 10, "Initializing SPIFFS...");
  Canvas.waitCompletion();
  initSPIFFS();

  Canvas.drawText(25, 20, "Copying embedded programs...");
  Canvas.waitCompletion();
  copyEmbeddedPrograms();

  Canvas.clear();
}


void loop()
{

  // DEBUG
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

    // read keyboard
    if (Keyboard.virtualKeyAvailable()) {
      bool keyDown;
      VirtualKey vk = Keyboard.getNextVirtualKey(&keyDown);
      switch (vk) {

        // F12 - open emulation menu
        case VirtualKey::VK_F12:
          if (!keyDown) {
            machine.VIC().enableAudio(false);
            Menu menu;
            int exitCode = menu.run();
            Keyboard.emptyVirtualKeyQueue();
            machine.VIC().enableAudio(true);
            Canvas.setBrushColor(0, 0, 0);
            Canvas.clear();
            if (exitCode >= 0) {
              // run selected program (PRG or CRT)
              /*
              progIdx = exitCode;
              Prog const * prog = getCurrentProgList() + progIdx;
              machine.removeCartridges();
              machine.setRAMExpansion(prog->expansion);
              switch (prog->cont) {
                case PRG:
                  machine.loadPRG(prog->data, prog->size, true, true);
                  break;
                case CRT:
                  machine.setCartridge(prog->data, prog->size, true, prog->crtAddr);
                  if (prog->data2)
                    machine.setCartridge(prog->data2, prog->size2, false, prog->crtAddr2);
                  break;
              }
              */
            } else if (exitCode == -2) {
              // unload and reset
              machine.removeCartridges();
              machine.reset();
            }
          }
          break;

        // other keys
        default:
          // send to emulated machine
          machine.setKeyboard(vk, keyDown);
          break;
      }
    }

  }
}

