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



// free games
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


// games (commercial)
#include "progs/games/blitz.h"
#include "progs/games/breakout.h"
#include "progs/games/abductor.h"
#include "progs/games/alien_attack.h"
#include "progs/games/alien_blitz.h"
#include "progs/games/block_buster.h"
#include "progs/games/bomber.h"
#include "progs/games/car_race.h"
#include "progs/games/matrix.h"
#include "progs/games/tomthumbs.h"
#include "progs/games/cosmicfirebirds.h"
#include "progs/games/moleattack.h"
#include "progs/games/pacman.h"
#include "progs/games/polaris.h"
#include "progs/games/omegarace.h"
#include "progs/games/aggressor.h"
#include "progs/games/amok.h"
#include "progs/games/applepanic.h"
#include "progs/games/arcadia.h"
#include "progs/games/attack_of_the_mutant_camels.h"
#include "progs/games/avenger.h"
#include "progs/games/choplifter.h"
#include "progs/games/crossfire.h"
#include "progs/games/cyclon.h"
#include "progs/games/d_fuse.h"
#include "progs/games/demonattack.h"
#include "progs/games/dragonfire.h"
#include "progs/games/frogger.h"
#include "progs/games/galaxian.h"
#include "progs/games/garden_wars.h"
#include "progs/games/jelly_monsters_v1.h"
#include "progs/games/jupiter_lander.h"
#include "progs/games/key_quest.h"
#include "progs/games/lazer_zone.h"
#include "progs/games/midnight_drive.h"
#include "progs/games/miner_2049.h"
#include "progs/games/outworld.h"
#include "progs/games/protector.h"
#include "progs/games/quackers.h"
#include "progs/games/river_rescue.h"
#include "progs/games/robot_panic.h"
#include "progs/games/satellite_patrol.h"
#include "progs/games/satellites_and_meteorites.h"
#include "progs/games/scorpion.h"
#include "progs/games/serpentine.h"
#include "progs/games/shamus.h"
#include "progs/games/spiders_of_mars.h"
#include "progs/games/star_battle.h"
#include "progs/games/star_trek_sos.h"
#include "progs/games/sub_chase.h"
#include "progs/games/super_amok.h"
#include "progs/games/super_smash.h"
#include "progs/games/terraguard.h"
#include "progs/games/threshold.h"
#include "progs/games/vic1923_gorf.h"
#include "progs/games/witch_way.h"
#include "progs/games/battlezone.h"
#include "progs/games/centipede.h"
#include "progs/games/donkey_kong.h"
#include "progs/games/pole_position.h"



const Prog appsList[] = {
};


const Prog freeGamesList[] = {
arukanoido,
demo_bad_scene_poetry,
demo_bah_bah_final,
demo_digit,
demo_frozen,
demo_happyhour,
demo_intro_2,
demo_plotter,
demo_rasterbar,
demopart,
dragonwing,
kikstart,
kweepoutmc,
lunar_blitz,
nibbler,
pitfall,
pong,
pooyan,
popeye,
pulse,
screen_demo,
spikes,
tank_battalion,
tetris_plus,
vicmine,


  blue_star,
  commodoredemo3,
  demo_birthday,
  omega_fury,
  pu239demo1,
  splatform,
  tetrisdeluxe,
  vic20revivaldemo,
  quikman,
};


const Prog commGamesList[] = {
  abductor,
  aggressor,
  alien_attack,
  alien_blitz,
  amok,
  applepanic,
  arcadia,
  attack_of_the_mutant_camels,
  avenger,
  battlezone,
  breakout,
  blitz,
  block_buster,
  bomber,
  car_race,
  centipede,
  choplifter,
  cosmicfirebirds,
  crossfire,
  cyclon,
  d_fuse,
  demonattack,
  donkey_kong,
  dragonfire,
  frogger,
  galaxian,
  garden_wars,
  jelly_monsters_v1,
  jupiter_lander,
  key_quest,
  lazer_zone,
  matrix,
  midnight_drive,
  miner_2049,
  moleattack,
  omegarace,
  outworld,
  pacman,
  polaris,
  protector,
  pole_position,
  quackers,
  river_rescue,
  robot_panic,
  satellite_patrol,
  satellites_and_meteorites,
  scorpion,
  serpentine,
  shamus,
  spiders_of_mars,
  star_battle,
  star_trek_sos,
  sub_chase,
  super_amok,
  super_smash,
  terraguard,
  threshold,
  tomthumbs,
  vic1923_gorf,
  witch_way,
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


// last selected prog group
enum ProgGroup {
  PG_Apps,
  PG_FreeGames,
  PG_CommGames,
} progGroup = PG_FreeGames;


// Joystick emulation
enum JoystickEmu {
  JE_None,
  JE_CursorKeys,
  JE_Mouse,
} joystickEmu = JE_CursorKeys;   // TODO: restore to JE_Mouse


Prog const * getCurrentProgList(int * count = nullptr) {
  switch (progGroup) {
    case PG_Apps:
      if (count) *count = sizeof(appsList) / sizeof(Prog);
      return appsList;
    case PG_FreeGames:
      if (count) *count = sizeof(freeGamesList) / sizeof(Prog);
      return freeGamesList;
    case PG_CommGames:
      if (count) *count = sizeof(commGamesList) / sizeof(Prog);
      return commGamesList;
    default:
      return nullptr;
  }
}


// this is the UI app that shows the emulator menu
class Menu : public uiApp {

  uiComboBox * progGroupComboBox;
  uiListBox * progsListBox;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB(3, 3, 3);

    // programs group selection combobox
    new uiLabel(rootWindow(), "Group:", Point(5, 13));
    progGroupComboBox = new uiComboBox(rootWindow(), Point(42, 10), Size(103, 25), 50);
    progGroupComboBox->items().append("Apps");
    progGroupComboBox->items().append("Free Games");
    progGroupComboBox->items().append("Comm Games");
    progGroupComboBox->selectItem((int)progGroup);
    progGroupComboBox->onChange = [&]() {
      progGroup = (ProgGroup)(progGroupComboBox->selectedItem());
      fillProgsListBox();
    };

    // fills programs listbox
    progsListBox = new uiListBox(rootWindow(), Point(5, 40), Size(140, 310));
    progsListBox->listBoxStyle().backgroundColor = RGB(2, 2, 0);
    progsListBox->listBoxStyle().focusedBackgroundColor = RGB(3, 3, 0);
    fillProgsListBox();
    progsListBox->selectItem(progIdx);
    // these trigger "run" pressing ENTER or DBL CLICK on the listbox
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

  void fillProgsListBox() {
    int count;
    Prog const * progs = getCurrentProgList(&count);
    progsListBox->items().clear();
    for (int i = 0; i < count; ++i)
      progsListBox->items().append(progs[i].desc);
    progsListBox->repaint();
    progsListBox->selectItem(0);
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

