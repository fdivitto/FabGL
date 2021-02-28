/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
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


#pragma once


#include "fabgl.h"


#define BIOS_SEG             0xF000
#define BIOS_OFF             0x0100
#define BIOS_ADDR            (BIOS_SEG * 16 + BIOS_OFF)


// BIOS Data Area

#define BIOS_DATAAREA_ADDR   (0x40 * 16)

#define BIOS_KBDSHIFTFLAGS1    0x17     // keyboard shift flags
#define BIOS_KBDSHIFTFLAGS2    0x18     // more keyboard shift flags
#define BIOS_KBDALTKEYPADENTRY 0x19     // Storage for alternate keypad entry
#define BIOS_KBDBUFHEAD        0x1a     // pointer to next character in keyboard buffer
#define BIOS_KBDBUFTAIL        0x1c     // pointer to first available spot in keyboard buffer
#define BIOS_KBDBUF            0x1e     // keyboard buffer (32 bytes, 16 keys, but actually 15)
#define BIOS_CTRLBREAKFLAG     0x71     // Ctrl-Break flag
#define BIOS_KBDMODE           0x96     // keyboard mode and other shift flags
#define BIOS_KBDLEDS           0x97     // keyboard LEDs
#define BIOS_PRINTSCREENFLAG   0x100    // PRINTSCREEN flag



using fabgl::PS2Controller;
using fabgl::Keyboard;


class BIOS {

public:

  // callbacks
  typedef void (*WritePort)(void * context, int address, uint8_t value);
  typedef uint8_t (*ReadPort)(void * context, int address);


  void init(uint8_t * memory, void * context, ReadPort readPort, WritePort writePort, Keyboard * keyboard);

  void helpersEntry();

private:

  void getKeyFromKeyboard();
  void getKeyFromBuffer();
  void getKeyboardFlags();
  void setKeyboardTypematicAndDelay();
  void storeKeyboardKeyData();

  bool storeKeyInKbdBuffer(uint16_t syscode);
  bool processScancode(int scancode, uint16_t * syscode);
  void emptyKbdBuffer();


  uint8_t *       m_memory;

  // callbacks
  void *          m_context;
  ReadPort        m_readPort;
  WritePort       m_writePort;

  Keyboard      * m_keyboard;

  // state of multibyte scancode intermediate reception:
  // 0 = none
  //    pause (0xe1 0x1d 0x45 0xe1 0x9d 0xc5): 1 = 0x1d, 2 = 0x45, 3 = 0x9d
  uint8_t         m_kbdScancodeComp;


};



