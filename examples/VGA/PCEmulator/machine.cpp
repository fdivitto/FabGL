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


#include "machine.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <math.h>


#pragma GCC optimize ("O3")



#define BIOS_SEG             0xF000
#define BIOS_OFF             0x0100
#define BIOS_ADDR            (BIOS_SEG * 16 + BIOS_OFF)

#define RAM_SIZE             1048576    // must correspond to bios MEMSIZE
#define VIDEOMEMSIZE         65536

// PIT (timers) frequency in Hertz
#define PIT_TICK_FREQ        1193182

// number of times PIT is updated every second
#define PIT_UPDATES_PER_SEC  500




// CGA Craphics Card Ports Bits

#define CGA_MODECONTROLREG_TEXT80         0x01   // 0 = 40x25, 1 = 80x25
#define CGA_MODECONTROLREG_GRAPHICS       0x02   // 0 = text,  1 = graphics
#define CGA_MODECONTROLREG_COLOR          0x04   // 0 = color, 1 = monochrome
#define CGA_MODECONTROLREG_ENABLED        0x08   // 0 = video off, 1 = video on
#define CGA_MODECONTROLREG_GRAPH640       0x10   // 0 = 320x200 graphics, 1 = 640x200 graphics
#define CGA_MODECONTROLREG_BIT7BLINK      0x20   // 0 = text mode bit 7 controls background, 1 = text mode bit 7 controls blinking

#define CGA_COLORCONTROLREG_BACKCOLR_MASK 0x0f   // mask for 320x200 background color index (on 640x200 is the foreground)
#define CGA_COLORCONTROLREG_HIGHINTENSITY 0x10   // select high intensity colors
#define CGA_COLORCONTROLREG_PALETTESEL    0x20   // 0 is Green, red and brown, 1 is Cyan, magenta and white


// Hercules (HGC) Ports Bits

#define HGC_MODECONTROLREG_GRAPHICS       0x02   // 0 = text mode, 1 = graphics mode
#define HGC_MODECONTROLREG_ENABLED        0x08   // 0 = video off, 1 = video on
#define HGC_MODECONTROLREG_BIT7BLINK      0x20   // 0 = text mode bit 7 controls background, 1 = text mode bit 7 controls blinking
#define HGC_MODECONTROLREG_GRAPHICSPAGE   0x80   // 0 = graphics mapped on page 0 (0xB0000), 1 = graphics mapped on page 1 (0xB8000)

#define HGC_CONFSWITCH_ALLOWGRAPHICSMODE  0x01   // 0 = prevents graphics mode, 1 = allows graphics mode
#define HGC_CONFSWITCH_ALLOWPAGE1         0x02   // 0 = prevents access to page 1, 1 = allows access to page 1



//////////////////////////////////////////////////////////////////////////////////////
// Machine


using fabgl::i8086;


uint8_t *         Machine::s_memory;
uint8_t *         Machine::s_videoMemory;


Machine::Machine()
{
}


Machine::~Machine()
{
  free(s_videoMemory);
}


