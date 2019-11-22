#include "machine.h"

#include "ROM/kernal_rom.h"
#include "ROM/basic_rom.h"
#include "ROM/char_rom.h"




Machine::Machine()
  : m_CPU(this),
    m_VIA1(this, 1, &Machine::VIA1PortOut, &Machine::VIA1PortIn),
    m_VIA2(this, 2, &Machine::VIA2PortOut, &Machine::VIA2PortIn),
    m_VIC(this)
{
  m_RAM1K    = new uint8_t[0x0400];
  m_RAM4K    = new uint8_t[0x1000];
  m_RAMColor = new uint8_t[0x0400];

  m_expRAM[0] = m_expRAM[1] = m_expRAM[2] = m_expRAM[3] = m_expRAM[4] = nullptr;
  m_expROM[0] = m_expROM[1] = m_expROM[2] = m_expROM[3] = nullptr;

  reset();
}


Machine::~Machine()
{
  delete [] m_RAM1K;
  delete [] m_RAM4K;
  delete [] m_RAMColor;

  for (int i = 0; i < 4; ++i)
    delete [] m_expRAM[i];
}


void Machine::reset()
{
  #if DEBUGMSG
  printf("Reset\n");
  #endif

  m_NMI           = false;
  m_lastSyncCycle = 0;
  m_typingString  = nullptr;
  m_lastSyncTime  = 0;

  VIA1().reset();
  VIA2().reset();

  VIC().reset();

  // TODO: check these!
  VIA1().setCA1(1);   // restore high (pulled up)
  VIA1().setPA(0x7E);
  VIA1().setPB(0xFF);

  resetJoy();
  resetKeyboard();

  m_cycle = m_CPU.Reset();
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


void Machine::setRAMExpansion(RAMExpansion value)
{
  static const uint8_t CONFS[RAM_35K + 1][5] = { { 1, 0, 0, 0, 0 },   // RAM_3K
                                                 { 0, 1, 0, 0, 0 },   // RAM_8K
                                                 { 0, 1, 1, 0, 0 },   // RAM_16K
                                                 { 0, 1, 1, 1, 0 },   // RAM_24K
                                                 { 1, 1, 1, 1, 0 },   // RAM_27K
                                                 { 0, 1, 1, 1, 1 },   // RAM_32K
                                                 { 1, 1, 1, 1, 1 },   // RAM_35K
                                               };
  for (int i = 0; i < 5; ++i)
    enableRAMBlock(i, CONFS[value][i]);
}


// Address can be: 0x2000, 0x4000, 0x6000 or 0xA000. -1 = auto
// block 0 : 0x2000
// block 1 : 0x4000
// block 2 : 0x6000
// block 3 : 0xA000
// If size is >4096 or >8192 then first bytes are discarded
void Machine::setCartridge(uint8_t const * data, size_t size, bool reset, int address)
{
  // get from data or default to 0xa000
  if (address == -1 && (size == 4098 || size == 8194)) {
    address = data[0] | (data[1] << 8);
    size -= 2;
    data += 2;
  }
  int block = (address == 0x2000 ? 0 : (address == 0x4000 ? 1 : (address == 0x6000 ? 2 : 3)));
  for (; size != 4096 && size != 8192; --size)
    ++data;
  m_expROM[block] = data;
  if (reset)
    this->reset();
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

    int cycles = m_CPU.Run();

    // update timers, current scanline, check interrupts...
    for (int c = 0; c < cycles; ++c) {

  ///*
      // VIA1
      if (m_VIA1.tick() != m_NMI) {  // true = NMI request
        // NMI happens only on transition high->low (that is when was m_NMI=false)
        m_NMI = !m_NMI;
        if (m_NMI)
          cycles += m_CPU.NMI();
      }
  //*/

  ///*
      // VIA2
      if (m_VIA2.tick())  // true = IRQ request
        cycles += m_CPU.IRQ();
  //*/

    }

  ///*
    // VIC
    m_VIC.tick(cycles);
  //*/

    runCycles += cycles;
  }

  m_cycle += runCycles;

  handleCharInjecting();
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


// delay number of cycles elapsed since last call to syncTime()
void Machine::syncTime()
{
  //uint64_t t1 = SDL_GetPerformanceCounter() + (m_cycle - m_lastSyncCycle) * 1000 /*1108*/;  // ns
  //while (SDL_GetPerformanceCounter() < t1)
  //  ;
  /*
  int diff = (int) (SDL_GetPerformanceCounter() - m_lastSyncTime);
  int delayNS = (m_cycle - m_lastSyncCycle) * 1108 - diff;
  if (delayNS > 0 && delayNS < 30000000) {
    const timespec rqtp = { 0, delayNS };
    nanosleep(&rqtp, nullptr);
  }

  m_lastSyncCycle = m_cycle;
  m_lastSyncTime  = SDL_GetPerformanceCounter();
  */
}


// just do change PC
void Machine::go(int addr)
{
  m_CPU.setPC(addr);
}


// only addresses restricted to characters definitions
uint8_t Machine::busReadCharDefs(uint16_t addr)
{
  // 1K RAM (0000-03FF)
  if (addr < 0x400)
    return m_RAM1K[addr];

  // 4K RAM (1000-1FFF)
  else if (addr < 0x1fff)
    return m_RAM4K[addr & 0xFFF];

  // 4K ROM (8000-8FFF)
  else
    return char_rom[addr & 0xfff];
}


// only addresses restricted to video RAM
uint8_t const * Machine::busReadVideoP(uint16_t addr)
{
  // 1K RAM (0000-03FF)
  if (addr < 0x400)
    return m_RAM1K + addr;

  // 4K RAM (1000-1FFF)
  else
    return m_RAM4K + (addr & 0xFFF);
}


// only addresses restricted to color RAM
uint8_t const * Machine::busReadColorP(uint16_t addr)
{
  return m_RAMColor + (addr & 0x3ff);
}


uint8_t Machine::busRead(uint16_t addr)
{
  int addr_hi = (addr >> 8) & 0xff; // 256B blocks
  int block   = (addr >> 12) & 0xf; // 4K blocks

  // 1K RAM (0000-03FF)
  if (addr < 0x400)
    return m_RAM1K[addr];

  // 3K RAM Expansion (0400-0FFF)
  else if (block == 0 && m_expRAM[0])
    return m_expRAM[0][addr - 0x400];

  // 4K RAM (1000-1FFF)
  else if (block == 1)
    return m_RAM4K[addr & 0xFFF];

  // 8K RAM Expansion or Cartridge (2000-3FFF)
  else if (block >= 2 && block <= 3) {
    if (m_expROM[0])
      return m_expROM[0][addr & 0x1fff];
    else if (m_expRAM[1])
      return m_expRAM[1][addr & 0x1fff];
  }

  // 8K RAM expansion or Cartridge (4000-5fff)
  else if (block >= 4 && block <= 5) {
    if (m_expROM[1])
      return m_expROM[1][addr & 0x1fff];
    else if (m_expRAM[2])
      return m_expRAM[2][addr & 0x1fff];
  }

  // 8K RAM expansion or Cartridge (6000-7fff)
  else if (block >= 6 && block <= 7) {
    if (m_expROM[2])
      return m_expROM[2][addr & 0x1fff];
    else if (m_expRAM[3])
      return m_expRAM[3][addr & 0x1fff];
  }

  // 4K ROM (8000-8FFF)
  else if (block == 8)
    return char_rom[addr & 0xfff];

  // VIC (9000-90FF)
  else if (addr_hi == 0x90)
    return m_VIC.readReg(addr & 0xf);

  // VIAs (9100-93FF)
  else if (addr_hi >= 0x91 && addr_hi <= 0x93) {
    if (addr & 0x10)
      return m_VIA1.readReg(addr & 0xf);
    else if (addr & 0x20)
      return m_VIA2.readReg(addr & 0xf);
  }

  // 1Kx4 RAM (9400-97FF)
  else if (addr_hi >= 0x94 && addr_hi <= 0x97)
    return m_RAMColor[addr & 0x3ff] & 0x0f;

  // 8K Cartridge (A000-BFFF)
  else if (block >= 0xa && block <= 0xb) {
    if (m_expROM[3])
      return m_expROM[3][addr & 0x1fff];
    else if (m_expRAM[4])
      return m_expRAM[4][addr & 0x1fff];
  }

  // 8K ROM (C000-DFFF)
  else if (block >= 0xc && block <= 0xd)
    return basic_rom[addr & 0x1fff];

  // 8K ROM (E000-FFFF)
  else if (block >= 0xe && block <= 0xf)
    return kernal_rom[addr & 0x1fff];

  // unwired address returns high byte of the address
  return addr >> 8;
}


void Machine::busWrite(uint16_t addr, uint8_t value)
{
  int addr_hi = (addr >> 8) & 0xff; // 256B blocks
  int block   = (addr >> 12) & 0xf; // 4K blocks

  // 1K RAM (0000-03FF)
  if (addr < 0x400)
    m_RAM1K[addr] = value;

  // 3K RAM Expansion (0400-0FFF)
  else if (m_expRAM[0] && block == 0)
    m_expRAM[0][addr - 0x400] = value;

  // 4K RAM (1000-1FFF)
  else if (block == 1)
    m_RAM4K[addr & 0xFFF] = value;

  // 8K RAM Expansion (2000-3FFF)
  else if (m_expRAM[1] && block >= 2 && block <= 3)
    m_expRAM[1][addr & 0x1fff] = value;

  // 8K RAM expansion (4000-5fff)
  else if (m_expRAM[2] && block >= 4 && block <= 5)
    m_expRAM[2][addr & 0x1fff] = value;

  // 8K RAM expansion (6000-7fff)
  else if (m_expRAM[3] && block >= 6 && block <= 7)
    m_expRAM[3][addr & 0x1fff] = value;

  // VIC (9000-90FF)
  else if (addr_hi == 0x90)
    m_VIC.writeReg(addr & 0xf, value);

  // VIAs (9100-93FF)
  else if (addr_hi >= 0x91 && addr_hi <= 0x93) {
    if (addr & 0x10)
      m_VIA1.writeReg(addr & 0xf, value);
    else if (addr & 0x20)
      m_VIA2.writeReg(addr & 0xf, value);
  }

  // 1Kx4 RAM (9400-97FF)
  else if (addr_hi >= 0x94 && addr_hi <= 0x97)
    m_RAMColor[addr & 0x3ff] = value;

}


// TODO: check all keys!!!
void Machine::setKeyboard(VirtualKey key, bool down)
{
  switch (key) {
    case VirtualKey::VK_0:
      m_KBD[4][7] = down;
      break;
    case VirtualKey::VK_1:
      m_KBD[0][0] = down;
      break;
    case VirtualKey::VK_2:
      m_KBD[0][7] = down;
      break;
    case VirtualKey::VK_3:
      m_KBD[1][0] = down;
      break;
    case VirtualKey::VK_4:
      m_KBD[1][7] = down;
      break;
    case VirtualKey::VK_5:
      m_KBD[2][0] = down;
      break;
    case VirtualKey::VK_6:
      m_KBD[2][7] = down;
      break;
    case VirtualKey::VK_7:
      m_KBD[3][0] = down;
      break;
    case VirtualKey::VK_8:
      m_KBD[3][7] = down;
      break;
    case VirtualKey::VK_9:
      m_KBD[4][0] = down;
      break;
    case VirtualKey::VK_w:
      if (Keyboard.isVKDown(VirtualKey::VK_LALT)) {
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
    case VirtualKey::VK_r:
      m_KBD[2][1] = down;
      break;
    case VirtualKey::VK_y:
      m_KBD[3][1] = down;
      break;
    case VirtualKey::VK_i:
      m_KBD[4][1] = down;
      break;
    case VirtualKey::VK_p:
      m_KBD[5][1] = down;
      break;
    case VirtualKey::VK_a:
      if (Keyboard.isVKDown(VirtualKey::VK_LALT)) {
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
    case VirtualKey::VK_d:
      m_KBD[2][2] = down;
      break;
    case VirtualKey::VK_g:
      m_KBD[3][2] = down;
      break;
    case VirtualKey::VK_j:
      m_KBD[4][2] = down;
      break;
    case VirtualKey::VK_l:
      m_KBD[5][2] = down;
      break;
    case VirtualKey::VK_x:
      m_KBD[2][3] = down;
      break;
    case VirtualKey::VK_v:
      m_KBD[3][3] = down;
      break;
    case VirtualKey::VK_n:
      m_KBD[4][3] = down;
      break;
    case VirtualKey::VK_z:
      if (Keyboard.isVKDown(VirtualKey::VK_LALT)) {
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
    case VirtualKey::VK_c:
      m_KBD[2][4] = down;
      break;
    case VirtualKey::VK_b:
      m_KBD[3][4] = down;
      break;
    case VirtualKey::VK_m:
      m_KBD[4][4] = down;
      break;
    case VirtualKey::VK_s:
      if (Keyboard.isVKDown(VirtualKey::VK_LALT)) {
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
    case VirtualKey::VK_f:
      m_KBD[2][5] = down;
      break;
    case VirtualKey::VK_h:
      m_KBD[3][5] = down;
      break;
    case VirtualKey::VK_k:
      m_KBD[4][5] = down;
      break;
    case VirtualKey::VK_q:
      m_KBD[0][6] = down;
      break;
    case VirtualKey::VK_e:
      m_KBD[1][6] = down;
      break;
    case VirtualKey::VK_t:
      m_KBD[2][6] = down;
      break;
    case VirtualKey::VK_u:
      m_KBD[3][6] = down;
      break;
    case VirtualKey::VK_o:
      m_KBD[4][6] = down;
      break;
    case VirtualKey::VK_SPACE:
      m_KBD[0][4] = down;
      break;
    case VirtualKey::VK_BACKSPACE:
      m_KBD[7][0] = down;
      break;
    case VirtualKey::VK_RETURN:
      m_KBD[7][1] = down;
      break;
    case VirtualKey::VK_LCTRL:
    case VirtualKey::VK_RCTRL:
      m_KBD[0][2] = down;
      break;
    case VirtualKey::VK_HOME:
      // HOME
      m_KBD[6][7] = down;
      break;
    case VirtualKey::VK_ESCAPE:
      // ESC => RUNSTOP
      m_KBD[0][3] = down;
      break;
    case VirtualKey::VK_LSHIFT:
      m_KBD[1][3] = down;
      break;
    case VirtualKey::VK_LGUI:
      // LGUI => CBM
      m_KBD[0][5] = down;
      break;
    case VirtualKey::VK_RSHIFT:
      m_KBD[6][4] = down;
      break;
    case VirtualKey::VK_F1:
      m_KBD[7][4] = down;
      break;
    case VirtualKey::VK_F2:
      m_KBD[7][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_F3:
      m_KBD[7][5] = down;
      break;
    case VirtualKey::VK_F4:
      m_KBD[7][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_F5:
      m_KBD[7][6] = down;
      break;
    case VirtualKey::VK_F6:
      m_KBD[7][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_F7:
      m_KBD[7][7] = down;
      break;
    case VirtualKey::VK_F8:
      m_KBD[7][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_DELETE:
      // DELETE (CANC) = RESTORE
      VIA1().setCA1(!down);
      break;
    case VirtualKey::VK_CARET:
      // '^' => UP ARROW (same ASCII of '^')
      m_KBD[6][6] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_TILDE:
      // '~' => pi
      m_KBD[6][6] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_EQUALS:
      m_KBD[6][5] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_POUND:
      m_KBD[6][0] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_SLASH:
      m_KBD[6][3] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_EXCLAIM:
      m_KBD[0][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_DOLLAR:
      m_KBD[1][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_PERCENT:
      m_KBD[2][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_AMPERSAND:
      m_KBD[2][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_LEFTPAREN:
      m_KBD[3][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_RIGHTPAREN:
      m_KBD[4][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_QUOTE:
      m_KBD[3][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_QUOTEDBL:
      m_KBD[0][7] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_AT:
      m_KBD[5][6] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_SEMICOLON:
      m_KBD[6][2] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_COMMA:
      m_KBD[5][3] = down;
      break;
    case VirtualKey::VK_UNDERSCORE:
      // '_' => LEFT-ARROW (same ASCII of UNDERSCORE)
      m_KBD[0][1] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_MINUS:
      m_KBD[5][7] = down;
      break;
    case VirtualKey::VK_LEFTBRACKET:
      m_KBD[5][5] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_RIGHTBRACKET:
      m_KBD[6][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_ASTERISK:
      m_KBD[6][1] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_PLUS:
      m_KBD[5][0] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_HASH:
      m_KBD[1][0] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_GREATER:
      m_KBD[5][4] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_LESS:
      m_KBD[5][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_QUESTION:
      m_KBD[6][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_COLON:
      m_KBD[5][5] = down;
      m_KBD[1][3] = m_KBD[6][4] = false; // release LSHIFT, RSHIFT
      break;
    case VirtualKey::VK_PERIOD:
      m_KBD[5][4] = down;
      break;
    case VirtualKey::VK_LEFT:
      if (Keyboard.isVKDown(VirtualKey::VK_RALT)) {
        // RALT-LEFT move joystick left
        setJoy(JoyLeft, down);
        break;
      }
      m_KBD[7][2] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_RIGHT:
      if (Keyboard.isVKDown(VirtualKey::VK_RALT)) {
        // RALT-RIGHT move joystick right
        setJoy(JoyRight, down);
        break;
      }
      m_KBD[7][2] = down;
      break;
    case VirtualKey::VK_UP:
      if (Keyboard.isVKDown(VirtualKey::VK_RALT)) {
        // RALT-UP move joystick up
        setJoy(JoyUp, down);
        break;
      }
      m_KBD[7][3] = down;
      m_KBD[1][3] = down; // press LSHIFT
      break;
    case VirtualKey::VK_DOWN:
      if (Keyboard.isVKDown(VirtualKey::VK_RALT)) {
        // RALT-DOWN move joystick down
        setJoy(JoyDown, down);
        break;
      }
      m_KBD[7][3] = down;
      break;
    case VirtualKey::VK_APPLICATION:  // also called MENU key
      if (Keyboard.isVKDown(VirtualKey::VK_RALT)) {
        // RALT-MENU fires joystick
        setJoy(JoyFire, down);
        break;
      }
      break;
    default:
      break;
  }

}


void Machine::VIA1PortOut(MOS6522 * via, VIAPort port)
{
}


void Machine::VIA2PortOut(MOS6522 * via, VIAPort port)
{
}


void Machine::VIA1PortIn(MOS6522 * via, VIAPort port)
{
  Machine * m = via->machine();

  switch (port) {

    // joystick (up, down, left, fire). Right on VIA2:PB
    case Port_PA:
      via->setBitPA(2, !m->m_JOY[JoyUp]);
      via->setBitPA(3, !m->m_JOY[JoyDown]);
      via->setBitPA(4, !m->m_JOY[JoyLeft]);
      via->setBitPA(5, !m->m_JOY[JoyFire]);
      break;

    default:
      break;
  }
}


void Machine::VIA2PortIn(MOS6522 * via, VIAPort port)
{
  Machine * m = via->machine();

  switch (port) {

    // Keyboard Row on PA (input)
    case Port_PA:
    {
      // Keyboard column on PB (output)
      int PA = 0;
      int col = ~(via->PB()) & via->DDRB();
      if (col) {
        for (int c = 0; c < 8; ++c)
          if (col & (1 << c))
            for (int r = 0; r < 8; ++r)
              PA |= (m->m_KBD[r][c] & 1) << r;
      }
      via->setPA(~PA);
      break;
    }

    // PB:7 -> joystick right (this is also used as output for column selection)
    case Port_PB:
    {
      // keyboard can also be queried using PA as output and PB as input
      int row = ~(via->PA()) & via->DDRA();
      if (row) {
        int PB = 0;
        for (int r = 0; r < 8; ++r)
          if (row & (1 << r))
            for (int c = 0; c < 8; ++c)
              PB |= (m->m_KBD[r][c] & 1) << c;
        via->setPB(~PB);
      }
      // joystick
      if ((via->DDRB() & 0x80) == 0)
        via->setBitPB(7, !m->m_JOY[JoyRight]);
      break;
    }

    default:
      break;

  }
}


void Machine::loadPRG(uint8_t const * data, size_t size, bool run)
{
  if (data && size > 2) {
    int loadAddr = data[0] | (data[1] << 8);
    size -= 2;
    for (int i = 0; i < size; ++i)
      busWrite(loadAddr + i, data[2 + i]);

    //// set basic pointers

    // read "Start of Basic"
    int lo = busRead(0x2b);
    int hi = busRead(0x2c);
    int basicStart = lo | (hi << 8);
    int basicEnd   = basicStart + (int)size;

    // "Tape buffer scrolling"
    //busWrite(0xac, lo);
    //busWrite(0xad, hi);
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

    if (run)
      type("RUN\r");
  }
}


void Machine::resetJoy()
{
  for (int i = JoyUp; i <= JoyFire; ++i)
    m_JOY[i] = false;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// VIA (6522 - Versatile Interface Adapter)


MOS6522::MOS6522(Machine * machine, int tag, VIAPortIO portOut, VIAPortIO portIn)
  : m_machine(machine),
    m_tag(tag),
    m_portOut(portOut),
    m_portIn(portIn)
{
  reset();
}


void MOS6522::reset()
{
  m_timer1Counter   = 0x0000;
  m_timer1Latch     = 0x0000;
  m_timer2Counter   = 0x0000;
  m_timer2Latch     = 0x00;
  m_CA1             = 0;
  m_CA1_prev        = 0;
  m_CA2             = 0;
  m_CA2_prev        = 0;
  m_CB1             = 0;
  m_CB1_prev        = 0;
  m_CB2             = 0;
  m_CB2_prev        = 0;
  m_IFR             = 0;
  m_IER             = 0;
  m_ACR             = 0;
  m_timer1Triggered = false;
  m_timer2Triggered = false;
  memset(m_regs, 0, 16);
}


#if DEBUGMSG
void MOS6522::dump()
{
  for (int i = 0; i < 16; ++i)
    printf("%02x ", m_regs[i]);
}
#endif


void MOS6522::writeReg(int reg, int value)
{
  #if DEBUGMSG
  printf("VIA %d, writeReg 0x%02x = 0x%02x\n", m_tag, reg, value);
  #endif
  m_regs[reg] = value;
  switch (reg) {
    case VIA_REG_T1_C_LO:
      // timer1: write into low order latch
      m_timer1Latch = (m_timer1Latch & 0xff00) | value;
      break;
    case VIA_REG_T1_C_HI:
      // timer1: write into high order latch
      m_timer1Latch = (m_timer1Latch & 0x00ff) | (value << 8);
      // timer1: write into high order counter
      // timer1: transfer low order latch into low order counter
      m_timer1Counter = (m_timer1Latch & 0x00ff) | (value << 8);
      // clear T1 interrupt flag
      m_IFR &= ~VIA_I_T1;
      m_timer1Triggered = false;
      //m_timer1Precount = 2; // TODO: check
      break;
    case VIA_REG_T1_L_LO:
      // timer1: write low order latch
      m_timer1Latch = (m_timer1Latch & 0xff00) | value;
      break;
    case VIA_REG_T1_L_HI:
      // timer1: write high order latch
      m_timer1Latch = (m_timer1Latch & 0x00ff) | (value << 8);
      // clear T1 interrupt flag
      m_IFR &= ~VIA_I_T1;
      break;
    case VIA_REG_T2_C_LO:
      // timer2: write low order latch
      m_timer2Latch = value;
      break;
    case VIA_REG_T2_C_HI:
      // timer2: write high order counter
      // timer2: copy low order latch into low order counter
      m_timer2Counter = (value << 8) | m_timer2Latch;
      // clear T2 interrupt flag
      m_IFR &= ~VIA_I_T2;
      m_timer2Triggered = false;
      break;
    case VIA_REG_ACR:
      m_ACR = value;
      break;
    case VIA_REG_PCR:
    {
      m_regs[VIA_REG_PCR] = value;
      // CA2 control
      switch ((m_regs[VIA_REG_PCR] >> 1) & 0b111) {
        case 0b110:
          // manual output - low
          m_CA2 = 0;
          m_portOut(this, Port_CA2);
          break;
        case 0b111:
          // manual output - high
          m_CA2 = 1;
          m_portOut(this, Port_CA2);
          break;
        default:
          break;
      }
      // CB2 control
      switch ((m_regs[VIA_REG_PCR] >> 5) & 0b111) {
        case 0b110:
          // manual output - low
          m_CB2 = 0;
          m_portOut(this, Port_CB2);
          break;
        case 0b111:
          // manual output - high
          m_CB2 = 1;
          m_portOut(this, Port_CB2);
          break;
        default:
          break;
      }
      break;
    }
    case VIA_REG_IER:
      // interrupt enable register
      if (value & VIA_I_CTRL) {
        // set 0..6 bits
        m_IER |= value & 0x7f;
      } else {
        // reset 0..6 bits
        m_IER &= ~value & 0x7f;
      }
      break;
    case VIA_REG_IFR:
      // flag register, reset each bit at 1
      m_IER &= ~value;
      break;
    case VIA_REG_DDRA:
      // Direction register Port A
      m_regs[VIA_REG_DDRA] = value;
      break;
    case VIA_REG_DDRB:
      // Direction register Port B
      m_regs[VIA_REG_DDRB] = value;
      break;
    case VIA_REG_ORA:
      // Output on Port A
      m_regs[VIA_REG_ORA] = value | (m_regs[VIA_REG_ORA] & ~m_regs[VIA_REG_DDRA]);
      m_portOut(this, Port_PA);
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_I_CA1;
      m_IFR &= ~VIA_I_CA2;
      break;
    case VIA_REG_ORA_NH:
      // Output on Port A (no handshake)
      m_regs[VIA_REG_ORA] = value | (m_regs[VIA_REG_ORA] & ~m_regs[VIA_REG_DDRA]);
      m_portOut(this, Port_PA);
      break;
    case VIA_REG_ORB:
      // Output on Port B
      m_regs[VIA_REG_ORB] = value | (m_regs[VIA_REG_ORB] & ~m_regs[VIA_REG_DDRB]);
      m_portOut(this, Port_PB);
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_I_CB1;
      m_IFR &= ~VIA_I_CB2;
      break;
    default:
      break;
  };
}


int MOS6522::readReg(int reg)
{
  #if DEBUGMSG
  printf("VIA %d, readReg 0x%02x\n", m_tag, reg);
  #endif
  switch (reg) {
    case VIA_REG_T1_C_LO:
      // clear T1 interrupt flag
      m_IFR &= ~VIA_I_T1;
      // read T1 low order counter
      return m_timer1Counter & 0xff;
    case VIA_REG_T1_C_HI:
      // read T1 high order counter
      return m_timer1Counter >> 8;
    case VIA_REG_T1_L_LO:
      // read T1 low order latch
      return m_timer1Latch & 0xff;
    case VIA_REG_T1_L_HI:
      // read T1 high order latch
      return m_timer1Latch >> 8;
    case VIA_REG_T2_C_LO:
      // clear T2 interrupt flag
      m_IFR &= ~VIA_I_T2;
      // read T2 low order counter
      return m_timer2Counter & 0xff;
    case VIA_REG_T2_C_HI:
      // read T2 high order counter
      return m_timer2Counter >> 8;
    case VIA_REG_ACR:
      return m_ACR;
    case VIA_REG_PCR:
      return m_regs[VIA_REG_PCR];
    case VIA_REG_IER:
      return m_IER | 0x80;
    case VIA_REG_IFR:
      return m_IFR | (m_IFR & m_IER ? 0x80 : 0);
    case VIA_REG_DDRA:
      // Direction register Port A
      return m_regs[VIA_REG_DDRA];
    case VIA_REG_DDRB:
      // Direction register Port B
      return m_regs[VIA_REG_DDRB];
    case VIA_REG_ORA:
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_I_CA1;
      m_IFR &= ~VIA_I_CA2;
      // Input from Port A
      m_portIn(this, Port_PA);
      return m_regs[VIA_REG_ORA];
    case VIA_REG_ORA_NH:
      // Input from Port A (no handshake)
      m_portIn(this, Port_PA);
      return m_regs[VIA_REG_ORA];
    case VIA_REG_ORB:
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_I_CB1;
      m_IFR &= ~VIA_I_CB2;
      // Input from Port B
      m_portIn(this, Port_PB);
      return m_regs[VIA_REG_ORB];
    default:
      return m_regs[reg];
  }
}


// ret. true on interrupt
bool MOS6522::tick()
{
  // handle Timer 1
  --m_timer1Counter;
  if (m_timer1Counter == 0) {
    if (m_ACR & VIA_ACR_T1_FREERUN) {
      // free run, reload from latch
      m_timer1Counter = m_timer1Latch + 2;  // +2 delay before next start
      m_IFR |= VIA_I_T1;  // set interrupt flag
    } else if (!m_timer1Triggered) {
      // one shot
      m_timer1Triggered = true;
      m_IFR |= VIA_I_T1;  // set interrupt flag
    }
  }

  // handle Timer 2
  if ((m_ACR & VIA_ACR_T2_COUNTPULSES) == 0) {
    --m_timer2Counter;
    if (m_timer2Counter == 0 && !m_timer2Triggered) {
      m_timer2Triggered = true;
      m_IFR |= VIA_I_T2;  // set interrupt flag
    }
  }

  // handle CA1 (RESTORE key)
  if (m_CA1 != m_CA1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_regs[VIA_REG_PCR] & 1) && m_CA1) || (!(m_regs[VIA_REG_PCR] & 1) && !m_CA1)) {
      m_IFR |= VIA_I_CA1;
    }
    m_CA1_prev = m_CA1;  // set interrupt flag
  }

/*
  // handle CB1
  if (m_CB1 != m_CB1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_regs[VIA_REG_PCR] & 0x10) && m_CB1) || (!(m_regs[VIA_REG_PCR] & 0x10) && !m_CB1)) {
      m_IFR |= VIA_I_CB1;  // set interrupt flag
    }
    m_CB1_prev = m_CB1;
  }
*/

  return m_IER & m_IFR & 0x7f;

}




/////////////////////////////////////////////////////////////////////////////////////////////
// VIC (6561 - Video Interface Chip)


static const RGB COLORS[16] = { {0, 0, 0},   // black
                                {3, 3, 3},   // white
                                {3, 0, 0},   // red
                                {0, 2, 2},   // cyan
                                {2, 0, 2},   // magenta
                                {0, 2, 0},   // green
                                {0, 0, 2},   // blue
                                {2, 2, 0},   // yellow
                                {2, 1, 0},   // orange
                                {3, 2, 0},   // light orange
                                {3, 2, 2},   // pink
                                {0, 3, 3},   // light cyan
                                {3, 0, 3},   // light magenta
                                {0, 3, 0},   // light green
                                {0, 0, 3},   // light blue
                                {3, 3, 0} }; // light yellow

static uint8_t RAWCOLORS[16];


MOS6561::MOS6561(Machine * machine)
  : m_machine(machine)
{
  for (int i = 0; i < 16; ++i)
    RAWCOLORS[i] = VGAController.createRawPixel(COLORS[i]);
  reset();
}


void MOS6561::reset()
{
  memset(m_regs, 0, 16);
  m_colCount        = 0;
  m_rowCount        = 23;
  m_charHeight      = 8;
  m_videoMatrixAddr = 0x0000;
  m_colorMatrixAddr = 0x0000;
  m_charTableAddr   = 0x0000;
  m_scanX           = 0;
  m_scanY           = 0;
  m_Y               = 0;
  m_charRow         = 0;
  m_isVBorder       = false;
  m_colorLine       = nullptr;
  m_videoLine       = nullptr;
  m_charInvertMask  = 0x00;
  m_auxColor        = RAWCOLORS[0];
  m_mcolors[3]      = m_auxColor;
}


void MOS6561::tick(int cycles)
{

  for (int c = 0; c < cycles; ++c) {

    m_scanX += 4;

    if (m_scanX == FrameWidth) {
      m_scanX = 0;
      ++m_scanY;

      if (m_scanY == FrameHeight) {
        m_scanY = 0;
        m_isVBorder = false;
        m_videoLine = nullptr;
      }
      else if (m_scanY >= VerticalBlanking) {
        m_Y            = m_scanY - VerticalBlanking;
        m_destScanline = (uint32_t*) VGAController.getScanline(m_Y);
        m_isVBorder    = (m_Y < m_topPos || m_Y >= m_topPos + m_charAreaHeight);
        if (!m_isVBorder) {
          m_charColumn = 0;
          m_charRow    = (m_Y - m_topPos) / m_charHeight;
          m_inCharRow  = (m_Y - m_topPos) % m_charHeight;
          m_videoLine  = m_machine->busReadVideoP(m_videoMatrixAddr + m_charRow * m_colCount);
          m_colorLine  = m_machine->busReadColorP(m_colorMatrixAddr + m_charRow * m_colCount);
        }
      }
    }

    if ((m_videoLine || m_isVBorder) && m_scanX >= HorizontalBlanking)
      drawNextPixels();

  }
}


//#define SETPIXEL(x, rgb) PIXELINROW(m_destScanline, x) = rgb

//#define SETPIXEL4(x, rgb4) *((uint32_t*)(m_destScanline + x)) = rgb4
//#define SETPIXEL4(rgb4) *m_destScanline++ = (rgb4)


// converts VIC char table address to CPU address
// this produces (m_charTableAddr+addr) with correct wrappings at 0x9C00 and 0x1C00
inline int CHARTABLE_VIC2CPU(int addr)
{
  return ((addr & 0x1fff) | (~((addr & 0x2000) << 2) & 0x8000));
}


// draw next 4 pixels
void MOS6561::drawNextPixels()
{
  // column to draw relative to frame buffer
  int x = m_scanX - HorizontalBlanking - 1; // makes draw from 0

  if (m_isVBorder || x < m_leftPos || x >= m_leftPos + m_charAreaWidth) {

    //// top/bottom/left/right borders
    *m_destScanline++ = m_borderColor4;

  } else {

    // chars area

    // charStart is 0x4 when 'x' points to start of character data, 0x0 otherwise
    int charStart = ~(m_leftPos + x) & 0x4;

    if (charStart) {
      // character pixels from char table
      int charIndex = m_videoLine[m_charColumn];
      m_charData = m_machine->busReadCharDefs(CHARTABLE_VIC2CPU(m_charTableAddr + charIndex * m_charHeight + m_inCharRow));

      // character foreground color from color RAM
      m_foregroundColorCode = m_colorLine[m_charColumn];
      m_mcolors[2] = m_hcolors[1] = RAWCOLORS[m_foregroundColorCode & 7];

      ++m_charColumn; // prepare for next column (when charStart will be 0x4 again)
    }

    // select nibble to draw
    int cv = m_charData >> charStart;

    if (m_foregroundColorCode & 0x8) {

      /** Multicolor **/

      // packs 4 pixels in raw 32 bit word
      *m_destScanline++ =  (m_mcolors[(cv >> 2) & 3] << 16)
                         | (m_mcolors[(cv >> 2) & 3] << 24)
                         | (m_mcolors[(cv >> 0) & 3]      )
                         | (m_mcolors[(cv >> 0) & 3] << 8 );

    } else {

      /** HI-RES **/

      // invert foreground and background colors?
      cv ^= m_charInvertMask;

      // packs 4 pixels in raw 32 bit word
      *m_destScanline++ =  (m_hcolors[(cv >> 3) & 1] << 16)
                         | (m_hcolors[(cv >> 2) & 1] << 24)
                         | (m_hcolors[(cv >> 1) & 1]      )
                         | (m_hcolors[(cv)      & 1] << 8 );

    }

  }
}


void MOS6561::writeReg(int reg, int value)
{
  if (m_regs[reg] != value) {
    m_regs[reg] = value;
    switch (reg) {
      case 0x0:
        m_leftPos = ((m_regs[0] & 0x7f) - 5) * 4;
        break;
      case 0x1:
        m_topPos = (m_regs[1] - 14) * 2;
        break;
      case 0x2:
        m_videoMatrixAddr = ((m_regs[2] & 0x80) << 2) | ((m_regs[5] & 0x70) << 6) | ((~m_regs[5] & 0x80) << 8);
        m_colorMatrixAddr = m_regs[2] & 0x80 ? 0x9600 : 0x9400;
        m_colCount        = m_regs[2] & 0x7f;
        m_charAreaWidth   = m_colCount * CharWidth;
        break;
      case 0x3:
        m_charHeight     = m_regs[3] & 1 ? 16 : 8;
        m_rowCount       = (m_regs[3] >> 1) & 0x3f;
        m_charAreaHeight = m_rowCount * m_charHeight;
        break;
      case 0x5:
        m_charTableAddr   = ((m_regs[5] & 0xf) << 10);
        m_videoMatrixAddr = ((m_regs[2] & 0x80) << 2) | ((m_regs[5] & 0x70) << 6) | ((~m_regs[5] & 0x80) << 8);
        break;
      case 0xe:
        m_auxColor        = RAWCOLORS[(m_regs[0xe] >> 4) & 0xf];
        m_mcolors[3]      = m_auxColor;
        break;
      case 0xf:
      {
        int backColorCode   = (m_regs[0xf] >> 4) & 0xf;
        m_charInvertMask    = (m_regs[0xf] & 0x8) == 0 ? 0xff : 0x00;
        uint8_t borderColor = RAWCOLORS[m_regs[0xf] & 7];
        m_borderColor4      = borderColor | (borderColor << 8) | (borderColor << 16) | (borderColor << 24);
        m_mcolors[1]        = borderColor;
        m_mcolors[0]        = m_hcolors[0] = RAWCOLORS[backColorCode];
        break;
      }
    }
  }
}


int MOS6561::readReg(int reg)
{
  switch (reg) {
    case 0x3:
      m_regs[0x3] = (m_regs[0x3] & 0x7f) | ((m_scanY & 1) << 7);
      break;
    case 0x4:
      m_regs[0x4] = (m_scanY >> 1) & 0xff;
      break;
  }
  #if DEBUGMSG
  printf("VIC, read reg 0x%02x, val = 0x%02x\n", reg, m_regs[reg]);
  #endif
  return m_regs[reg];
}






