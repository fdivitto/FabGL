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


#pragma once

#include "fabgl.h"

#include "MOS6502.h"


#define DEBUGMSG 0

class Machine;


/////////////////////////////////////////////////////////////////////////////////////////////
// VIA (6522 - Versatile Interface Adapter)


// VIA registers
#define VIA_REG_ORB             0x0
#define VIA_REG_ORA             0x1
#define VIA_REG_DDRB            0x2
#define VIA_REG_DDRA            0x3
#define VIA_REG_T1_C_LO         0x4
#define VIA_REG_T1_C_HI         0x5
#define VIA_REG_T1_L_LO         0x6
#define VIA_REG_T1_L_HI         0x7
#define VIA_REG_T2_C_LO         0x8
#define VIA_REG_T2_C_HI         0x9
#define VIA_REG_SR              0xa
#define VIA_REG_ACR             0xb   // Auxiliary Control Register
#define VIA_REG_PCR             0xc   // Peripherical Control Register
#define VIA_REG_IFR             0xd   // Interrupt Flag Register
#define VIA_REG_IER             0xe   // Interrupt Enable Register
#define VIA_REG_ORA_NH          0xf

// VIA interrupt flags/control (bit mask)
#define VIA_I_CA2               0x01
#define VIA_I_CA1               0x02
#define VIA_I_SR                0x04
#define VIA_I_CB2               0x08
#define VIA_I_CB1               0x10
#define VIA_I_T2                0x20
#define VIA_I_T1                0x40
#define VIA_I_CTRL              0x80

// VIA, ACR flags
#define VIA_ACR_T2_COUNTPULSES  0x20
#define VIA_ACR_T1_FREERUN      0x40
#define VIA_ACR_T1_OUTENABLE    0x80


enum VIAPort {
  Port_PA,  // (8 bit)
  Port_PB,  // (8 bit)
  Port_CA1, // (1 bit)
  Port_CA2, // (1 bit)
  Port_CB1, // (1 bit)
  Port_CB2, // (1 bit)
};


class MOS6522 {
public:

  typedef void (*VIAPortIO)(MOS6522 * via, VIAPort port);

  MOS6522(Machine * machine, int tag, VIAPortIO portOut, VIAPortIO portIn);

  void reset();

  Machine * machine() { return m_machine; }

  void writeReg(int reg, int value);
  int readReg(int reg);

  bool tick(int cycles);

  uint8_t PA()                       { return m_regs[VIA_REG_ORA]; }
  void setPA(int value)              { m_regs[VIA_REG_ORA] = value; }
  void setBitPA(int bit, bool value) { m_regs[VIA_REG_ORA] = (m_regs[VIA_REG_ORA] & ~(1 << bit)); if (value) m_regs[VIA_REG_ORA] |= (1 << bit); }

  uint8_t PB()                       { return m_regs[VIA_REG_ORB]; }
  void setPB(int value)              { m_regs[VIA_REG_ORB] = value; }
  void setBitPB(int bit, bool value) { m_regs[VIA_REG_ORB] = (m_regs[VIA_REG_ORB] & ~(1 << bit)); if (value) m_regs[VIA_REG_ORB] |= (1 << bit); }

  uint8_t CA1()                      { return m_CA1; }
  void setCA1(int value)             { m_CA1_prev = m_CA1; m_CA1 = value; }

  uint8_t CA2()                      { return m_CA2; }
  void setCA2(int value)             { m_CA2_prev = m_CA2; m_CA2 = value; }

  uint8_t CB1()                      { return m_CB1; }
  void setCB1(int value)             { m_CB1_prev = m_CB1; m_CB1 = value; }

  uint8_t CB2()                      { return m_CB2; }
  void setCB2(int value)             { m_CB2_prev = m_CB2; m_CB2 = value; }

  uint8_t DDRA()                     { return m_regs[VIA_REG_DDRA]; }

  uint8_t DDRB()                     { return m_regs[VIA_REG_DDRB]; }

  uint8_t tag()                      { return m_tag; }

  #if DEBUGMSG
  void dump();
  #endif

private:

  Machine *  m_machine;
  int        m_timer1Counter;
  uint16_t   m_timer1Latch;
  int        m_timer2Counter;
  uint8_t    m_regs[16];
  uint8_t    m_timer2Latch; // timer 2 latch is 8 bits
  uint8_t    m_tag;
  uint8_t    m_CA1;
  uint8_t    m_CA1_prev;
  uint8_t    m_CA2;
  uint8_t    m_CA2_prev;
  uint8_t    m_CB1;
  uint8_t    m_CB1_prev;
  uint8_t    m_CB2;
  uint8_t    m_CB2_prev;
  bool       m_timer1Triggered;
  bool       m_timer2Triggered;
  VIAPortIO  m_portOut;
  VIAPortIO  m_portIn;
  uint32_t   m_IFR;
  uint32_t   m_IER;
  uint32_t   m_ACR;
};


/////////////////////////////////////////////////////////////////////////////////////////////
// "tries" to emulate VIC6561 noise generator
// derived from a reverse enginnered VHDL code: http://www.denial.shamani.dk/bb/viewtopic.php?t=8733&start=210

class VICNoiseGenerator : public WaveformGenerator {
public:
  VICNoiseGenerator();

  void setFrequency(int value);
  uint16_t frequency() { return m_frequency; }

  int getSample();

private:
  static const uint16_t LFSRINIT = 0x0202;
  static const int      CLK      = 4433618;

  uint16_t m_frequency;
  uint16_t m_counter;
  uint16_t m_LFSR;
  uint16_t m_outSR;
};