void Machine::init()
{
  m_PS2Controller.begin(PS2Preset::KeyboardPort0);
  m_PS2Controller.keyboard()->setScancodeSet(1);
  m_vkItem.scancode[0] = 0;
  m_curScancode = m_vkItem.scancode;

  // to avoid PSRAM bug without -mfix-esp32-psram-cache-issue
  // core 0 can only work reliably with the lower 2 MB and core 1 only with the higher 2 MB.
  s_memory = (uint8_t*)(0x3F800000 + (xPortGetCoreID() == 1 ? 2 * 1024 * 1024 : 0));

  s_videoMemory = (uint8_t*)heap_caps_malloc(VIDEOMEMSIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

  memset(s_memory, 0, RAM_SIZE);

  m_keyInputFull = false;
  m_ticksCounter = 0;

  m_CGAMemoryOffset = 0;
  m_CGAModeReg      = 0;
  m_CGAColorReg     = 0;
  m_CGAVSyncQuery   = 0;

  m_HGCMemoryOffset = 0;
  m_HGCModeReg      = 0;
  m_HGCSwitchReg    = 0;
  m_HGCVSyncQuery   = 0;

  m_PIC8259.reset();

  m_PIT8253.setCallbacks(this, PITChangeOut, PITTick);
  m_PIT8253.reset();
  m_PIT8253.runAutoTick(PIT_TICK_FREQ, PIT_UPDATES_PER_SEC);

  memset(m_CGA6845, 0, sizeof(m_CGA6845));
  memset(m_HGC6845, 0, sizeof(m_HGC6845));

	// copy bios
  memcpy(s_memory + BIOS_ADDR, biosrom, sizeof(biosrom));

  // setup bootstrap code (starting from 0xFFFF0)
  // for example: "JMP FC00:0100"
  s_memory[0xffff0] = 0xea;
  s_memory[0xffff1] = BIOS_OFF & 0xff;
  s_memory[0xffff2] = BIOS_OFF >> 8;
  s_memory[0xffff3] = BIOS_SEG & 0xff;
  s_memory[0xffff4] = BIOS_SEG >> 8;

  i8086::setCallbacks(this, readPort, writePort, writeVideoMemory8, writeVideoMemory16, interrupt);
  i8086::setMemory(s_memory);
  i8086::reset();

  m_disk[0] = fopen("/SD/hd.img", "r+b");
  m_disk[1] = fopen("/SD/fd.img", "r+b");

	// Set CX:AX equal to the hard disk image size, if present
  if (m_disk[0]) {
    fseek(m_disk[0], 0, SEEK_END);
    auto sz = ftell(m_disk[0]) >> 9;
    i8086::setAX(sz & 0xffff);
    i8086::setCX(sz >> 16);
  }

  // Set DL to boot from FD (0), or HD (0x80)
  i8086::setDX(0x0000);
}


void Machine::run()
{
  xTaskCreatePinnedToCore(&runTask, "", 3000, this, 5, &m_taskHandle, CoreUsage::quietCore());
}


void Machine::runTask(void * pvParameters)
{
  auto m = (Machine*)pvParameters;

  m->init();

	while (true) {

    i8086::step();
		m->tick();

	}
}


void Machine::tick()
{
  ++m_ticksCounter;

  if (m_PIC8259.pendingInterrupt() && i8086::IRQ(m_PIC8259.pendingInterruptNum()))
    m_PIC8259.ackPendingInterrupt();
}


void Machine::setCGA6845Register(uint8_t value)
{
  m_CGA6845[m_CGA6845SelectRegister] = value;

  switch (m_CGA6845SelectRegister) {

    // cursor start: (bits 5,6 = cursor blink and visibility control), bits 0..4 cursor start scanline
    case 0x0a:
      m_graphicsAdapter.setCursorVisible((m_CGA6845[0xa] >> 5) >= 2);
      // no break!
    // cursor end: bits 0..4 cursor end scanline
    case 0x0b:
      m_graphicsAdapter.setCursorShape(2 * (m_CGA6845[0xa] & 0x1f), 2 * (m_CGA6845[0xb] & 0x1f));
      break;

    // video memory start offset (0x0c = H, 0x0d = L)
    case 0x0c:
    case 0x0d:
      m_CGAMemoryOffset = ((m_CGA6845[0xc] << 8) | m_CGA6845[0xd]) << 1;
      setCGAMode();
      break;

    // cursor position (0x0e = H and 0x0f = L)
    case 0x0e:
    case 0x0f:
    {
      int pos = (m_CGA6845[0xe] << 8) | m_CGA6845[0xf];
      m_graphicsAdapter.setCursorPos(pos / m_graphicsAdapter.getTextColumns(), pos % m_graphicsAdapter.getTextColumns());
      break;
    }

  }
}


void Machine::setCGAMode()
{
  if ((m_CGAModeReg & CGA_MODECONTROLREG_ENABLED) == 0) {

    // video disabled
    //printf("CGA, video disabled\n");
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::None);

  } else if ((m_CGAModeReg & CGA_MODECONTROLREG_TEXT80) == 0 && (m_CGAModeReg & CGA_MODECONTROLREG_GRAPHICS) == 0) {

    // 40 column text mode
    //printf("CGA, 40 columns text mode\n");
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + 0x8000 + m_CGAMemoryOffset);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Text_40x25_16Colors);
    m_graphicsAdapter.setBit7Blink(m_CGAModeReg & CGA_MODECONTROLREG_BIT7BLINK);

  } else if ((m_CGAModeReg & CGA_MODECONTROLREG_TEXT80) && (m_CGAModeReg & CGA_MODECONTROLREG_GRAPHICS) == 0) {

    // 80 column text mode
    //printf("CGA, 80 columns text mode\n");
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + 0x8000 + m_CGAMemoryOffset);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Text_80x25_16Colors);
    m_graphicsAdapter.setBit7Blink(m_CGAModeReg & CGA_MODECONTROLREG_BIT7BLINK);

  } else if ((m_CGAModeReg & CGA_MODECONTROLREG_GRAPHICS) && (m_CGAModeReg & CGA_MODECONTROLREG_GRAPH640) == 0) {

    // 320x200 graphics
    //printf("CGA, 320x200 graphics mode\n");
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + 0x8000 + m_CGAMemoryOffset);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Graphics_320x200_4Colors);
    int paletteIndex = (bool)(m_CGAColorReg & CGA_COLORCONTROLREG_PALETTESEL) * 2 + (bool)(m_CGAColorReg & CGA_COLORCONTROLREG_HIGHINTENSITY);
    m_graphicsAdapter.setPCGraphicsPaletteInUse(paletteIndex);
    m_graphicsAdapter.setPCGraphicsBackgroundColorIndex(m_CGAColorReg & CGA_COLORCONTROLREG_BACKCOLR_MASK);

  } else if ((m_CGAModeReg & CGA_MODECONTROLREG_GRAPHICS) && (m_CGAModeReg & CGA_MODECONTROLREG_GRAPH640)) {

    // 640x200 graphics
    //printf("CGA, 640x200 graphics mode\n");
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + 0x8000 + m_CGAMemoryOffset);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Graphics_640x200_2Colors);
    m_graphicsAdapter.setPCGraphicsForegroundColorIndex(m_CGAColorReg & CGA_COLORCONTROLREG_BACKCOLR_MASK);

  }
}


