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

#include "esp_spiffs.h"

#include "fabgl.h"

#include "src/machine.h"


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
// Disks configuration


// Enable this when using 8'' disk images ("disks" folder)
#define DISKFORMAT Disk_338K

// Enable this when using minidisk images ("minidisk" folder)
//#define DISKFORMAT MiniDisk_76K

// Specify which disk image assign to drives
#define DRIVE_A cpm22_dsk             // A: read only
#define DRIVE_B games_dsk             // B: read only
#define DRIVE_C "/spiffs/diskC.dsk"   // C: read/write
#define DRIVE_D "/spiffs/diskD.dsk"   // D: read/write


//////////////////////////////////////////////////////////////////////////////////



// consts

constexpr int DefaultCPU = 1; // 0 = i8080, 1 = Z80

const char *  TermStr[] = { "ANSI/VT", "Lear Siegler ADM-3A", "Lear Siegler ADM-31", "Hazeltine 1500", "Osborne I", "Kaypro" };
constexpr int DefaultTermIndex = 2; // Default: "ADM-31"
constexpr int MaxTermIndex = 5;     // Max: "Kaypro"

const char *  KbdLayStr[] = { "US", "UK", "DE", "IT" };
const fabgl::KeyboardLayout * KdbLay[] = { &fabgl::USLayout, &fabgl::UKLayout, &fabgl::GermanLayout, &fabgl::ItalianLayout };
constexpr int DefaultKbdLayIndex = 1; // Default: "UK"
constexpr int MaxKbdLayIndex = 3;     // Max: "IT"

const char * ColorsStr[] = { "Green/Black", "Yellow/Black", "White/Black", "Black/White", "Yellow/Blue" };
const Color TextColors[] = { Color::BrightGreen, Color::BrightYellow, Color::BrightWhite, Color::Black,       Color::BrightYellow };
const Color BackColors[] = { Color::Black,       Color::Black,        Color::Black,       Color::BrightWhite, Color::Blue };
constexpr int DefaultColorsIndex = 0; // Default: Green/Black
constexpr int MaxColorsIndex     = 4; // Max: Yellow/Blue


// globals

TerminalClass Terminal;
Machine       altair;
Mits88Disk    diskDrive(&altair, DISKFORMAT);
Preferences   preferences;


void setup()
{
  preferences.begin("altair8800", false);

  Serial.begin(115200);

  // setup Keyboard (default configuration)
  PS2Controller.begin(PS2Preset::KeyboardPort0);

  // setup VGA (default configuration with 64 colors)
  VGAController.begin();

  if (preferences.getBool("emuCRT", false))
    VGAController.setResolution(VGA_640x200_70HzRetro);
  else
    VGAController.setResolution(VGA_640x200_70Hz);

  //VGAController.shrinkScreen(5, 0);
  //VGAController.moveScreen(-1, 0);

  // this speed-up display but may generate flickering
  VGAController.enableBackgroundPrimitiveExecution(false);

  Terminal.begin();
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();

  Terminal.enableCursor(true);
}


int getTermChar()
{
  while (!Terminal.available())
    delay(10);
  return Terminal.read();
}


// shown pressing F12
void emulator_menu()
{
  Terminal.write("\b\b\b\b\b\b\e[97m\e[40mEmulator menu:\e[K\n\r");
  Terminal.write( "\e[93m X \e[37m Exit\e[K\n\r");
  Terminal.write( "\e[93m S \e[37m Send a disk image to Serial Port\e[K\n\r");
  Terminal.write( "\e[93m R \e[37m Receive a disk image from Serial Port\e[K\n\r");
  Terminal.write( "\e[93m F \e[37m Format SPIFFS\e[K\n\r");
  Terminal.write( "\e[93m Z \e[37m Reset\e[K\n\r");
  Terminal.printf("\e[93m P \e[37m Real CPU speed: \e[93m%s\e[K\n\r", altair.realSpeed() ? "YES" : "NO");
  Terminal.printf("\e[93m G \e[37m Emulate CRT: \e[93m%s\e[K\n\r", preferences.getBool("emuCRT", false) ? "YES" : "NO");
  Terminal.printf("\e[93m U \e[37m CPU: \e[93m%s\e[K\n\r", preferences.getInt("CPU", DefaultCPU) == 1 ? "Z80" : "i8080");
  Terminal.printf("\e[93m T \e[37m Terminal emulation: \e[93m%s\e[K\n\r", TermStr[preferences.getInt("termEmu", DefaultTermIndex)] );
  Terminal.printf("\e[93m K \e[37m Keyboard layout: \e[93m%s\e[K\n\r", KbdLayStr[preferences.getInt("kbdLay", DefaultKbdLayIndex)] );
  Terminal.printf("\e[93m C \e[37m Text/Background colors: \e[93m%s\e[K\n\n\r", ColorsStr[preferences.getInt("colors", DefaultColorsIndex)] );
  Terminal.write("\e[97mPlease select an option: \e[K\e[92m");
  int ch = toupper(getTermChar());
  Terminal.write(ch);
  Terminal.write("\n\r");
  switch (ch) {

    // Exit
    case 'X':
      break;

    // 1. Send a disk image to Serial Port
    // 2. Receive a disk image from Serial Port
    case 'S':
    case 'R':
    {
      Terminal.write("Select a drive (A, B, C, D...): ");
      char drive = toupper(getTermChar());
      if (ch == 'S') {
        Terminal.printf("\n\rSending drive %c...\n\r", drive);
        diskDrive.sendDiskImageToStream(drive - 'A', &Serial);
      } else if (ch == 'R') {
        Terminal.printf("\n\rReceiving drive %c...\n\r", drive);
        diskDrive.receiveDiskImageFromStream(drive - 'A', &Serial);
      }
      break;
    }

    // Format SPIFFS
    case 'F':
      Terminal.write("Formatting SPIFFS removes RW disks and resets the Altair. Are you sure? (Y/N)");
      if (toupper(getTermChar()) == 'Y') {
        Terminal.write("\n\rFormatting SPIFFS...");
        suspendInterrupts();
        esp_spiffs_format(nullptr);
        resumeInterrupts();
        ESP.restart();
      }
      break;

    // Reset
    case 'Z':
      Terminal.write("Resetting the Altair. Are you sure? (Y/N)");
      if (toupper(getTermChar()) == 'Y')
        ESP.restart();
      break;

    // Real CPU speed
    case 'P':
      altair.setRealSpeed(!altair.realSpeed());
      break;

    // Emulate CRT
    case 'G':
      preferences.putBool("emuCRT", !preferences.getBool("emuCRT", false));
      Terminal.write("Reset required. Reset now? (Y/N)");
      if (toupper(getTermChar()) == 'Y')
        ESP.restart();
      break;

    // CPU
    case 'U':
      preferences.putInt("CPU", preferences.getInt("CPU", DefaultCPU) ^ 1);
      Terminal.write("Reset required. Reset now? (Y/N)");
      if (toupper(getTermChar()) == 'Y')
        ESP.restart();
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
      Keyboard.setLayout(KdbLay[kbdLayIndex]);
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

  }
  Terminal.localWrite("\r\n");
  setTerminalColors();
}


