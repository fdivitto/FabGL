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


enum ProgContainer { PRG, CRT };

struct Prog {
  ProgContainer     cont;
  RAMExpansion      expansion;
  const char *      desc;
  const uint8_t *   data;
  int               size;
  int               crtAddr;
  // multipart cartridges (part2)
  const uint8_t *   data2;
  int               size2;
  int               crtAddr2;
};



#include "progs/noexp/blitz.h"
#include "progs/noexp/commodoredemo3.h"
#include "progs/noexp/vic20revivaldemo.h"
#include "progs/noexp/breakout.h"
#include "progs/noexp/abductor.h"
#include "progs/noexp/alien_attack.h"
#include "progs/noexp/alien_blitz.h"
#include "progs/noexp/astro_nell.h"
#include "progs/noexp/block_buster.h"
#include "progs/noexp/blue_star.h"
#include "progs/noexp/bomber.h"
#include "progs/noexp/car_race.h"

#include "progs/3k/tetrisdeluxe.h"
#include "progs/3k/pu239demo1.h"

#include "progs/8k/matrix.h"

#include "progs/16k/tomthumbs.h"
#include "progs/16k/cosmicfirebirds.h"

#include "progs/CRT/moleattack.h"
#include "progs/CRT/pacman.h"
#include "progs/CRT/polaris.h"
#include "progs/CRT/omegarace.h"
#include "progs/CRT/aggressor.h"
#include "progs/CRT/amok.h"
#include "progs/CRT/applepanic.h"
#include "progs/CRT/arcadia.h"
#include "progs/CRT/attack_of_the_mutant_camels.h"
#include "progs/CRT/avenger.h"
#include "progs/CRT/choplifter.h"
#include "progs/CRT/crossfire.h"
#include "progs/CRT/cyclon.h"
#include "progs/CRT/d_fuse.h"
#include "progs/CRT/demonattack.h"
#include "progs/CRT/dragonfire.h"
#include "progs/CRT/frogger.h"
#include "progs/CRT/galaxian.h"
#include "progs/CRT/garden_wars.h"
#include "progs/CRT/jelly_monsters_v1.h"
#include "progs/CRT/jupiter_lander.h"
#include "progs/CRT/key_quest.h"
#include "progs/CRT/lazer_zone.h"
#include "progs/CRT/midnight_drive.h"
#include "progs/CRT/miner_2049.h"
#include "progs/CRT/outworld.h"
#include "progs/CRT/protector.h"
#include "progs/CRT/quackers.h"
#include "progs/CRT/river_rescue.h"
#include "progs/CRT/robot_panic.h"
#include "progs/CRT/satellite_patrol.h"
#include "progs/CRT/satellites_and_meteorites.h"
#include "progs/CRT/scorpion.h"
#include "progs/CRT/serpentine.h"
#include "progs/CRT/shamus.h"
#include "progs/CRT/spiders_of_mars.h"
#include "progs/CRT/star_battle.h"
#include "progs/CRT/star_trek_sos.h"
#include "progs/CRT/sub_chase.h"
#include "progs/CRT/super_amok.h"
#include "progs/CRT/super_smash.h"
#include "progs/CRT/terraguard.h"
#include "progs/CRT/threshold.h"
#include "progs/CRT/vic1923_gorf.h"
#include "progs/CRT/witch_way.h"



#include "progs/multiCRT/battlezone_60.h"
#include "progs/multiCRT/battlezone_a0.h"
#include "progs/multiCRT/centipede_20.h"
#include "progs/multiCRT/centipede_a0.h"
#include "progs/multiCRT/donkey_kong_20.h"
#include "progs/multiCRT/donkey_kong_a0.h"
#include "progs/multiCRT/pole_position_60.h"
#include "progs/multiCRT/pole_position_a0.h"