void Machine::setHGC6845Register(uint8_t value)
{
  m_HGC6845[m_HGC6845SelectRegister] = value;
  switch (m_HGC6845SelectRegister) {

    // cursor start: (bits 5,6 = cursor blink and visibility control), bits 0..4 cursor start scanline
    case 0x0a:
      m_graphicsAdapter.setCursorVisible((m_HGC6845[0xa] >> 5) >= 2);
    // cursor end: bits 0..4 cursor end scanline
    case 0x0b:
      m_graphicsAdapter.setCursorShape((m_HGC6845[0xa] & 0x1f), (m_HGC6845[0xb] & 0x1f));
      break;

    // video memory start offset (0x0c = H, 0x0d = L)
    case 0x0c:
    case 0x0d:
      m_HGCMemoryOffset = ((m_HGC6845[0xc] << 8) | m_HGC6845[0xd]) << 1;
      setHGCMode();
      break;

    // cursor position (0x0e = H and 0x0f = L)
    case 0x0e:
    case 0x0f:
    {
      int pos = (m_HGC6845[0xe] << 8) | m_HGC6845[0xf];
      m_graphicsAdapter.setCursorPos(pos / m_graphicsAdapter.getTextColumns(), pos % m_graphicsAdapter.getTextColumns());
      break;
    }

  }
}


