/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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



 /*
  * Optional SD Card connections:
  *   MISO => GPIO 16
  *   MOSI => GPIO 17
  *   CLK  => GPIO 14
  *   CS   => GPIO 13
  *
  * To change above assignment fill other paramaters of FileBrowser::mountSDCard().
  */



#include <Preferences.h>

#include "fabgl.h"
#include "fabutils.h"

#include "src/machine.h"



// Flash and SDCard configuration
#define FORMAT_ON_FAIL     true
#define SPIFFS_MOUNT_PATH  "/flash"
#define SDCARD_MOUNT_PATH  "/SD"



//////////////////////////////////////////////////////////////////////////////////
// 8'' disk images (338K)

// CP/M 2.2
#include "disks/CPM22/cpm22_dsk.h"
#include "disks/CPM22/games_dsk.h"
#include "disks/CPM22/turbopascal3_dsk.h"
#include "disks/CPM22/wordstar3_dsk.h"
#include "disks/CPM22/wordstar4_dsk.h"
#include "disks/CPM22/multiplan_dsk.h"
#include "disks/CPM22/dbaseii_dsk.h"
#include "disks/CPM22/BDSC_dsk.h"
#include "disks/CPM22/langs_dsk.h"

// CP/M 1.4
#include "disks/CPM14/cpm141_dsk.h"

// CP/M 3
#include "disks/CPM3/cpm3_disk1_dsk.h"
#include "disks/CPM3/cpm3_disk2_dsk.h"
#include "disks/CPM3/cpm3_build_dsk.h"

// Altair DOS
//   MEMORY SIZE? [insert "64"]
//   INTERRUPTS? [press ENTER]
//   HIGHEST DISK NUMBER? [insert "1"]
//   HOW MANY DISK FILES? [insert "4"]
//   HOW MANY RANDOM FILES? [insert "4"]
#include "disks/AltairDOS/DOS_1_dsk.h"
#include "disks/AltairDOS/DOS_2_dsk.h"

// Basic
//    MEMORY SIZE? [press ENTER]
//    LINEPRINTER? [insert "C"]
//    HIGHEST DISK NUMBER? [press ENTER]
//    HOW MANY FILES? [press ENTER]
//    HOW MANY RANDOM FILES? [press ENTER]
#include "disks/basic/basic5_dsk.h"

//////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////
// Mini disk images (76K)

// CP/M 2.2
#include "minidisks/CPM22/cpm22_disk1_minidsk.h"
#include "minidisks/CPM22/cpm22_disk2_minidsk.h"

// Basic (need i8080 CPU)
//   MEMORY SIZE? [press ENTER]
//   LINEPRINTER? [insert "C"]
//   HIGHEST DISK NUMBER? [insert "0"]
//   HOW MANY FILES? [insert "3"]
//   HOW MANY RANDOM FILES? [insert "2"]
#include "minidisks/basic/basic300_5F_minidisk.h"

//////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////
// Disks configuration


// Enable this when using 8'' disk images ("disks" folder)
#define DISKFORMAT Disk_338K

// Enable this when using minidisk images ("minidisk" folder)
//#define DISKFORMAT MiniDisk_76K

// Specify which disk image or file name assign to drives
#define DRIVE_A cpm22_dsk     // A: read only
#define DRIVE_B games_dsk     // B: read only
#define DRIVE_C "diskC.dsk"   // C: read/write
#define DRIVE_D "diskD.dsk"   // D: read/write

//////////////////////////////////////////////////////////////////////////////////



// consts

constexpr int DefaultCPU = 1;       // 0 = i8080, 1 = Z80

const char *  TermStr[]        = { "ANSI/VT", "Lear Siegler ADM-3A", "Lear Siegler ADM-31", "Hazeltine 1500", "Osborne I", "Kaypro" };
constexpr int DefaultTermIndex = 2;   // Default: "ADM-31"
constexpr int MaxTermIndex     = 5;   // Max: "Kaypro"

