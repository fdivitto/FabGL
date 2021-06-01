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


#include "machine.h"

#include "fabgl.h"

#include "../ROM/kernal_rom.h"
#include "../ROM/basic_rom.h"
#include "../ROM/char_rom.h"



#if DEBUGIEC
int testTiming = 0;
#endif


#pragma GCC optimize ("O2")


Machine::Machine(fabgl::VGAController * displayController)
  : m_VIA1(1),
    m_VIA2(2),
    m_VIC(this, displayController),
    m_joyEmu(JE_CursorKeys),
    m_IECDrive(this, 8)
{
  m_RAM1K    = new uint8_t[0x0400];
  m_RAM4K    = new uint8_t[0x1000];
  m_RAMColor = new uint8_t[0x0400];

  m_expRAM[0] = m_expRAM[1] = m_expRAM[2] = m_expRAM[3] = m_expRAM[4] = nullptr;
  m_expROM[0] = m_expROM[1] = m_expROM[2] = m_expROM[3] = nullptr;

  m_CPU.setCallbacks(this, busRead, busWrite, page0Read, page0Write, page1Read, page1Write);

  m_VIA1.setCallbacks(this, VIA1PortIn, VIA1PortOut);
  m_VIA2.setCallbacks(this, VIA2PortIn, VIA2PortOut);

  reset();
}


Machine::~Machine()
{
  removeCRT();

  delete [] m_RAM1K;
  delete [] m_RAM4K;
  delete [] m_RAMColor;

  for (int i = 0; i < 5; ++i)
    delete [] m_expRAM[i];
}


void Machine::reset()
{
  #if DEBUGMACHINE
  printf("Reset\n");
  #endif

  m_NMI           = false;
  m_typingString  = nullptr;
  m_lastSyncCycle = 0;
  m_lastSyncTime  = 0;

  VIA1().reset();
  VIA2().reset();

  VIC().reset();

  resetJoy();
  resetKeyboard();

  m_IECDrive.reset();

  m_cycle = m_CPU.reset();
}


// 0: 3K expansion (0x0400 - 0x0fff)
// 1: 8K expansion (0x2000 - 0x3fff)
// 2: 8K expansion (0x4000 - 0x5fff)
// 3: 8K expansion (0x6000 - 0x7fff)
// 4: 8K expansion (0xA000 - 0xBfff)
void Machine::enableRAMBlock(int block, bool enabled)
{
  static const uint16_t BLKSIZE[] = { 0x0c00, 0x2000, 0x2000, 0x2000, 0x2000 };
  if (enabled && !m_expRAM[block]) {
    m_expRAM[block] = new uint8_t[BLKSIZE[block]];
  } else if (!enabled && m_expRAM[block]) {
    delete [] m_expRAM[block];
    m_expRAM[block] = nullptr;
  }
}


void Machine::setRAMExpansion(RAMExpansionOption value)
{
  static const uint8_t CONFS[RAM_35K + 1][5] = { { 0, 0, 0, 0, 0 },   // RAM_unexp
                                                 { 1, 0, 0, 0, 0 },   // RAM_3K
                                                 { 0, 1, 0, 0, 0 },   // RAM_8K
                                                 { 0, 1, 1, 0, 0 },   // RAM_16K
                                                 { 0, 1, 1, 1, 0 },   // RAM_24K
                                                 { 1, 1, 1, 1, 0 },   // RAM_27K
                                                 { 0, 1, 1, 1, 1 },   // RAM_32K
                                                 { 1, 1, 1, 1, 1 },   // RAM_35K
                                               };
  for (int i = 0; i < 5; ++i)
    enableRAMBlock(i, CONFS[value][i]);
  m_RAMExpansion = value;
}


void Machine::resetKeyboard()
{
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col)
      m_KBD[row][col] = 0;
}