void Machine::setHGCMode()
{
  constexpr int HGC_OFFSET_PAGE0 = 0x0000;
  constexpr int HGC_OFFSET_PAGE1 = 0x8000;

  if ((m_HGCModeReg & HGC_MODECONTROLREG_ENABLED) == 0) {

    // video disabled
    //printf("Hercules, video disabled\n");
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::None);

  } else if ((m_HGCModeReg & HGC_MODECONTROLREG_GRAPHICS) == 0 || (m_HGCSwitchReg & HGC_CONFSWITCH_ALLOWGRAPHICSMODE) == 0) {

    // text mode
    //printf("Hercules, text mode\n");
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + HGC_OFFSET_PAGE0);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Text_80x25_16Colors);
    m_graphicsAdapter.setBit7Blink(m_HGCModeReg & HGC_MODECONTROLREG_BIT7BLINK);

  } else if ((m_HGCModeReg &HGC_MODECONTROLREG_GRAPHICS)) {

    // graphics mode
    //printf("Hercules, graphics mode\n");
    int offset = (m_HGCModeReg & HGC_MODECONTROLREG_GRAPHICSPAGE) && (m_HGCSwitchReg & HGC_CONFSWITCH_ALLOWPAGE1) ? HGC_OFFSET_PAGE1 : HGC_OFFSET_PAGE0;
    m_graphicsAdapter.setVideoBuffer(s_videoMemory + offset);
    m_graphicsAdapter.setEmulation(GraphicsAdapter::Emulation::PC_Graphics_HGC_720x348);

  }

}


void Machine::writePort(void * context, int address, uint8_t value)
{
  auto m = (Machine*)context;

  //printf("OUT %04x=%02x\n", address, value);

  switch (address) {

    // PIC8259
    case 0x20:
    case 0x21:
      m->m_PIC8259.write(address & 1, value);
      break;

    // PIT8253
    case 0x0040:
    case 0x0041:
    case 0x0042:
    case 0x0043:
      m->m_PIT8253.write(address & 3, value);
      break;

    // CGA - CRT 6845 - register selection register
    case 0x3d4:
      m->m_CGA6845SelectRegister = value;
      break;

    // CGA - CRT 6845 - selected register write
    case 0x3d5:
      m->setCGA6845Register(value);
      break;

    // CGA - Mode Control Register
    case 0x3d8:
      m->m_CGAModeReg = value;
      m->setCGAMode();
      break;

    // CGA - Color Select register
    case 0x3d9:
      m->m_CGAColorReg = value;
      m->setCGAMode();
      break;

    // Hercules (HGC) - CRT 6845 - register selection register
    case 0x3b4:
      m->m_HGC6845SelectRegister = value;
      break;

    // Hercules (HGC) - CRT 6845 - selected register write
    case 0x3b5:
      m->setHGC6845Register(value);
      break;

    // Hercules (HGC) - Display Mode Control Port
    case 0x3b8:
      m->m_HGCModeReg = value;
      m->setHGCMode();
      break;

    // Hercules (HGC) - Configuration Switch
    case 0x3bf:
      m->m_HGCSwitchReg = value;
      m->setHGCMode();
      break;

    default:
      //printf("OUT %04x=%02x\n", address, value);
      break;
  }
}


uint8_t Machine::readPort(void * context, int address)
{
  auto m = (Machine*)context;

  //printf("IN %04X\n", address);

  switch (address) {

    // PIC8259
    case 0x0020:
    case 0x0021:
      return m->m_PIC8259.read(address & 1);

    // PIT8253
    case 0x0040:
    case 0x0041:
    case 0x0042:
    case 0x0043:
      return m->m_PIT8253.read(address & 3);

    case 0x0060:
      m->m_keyInputFull = false;
      return m->m_keyInputBuffer;

    case 0x0064:
      return 0x14 | (m->m_keyInputFull ? 0x01 : 0x00);

    // CGA - CRT 6845 - register selection register
    case 0x3d4:
      return 0x00;  // not readable

    // CGA - CRT 6845 - selected register read
    case 0x3d5:
      return m->m_CGA6845SelectRegister >= 14 && m->m_CGA6845SelectRegister < 16 ? m->m_CGA6845[m->m_CGA6845SelectRegister] : 0x00;

    // CGA - Status Register
    // real vertical sync is too fast for our slowly emulated 8086, so
    // here it is just a fake, just to allow programs that check it to keep going anyway.
    case 0x3da:
      m->m_CGAVSyncQuery += 1;
      return (m->m_CGAVSyncQuery & 0x7) != 0 ? 0x09 : 0x00; // "not VSync" (0x00) every 7 queries

    // Hercules (HGC) - register selection register
    case 0x3b4:
      return 0x00;  // not readable

    // Hercules (HGC) - selected register read
    case 0x3b5:
      return m->m_HGC6845SelectRegister >= 14 && m->m_HGC6845SelectRegister < 16 ? m->m_HGC6845[m->m_HGC6845SelectRegister] : 0x00;

    // Hercules (HGC) - Display Status Port
    // real vertical sync is too fast for our slowly emulated 8086, so
    // here it is just a fake, just to allow programs that check it to keep going anyway.
    case 0x3ba:
      m->m_HGCVSyncQuery += 1;
      return (m->m_HGCVSyncQuery & 0x7) != 0 ? 0x00 : 0x80; // "not VSync" (0x80) every 7 queries

  }

  //printf("IN %04X\n", address);
  return 0xff;
}


