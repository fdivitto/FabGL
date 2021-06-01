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


#pragma once

#include "fabgl.h"

#include "emudevs/MOS6502.h"
#include "emudevs/VIA6522.h"
#include "MOS6561.h"
#include "IECDrive.h"


#define DEBUGMACHINE 0


#if DEBUGMACHINE || DEBUG6522 || DEBUG6561
#define DEBUG 1
#endif



using fabgl::MOS6502;
using fabgl::VIA6522;
using fabgl::VIA6522Port;


/////////////////////////////////////////////////////////////////////////////////////////////
// Machine (Commodore VIC 20)


enum Joy {
  JoyUp,
  JoyDown,
  JoyLeft,
  JoyRight,
  JoyFire,
};


enum RAMExpansionOption {
  RAM_unexp,
  RAM_3K,
  RAM_8K,
  RAM_16K,
  RAM_24K,
  RAM_27K,  // 3K + 24K
  RAM_32K,  // last 8K mapped to A000, not visible to Basic
  RAM_35K,  // as RAM_32K + 3K
};


enum JoyEmu {
  JE_None,
  JE_CursorKeys,
  JE_Mouse,
};



class Machine {

public:

  Machine(fabgl::VGAController * displayController);
  ~Machine();

  void reset();

  int run();

  VIA6522 & VIA1() { return m_VIA1; }
  VIA6522 & VIA2() { return m_VIA2; }
  MOS6561 & VIC()  { return m_VIC; }
  MOS6502 & CPU()  { return m_CPU; }

  void setKeyboard(VirtualKey key, bool down);
  void resetKeyboard();

  void setJoy(Joy joy, bool value);
  void resetJoy();
  void setJoyEmu(JoyEmu value) { m_joyEmu = value; }
  JoyEmu joyEmu()              { return m_joyEmu; }

  void loadPRG(char const * filename, bool resetRequired, bool execRun);

  int loadCRT(char const * filename, bool reset, int address = -1);

  void removeCRT();

  uint8_t busReadCharDefs(int addr);
  uint8_t const * busReadVideoP(int addr);
  uint8_t const * busReadColorP(int addr);

  uint8_t busRead(int addr);
  void busWrite(int addr, uint8_t value);

  void type(char const * str) { m_typingString = str; }   // TODO: multiple type() calls not possible!!

  void setRAMExpansion(RAMExpansionOption value);
  RAMExpansionOption RAMExpansion() { return m_RAMExpansion; }

  FileBrowser * fileBrowser()       { return &m_fileBrowser; }

private:

  static int busRead(void * context, int addr)                 { return ((Machine*)context)->busRead(addr); }
  static void busWrite(void * context, int addr, int value)    { ((Machine*)context)->busWrite(addr, value); }

  static int page0Read(void * context, int addr)               { return ((Machine*)context)->m_RAM1K[addr]; }
  static void page0Write(void * context, int addr, int value)  { ((Machine*)context)->m_RAM1K[addr] = value; }

  static int page1Read(void * context, int addr)               { return ((Machine*)context)->m_RAM1K[0x100 + addr]; }
  static void page1Write(void * context, int addr, int value)  { ((Machine*)context)->m_RAM1K[0x100 + addr] = value; }


  int VICRead(int reg);
  void VICWrite(int reg, int value);

  static void VIA1PortOut(void * context, VIA6522 * via, VIA6522Port port);
  static void VIA1PortIn(void * context, VIA6522 * via, VIA6522Port port);
  static void VIA2PortOut(void * context, VIA6522 * via, VIA6522Port port);
  static void VIA2PortIn(void * context, VIA6522 * via, VIA6522Port port);

  void syncTime();
  void handleCharInjecting();
  void handleMouse();

  // 0: 3K RAM expansion (0x0400 - 0x0fff)
  // 1: 8K RAM expansion (0x2000 - 0x3fff)
  // 2: 8K RAM expansion (0x4000 - 0x5fff)
  // 3: 8K RAM expansion (0x6000 - 0x7fff)
  // 4: 8K RAM expansion (0xA000 - 0xBfff)
  void enableRAMBlock(int block, bool enabled);


  MOS6502               m_CPU;

  // standard RAM
  uint8_t *             m_RAM1K;
  uint8_t *             m_RAM4K;
  uint8_t *             m_RAMColor;

  // expansion RAM
  // 0: 3K (0x0400 - 0x0fff)
  // 1: 8K (0x2000 - 0x3fff)
  // 2: 8K (0x4000 - 0x5fff)
  // 3: 8K (0x6000 - 0x7fff)
  // 4: 8K (0xA000 - 0xBfff)
  uint8_t *             m_expRAM[5];
  RAMExpansionOption    m_RAMExpansion;

  // Cartridges:
  //   block 0 : 0x2000 - 0x3fff
  //   block 1 : 0x4000 - 0x5fff
  //   block 2 : 0x6000 - 0x7fff
  //   block 3 : 0xA000 - 0xbfff
  uint8_t *             m_expROM[4];

  // ** VIA1 **
  // IRQ      -> NMI
  // CA1      -> RESTORE key
  // CA2      -> CASS MOTOR
  // CB1      -> USER PORT (CB1)
  // CB2      -> USER PORT (CB2)
  // PB0..PB7 -> USER PORT (PB0..PB7)
  // PA0      -> SERIAL CLK IN
  // PA1      -> SERIAL DATA IN
  // PA2      -> JOY0
  // PA3      -> JOY1
  // PA4      -> JOY2
  // PA5      -> LIGHT PEN (FIRE)
  // PA6      -> CASS SW
  // PA7      -> /SERIAL ATN OUT
  VIA6522               m_VIA1;

  // ** VIA2 **
  // IRQ      -> CPU IRQ
  // CA1      -> CASS READ
  // CA2      -> /SERIAL CLK OUT
  // CB1      -> SERIAL SRQ IN
  // CB2      -> /SERIAL DATA OUT
  // PB0..PB7 -> keyboard Col
  // PA0..PA7 -> Keyboard Row
  // PB7      -> JOY3
  VIA6522               m_VIA2;

  // Video Interface
  MOS6561               m_VIC;

  // to store current NMI status (true=active, false=inactive)
  bool                  m_NMI;

  // overflows about every hour
  uint32_t              m_cycle;

  // row x col (1=down, 0 = up)
  uint8_t               m_KBD[8][8];

  // joystick states and emulation
  uint8_t               m_JOY[JoyFire + 1];
  JoyEmu                m_joyEmu;

  // keyboard states
  uint8_t               m_rowStatus;  // connected to VIA2 - PA
  uint8_t               m_colStatus;  // connected to VIA2 - PB

  // triggered by type() method
  char const *          m_typingString;

  uint32_t              m_lastSyncCycle;
  uint64_t              m_lastSyncTime;   // uS

  IECDrive              m_IECDrive;
  FileBrowser           m_fileBrowser;

};



/////////////////////////////////////////////////////////////////////////////////////////////
// PRGCreator
// Creates a PRG in memory

class PRGCreator {

public:

  PRGCreator(int startingAddress);
  ~PRGCreator();

  // line must end with "0"
  void addline(int linenumber, char const * data);

  void addline(int linenumber, void const * data, size_t datalen);

  uint8_t const * get() { return m_prg; }
  int len()             { return m_prglen; }

private:

  int       m_startingAddress;
  uint8_t * m_prg;
  int       m_prglen;

};