const char *  KbdLayStr[]              = { "US", "UK", "DE", "IT" };
const fabgl::KeyboardLayout * KdbLay[] = { &fabgl::USLayout, &fabgl::UKLayout, &fabgl::GermanLayout, &fabgl::ItalianLayout };
constexpr int DefaultKbdLayIndex       = 1;   // Default: "UK"
constexpr int MaxKbdLayIndex           = 3;   // Max: "IT"

const char * ColorsStr[] = { "Green/Black", "Yellow/Black", "White/Black", "Black/White", "Yellow/Blue", "Black/Yellow" };
const Color TextColors[] = { Color::BrightGreen, Color::BrightYellow, Color::BrightWhite, Color::Black,       Color::BrightYellow, Color::Black };
const Color BackColors[] = { Color::Black,       Color::Black,        Color::Black,       Color::BrightWhite, Color::Blue,         Color::BrightYellow };
constexpr int DefaultColorsIndex = 0; // Default: Green/Black
constexpr int MaxColorsIndex     = 5;


// globals

fabgl::VGAController DisplayController;
fabgl::PS2Controller PS2Controller;
fabgl::Terminal      Terminal;

Machine              altair;
Mits88Disk           diskDrive(&altair, DISKFORMAT);
SIO                  SIO0(&altair, 0x00);
SIO                  SIO1(&altair, 0x10);
SIO                  SIO2(&altair, 0x12);
Preferences          preferences;


// base path (can be SPIFFS_MOUNT_PATH or SDCARD_MOUNT_PATH depending from what was successfully mounted first)
char const * basepath = nullptr;




void setTerminalColors()
{
  int colorsIndex = preferences.getInt("colors", DefaultColorsIndex);
  Terminal.setForegroundColor(TextColors[colorsIndex]);
  Terminal.setBackgroundColor(BackColors[colorsIndex]);
}