const Prog progs[] = {


  { PRG, RAM_unexp, "Abductor", abductor, sizeof(abductor), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Aggressor", aggressor, sizeof(aggressor), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Alien Attack", alien_attack, sizeof(alien_attack), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Alien Blitz", alien_blitz, sizeof(alien_blitz), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Amok", amok, sizeof(amok), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Apple Panic", applepanic, sizeof(applepanic), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Arcadia", arcadia, sizeof(arcadia), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Astro Nell", astro_nell, sizeof(astro_nell), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Attack of the Mutant Camels", attack_of_the_mutant_camels, sizeof(attack_of_the_mutant_camels), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Avenger", avenger, sizeof(avenger), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Battle Zone", battlezone_60, sizeof(battlezone_60), 0x6000, battlezone_a0, sizeof(battlezone_a0), 0xa000 },
  { PRG, RAM_unexp, "Breakout", breakout, sizeof(breakout), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Blitz", blitz, sizeof(blitz), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Block Buster", block_buster, sizeof(block_buster), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Blue Star", blue_star, sizeof(blue_star), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Bomber", bomber, sizeof(bomber), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Car Race", car_race, sizeof(car_race), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Centipede", centipede_20, sizeof(centipede_20), 0x2000, centipede_a0, sizeof(centipede_a0), 0xa000 },
  { CRT, RAM_unexp, "Choplifter", choplifter, sizeof(choplifter), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "Commodore Demo",commodoredemo3, sizeof(commodoredemo3), -1, nullptr, 0, -1 },
  { PRG, RAM_16K,   "Cosmic Firebirds", cosmicfirebirds, sizeof(cosmicfirebirds), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Crossfire", crossfire, sizeof(crossfire), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Cyclon", cyclon, sizeof(cyclon), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "d'fuse", d_fuse, sizeof(d_fuse), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Demon Attack", demonattack, sizeof(demonattack), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Donkey Kong", donkey_kong_20, sizeof(donkey_kong_20), 0x2000, donkey_kong_a0, sizeof(donkey_kong_a0), 0xa000 },
  { CRT, RAM_unexp, "Dragonfire", dragonfire, sizeof(dragonfire), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Frogger", frogger, sizeof(frogger), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Galaxian", galaxian, sizeof(galaxian), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Garden Wars", garden_wars, sizeof(garden_wars), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Jelly Monsters v1", jelly_monsters_v1, sizeof(jelly_monsters_v1), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Jupiter Lander", jupiter_lander, sizeof(jupiter_lander), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Key Quest", key_quest, sizeof(key_quest), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Lazer Zone", lazer_zone, sizeof(lazer_zone), -1, nullptr, 0, -1 },
  //{ PRG, RAM_8K,    "Matrix", matrix, sizeof(matrix), -1, nullptr, 0, -1 },
  matrix,
  { CRT, RAM_unexp, "Midnight Drive", midnight_drive, sizeof(midnight_drive), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Miner 2049", miner_2049, sizeof(miner_2049), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Mole Attack", moleattack, sizeof(moleattack), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Omega Rage", omegarace, sizeof(omegarace), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Outworld", outworld, sizeof(outworld), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Pac-Man", pacman, sizeof(pacman), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Polaris", polaris, sizeof(polaris), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Pole Position", pole_position_60, sizeof(pole_position_60), 0x6000, pole_position_a0, sizeof(pole_position_a0), 0xa000 },
  { CRT, RAM_unexp, "Protector", protector, sizeof(protector), -1, nullptr, 0, -1 },
  { PRG, RAM_3K,    "PU239 Demo 1", pu239demo1, sizeof(pu239demo1), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Quackers", quackers, sizeof(quackers), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "River Rescue", river_rescue, sizeof(river_rescue), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Robot Panic", robot_panic, sizeof(robot_panic), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Satellite Patrol", satellite_patrol, sizeof(satellite_patrol), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Satellites and Meteorites", satellites_and_meteorites, sizeof(satellites_and_meteorites), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Scorpion", scorpion, sizeof(scorpion), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Serpentine", serpentine, sizeof(serpentine), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Shamus", shamus, sizeof(shamus), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Spiders of Mars", spiders_of_mars, sizeof(spiders_of_mars), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Star Battle", star_battle, sizeof(star_battle), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Star Trek SOS", star_trek_sos, sizeof(star_trek_sos), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Sub Chase", sub_chase, sizeof(sub_chase), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Super Amok", super_amok, sizeof(super_amok), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Super Smash", super_smash, sizeof(super_smash), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Terraguard", terraguard, sizeof(terraguard), -1, nullptr, 0, -1 },
  { PRG, RAM_3K,    "Tetris Deluxe", tetrisdeluxe, sizeof(tetrisdeluxe), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Threshold", threshold, sizeof(threshold), -1, nullptr, 0, -1 },
  { PRG, RAM_16K,   "Tom Thumbs", tomthumbs, sizeof(tomthumbs), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "VIC1923 Gorf", vic1923_gorf, sizeof(vic1923_gorf), -1, nullptr, 0, -1 },
  { PRG, RAM_unexp, "VIC20 Revival Demo", vic20revivaldemo, sizeof(vic20revivaldemo), -1, nullptr, 0, -1 },
  { CRT, RAM_unexp, "Witch Way", witch_way, sizeof(witch_way), -1, nullptr, 0, -1 },
};


void setup()
{
  Serial.begin(115200);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);
  Keyboard.setLayout(&fabgl::UKLayout);

  VGAController.begin();
  //VGAController.setResolution(VGA_400x300_60Hz);
  VGAController.setResolution(VGA_256x384_60Hz);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(20, -2);
}


static volatile int scycles = 0;
static volatile bool restart = false;


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


// last selected prog index
int progIdx = 0;


// Joystick emulation
enum JoystickEmu {
  JE_None,
  JE_CursorKeys,
  JE_Mouse,
} joystickEmu = JE_CursorKeys;



// this is the UI app that shows the emulator menu
class Menu : public uiApp {

  uiListBox * progsListBox;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB(3, 3, 3);

    // fills programs listbox
    progsListBox = new uiListBox(rootWindow(), Point(5, 10), Size(140, 300));
    progsListBox->listBoxStyle().backgroundColor = RGB(2, 2, 0);
    progsListBox->listBoxStyle().focusedBackgroundColor = RGB(3, 3, 0);
    for (int i = 0; i < sizeof(progs) / sizeof(Prog); ++i)
      progsListBox->items().append(progs[i].desc);
    progsListBox->selectItem(progIdx);
    // these trigger "run" pressing ENTER on the listbox
    progsListBox->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_RETURN)
        quit(progsListBox->firstSelectedItem());
    };
    progsListBox->onDblClick = [&]() {
      quit(progsListBox->firstSelectedItem());
    };

    // "run" button - run selected item
    auto runButton = new uiButton(rootWindow(), "RUN", Point(150, 10), Size(45, 25));
    runButton->onClick = [&]() { quit(progsListBox->firstSelectedItem()); };

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
    radioJNone->setChecked(joystickEmu == JE_None);
    radioJCurs->setChecked(joystickEmu == JE_CursorKeys);
    radioJMous->setChecked(joystickEmu == JE_Mouse);
    radioJNone->onChange = [&]() { joystickEmu = JE_None; };
    radioJCurs->onChange = [&]() { joystickEmu = JE_CursorKeys; };
    radioJMous->onChange = [&]() { joystickEmu = JE_Mouse; };

    // process ESC and F12: boths quit without action
    rootWindow()->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_ESCAPE || key.VK == VirtualKey::VK_F12)
        quit(-1);
    };

    // focus on programs list
    setFocusedWindow(progsListBox);
  }

};



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

    // read keyboard
    if (Keyboard.virtualKeyAvailable()) {
      bool keyDown;
      VirtualKey vk = Keyboard.getNextVirtualKey(&keyDown);
      switch (vk) {

        // F11 - reset machine
        case VirtualKey::VK_F11:
          if (!keyDown)
            machine.reset();
          break;

        // F12 - open emulation menu
        case VirtualKey::VK_F12:
          if (!keyDown) {
            machine.VIC().enableAudio(false);
            Menu menu;
            int exitCode = menu.run();
            machine.VIC().enableAudio(true);
            Canvas.setBrushColor(0, 0, 0);
            Canvas.clear();
            if (exitCode >= 0) {
              // run selected program (PRG or CRT)
              progIdx = exitCode;
              machine.removeCartridges();
              machine.setRAMExpansion(progs[progIdx].expansion);
              switch (progs[progIdx].cont) {
                case PRG:
                  machine.loadPRG(progs[progIdx].data, progs[progIdx].size, true, true);
                  break;
                case CRT:
                  machine.setCartridge(progs[progIdx].data, progs[progIdx].size, true, progs[progIdx].crtAddr);
                  if (progs[progIdx].data2)
                    machine.setCartridge(progs[progIdx].data2, progs[progIdx].size2, false, progs[progIdx].crtAddr2);
                  break;
              }
            } else if (exitCode == -2) {
              // unload and reset
              machine.removeCartridges();
              machine.reset();
            }
          }
          break;

        // other keys
        default:
          // cursor keys emulate joystick?
          if (joystickEmu == JE_CursorKeys) {
            switch (vk) {
              case VirtualKey::VK_LEFT:
                machine.setJoy(JoyLeft, keyDown);
                break;
              case VirtualKey::VK_RIGHT:
                machine.setJoy(JoyRight, keyDown);
                break;
              case VirtualKey::VK_UP:
                machine.setJoy(JoyUp, keyDown);
                break;
              case VirtualKey::VK_DOWN:
                machine.setJoy(JoyDown, keyDown);
                break;
              case VirtualKey::VK_APPLICATION:  // FIRE: also called MENU key
                machine.setJoy(JoyFire, keyDown);
                break;
              default:
                // send to emulated machine
                machine.setKeyboard(vk, keyDown);
                break;
            }
          } else {
            // send to emulated machine
            machine.setKeyboard(vk, keyDown);
          }
          break;
      }
    }

    // read mouse (emulates joystick)
    if (joystickEmu == JE_Mouse) {
      machine.setJoy(JoyUp,    false);
      machine.setJoy(JoyDown,  false);
      machine.setJoy(JoyLeft,  false);
      machine.setJoy(JoyRight, false);
      if (Mouse.deltaAvailable()) {
        MouseDelta d;
        Mouse.getNextDelta(&d);
        if (d.deltaX < 0)
          machine.setJoy(JoyLeft,  true);
        else if (d.deltaX > 0)
          machine.setJoy(JoyRight, true);
        if (d.deltaY > 0)
          machine.setJoy(JoyUp,    true);
        else if (d.deltaY < 0)
          machine.setJoy(JoyDown,  true);
        machine.setJoy(JoyFire, d.buttons.left | d.buttons.middle | d.buttons.right);
      }
    }

  }
}