int Machine::run()
{
  int runCycles = 0;
  while (runCycles < MOS6561::CyclesPerFrame) {

    int cycles = m_CPU.step();

    #if DEBUGIEC
    testTiming += cycles;
    #endif

    // VIA1
    if (m_VIA1.tick(cycles) != m_NMI) {  // true = NMI request
      // NMI happens only on transition high->low (that is when was m_NMI=false)
      m_NMI = !m_NMI;
      if (m_NMI) {
        int addCycles = m_CPU.NMI();
        cycles += addCycles;
        m_VIA1.tick(addCycles);
      }
    }

    // VIA2
    if (m_VIA2.tick(cycles)) { // true = IRQ request
      int addCycles = m_CPU.IRQ();
      cycles += addCycles;
      m_VIA1.tick(addCycles); // TODO: may this to miss an MMI?
      m_VIA2.tick(addCycles);
    }

    // VIC
    m_VIC.tick(cycles);

    // IECDrive
    if (m_IECDrive.isActive())
      m_IECDrive.tick(cycles);

    runCycles += cycles;
  }

  m_cycle += runCycles;

  handleCharInjecting();
  handleMouse();
  syncTime();

  return runCycles;
}


void Machine::handleCharInjecting()
{
  // char injecting?
  while (m_typingString) {
    int kbdBufSize = busRead(0x00C6);   // $00C6 = number of chars in keyboard buffer
    if (kbdBufSize < busRead(0x0289)) { // $0289 = maximum keyboard buffer size
      busWrite(0x0277 + kbdBufSize, *m_typingString++); // $0277 = keyboard buffer
      busWrite(0x00C6, ++kbdBufSize);
      if (!*m_typingString)
        m_typingString = nullptr;
    } else
      break;
  }
}


void Machine::handleMouse()
{
  // read mouse deltas (emulates joystick)
  if (m_joyEmu == JE_Mouse) {
    setJoy(JoyUp,    false);
    setJoy(JoyDown,  false);
    setJoy(JoyLeft,  false);
    setJoy(JoyRight, false);
    auto mouse = fabgl::PS2Controller::mouse();
    if (mouse->deltaAvailable()) {
      MouseDelta d;
      mouse->getNextDelta(&d);
      if (d.deltaX < 0)
        setJoy(JoyLeft,  true);
      else if (d.deltaX > 0)
        setJoy(JoyRight, true);
      if (d.deltaY > 0)
        setJoy(JoyUp,    true);
      else if (d.deltaY < 0)
        setJoy(JoyDown,  true);
      setJoy(JoyFire, d.buttons.left | d.buttons.middle | d.buttons.right);
    }
  }
}


// delay number of cycles elapsed since last call to syncTime()
void Machine::syncTime()
{
  int diffNS = (int) (esp_timer_get_time() - m_lastSyncTime) * 1000;
  int delayNS = (m_cycle - m_lastSyncCycle) * 900/*1108*/ - diffNS;
  if (delayNS > 0 && delayNS < 30000000) {
    ets_delay_us(delayNS / 1000);
  }

  m_lastSyncCycle = m_cycle;
  m_lastSyncTime  = esp_timer_get_time();
}


// only addresses restricted to characters definitions
uint8_t Machine::busReadCharDefs(int addr)
{
  int block = (addr >> 12) & 0xf; // 4K blocks
  switch (block) {
    case 0:
      // 1K RAM (0000-03FF)
      return m_RAM1K[addr & 0x3ff];
    case 1:
      // 4K RAM (1000-1FFF)
      return m_RAM4K[addr & 0xFFF];
    default:
      return char_rom[addr & 0xfff];
  }
}


// only addresses restricted to video RAM
uint8_t const * Machine::busReadVideoP(int addr)
{
  // 1K RAM (0000-03FF)
  if (addr < 0x400)
    return m_RAM1K + addr;

  // 4K RAM (1000-1FFF)
  else
    return m_RAM4K + (addr & 0xFFF);
}


// only addresses restricted to color RAM
uint8_t const * Machine::busReadColorP(int addr)
{
  return m_RAMColor + (addr & 0x3ff);
}