void Machine::PITChangeOut(void * context, int timerIndex)
{
  auto m = (Machine*)context;

  // timer 0 trigged?
  if (timerIndex == 0 &&  m->m_PIT8253.readOut(0) == true) {
    // yes, report IR0 (IRQ8)
    m->m_PIC8259.signalInterrupt(0);
  }
}


void Machine::PITTick(void * context, int timerIndex)
{
  auto m = (Machine*)context;
  m->queryKeyboard();
}


void Machine::queryKeyboard()
{
  if ((*m_curScancode || m_PS2Controller.keyboard()->virtualKeyAvailable()) && !m_keyInputFull) {
    if (*m_curScancode == 0) {
      m_PS2Controller.keyboard()->getNextVirtualKey(&m_vkItem);
      m_curScancode = m_vkItem.scancode;
    }
    m_keyInputBuffer = *m_curScancode++;
    if (m_curScancode - m_vkItem.scancode == 8) {
      // 8 bytes scancodes check
      m_vkItem.scancode[0] = 0;
      m_curScancode = m_vkItem.scancode;
    }
    m_keyInputFull = true;
    m_PIC8259.signalInterrupt(1); // report IR1 (IRQ9)
    //printf("kbd %02X\n", m_keyInputBuffer);
  }
}


void Machine::writeVideoMemory8(void * context, int address, uint8_t value)
{
  //printf("WVMEM %05X <= %02X\n", address, value);
  s_videoMemory[address - 0xb0000] = value;
}


void Machine::writeVideoMemory16(void * context, int address, uint16_t value)
{
  //printf("WVMEM %05X <= %04X\n", address, value);
  *(uint16_t*)(s_videoMemory + address - 0xb0000) = value;
}


bool Machine::interrupt(void * context, int num)
{
  auto m = (Machine*)context;

  switch (num) {

    // Disk read
    case 0xf1:
    {
      int diskIndex  = i8086::DX() & 0xff;
      uint32_t pos   = (i8086::BP() | (i8086::SI() << 16)) << 9;
      uint32_t dest  = i8086::ES() * 16 + i8086::BX();
      uint32_t count = i8086::AX();
      fseek(m->m_disk[diskIndex], pos, 0);
      auto r = fread(s_memory + dest, 1, count, m->m_disk[diskIndex]);
      i8086::setAL(r & 0xff);
      //printf("read(0x%05X, %d, %d) => %d\n", dest, count, diskIndex, r);
      return true;
    }

    // Disk write
    case 0xf2:
    {
      int diskIndex  = i8086::DX() & 0xff;
      uint32_t pos   = (i8086::BP() | (i8086::SI() << 16)) << 9;
      uint32_t src   = i8086::ES() * 16 + i8086::BX();
      uint32_t count = i8086::AX();
      fseek(m->m_disk[diskIndex], pos, 0);
      auto r = fwrite(s_memory + src, 1, count, m->m_disk[diskIndex]);
      i8086::setAL(r & 0xff);
      //printf("write(0x%05X, %d, %d) => %d\n", src, count, diskIndex, r);
      return true;
    }

    // Get RTC
    case 0xf3:
    {
      // @TODO
      //printf("Get RTC\n");
      uint32_t dest = i8086::ES() * 16 + i8086::BX();
      memset(s_memory + dest, 0, 36);
      *(int16_t*)(s_memory + dest + 36) = xTaskGetTickCount() * portTICK_PERIOD_MS;
      return true;
    }

    // Put Char for debug (AL)
    case 0xf4:
      printf("debug: %c\n", i8086::AX() & 0xff);
      return true;

  }

  // not hanlded
  return false;
}