void setTerminalColors()
{
  int colorsIndex = preferences.getInt("colors", DefaultColorsIndex);
  Terminal.setForegroundColor(TextColors[colorsIndex]);
  Terminal.setBackgroundColor(BackColors[colorsIndex]);
}


// used to intercept F12 and execute emulator menu
int getCharCRTSIO(int ch)
{
  /*
  static int cseq = 0;

  // detect "ESC [ 24~"   that is F12
  cseq = ((cseq == 0 && ch == 0x1B) || (cseq == 1 && ch == '[') || (cseq == 2 && ch == '2') || (cseq == 3 && ch == '4') || (cseq == 4 && ch == '~') ? cseq + 1 : 0);
  if (cseq == 5) {
    cseq = 0;
    emulator_menu();
    Terminal.localWrite("\b\b\b\b\b\r");
    setTerminalColors();
  }
  */
  return ch;
}


void loop()
{
  Terminal.write("Formatting SPIFFS...\r");

  // setup SPIFFS
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 4,
      .format_if_mount_failed = true
  };
  suspendInterrupts();
  esp_vfs_spiffs_register(&conf);
  resumeInterrupts();

  // setup disk drives

  diskDrive.attachReadOnlyBuffer(0, DRIVE_A);
  diskDrive.attachReadOnlyBuffer(1, DRIVE_B);

  Terminal.write("Creating Disk C...  \r");
  diskDrive.attachFile(2, DRIVE_C);

  Terminal.write("Creating Disk D...  \r");
  diskDrive.attachFile(3, DRIVE_D);

  // setup SIOs

  // TTY
  SIO SIO0(&altair, 0x00);
  SIO0.attachStream(&Terminal, &getCharCRTSIO);

  // CRT/Keyboard
  SIO SIO1(&altair, 0x10);
  SIO1.attachStream(&Terminal, &getCharCRTSIO);

  // Serial
  SIO SIO2(&altair, 0x12);
  SIO2.attachStream(&Serial);

  // RAM
  altair.attachRAM(65536);

  // boot ROM
  altair.load(Altair88DiskBootROMAddr, Altair88DiskBootROM, sizeof(Altair88DiskBootROM));

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

  Terminal.printf("\e[33mFree DMA Memory       :\e[32m %d bytes\e[K\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Terminal.printf("\e[33mFree 32 bit Memory    :\e[32m %d bytes\e[K\r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));

  size_t total = 0, used = 0;
  esp_spiffs_info(NULL, &total, &used);
  Terminal.printf("\e[33mSPI Flash File System :\e[32m %d bytes used, %d bytes free\e[92m\e[K\r\n\e[K\n", used, total - used);
  Terminal.printf("Press \e[93mF12\e[92m to display emulator menu\e[K\r\n");

  setTerminalColors();

  // setup keyboard layout
  int kbdLayIndex = preferences.getInt("kbdLay", DefaultKbdLayIndex);
  Keyboard.setLayout(KdbLay[kbdLayIndex]);

  // setup terminal emulation
  int termIndex = preferences.getInt("termEmu", DefaultTermIndex);
  Terminal.setTerminalType((TermType)termIndex);

  CPU cpu = (preferences.getInt("CPU", DefaultCPU) == 1 ? Z80 : i8080);
  altair.run(cpu, Altair88DiskBootROMRun);
}

