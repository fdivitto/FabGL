/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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
  // to avoid PSRAM bug without -mfix-esp32-psram-cache-issue
  // core 0 can only work reliably with the lower 2 MB and core 1 only with the higher 2 MB.
  s_memory = (uint8_t*)(0x3F800000 + (xPortGetCoreID() == 1 ? 2 * 1024 * 1024 : 0));

  s_videoMemory = (uint8_t*)heap_caps_malloc(VIDEOMEMSIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

  memset(s_memory, 0, RAM_SIZE);

  m_ticksCounter = 0;

  m_CGAMemoryOffset = 0;
  m_CGAModeReg      = 0;
  m_CGAColorReg     = 0;
  m_CGAVSyncQuery   = 0;

  m_HGCMemoryOffset = 0;
  m_HGCModeReg      = 0;
  m_HGCSwitchReg    = 0;
  m_HGCVSyncQuery   = 0;

  m_speakerDataEnable = false;

  m_soundGen.play(true);
  m_soundGen.attach(&m_sinWaveGen);

  m_i8042.init();
  m_i8042.setCallbacks(this, keyboardInterrupt, mouseInterrupt);

  m_PIC8259A.reset();
  m_PIC8259B.reset();

  m_PIT8253.setCallbacks(this, PITChangeOut, PITTick);
  m_PIT8253.reset();
  m_PIT8253.runAutoTick(PIT_TICK_FREQ, PIT_UPDATES_PER_SEC);
  m_PIT8253.setGate(0, true);
  m_PIT8253.setGate(1, true);

  m_MC146818.init("PCEmulator");
  m_MC146818.setCallbacks(this, MC146818Interrupt);
  m_MC146818.reset();

  memset(m_CGA6845, 0, sizeof(m_CGA6845));
  memset(m_HGC6845, 0, sizeof(m_HGC6845));

  m_BIOS.init(this);

  i8086::setCallbacks(this, readPort, writePort, writeVideoMemory8, writeVideoMemory16, readVideoMemory8, readVideoMemory16, interrupt);
  i8086::setMemory(s_memory);
  i8086::reset();

  FileBrowser fb;
  fb.setDirectory("/SD");
  m_disk[0] = m_diskImageFile[0] ? fb.openFile(m_diskImageFile[0], "r+b") : nullptr; // drive C
  m_disk[1] = m_diskImageFile[1] ? fb.openFile(m_diskImageFile[1], "r+b") : nullptr; // drive A

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
  xTaskCreatePinnedToCore(&runTask, "", 4000, this, 5, &m_taskHandle, CoreUsage::quietCore());
}


void IRAM_ATTR Machine::runTask(void * pvParameters)
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

  if (m_PIC8259A.pendingInterrupt() && i8086::IRQ(m_PIC8259A.pendingInterruptNum()))
    m_PIC8259A.ackPendingInterrupt();
  if (m_PIC8259B.pendingInterrupt() && i8086::IRQ(m_PIC8259B.pendingInterruptNum()))
    m_PIC8259B.ackPendingInterrupt();
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

    // PIC8259A
    case 0x20:
    case 0x21:
      m->m_PIC8259A.write(address & 1, value);
      break;

    // PIC8259B
    case 0xa0:
    case 0xa1:
      m->m_PIC8259B.write(address & 1, value);
      break;

    // PIT8253
    case 0x0040:
    case 0x0041:
    case 0x0042:
    case 0x0043:
      m->m_PIT8253.write(address & 3, value);
      if ((address == 0x43 && (value >> 6) == 2) || address == 0x42)
        m->speakerSetFreq();
      break;

    // 8042 keyboard controller input
    case 0x0060:
      m->m_i8042.write(0, value);
      break;

    // PortB
    //   bit 1 : speaker data enable
    //   bit 0 : timer 2 gate
    case 0x0061:
      m->m_speakerDataEnable = value & 0x02;
      m->m_PIT8253.setGate(2, value & 0x01);
      m->speakerEnableDisable();
      break;

    // 8042 keyboard controller input
    case 0x0064:
      m->m_i8042.write(1, value);
      break;

    // MC146818 RTC & RAM
    case 0x0070:
    case 0x0071:
      m->m_MC146818.write(address & 1, value);
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

    // PIC8259A
    case 0x0020:
    case 0x0021:
      return m->m_PIC8259A.read(address & 1);

    // PIC8259B
    case 0x00a0:
    case 0x00a1:
      return m->m_PIC8259B.read(address & 1);

    // PIT8253
    case 0x0040:
    case 0x0041:
    case 0x0042:
    case 0x0043:
      return m->m_PIT8253.read(address & 3);

    // 8042 keyboard controller output
    case 0x0060:
      return m->m_i8042.read(0);

    // Port B
    //   bit 5 : timer 2 out
    //   bit 4 : toggles every 15.085us (DMA refresh)
    //   bit 1 : speaker data enable
    //   bit 0 : timer 2 gate
    case 0x0061:
      return ((int)m->m_PIT8253.getOut(2) << 5) |    // bit 5
             (esp_timer_get_time() & 0x10)       |   // bit 4 (toggles every 16us)
             ((int)m->m_speakerDataEnable << 1)  |   // bit 1
             ((int)m->m_PIT8253.getGate(2));         // bit 0

    // I/O port
    case 0x0062:
      return 0x20 * m->m_PIT8253.getOut(2);  // bit 5 = timer 2 output

    // 8042 keyboard controller status register
    case 0x0064:
      return m->m_i8042.read(1);

    // MC146818 RTC & RAM
    case 0x0070:
    case 0x0071:
      return m->m_MC146818.read(address & 1);

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
  if (timerIndex == 0 &&  m->m_PIT8253.getOut(0) == true) {
    // yes, report 8259A-IR0 (IRQ0, INT 08h)
    m->m_PIC8259A.signalInterrupt(0);
  }
}