// shown pressing PAUSE
void emulator_menu()
{
  bool resetRequired = false;
  for (bool loop = true; loop; ) {

    diskDrive.flush();

    Terminal.write("\r\n\n\e[97m\e[40m                            ** Emulator Menu **\e[K\r\n\e[K\n");
    Terminal.write( "\e[93m Z \e[37m Reset\e[K\n\r");
    Terminal.write( "\e[93m S \e[37m Send Disk to Serial\e[K\n\r");
    Terminal.write( "\e[93m R \e[37m Get Disk from Serial\e[K\n\r");
    Terminal.write( "\e[93m F \e[37m Format FileSystem\e[K\n\r");
    Terminal.printf("\e[93m U \e[37m CPU: \e[33m%s\e[K\n\r", preferences.getInt("CPU", DefaultCPU) == 1 ? "Z80" : "i8080");
    Terminal.printf("\e[93m P \e[37m Real CPU Speed: \e[33m%s\e[K\n\r", preferences.getBool("realSpeed", false) ? "YES" : "NO");
    Terminal.write("\e[6A");  // cursor UP
    Terminal.printf("\t\t\t\t\t\e[93m T \e[37m Terminal: \e[33m%s\e[K\n\r", TermStr[preferences.getInt("termEmu", DefaultTermIndex)] );
    Terminal.printf("\t\t\t\t\t\e[93m K \e[37m Keyboard Layout: \e[33m%s\e[K\n\r", KbdLayStr[preferences.getInt("kbdLay", DefaultKbdLayIndex)] );
    Terminal.printf("\t\t\t\t\t\e[93m G \e[37m CRT Mode: \e[33m%s\e[K\n\r", preferences.getBool("emuCRT", false) ? "YES" : "NO");
    Terminal.printf("\t\t\t\t\t\e[93m C \e[37m Colors: \e[33m%s\e[K\n\r", ColorsStr[preferences.getInt("colors", DefaultColorsIndex)] );
    Terminal.write( "\t\t\t\t\t\e[93m O \e[37m Reset Configuration\e[K\n\r");

    Terminal.write("\n\n\e[97mPlease select an option (ENTER to exit): \e[K\e[92m");
    int ch = toupper(Terminal.read());
    Terminal.write(ch);
    Terminal.write("\n\r");
    switch (ch) {

      // 1. Send a disk image to Serial Port
      // 2. Receive a disk image from Serial Port
      case 'S':
      case 'R':
      {
        Terminal.write("Select a drive (A, B, C, D...): ");
        char drive = toupper(Terminal.read());
        if (ch == 'S') {
          Terminal.printf("\n\rSending drive %c...\n\r", drive);
          diskDrive.sendDiskImageToStream(drive - 'A', &Serial);
        } else if (ch == 'R') {
          Terminal.printf("\n\rReceiving drive %c...\n\r", drive);
          diskDrive.receiveDiskImageFromStream(drive - 'A', &Serial);
        }
        break;
      }

      // Format FileSystem
      case 'F':
        Terminal.write("Formatting filesystem removes RW disks and resets the Altair. Are you sure? (Y/N)");
        if (toupper(Terminal.read()) == 'Y') {
          Terminal.write("\n\rFormatting...");
          Terminal.flush();
          FileBrowser::format(FileBrowser::getDriveType(basepath), 0);
          resetRequired = true;
        }
        break;

      // Reset
      case 'Z':
        Terminal.write("Resetting the Altair. Are you sure? (Y/N)");
        if (toupper(Terminal.read()) == 'Y')
          ESP.restart();
        break;

      // Real CPU speed
      case 'P':
        preferences.putBool("realSpeed", !preferences.getBool("realSpeed", false));
        altair.setRealSpeed(preferences.getBool("realSpeed", false));
        break;

      // Emulate CRT
      case 'G':
        preferences.putBool("emuCRT", !preferences.getBool("emuCRT", false));
        resetRequired = true;
        break;

      // CPU
      case 'U':
        preferences.putInt("CPU", preferences.getInt("CPU", DefaultCPU) ^ 1);
        resetRequired = true;
        break;

      // Terminal emulation
      case 'T':
      {
        int termIndex = preferences.getInt("termEmu", DefaultTermIndex) + 1;
        if (termIndex > MaxTermIndex)
          termIndex = 0;
        preferences.putInt("termEmu", termIndex);
        Terminal.setTerminalType((TermType)termIndex);
        break;
      }

      // Keyboard layout
      case 'K':
      {
        int kbdLayIndex = preferences.getInt("kbdLay", DefaultKbdLayIndex) + 1;
        if (kbdLayIndex > MaxKbdLayIndex)
          kbdLayIndex = 0;
        preferences.putInt("kbdLay", kbdLayIndex);
        PS2Controller.keyboard()->setLayout(KdbLay[kbdLayIndex]);
        break;
      }

      // Colors
      case 'C':
      {
        int colorsIndex = preferences.getInt("colors", DefaultColorsIndex) + 1;
        if (colorsIndex > MaxColorsIndex)
          colorsIndex = 0;
        preferences.putInt("colors", colorsIndex);
        break;
      }

      // Reset configuration
      case 'O':
        preferences.clear();
        resetRequired = true;
        break;

      default:
        loop = false;
        break;

    }
  }
  if (resetRequired) {
    Terminal.write("Reset required. Reset now? (Y/N)");
    if (toupper(Terminal.read()) == 'Y') {
      diskDrive.detachAll();
      ESP.restart();
    }
  }
  Terminal.localWrite("\n");
  setTerminalColors();
}


void setup()
{
  preferences.begin("altair8800", false);

  Serial.begin(115200);

  // setup Keyboard (default configuration)
  PS2Controller.begin(PS2Preset::KeyboardPort0);

  // setup VGA (default configuration with 64 colors)
  DisplayController.begin();

  if (preferences.getBool("emuCRT", false))
    DisplayController.setResolution(VGA_640x200_70HzRetro);
  else
    DisplayController.setResolution(VGA_640x200_70Hz);

  // uncomment to adjust screen alignment and size
  //DisplayController.shrinkScreen(5, 0);
  //DisplayController.moveScreen(-1, 0);

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();

  Terminal.enableCursor(true);
}