/////////////////////////////////////////////////////////////////////////////////////////////
// VIC (6561 - Video Interface Chip)


class MOS6561 {

public:

  static const int CharWidth          = 8;

  // assume VGA_256x384_60Hz
  static const int VGAWidth           = 256;
  static const int VGAHeight          = 384;

  // PAL specific params
  static const int PHI2               = 4433618;  // VIC frequency (CPU frequency is PHI2/4)
  static const int FrameWidth         = 284;   // includes horizontal blanking (must be a multiple of 4)
  static const int FrameHeight        = 312;   // includes vertical blanking
  static const int HorizontalBlanking = 63;
  static const int VerticalBlanking   = 28;
  static const int ScreenWidth        = FrameWidth - HorizontalBlanking;  // ScreenWidth  = 233
  static const int ScreenHeight       = FrameHeight - VerticalBlanking;   // ScreenHeight = 284
  static const int ScreenOffsetX      = (((VGAWidth - ScreenWidth) / 2) & 0xffc) + 4; // must be 32 bit aligned
  static const int ScreenOffsetY      = (VGAHeight - ScreenHeight) / 2;
  static const int CyclesPerFrame     = FrameWidth * FrameHeight / 4;
  static const int MaxTextColumns     = 32;


  MOS6561(Machine * machine, fabgl::VGAController * displayController);

  void reset();

  void writeReg(int reg, int value);
  int readReg(int reg);

  void tick(int cycles);

  Machine * machine() { return m_machine; }

  void enableAudio(bool value) { m_soundGen.play(value); }


private:

  void drawNextPixels();

  Machine *               m_machine;
  fabgl::VGAController *  m_displayController;

  int                     m_charHeight;
  int                     m_colCount;
  int                     m_rowCount;
  int                     m_scanX;
  int                     m_scanY;
  int                     m_topPos;
  int                     m_leftPos;   // where text starts
  int                     m_rightPos;  // where right border starts
  int                     m_charRow;
  int                     m_inCharRow; // row inside character tab
  int                     m_Y;
  int                     m_charAreaHeight;
  int                     m_foregroundColorCode;
  int                     m_charColumn;
  bool                    m_loadChar;
  bool                    m_isVBorder;
  uint8_t                 m_charData;
  uint8_t                 m_auxColor;
  uint8_t                 m_charInvertMask;  // 0x00 = no invert, 0xff = invert
  uint32_t                m_borderColor4;    // 4 times border color
  uint32_t                m_loNibble;
  uint32_t                m_hiNibble;
  uint32_t *              m_destScanline;
  uint8_t const *         m_videoLine;
  uint8_t const *         m_colorLine;
  uint16_t                m_videoMatrixAddr; // in the CPU address space
  uint16_t                m_charTableAddr;   // in the CPU address space
  uint8_t                 m_regs[16];
  uint8_t                 m_mcolors[4];
  uint8_t                 m_hcolors[2];

  SquareWaveformGenerator m_sqGen1;
  SquareWaveformGenerator m_sqGen2;
  SquareWaveformGenerator m_sqGen3;
  VICNoiseGenerator       m_noGen;
  SoundGenerator          m_soundGen;
};



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

  MOS6522 & VIA1() { return m_VIA1; }
  MOS6522 & VIA2() { return m_VIA2; }
  MOS6561 & VIC()  { return m_VIC; }
  MOS6502 & CPU()  { return m_CPU; }

  void setKeyboard(VirtualKey key, bool down);
  void resetKeyboard();

  void setJoy(Joy joy, bool value) { m_JOY[joy] = value; }
  void resetJoy();
  void setJoyEmu(JoyEmu value) { m_joyEmu = value; }
  JoyEmu joyEmu()              { return m_joyEmu; }

  void loadPRG(char const * filename, bool resetRequired, bool execRun);

  int loadCRT(char const * filename, bool reset, int address = -1);

  void removeCRT();

  uint8_t busRead(int addr);

  uint8_t busReadCharDefs(int addr);
  uint8_t const * busReadVideoP(int addr);
  uint8_t const * busReadColorP(int addr);

  void busWrite(int addr, uint8_t value);

  uint8_t page0Read(int addr)               { return m_RAM1K[addr]; }
  void page0Write(int addr, uint8_t value)  { m_RAM1K[addr] = value; }

  uint8_t page1Read(int addr)               { return m_RAM1K[0x100 + addr]; }
  void page1Write(int addr, uint8_t value)  { m_RAM1K[0x100 + addr] = value; }

  void type(char const * str) { m_typingString = str; }   // TODO: multiple type() calls not possible!!

  void setRAMExpansion(RAMExpansionOption value);
  RAMExpansionOption RAMExpansion() { return m_RAMExpansion; }

private:

  int VICRead(int reg);
  void VICWrite(int reg, int value);

  static void VIA1PortOut(MOS6522 * via, VIAPort port);
  static void VIA1PortIn(MOS6522 * via, VIAPort port);
  static void VIA2PortOut(MOS6522 * via, VIAPort port);
  static void VIA2PortIn(MOS6522 * via, VIAPort port);

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

  // VIA1 -> NMI, Restore key, joystick
  MOS6522               m_VIA1;

  // VIA2 -> IRQ, keyboard Col (PB0..PB7), Keyboard Row (PA0..PA7), joystick (right)
  MOS6522               m_VIA2;

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

  // triggered by type() method
  char const *          m_typingString;

  uint32_t              m_lastSyncCycle;
  uint64_t              m_lastSyncTime;   // uS

};