uint8_t Machine::busRead(int addr)
{
  switch ((addr >> 12) & 0xf) { // 4K blocks

    case 0:
      // 1K RAM (0000-03FF)
      if (addr < 0x400)
        return m_RAM1K[addr];
      // 3K RAM Expansion (0400-0FFF)
      else if (m_expRAM[0])
        return m_expRAM[0][addr - 0x400];
      break;

    case 1:
      // 4K RAM (1000-1FFF)
      return m_RAM4K[addr & 0xFFF];

    case 2 ... 3:
      // 8K RAM Expansion or Cartridge (2000-3FFF)
      if (m_expROM[0])
        return m_expROM[0][addr & 0x1fff];
      else if (m_expRAM[1])
        return m_expRAM[1][addr & 0x1fff];
      break;

    case 4 ... 5:
      // 8K RAM expansion or Cartridge (4000-5fff)
      if (m_expROM[1])
        return m_expROM[1][addr & 0x1fff];
      else if (m_expRAM[2])
        return m_expRAM[2][addr & 0x1fff];
      break;

    case 6 ... 7:
      // 8K RAM expansion or Cartridge (6000-7fff)
      if (m_expROM[2])
        return m_expROM[2][addr & 0x1fff];
      else if (m_expRAM[3])
        return m_expRAM[3][addr & 0x1fff];
      break;

    case 8:
      // 4K ROM (8000-8FFF)
      return char_rom[addr & 0xfff];

    case 9:
    {
      // VIC (9000-90FF)
      switch ((addr >> 8) & 0xf) {
        case 0:
          return m_VIC.readReg(addr & 0xf);
        case 1 ... 3:
          // VIAs (9100-93FF)
          if (addr & 0x10)
            return m_VIA1.readReg(addr & 0xf);
          else if (addr & 0x20)
            return m_VIA2.readReg(addr & 0xf);
          break;
        case 4 ... 7:
          // 1Kx4 RAM (9400-97FF)
          return m_RAMColor[addr & 0x3ff] & 0x0f;
      }
      break;
    }

    case 0xa ... 0xb:
      // 8K Cartridge or RAM expansion (A000-BFFF)
      if (m_expROM[3])
        return m_expROM[3][addr & 0x1fff];
      else if (m_expRAM[4])
        return m_expRAM[4][addr & 0x1fff];
      break;

    case 0xc ... 0xd:
      // 8K ROM (C000-DFFF)
      return basic_rom[addr & 0x1fff];

    case 0xe ... 0xf:
      // 8K ROM (E000-FFFF)
      return kernal_rom[addr & 0x1fff];

  }

  // unwired address returns high byte of the address
  return addr >> 8;
}


void Machine::busWrite(int addr, uint8_t value)
{
  // optimization for zero page, stack... (from tests this is useful just on write!)
  // 1K RAM (0000-03FF)
  if (addr < 0x400) {
    m_RAM1K[addr] = value;
    return;
  }

  switch ((addr >> 12) & 0xf) { // 4K blocks

    case 0:
      // 3K RAM Expansion (0400-0FFF)
      if (m_expRAM[0])
        m_expRAM[0][addr - 0x400] = value;
      break;

    case 1:
      // 4K RAM (1000-1FFF)
      m_RAM4K[addr & 0xFFF] = value;
      break;

    case 2 ... 3:
      // 8K RAM Expansion (2000-3FFF)
      if (m_expRAM[1])
        m_expRAM[1][addr & 0x1fff] = value;
      break;

    case 4 ... 5:
      // 8K RAM expansion (4000-5fff)
      if (m_expRAM[2])
        m_expRAM[2][addr & 0x1fff] = value;
      break;

    case 6 ... 7:
      // 8K RAM expansion (6000-7fff)
      if (m_expRAM[3])
        m_expRAM[3][addr & 0x1fff] = value;
      break;

    case 9:
    {
      switch ((addr >> 8) & 0xf) {
        case 0:
          // VIC (9000-90FF)
          m_VIC.writeReg(addr & 0xf, value);
          break;
        case 1 ...3:
          // VIAs (9100-93FF)
          if (addr & 0x10)
            m_VIA1.writeReg(addr & 0xf, value);
          else if (addr & 0x20)
            m_VIA2.writeReg(addr & 0xf, value);
          break;
        case 4 ... 7:
          // 1Kx4 RAM (9400-97FF)
          m_RAMColor[addr & 0x3ff] = value;
          break;
      }
      break;
    }

    case 0xa ... 0xb:
      // RAM expansion (A000-BFFF)
      if (m_expRAM[4])
        m_expRAM[4][addr & 0x1fff] = value;
      break;

  }
}


