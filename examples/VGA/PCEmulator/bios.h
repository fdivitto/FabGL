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


#pragma once


#include "fabgl.h"
#include "emudevs/i8042.h"
#include "emudevs/MC146818.h"



// 0 = floppy 0 (fd0, A:)
// 1 = floppy 1 (fd1, B:)
// 2 = hard disk 0 (hd0, C: or D:, depends by partitions)
// 3 = hard disk 1 (hd1)
constexpr int DISKCOUNT = 4;


#define BIOS_SEG             0xF000
#define BIOS_OFF             0x0100
#define BIOS_ADDR            (BIOS_SEG * 16 + BIOS_OFF)


// BIOS Data Area

#define BIOS_DATAAREA_SEG      0x40
#define BIOS_DATAAREA_ADDR     (BIOS_DATAAREA_SEG << 4)

#define BIOS_KBDSHIFTFLAGS1    0x17     // keyboard shift flags
#define BIOS_KBDSHIFTFLAGS2    0x18     // more keyboard shift flags
#define BIOS_KBDALTKEYPADENTRY 0x19     // Storage for alternate keypad entry
#define BIOS_KBDBUFHEAD        0x1a     // pointer to next character in keyboard buffer
#define BIOS_KBDBUFTAIL        0x1c     // pointer to first available spot in keyboard buffer
#define BIOS_KBDBUF            0x1e     // keyboard buffer (32 bytes, 16 keys, but actually 15)
#define BIOS_DISKLASTSTATUS    0x41     // diskette status return code
#define BIOS_SYSTICKS          0x6c     // system ticks (dword)
#define BIOS_CLKROLLOVER       0x70     // system tick rollover flag (24h)
#define BIOS_CTRLBREAKFLAG     0x71     // Ctrl-Break flag
#define BIOS_HDLASTSTATUS      0x74     // HD status return code
#define BIOS_NUMHD             0x75     // number of fixed disk drives
#define BIOS_DRIVE0MEDIATYPE   0x90     // media type of drive 0
#define BIOS_DRIVE1MEDIATYPE   0x91     // media type of drive 1
#define BIOS_KBDMODE           0x96     // keyboard mode and other shift flags
#define BIOS_KBDLEDS           0x97     // keyboard LEDs
#define BIOS_PRINTSCREENFLAG   0x100    // PRINTSCREEN flag


// Extended BIOS Data Area (EBDA)

#define EBDA_SEG               0x9fc0   // EBDA Segment, must match with same value in bios.asm
#define EBDA_ADDR              (EBDA_SEG << 4)

#define EBDA_DRIVER_OFFSET     0x22     // Pointing device device driver far call offset
#define EBDA_DRIVER_SEG        0x24     // Pointing device device driver far call segment
#define EBDA_FLAGS1            0x26     // Flags 1 (bits 0-2: recv data index)
#define EBDA_FLAGS2            0x27     // Flags 2 (bits 0-2: packet size, bit 7: device handler installed)
#define EBDA_PACKET            0x28     // Start of packet




using fabgl::PS2Controller;
using fabgl::Keyboard;
using fabgl::Mouse;
using fabgl::i8042;
using fabgl::MC146818;


enum MediaType {
  mediaUnknown,
  floppy160KB,
  floppy180KB,
  floppy320KB,
  floppy360KB,
  floppy720KB,
  floppy1M2K,
  floppy1M44K,
  floppy2M88K,

  HDD,
};

class Machine;


class BIOS {

public:

  BIOS();

  void init(Machine * machine);
  void reset();

  void helpersEntry();

  void diskHandlerEntry();

  void videoHandlerEntry();

  void setDriveMediaType(int drive, MediaType media);

private:

  void getKeyFromKeyboard();
  void getKeyFromBuffer();
  void getKeyboardFlags();
  void setKeyboardTypematicAndDelay();
  void storeKeyboardKeyData();

  bool storeKeyInKbdBuffer(uint16_t syscode);
  bool processScancode(int scancode, uint16_t * syscode);
  void emptyKbdBuffer();

  void pointingDeviceInterface();

  void syncTicksWithRTC();

  void diskHandler_floppy();
  bool diskHandler_calcAbsAddr(int drive, uint32_t * pos, uint32_t * dest, uint32_t * count);
  void diskHandler_floppyExit(uint8_t err, bool setErrStat);
  void diskHandler_HD();
  void diskHandler_HDExit(uint8_t err, bool setErrStat);

  bool checkDriveMediaType(int drive);
  uint32_t getDriveMediaTableAddr(int drive);


  Machine *       m_machine;
  uint8_t *       m_memory;
  Keyboard *      m_keyboard;
  Mouse *         m_mouse;
  i8042 *         m_i8042;
  MC146818 *      m_MC146818;

  // state of multibyte scancode intermediate reception:
  // 0 = none
  //    pause (0xe1 0x1d 0x45 0xe1 0x9d 0xc5): 1 = 0x1d, 2 = 0x45, 3 = 0x9d
  uint8_t         m_kbdScancodeComp;

  // address of bios.asm:media_drive_parameters (0, 1)
  // address of int41 (2), int46 (3)
  //uint32_t        m_mediaDriveParametersAddr[4];

  // original int 1e address (may be changed by O.S.)
  uint32_t        m_origInt1EAddr;

  // media type for floppy (0,1) and HD (>=2)
  MediaType       m_mediaType[DISKCOUNT];


};