// 8259A-IR1 (IRQ1, INT 09h)
bool Machine::keyboardInterrupt(void * context)
{
  auto m = (Machine*)context;
  return m->m_PIC8259A.signalInterrupt(1);
}


// 8259B-IR4 (IRQ12, INT 074h)
bool Machine::mouseInterrupt(void * context)
{
  auto m = (Machine*)context;
  return m->m_PIC8259B.signalInterrupt(4);
}


// interrupt from MC146818, trig 8259B-IR0 (IRQ8, INT 70h)
bool Machine::MC146818Interrupt(void * context)
{
  auto m = (Machine*)context;
  return m->m_PIC8259B.signalInterrupt(0);
}


void Machine::PITTick(void * context, int timerIndex)
{
  auto m = (Machine*)context;
  // run keyboard controller every PIT tick (just to not overload CPU with continous checks)
  m->m_i8042.tick();
}


void Machine::writeVideoMemory8(void * context, int address, uint8_t value)
{
  //printf("WVMEM %05X <= %02X\n", address, value);
  if (address >= 0xb0000)
    s_videoMemory[address - 0xb0000] = value;
}


void Machine::writeVideoMemory16(void * context, int address, uint16_t value)
{
  //printf("WVMEM %05X <= %04X\n", address, value);
  if (address >= 0xb0000)
    *(uint16_t*)(s_videoMemory + address - 0xb0000) = value;
}


uint8_t Machine::readVideoMemory8(void * context, int address)
{
  if (address >= 0xb0000)
    return s_videoMemory[address - 0xb0000];
  else
    return 0xff;
}


uint16_t Machine::readVideoMemory16(void * context, int address)
{
  if (address >= 0xb0000)
    return *(uint16_t*)(s_videoMemory + address - 0xb0000);
  else
    return 0xffff;
}


bool Machine::interrupt(void * context, int num)
{
  auto m = (Machine*)context;

  //printf("INT %02X (AX = %04X)\n", num, i8086::AX());

  // emu interrupts callable only inside the BIOS segment
  if (i8086::CS() == BIOS_SEG) {
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

      // Put Char for debug (AL)
      case 0xf4:
        printf("debug: %c\n", i8086::AX() & 0xff);
        return true;

      // BIOS helpers (AH = select helper function)
      case 0xf5:
        m->m_BIOS.helpersEntry();
        return true;

      // set or reset flag CF before IRET, replacing value in stack with current value
      case 0xf6:
      {
        auto savedFlags = (uint16_t*) (s_memory + i8086::SS() * 16 + (uint16_t)(i8086::SP() + 4));
        *savedFlags = (*savedFlags & 0xfffe) | i8086::flagCF();
        return true;
      }

      // set or reset flag ZF before IRET, replacing value in stack with current value
      case 0xf7:
      {
        auto savedFlags = (uint16_t*) (s_memory + i8086::SS() * 16 + (uint16_t)(i8086::SP() + 4));
        *savedFlags = (*savedFlags & 0xffbf) | (i8086::flagZF() << 6);
        return true;
      }

      // set or reset flag IF before IRET, replacing value in stack with current value
      case 0xf8:
      {
        auto savedFlags = (uint16_t*) (s_memory + i8086::SS() * 16 + (uint16_t)(i8086::SP() + 4));
        *savedFlags = (*savedFlags & 0xfdff) | (i8086::flagIF() << 9);
        return true;
      }

      // test point P0
      case 0xf9:
        printf("P0 AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X\n", i8086::AX(), i8086::BX(), i8086::CX(), i8086::DX(), i8086::DS());
        return true;

      // test point P1
      case 0xfa:
        printf("P1 AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X\n", i8086::AX(), i8086::BX(), i8086::CX(), i8086::DX(), i8086::DS());
        return true;

    }
  }

  // not hanlded
  return false;
}


void Machine::speakerSetFreq()
{
  //printf("speakerSetFreq: count = %d\n", m_PIT8253.timerInfo(2).resetCount);
  int timerCount = m_PIT8253.timerInfo(2).resetCount;
  if (timerCount == 0)
    timerCount = 65536;
  int freq = PIT_TICK_FREQ / timerCount;
  //printf("   freq = %dHz\n", freq);
  m_sinWaveGen.setFrequency(freq);
}


void Machine::speakerEnableDisable()
{
  bool genEnabled = m_PIT8253.getGate(2);
  if (genEnabled && m_speakerDataEnable) {
    // speaker enabled
    m_sinWaveGen.enable(true);
  } else {
    // speaker disabled
    m_sinWaveGen.enable(false);
  }
}