/*
   VIC      |  PS/2
   ---------+----------
   CLR/HOME |  HOME
   RUNSTOP  |  ESC
   CBM      |  LGUI      (Left Windows Key)
   RESTORE  |  DELETE    (CANC)
   INST/DEL |  ⌫         (BACKSPACE)
   ↑        |  ^         (caret)
   ←        |  _         (underscore)
   π        |  ~         (tilde)

   Still TODO (when to produce these symbols you need to press SHIFT on your keyboard):
     SHIFT or CBM + @ .... produce graphic chars
     SHIFT or CBM + ↑ .... produce graphic chars
     SHIFT or CBM + + .... produce graphic chars
     SHIFT or CBM + - .... produce graphic chars
     SHIFT or CBM + £ .... produce graphic chars
     SHIFT or CBM + * .... produce graphic chars
     SHIFT or CBM + ↑ .... produce graphic chars
*/
void Machine::setKeyboard(VirtualKey key, bool down)
{
  auto keyboard = fabgl::PS2Controller::keyboard();

  #if DEBUGMACHINE
  Serial.printf("VirtualKey = %s %s\n", keyboard->virtualKeyToString(key), down ? "DN" : "UP");
  #endif

  switch (key) {

    // 0
    case VirtualKey::VK_0:
      m_KBD[4][7] = down;
      break;

    // 1
    case VirtualKey::VK_1:
      m_KBD[0][0] = down;
      break;

    // 2
    case VirtualKey::VK_2:
      m_KBD[0][7] = down;
      break;

    // 3
    case VirtualKey::VK_3:
      m_KBD[1][0] = down;
      break;

    // 4
    case VirtualKey::VK_4:
      m_KBD[1][7] = down;
      break;

    // 5
    case VirtualKey::VK_5:
      m_KBD[2][0] = down;
      break;

    // 6
    case VirtualKey::VK_6:
      m_KBD[2][7] = down;
      break;

    // 7
    case VirtualKey::VK_7:
      m_KBD[3][0] = down;
      break;

    // 8
    case VirtualKey::VK_8:
      m_KBD[3][7] = down;
      break;

    // 9
    case VirtualKey::VK_9:
      m_KBD[4][0] = down;
      break;

    // w
    case VirtualKey::VK_w:
      if (keyboard->isVKDown(VirtualKey::VK_LALT)) {
        // LALT-W move screen up
        if (down) {
          int c = (int) VIC().readReg(1) - 1;
          if (c < 0) c = 0;
          VIC().writeReg(1, c);
        }
        break;
      }
      m_KBD[1][1] = down;
      break;

    // W
    case VirtualKey::VK_W:
      m_KBD[1][1] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // r
    case VirtualKey::VK_r:
      m_KBD[2][1] = down;
      break;

    // R
    case VirtualKey::VK_R:
      m_KBD[2][1] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // y
    case VirtualKey::VK_y:
      m_KBD[3][1] = down;
      break;

    // Y
    case VirtualKey::VK_Y:
      m_KBD[3][1] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // i
    case VirtualKey::VK_i:
      m_KBD[4][1] = down;
      break;

    // I
    case VirtualKey::VK_I:
      m_KBD[4][1] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // p
    case VirtualKey::VK_p:
      m_KBD[5][1] = down;
      break;

    // P
    case VirtualKey::VK_P:
      m_KBD[5][1] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // a
    case VirtualKey::VK_a:
      if (keyboard->isVKDown(VirtualKey::VK_LALT)) {
        // ALT-A move screen left
        if (down) {
          int c = (int)(VIC().readReg(0) & 0x7f) - 1;
          if (c < 0) c = 0;
          VIC().writeReg(0, c);
        }
        break;
      }
      m_KBD[1][2] = down;
      break;

    // A
    case VirtualKey::VK_A:
      m_KBD[1][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // d
    case VirtualKey::VK_d:
      m_KBD[2][2] = down;
      break;

    // D
    case VirtualKey::VK_D:
      m_KBD[2][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // g
    case VirtualKey::VK_g:
      m_KBD[3][2] = down;
      break;

    // G
    case VirtualKey::VK_G:
      m_KBD[3][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // j
    case VirtualKey::VK_j:
      m_KBD[4][2] = down;
      break;

    // J
    case VirtualKey::VK_J:
      m_KBD[4][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // l
    case VirtualKey::VK_l:
      m_KBD[5][2] = down;
      break;

    // L
    case VirtualKey::VK_L:
      m_KBD[5][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // x
    case VirtualKey::VK_x:
      m_KBD[2][3] = down;
      break;

    // X
    case VirtualKey::VK_X:
      m_KBD[2][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // v
    case VirtualKey::VK_v:
      m_KBD[3][3] = down;
      break;

    // V
    case VirtualKey::VK_V:
      m_KBD[3][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // n
    case VirtualKey::VK_n:
      m_KBD[4][3] = down;
      break;

    // N
    case VirtualKey::VK_N:
      m_KBD[4][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // z
    case VirtualKey::VK_z:
      if (keyboard->isVKDown(VirtualKey::VK_LALT)) {
        // ALT-Z move screen down
        if (down) {
          int c = (int) VIC().readReg(1) + 1;
          if (c > 255) c = 255;
          VIC().writeReg(1, c);
        }
        break;
      }
      m_KBD[1][4] = down;
      break;

    // Z
    case VirtualKey::VK_Z:
      m_KBD[1][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // c
    case VirtualKey::VK_c:
      m_KBD[2][4] = down;
      break;

    // C
    case VirtualKey::VK_C:
      m_KBD[2][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // b
    case VirtualKey::VK_b:
      m_KBD[3][4] = down;
      break;

    // B
    case VirtualKey::VK_B:
      m_KBD[3][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // m
    case VirtualKey::VK_m:
      m_KBD[4][4] = down;
      break;

    // M
    case VirtualKey::VK_M:
      m_KBD[4][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // s
    case VirtualKey::VK_s:
      if (keyboard->isVKDown(VirtualKey::VK_LALT)) {
        // ALT-S move screen right
        if (down) {
          int c = (int)(VIC().readReg(0) & 0x7f) + 1;
          if (c == 128) c = 127;
          VIC().writeReg(0, c);
        }
        break;
      }
      m_KBD[1][5] = down;
      break;

    // S
    case VirtualKey::VK_S:
      m_KBD[1][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // f
    case VirtualKey::VK_f:
      m_KBD[2][5] = down;
      break;

    // F
    case VirtualKey::VK_F:
      m_KBD[2][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // h
    case VirtualKey::VK_h:
      m_KBD[3][5] = down;
      break;

    // H
    case VirtualKey::VK_H:
      m_KBD[3][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // k
    case VirtualKey::VK_k:
      m_KBD[4][5] = down;
      break;

    // K
    case VirtualKey::VK_K:
      m_KBD[4][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // q
    case VirtualKey::VK_q:
      m_KBD[0][6] = down;
      break;

    // Q
    case VirtualKey::VK_Q:
      m_KBD[0][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // e
    case VirtualKey::VK_e:
      m_KBD[1][6] = down;
      break;

    // E
    case VirtualKey::VK_E:
      m_KBD[1][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // t
    case VirtualKey::VK_t:
      m_KBD[2][6] = down;
      break;

    // T
    case VirtualKey::VK_T:
      m_KBD[2][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // u
    case VirtualKey::VK_u:
      m_KBD[3][6] = down;
      break;

    // U
    case VirtualKey::VK_U:
      m_KBD[3][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // o
    case VirtualKey::VK_o:
      m_KBD[4][6] = down;
      break;

    // O
    case VirtualKey::VK_O:
      m_KBD[4][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // SPACE
    case VirtualKey::VK_SPACE:
      m_KBD[0][4] = down;
      break;

    // BACKSPACE -> INST/DEL
    case VirtualKey::VK_BACKSPACE:
      m_KBD[7][0] = down;
      break;

    // RETURN
    case VirtualKey::VK_RETURN:
      m_KBD[7][1] = down;
      break;

    // HOME -> CLR/HOME
    case VirtualKey::VK_HOME:
      // HOME
      m_KBD[6][7] = down;
      break;

    // ESC -> RUNSTOP
    case VirtualKey::VK_ESCAPE:
      m_KBD[0][3] = down;
      break;

    // LCTRL, RCTRL
    case VirtualKey::VK_LCTRL:
    case VirtualKey::VK_RCTRL:
      m_KBD[0][2] = down;
      break;

    // LSHIFT
    case VirtualKey::VK_LSHIFT:
      m_KBD[1][3] = down;
      break;

    // RSHIFT
    case VirtualKey::VK_RSHIFT:
      m_KBD[6][4] = down;
      break;

    // LGUI -> CBM
    case VirtualKey::VK_LGUI:
      m_KBD[0][5] = down;
      break;

    // F1
    case VirtualKey::VK_F1:
      m_KBD[7][4] = down;
      break;

    // F2
    case VirtualKey::VK_F2:
      m_KBD[7][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // F3
    case VirtualKey::VK_F3:
      m_KBD[7][5] = down;
      break;

    // F4
    case VirtualKey::VK_F4:
      m_KBD[7][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // F5
    case VirtualKey::VK_F5:
      m_KBD[7][6] = down;
      break;

    // F6
    case VirtualKey::VK_F6:
      m_KBD[7][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // F7
    case VirtualKey::VK_F7:
      m_KBD[7][7] = down;
      break;

    // F8
    case VirtualKey::VK_F8:
      m_KBD[7][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // DELETE (CANC) -> RESTORE
    case VirtualKey::VK_DELETE:
      VIA1().setCA1(!down);
      break;

    // ^ -> ↑
    case VirtualKey::VK_CARET:
      // '^' => UP ARROW (same ASCII of '^')
      m_KBD[6][6] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // ~ -> π
    case VirtualKey::VK_TILDE:
      // '~' => pi
      m_KBD[6][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // =
    case VirtualKey::VK_EQUALS:
      m_KBD[6][5] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // £
    case VirtualKey::VK_POUND:
      m_KBD[6][0] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // /
    case VirtualKey::VK_SLASH:
      m_KBD[6][3] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // !
    case VirtualKey::VK_EXCLAIM:
      m_KBD[0][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // $
    case VirtualKey::VK_DOLLAR:
      m_KBD[1][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // %
    case VirtualKey::VK_PERCENT:
      m_KBD[2][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // &
    case VirtualKey::VK_AMPERSAND:
      m_KBD[2][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // (
    case VirtualKey::VK_LEFTPAREN:
      m_KBD[3][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // )
    case VirtualKey::VK_RIGHTPAREN:
      m_KBD[4][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // '
    case VirtualKey::VK_QUOTE:
      m_KBD[3][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // "
    case VirtualKey::VK_QUOTEDBL:
      m_KBD[0][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // @
    case VirtualKey::VK_AT:
      m_KBD[5][6] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // ;
    case VirtualKey::VK_SEMICOLON:
      m_KBD[6][2] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // ,
    case VirtualKey::VK_COMMA:
      m_KBD[5][3] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // _
    case VirtualKey::VK_UNDERSCORE:
      // '_' => LEFT-ARROW (same ASCII of UNDERSCORE)
      m_KBD[0][1] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // -
    case VirtualKey::VK_MINUS:
      m_KBD[5][7] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // [
    case VirtualKey::VK_LEFTBRACKET:
      m_KBD[5][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // ]
    case VirtualKey::VK_RIGHTBRACKET:
      m_KBD[6][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // *
    case VirtualKey::VK_ASTERISK:
      m_KBD[6][1] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // +
    case VirtualKey::VK_PLUS:
      m_KBD[5][0] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // #
    case VirtualKey::VK_HASH:
      m_KBD[1][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // >
    case VirtualKey::VK_GREATER:
      m_KBD[5][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // <
    case VirtualKey::VK_LESS:
      m_KBD[5][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // ?
    case VirtualKey::VK_QUESTION:
      m_KBD[6][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // :
    case VirtualKey::VK_COLON:
      m_KBD[5][5] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // .
    case VirtualKey::VK_PERIOD:
      m_KBD[5][4] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;

    // LEFT
    case VirtualKey::VK_LEFT:
      if (m_joyEmu == JE_CursorKeys || keyboard->isVKDown(VirtualKey::VK_RALT)) {
        // RALT-LEFT move joystick left
        setJoy(JoyLeft, down);
        break;
      }
      m_KBD[7][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // RIGHT
    case VirtualKey::VK_RIGHT:
      if (m_joyEmu == JE_CursorKeys || keyboard->isVKDown(VirtualKey::VK_RALT)) {
        // RALT-RIGHT move joystick right
        setJoy(JoyRight, down);
        break;
      }
      m_KBD[7][2] = down;
      break;

    // UP
    case VirtualKey::VK_UP:
      if (m_joyEmu == JE_CursorKeys || keyboard->isVKDown(VirtualKey::VK_RALT)) {
        // RALT-UP move joystick up
        setJoy(JoyUp, down);
        break;
      }
      m_KBD[7][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;

    // DOWN
    case VirtualKey::VK_DOWN:
      if (m_joyEmu == JE_CursorKeys || keyboard->isVKDown(VirtualKey::VK_RALT)) {
        // RALT-DOWN move joystick down
        setJoy(JoyDown, down);
        break;
      }
      m_KBD[7][3] = down;
      break;

    // RMENU -> JOYSTICK FIRE
    case VirtualKey::VK_APPLICATION:  // also called MENU key
      if (m_joyEmu == JE_CursorKeys || keyboard->isVKDown(VirtualKey::VK_RALT)) {
        // RALT-MENU fires joystick
        setJoy(JoyFire, down);
        break;
      }
      break;

    default:
      break;
  }

  #if DEBUGMACHINE
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x)
      Serial.printf("%02X ", m_KBD[y][x]);
    Serial.printf("\n");
  }
  #endif

}


void Machine::VIA1PortIn(void * context, VIA6522 * via, VIA6522Port port)
{
  auto m = (Machine*)context;

  switch (port) {
    case VIA6522Port::PA:
      if (m->m_JOY[JoyUp])
        via->setBitPA(2, 0);
      else
        via->openBitPA(2);
      if (m->m_JOY[JoyDown])
        via->setBitPA(3, 0);
      else
        via->openBitPA(3);
      if (m->m_JOY[JoyLeft])
        via->setBitPA(4, 0);
      else
        via->openBitPA(4);
      if (m->m_JOY[JoyFire])
        via->setBitPA(5, 0);
      else
        via->openBitPA(5);
      break;

    default:
      break;
  }
}


void Machine::VIA1PortOut(void * context, VIA6522 * via, VIA6522Port port)
{
  auto m = (Machine*)context;

  switch (port) {
    case VIA6522Port::PA:
      #if DEBUGIEC
      testTiming = 0;
      printf("%d: ATN => %s\n", testTiming, ((via->PA() & 0x80) >> 7) ? "true" : "false");
      #endif
      // don't need to negate because VIC20 has inverters out of DATA
      m->m_IECDrive.setInputATN((via->PA() & 0x80) != 0);
      break;

    default:
      break;
  }
}


void Machine::VIA2PortIn(void * context, VIA6522 * via, VIA6522Port port)
{
  auto m = (Machine*)context;

  switch (port) {

    case VIA6522Port::PB:
      via->setPB(m->m_colStatus);
      if (m->m_JOY[JoyRight])
        via->setBitPB(7, 0);
      break;

    case VIA6522Port::PA:
      via->setPA(m->m_rowStatus);
      break;

    default:
      break;
  }
}


void Machine::VIA2PortOut(void * context, VIA6522 * via, VIA6522Port port)
{
  auto m = (Machine*)context;

  switch (port) {

    // output on PA, select keyboard Row (store Column in PB)
    case VIA6522Port::PA:
    {
      // keyboard can also be queried using PA as output and PB as input
      int PB = 0;
      int row = ~(via->PA());
      if (row) {
        for (int r = 0; r < 8; ++r)
          if (row & (1 << r))
            for (int c = 0; c < 8; ++c)
              PB |= (m->m_KBD[r][c] & 1) << c;
      }
      m->m_colStatus = ~PB;
      break;
    }

    // output on PB, select keyboard Column (store Row in PA)
    case VIA6522Port::PB:
    {
      int PA = 0;
      int col = ~(via->PB());
      if (col) {
        for (int c = 0; c < 8; ++c)
          if (col & (1 << c))
            for (int r = 0; r < 8; ++r)
              PA |= (m->m_KBD[r][c] & 1) << r;
      }
      m->m_rowStatus = ~PA;
      break;
    }

    case VIA6522Port::CA2:
      #if DEBUGIEC
      printf("%d: CLK => %s\n", testTiming, via->CA2() ? "true" : "false");
      #endif
      m->m_IECDrive.setInputCLK(via->CA2());
      break;

    case VIA6522Port::CB2:
      #if DEBUGIEC
      printf("%d: DATA => %s\n", testTiming, via->CB2() ? "true" : "false");
      #endif
      // don't need to negate because VIC20 has inverters out of DATA
      m->m_IECDrive.setInputDATA(via->CB2());
      break;

    default:
      break;

  }

}


void Machine::loadPRG(char const * filename, bool resetRequired, bool execRun)
{
  FILE * f = fopen(filename, "rb");
  if (f) {

    // get file size
    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > 2) {

      // need to reset?
      if (resetRequired) {
        reset();
        // wait for kernel ends boot
        // 0xc9 is a Kernal variable that holds the input cursor row, it should be 5 just when "READY." has been displayed.
        busWrite(0xc9, 0);
        while (busRead(0xc9) != 5)
          run();
      }

      int loadAddr = fgetc(f) | (fgetc(f) << 8);
      size -= 2;
      for (int i = 0; i < size; ++i)
        busWrite(loadAddr + i, fgetc(f));

      //// set basic pointers

      // read "Start of Basic"
      int lo = busRead(0x2b);
      int hi = busRead(0x2c);
      int basicStart = lo | (hi << 8);
      int basicEnd   = basicStart + (int)size;

      // "Tape buffer scrolling"
      busWrite(0xac, 0);
      busWrite(0xad, 0);

      lo = basicEnd & 0xff;
      hi = (basicEnd >> 8) & 0xff;

      // "Start of Variables"
      busWrite(0x2d, lo);
      busWrite(0x2e, hi);

      // "Start of Arrays"
      busWrite(0x2f, lo);
      busWrite(0x30, hi);

      // "End of Arrays"
      busWrite(0x31, lo);
      busWrite(0x32, hi);

      // "Tape end addresses/End of program"
      busWrite(0xae, lo);
      busWrite(0xaf, hi);

      if (execRun)
        type("RUN\r");
    }

    fclose(f);
  }
}


// Address can be: 0x2000, 0x4000, 0x6000 or 0xA000. -1 = auto
// block 0 : 0x2000
// block 1 : 0x4000
// block 2 : 0x6000
// block 3 : 0xA000
// If size is >4096 or >8192 then first bytes are discarded
// return effective load address
int Machine::loadCRT(char const * filename, bool reset, int address)
{
  FILE * f = fopen(filename, "rb");
  if (f) {

    // get file size
    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // get from data or default to 0xa000
    if (address == -1 || size == 4098 || size == 8194) {
      address = fgetc(f) | (fgetc(f) << 8);
      size -= 2;
    }
    int block = (address == 0x2000 ? 0 : (address == 0x4000 ? 1 : (address == 0x6000 ? 2 : 3)));
    for (; size != 4096 && size != 8192; --size)
      fgetc(f); // bypass
    m_expROM[block] = (uint8_t*) malloc(size);
    fread(m_expROM[block], 1, size, f);

    if (reset)
      this->reset();

    fclose(f);

  }
  return address;
}


void Machine::removeCRT()
{
  for (int i = 0; i < 4; ++i) {
    free(m_expROM[i]);
    m_expROM[i] = nullptr;
  }
}


void Machine::resetJoy()
{
  for (int i = JoyUp; i <= JoyFire; ++i)
    setJoy((Joy)i, false);
}


void Machine::setJoy(Joy joy, bool value)
{
  m_JOY[joy] = value;
}




/////////////////////////////////////////////////////////////////////////////////////////////
// PRGCreator


PRGCreator::PRGCreator(int startingAddress)
  : m_startingAddress(startingAddress)
{
  m_prglen = 4; // 2 for starting address, 2 for next line address
  m_prg    = (uint8_t*) malloc(m_prglen);

  m_prg[0] = m_startingAddress & 0xff;
  m_prg[1] = m_startingAddress >> 8;
  m_prg[2] = 0;
  m_prg[3] = 0;
}


PRGCreator::~PRGCreator()
{
  free(m_prg);
}


// line must end with "0"
void PRGCreator::addline(int linenumber, char const * data)
{
  auto len = strlen((char const*)data);
  addline(linenumber, data, len);
}


// datalen must NOT include ending zero
void PRGCreator::addline(int linenumber, void const * data, size_t datalen)
{
  auto addlen = 2 + datalen + 1 + 2;  // linenumber (2) + datalen + endingzero (1) + nextlineaddr (2)
  m_prg = (uint8_t*) realloc(m_prg, m_prglen + addlen);
  auto nextLineAddr = m_startingAddress + m_prglen + addlen - 2;
  m_prg[m_prglen - 2] = nextLineAddr & 0xff;
  m_prg[m_prglen - 1] = nextLineAddr >> 8;
  m_prg[m_prglen + 0] = linenumber & 0xff;
  m_prg[m_prglen + 1] = linenumber >> 8;
  memcpy(m_prg + m_prglen + 2, data, datalen);
  m_prg[m_prglen + 2 + datalen] = 0;
  m_prg[m_prglen + 2 + datalen + 1] = 0;
  m_prg[m_prglen + 2 + datalen + 2] = 0;
  m_prglen += addlen;
}