void loop()
{
  Terminal.write("Initializing filesystem...\r");
  Terminal.flush();

  if (FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH))
    basepath = SDCARD_MOUNT_PATH;
  else if (FileBrowser::mountSPIFFS(FORMAT_ON_FAIL, SPIFFS_MOUNT_PATH))
    basepath = SPIFFS_MOUNT_PATH;

  // setup disk drives

  diskDrive.attachReadOnlyBuffer(0, DRIVE_A);
  diskDrive.attachReadOnlyBuffer(1, DRIVE_B);

  Terminal.write("Creating Disk C...        \r");
  Terminal.flush();
  diskDrive.attachFile(2, (String(basepath) + String("/") + String(DRIVE_C)).c_str());

  Terminal.write("Creating Disk D...        \r");
  Terminal.flush();
  diskDrive.attachFile(3, (String(basepath) + String("/") + String(DRIVE_D)).c_str());

  // setup SIOs (Serial I/O)

  // TTY
  SIO0.attachStream(&Terminal);

  // CRT/Keyboard
  SIO1.attachStream(&Terminal);

  // Serial
  SIO2.attachStream(&Serial);

  // RAM
  altair.attachRAM(65536);

  // boot ROM
  altair.load(Altair88DiskBootROMAddr, Altair88DiskBootROM, sizeof(Altair88DiskBootROM));

  // menu callback (pressing PAUSE)
  altair.setMenuCallback(emulator_menu);

  setTerminalColors();
  Terminal.clear();

  // scrolling region (24 rows)
  Terminal.write("\e[1;24r");

  Terminal.write("\e[97m\e[44m");
  Terminal.write("                    /* * * * * * * * * * * * * * * * * * * *\e[K\r\n");
  Terminal.write("\e#6");
  Terminal.write("\e#3              ALTAIR  8800\e[K\r\n");
  Terminal.write("\e#4              ALTAIR  8800\e[K\r\n");
  Terminal.write("\e#5");
  Terminal.write("\e[K\r\n");
  Terminal.write("                     \e[37mby Fabrizio Di Vittorio - www.fabgl.com\e[97m\e[K\r\n");
  Terminal.write("                     * * * * * * * * * * * * * * * * * * * */\e[K\r\n\e[K\n");

  Terminal.printf("\e[33mFree Memory :\e[32m %d bytes\e[K\r\n", heap_caps_get_free_size(0));

  int64_t total, used;
  FileBrowser::getFSInfo(FileBrowser::getDriveType(basepath), 0, &total, &used);
  Terminal.printf("\e[33mFile System :\e[32m %lld KiB used, %lld KiB free\e[K\r\n", used / 1024, (total - used) / 1024);

  Terminal.printf("\e[33mKbd Layout  : \e[32m%s\e[K\r\n", KbdLayStr[preferences.getInt("kbdLay", DefaultKbdLayIndex)] );
  Terminal.printf("\e[33mCPU         : \e[32m%s\e[92m\e[K\r\n\e[K\n", preferences.getInt("CPU", DefaultCPU) == 1 ? "Z80" : "i8080");

  Terminal.printf("Press \e[93mPAUSE\e[92m to display emulator menu\e[K\r\n");

  setTerminalColors();

  // setup keyboard layout
  int kbdLayIndex = preferences.getInt("kbdLay", DefaultKbdLayIndex);
  PS2Controller.keyboard()->setLayout(KdbLay[kbdLayIndex]);

  // setup terminal emulation
  int termIndex = preferences.getInt("termEmu", DefaultTermIndex);
  Terminal.setTerminalType((TermType)termIndex);

  // CPU speed
  altair.setRealSpeed(preferences.getBool("realSpeed", false));

  CPU cpu = (preferences.getInt("CPU", DefaultCPU) == 1 ? CPU::Z80 : CPU::i8080);
  altair.run(cpu, Altair88DiskBootROMRun);
}

