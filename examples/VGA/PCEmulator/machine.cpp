/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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


// I/O expander (based on MCP23S17) ports

#define EXTIO_CONFIG                    0x00e0   // configuration port (see EXTIO_CONFIG_.... flags)
// whole 8 bit ports handling
#define EXTIO_DIRA                      0x00e1   // port A direction (0 = input, 1 = output)
#define EXTIO_DIRB                      0x00e2   // port B direction (0 = input, 1 = output)
#define EXTIO_PULLUPA                   0x00e3   // port A pullup enable (0 = disabled, 1 = enabled)
#define EXTIO_PULLUPB                   0x00e4   // port B pullup enable (0 = disabled, 1 = enabled)
#define EXTIO_PORTA                     0x00e5   // port A read/write
#define EXTIO_PORTB                     0x00e6   // port B read/write
// single GPIO handling
#define EXTIO_GPIOSEL                   0x00e7   // GPIO selection (0..7 = PA0..PA7, 8..15 = PB0..PB8)
#define EXTIO_GPIOCONF                  0x00e8   // selected GPIO direction and pullup (0 = input, 1 = output, 2 = input with pullup)
#define EXTIO_GPIO                      0x00e9   // selected GPIO read or write (0 = low, 1 = high)

// I/O expander configuration bits
#define EXTIO_CONFIG_AVAILABLE            0x01   // 1 = external IO available, 0 = not available
#define EXTIO_CONFIG_INT_POLARITY         0x02   // 1 = positive polarity, 0 = negative polarity (default)



//////////////////////////////////////////////////////////////////////////////////////
// Machine


using fabgl::i8086;


uint8_t *         Machine::s_memory;
uint8_t *         Machine::s_videoMemory;


Machine::Machine()
  : m_disk(),
    m_bootDrive(0)
{
}


Machine::~Machine()
{
  vTaskDelete(m_taskHandle);
  free(s_videoMemory);
}


void Machine::init()
{
  srand((uint32_t)time(NULL));

  // to avoid PSRAM bug without -mfix-esp32-psram-cache-issue
  // core 0 can only work reliably with the lower 2 MB and core 1 only with the higher 2 MB.
  s_memory = (uint8_t*)(SOC_EXTRAM_DATA_LOW + (xPortGetCoreID() == 1 ? 2 * 1024 * 1024 : 0));

  s_videoMemory = (uint8_t*)heap_caps_malloc(VIDEOMEMSIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

  memset(s_memory, 0, RAM_SIZE);

  m_soundGen.play(true);
  m_soundGen.attach(&m_sinWaveGen);

  m_i8042.init();
  m_i8042.setCallbacks(this, keyboardInterrupt, mouseInterrupt, resetMachine, sysReq);

  m_PIT8253.setCallbacks(this, PITChangeOut, PITTick);
  m_PIT8253.reset();
  m_PIT8253.runAutoTick(PIT_TICK_FREQ, PIT_UPDATES_PER_SEC);

  m_MC146818.init("PCEmulator");
  m_MC146818.setCallbacks(this, MC146818Interrupt);

  m_MCP23S17.begin();
  m_MCP23S17Sel = 0;

  m_BIOS.init(this);

  i8086::setCallbacks(this, readPort, writePort, writeVideoMemory8, writeVideoMemory16, readVideoMemory8, readVideoMemory16, interrupt);
  i8086::setMemory(s_memory);

  m_reset = true;
}


void Machine::setDriveImage(int drive, char const * filename, int cylinders, int heads, int sectors)
{
  if (m_disk[drive]) {
    fclose(m_disk[drive]);
    m_disk[drive] = nullptr;
  }

  m_BIOS.setDriveMediaType(drive, mediaUnknown);

  m_diskCylinders[drive] = cylinders;
  m_diskHeads[drive]     = heads;
  m_diskSectors[drive]   = sectors;

  if (filename) {
    FileBrowser fb;
    fb.setDirectory("/SD");
    m_disk[drive] = fb.openFile(filename, "r+b");
    if (m_disk[drive]) {

      // get image file size
      fseek(m_disk[drive], 0L, SEEK_END);
      m_diskSize[drive] = ftell(m_disk[drive]);

      // need to detect geometry?
      if (cylinders == 0 || heads == 0 || sectors == 0)
        autoDetectDriveGeometry(drive);
    }
  }
}


void Machine::autoDetectDriveGeometry(int drive)
{
  // well known floppy formats
  static const struct {
    uint16_t tracks;
    uint8_t  sectors;
    uint8_t  heads;
  } FLOPPYFORMATS[] = {
    { 40,  8, 1 },   //  163840 bytes (160K, 5.25 inch)
    { 40,  9, 1 },   //  184320 bytes (180K, 5.25 inch)
    { 40,  8, 2 },   //  327680 bytes (320K, 5.25 inch)
    { 40,  9, 2 },   //  368640 bytes (360K, 5.25 inch)
    { 80,  9, 2 },   //  737280 bytes (720K, 3.5 inch)
    { 80, 15, 2 },   // 1228800 bytes (1200K, 5.25 inch)
    { 80, 18, 2 },   // 1474560 bytes (1440K, 3.5 inch)
    { 80, 36, 2 },   // 2949120 bytes (2880K, 3.5 inch)
  };

  // look for well known floppy formats
  for (auto const & ff : FLOPPYFORMATS) {
    if (512 * ff.tracks * ff.sectors * ff.heads == m_diskSize[drive]) {
      m_diskCylinders[drive] = ff.tracks;
      m_diskHeads[drive]     = ff.heads;
      m_diskSectors[drive]   = ff.sectors;
      //printf("autoDetectDriveGeometry, found floppy: t=%d s=%d h=%d\n", ff.tracks, ff.sectors, ff.heads);
      return;
    }
  }

  // maybe an hard disk, try to calculate geometry
  int t = 1, h = 1, s;
  int diskSectors = (int)(m_diskSize[drive] / 512);
  if (diskSectors > 63) {
    t = diskSectors / 63;
    s = 63;
  } else
    s = diskSectors;
  if (t > 1024) {
    h = t / 1024;
    t = 1024;
  }
  m_diskCylinders[drive] = t;
  m_diskHeads[drive]     = h;
  m_diskSectors[drive]   = s;
  //printf("autoDetectDriveGeometry, found HD: t=%d s=%d h=%d\n", t, s, h);

}


void Machine::reset()
{
  m_reset = false;

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

  m_i8042.reset();

  m_PIC8259A.reset();
  m_PIC8259B.reset();

  m_PIT8253.reset();
  m_PIT8253.setGate(0, true);
  m_PIT8253.setGate(1, true);

  m_MC146818.reset();

  memset(m_CGA6845, 0, sizeof(m_CGA6845));
  memset(m_HGC6845, 0, sizeof(m_HGC6845));

  m_BIOS.reset();

  i8086::reset();

  // set boot drive (0, 1, 0x80, 0x81)
  i8086::setDL((m_bootDrive & 1) | (m_bootDrive > 1 ? 0x80 : 0x00));
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

    if (m->m_reset)
      m->reset();

    #ifdef FABGL_EMULATED
    usleep(0);  // just to make simulation a bit slòwer
    if (m->m_stepCallback)
      m->m_stepCallback(m);
    #endif

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

    // I/O expander - Configuration
    case EXTIO_CONFIG:
      m->m_MCP23S17.setINTActiveHigh(value & EXTIO_CONFIG_INT_POLARITY);
      break;

    // I/O expander - Port A/B Direction
    case EXTIO_DIRA ... EXTIO_DIRB:
      m->m_MCP23S17.setPortDir(address - EXTIO_DIRA + MCP_PORTA, ~value);
      printf("dir %d = %02X\n", address - EXTIO_DIRA + MCP_PORTA, ~value);
      break;

    // I/O expander - Port A/B pullup
    case EXTIO_PULLUPA ... EXTIO_PULLUPB:
      m->m_MCP23S17.enablePortPullUp(address - EXTIO_PULLUPA + MCP_PORTA, value);
      break;

    // I/O expander - Port A/B write
    case EXTIO_PORTA ... EXTIO_PORTB:
      m->m_MCP23S17.writePort(address - EXTIO_PORTA + MCP_PORTA, value);
      printf("set %d = %02X\n", address - EXTIO_PORTA + MCP_PORTA, value);
      break;

    // I/O expander - GPIO selection
    case EXTIO_GPIOSEL:
      m->m_MCP23S17Sel = value & 0xf;
      break;

    // I/O expander - GPIO direction and pullup
    case EXTIO_GPIOCONF:
      m->m_MCP23S17.configureGPIO(m->m_MCP23S17Sel, value & 1 ? fabgl::MCPDir::Output : fabgl::MCPDir::Input, value & 2);
      break;

    // I/O expander - GPIO write
    case EXTIO_GPIO:
      m->m_MCP23S17.writeGPIO(m->m_MCP23S17Sel, value);
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

    // I/O expander - Configuration
    case EXTIO_CONFIG:
      return (m->m_MCP23S17.available()        ? EXTIO_CONFIG_AVAILABLE    : 0) |
             (m->m_MCP23S17.getINTActiveHigh() ? EXTIO_CONFIG_INT_POLARITY : 0);

    // I/O expander - Port A/B Direction
    case EXTIO_DIRA ... EXTIO_DIRB:
      return m->m_MCP23S17.getPortDir(address - EXTIO_DIRA + MCP_PORTA);

    // I/O expander - Port A/B pullup
    case EXTIO_PULLUPA ... EXTIO_PULLUPB:
      return m->m_MCP23S17.getPortPullUp(address - EXTIO_PULLUPA + MCP_PORTA);

    // I/O expander - Port A/B read
    case EXTIO_PORTA ... EXTIO_PORTB:
      return m->m_MCP23S17.readPort(address - EXTIO_PORTA + MCP_PORTA);

    // I/O expander - GPIO selection
    case EXTIO_GPIOSEL:
      return m->m_MCP23S17Sel;

    // I/O expander - GPIO read
    case EXTIO_GPIO:
      return m->m_MCP23S17.readGPIO(m->m_MCP23S17Sel);

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


// reset from 8042
bool Machine::resetMachine(void * context)
{
  auto m = (Machine*)context;
  m->trigReset();
  return true;
}

// ESP32 reset using SYSREQ (ALT + PRINTSCREEN)
bool Machine::sysReq(void * context)
{
  esp_restart();
  return true;
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


  // emu interrupts callable only inside the BIOS segment
  if (i8086::CS() == BIOS_SEG) {
    switch (num) {

      /*
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
      */

      // Put Char for debug (AL)
      case 0xf4:
        printf("%c", i8086::AX() & 0xff);
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

      // disk handler
      case 0xfb:
        m->m_BIOS.diskHandlerEntry();
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


// diskType:
//   0 = 320
//   1 = 360
//   2 = 720
//   3 = 1200
//   4 = 1440
//   5 = 2880
bool Machine::createEmptyDisk(int diskType, char const * filename)
{
  // boot sector for: 320K, 360K, 720K, 1440K, 2880K
  static const uint8_t BOOTSECTOR_WIN[512] = {
    0xeb, 0x3c, 0x90, 0x4d, 0x53, 0x57, 0x49, 0x4e, 0x34, 0x2e, 0x31, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x40, 0x0b, 0xf0, 0x09, 0x00,
    0x12, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x33, 0xc9, 0x8e, 0xd1, 0xbc, 0xfc, 0x7b, 0x16, 0x07, 0xbd,
    0x78, 0x00, 0xc5, 0x76, 0x00, 0x1e, 0x56, 0x16, 0x55, 0xbf, 0x22, 0x05, 0x89, 0x7e, 0x00, 0x89, 0x4e, 0x02, 0xb1, 0x0b, 0xfc, 0xf3, 0xa4, 0x06,
    0x1f, 0xbd, 0x00, 0x7c, 0xc6, 0x45, 0xfe, 0x0f, 0x38, 0x4e, 0x24, 0x7d, 0x20, 0x8b, 0xc1, 0x99, 0xe8, 0x7e, 0x01, 0x83, 0xeb, 0x3a, 0x66, 0xa1,
    0x1c, 0x7c, 0x66, 0x3b, 0x07, 0x8a, 0x57, 0xfc, 0x75, 0x06, 0x80, 0xca, 0x02, 0x88, 0x56, 0x02, 0x80, 0xc3, 0x10, 0x73, 0xed, 0x33, 0xc9, 0xfe,
    0x06, 0xd8, 0x7d, 0x8a, 0x46, 0x10, 0x98, 0xf7, 0x66, 0x16, 0x03, 0x46, 0x1c, 0x13, 0x56, 0x1e, 0x03, 0x46, 0x0e, 0x13, 0xd1, 0x8b, 0x76, 0x11,
    0x60, 0x89, 0x46, 0xfc, 0x89, 0x56, 0xfe, 0xb8, 0x20, 0x00, 0xf7, 0xe6, 0x8b, 0x5e, 0x0b, 0x03, 0xc3, 0x48, 0xf7, 0xf3, 0x01, 0x46, 0xfc, 0x11,
    0x4e, 0xfe, 0x61, 0xbf, 0x00, 0x07, 0xe8, 0x28, 0x01, 0x72, 0x3e, 0x38, 0x2d, 0x74, 0x17, 0x60, 0xb1, 0x0b, 0xbe, 0xd8, 0x7d, 0xf3, 0xa6, 0x61,
    0x74, 0x3d, 0x4e, 0x74, 0x09, 0x83, 0xc7, 0x20, 0x3b, 0xfb, 0x72, 0xe7, 0xeb, 0xdd, 0xfe, 0x0e, 0xd8, 0x7d, 0x7b, 0xa7, 0xbe, 0x7f, 0x7d, 0xac,
    0x98, 0x03, 0xf0, 0xac, 0x98, 0x40, 0x74, 0x0c, 0x48, 0x74, 0x13, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0xeb, 0xef, 0xbe, 0x82, 0x7d, 0xeb,
    0xe6, 0xbe, 0x80, 0x7d, 0xeb, 0xe1, 0xcd, 0x16, 0x5e, 0x1f, 0x66, 0x8f, 0x04, 0xcd, 0x19, 0xbe, 0x81, 0x7d, 0x8b, 0x7d, 0x1a, 0x8d, 0x45, 0xfe,
    0x8a, 0x4e, 0x0d, 0xf7, 0xe1, 0x03, 0x46, 0xfc, 0x13, 0x56, 0xfe, 0xb1, 0x04, 0xe8, 0xc2, 0x00, 0x72, 0xd7, 0xea, 0x00, 0x02, 0x70, 0x00, 0x52,
    0x50, 0x06, 0x53, 0x6a, 0x01, 0x6a, 0x10, 0x91, 0x8b, 0x46, 0x18, 0xa2, 0x26, 0x05, 0x96, 0x92, 0x33, 0xd2, 0xf7, 0xf6, 0x91, 0xf7, 0xf6, 0x42,
    0x87, 0xca, 0xf7, 0x76, 0x1a, 0x8a, 0xf2, 0x8a, 0xe8, 0xc0, 0xcc, 0x02, 0x0a, 0xcc, 0xb8, 0x01, 0x02, 0x80, 0x7e, 0x02, 0x0e, 0x75, 0x04, 0xb4,
    0x42, 0x8b, 0xf4, 0x8a, 0x56, 0x24, 0xcd, 0x13, 0x61, 0x61, 0x72, 0x0a, 0x40, 0x75, 0x01, 0x42, 0x03, 0x5e, 0x0b, 0x49, 0x75, 0x77, 0xc3, 0x03,
    0x18, 0x01, 0x27, 0x0d, 0x0a, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x64, 0x69, 0x73, 0x6b,
    0xff, 0x0d, 0x0a, 0x44, 0x69, 0x73, 0x6b, 0x20, 0x49, 0x2f, 0x4f, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0xff, 0x0d, 0x0a, 0x52, 0x65, 0x70, 0x6c,
    0x61, 0x63, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2c, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x74, 0x68, 0x65, 0x6e, 0x20, 0x70,
    0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x0d, 0x0a, 0x00, 0x00, 0x49, 0x4f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x53, 0x59, 0x53, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x7f, 0x01, 0x00, 0x41, 0xbb, 0x00, 0x07, 0x60, 0x66, 0x6a,
    0x00, 0xe9, 0x3b, 0xff, 0x00, 0x00, 0x55, 0xaa };

  // boot sector for: 1200K
  static const uint8_t BOOTSECTOR_MSDOS5[512] = {
    0xeb, 0x3c, 0x90, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x35, 0x2e, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x60, 0x09, 0xf9, 0x08, 0x00,
    0x0f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0xd1, 0x40, 0x38, 0xda, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16, 0x07,
    0xbb, 0x78, 0x00, 0x36, 0xc5, 0x37, 0x1e, 0x56, 0x16, 0x53, 0xbf, 0x3e, 0x7c, 0xb9, 0x0b, 0x00, 0xfc, 0xf3, 0xa4, 0x06, 0x1f, 0xc6, 0x45, 0xfe,
    0x0f, 0x8b, 0x0e, 0x18, 0x7c, 0x88, 0x4d, 0xf9, 0x89, 0x47, 0x02, 0xc7, 0x07, 0x3e, 0x7c, 0xfb, 0xcd, 0x13, 0x72, 0x79, 0x33, 0xc0, 0x39, 0x06,
    0x13, 0x7c, 0x74, 0x08, 0x8b, 0x0e, 0x13, 0x7c, 0x89, 0x0e, 0x20, 0x7c, 0xa0, 0x10, 0x7c, 0xf7, 0x26, 0x16, 0x7c, 0x03, 0x06, 0x1c, 0x7c, 0x13,
    0x16, 0x1e, 0x7c, 0x03, 0x06, 0x0e, 0x7c, 0x83, 0xd2, 0x00, 0xa3, 0x50, 0x7c, 0x89, 0x16, 0x52, 0x7c, 0xa3, 0x49, 0x7c, 0x89, 0x16, 0x4b, 0x7c,
    0xb8, 0x20, 0x00, 0xf7, 0x26, 0x11, 0x7c, 0x8b, 0x1e, 0x0b, 0x7c, 0x03, 0xc3, 0x48, 0xf7, 0xf3, 0x01, 0x06, 0x49, 0x7c, 0x83, 0x16, 0x4b, 0x7c,
    0x00, 0xbb, 0x00, 0x05, 0x8b, 0x16, 0x52, 0x7c, 0xa1, 0x50, 0x7c, 0xe8, 0x92, 0x00, 0x72, 0x1d, 0xb0, 0x01, 0xe8, 0xac, 0x00, 0x72, 0x16, 0x8b,
    0xfb, 0xb9, 0x0b, 0x00, 0xbe, 0xe6, 0x7d, 0xf3, 0xa6, 0x75, 0x0a, 0x8d, 0x7f, 0x20, 0xb9, 0x0b, 0x00, 0xf3, 0xa6, 0x74, 0x18, 0xbe, 0x9e, 0x7d,
    0xe8, 0x5f, 0x00, 0x33, 0xc0, 0xcd, 0x16, 0x5e, 0x1f, 0x8f, 0x04, 0x8f, 0x44, 0x02, 0xcd, 0x19, 0x58, 0x58, 0x58, 0xeb, 0xe8, 0x8b, 0x47, 0x1a,
    0x48, 0x48, 0x8a, 0x1e, 0x0d, 0x7c, 0x32, 0xff, 0xf7, 0xe3, 0x03, 0x06, 0x49, 0x7c, 0x13, 0x16, 0x4b, 0x7c, 0xbb, 0x00, 0x07, 0xb9, 0x03, 0x00,
    0x50, 0x52, 0x51, 0xe8, 0x3a, 0x00, 0x72, 0xd8, 0xb0, 0x01, 0xe8, 0x54, 0x00, 0x59, 0x5a, 0x58, 0x72, 0xbb, 0x05, 0x01, 0x00, 0x83, 0xd2, 0x00,
    0x03, 0x1e, 0x0b, 0x7c, 0xe2, 0xe2, 0x8a, 0x2e, 0x15, 0x7c, 0x8a, 0x16, 0x24, 0x7c, 0x8b, 0x1e, 0x49, 0x7c, 0xa1, 0x4b, 0x7c, 0xea, 0x00, 0x00,
    0x70, 0x00, 0xac, 0x0a, 0xc0, 0x74, 0x29, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0xeb, 0xf2, 0x3b, 0x16, 0x18, 0x7c, 0x73, 0x19, 0xf7, 0x36,
    0x18, 0x7c, 0xfe, 0xc2, 0x88, 0x16, 0x4f, 0x7c, 0x33, 0xd2, 0xf7, 0x36, 0x1a, 0x7c, 0x88, 0x16, 0x25, 0x7c, 0xa3, 0x4d, 0x7c, 0xf8, 0xc3, 0xf9,
    0xc3, 0xb4, 0x02, 0x8b, 0x16, 0x4d, 0x7c, 0xb1, 0x06, 0xd2, 0xe6, 0x0a, 0x36, 0x4f, 0x7c, 0x8b, 0xca, 0x86, 0xe9, 0x8a, 0x16, 0x24, 0x7c, 0x8a,
    0x36, 0x25, 0x7c, 0xcd, 0x13, 0xc3, 0x0d, 0x0a, 0x4e, 0x6f, 0x6e, 0x2d, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x20,
    0x6f, 0x72, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x0d, 0x0a, 0x52, 0x65, 0x70, 0x6c, 0x61, 0x63, 0x65, 0x20, 0x61,
    0x6e, 0x64, 0x20, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x77, 0x68, 0x65, 0x6e, 0x20, 0x72, 0x65,
    0x61, 0x64, 0x79, 0x0d, 0x0a, 0x00, 0x49, 0x4f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x20, 0x20,
    0x20, 0x53, 0x59, 0x53, 0x00, 0x00, 0x55, 0xaa };

  static const uint8_t * BOOTSECTOR[6] = {
    BOOTSECTOR_WIN,     // 320K
    BOOTSECTOR_WIN,     // 360K
    BOOTSECTOR_WIN,     // 720K
    BOOTSECTOR_MSDOS5,  // 1200K
    BOOTSECTOR_WIN,     // 1440K
    BOOTSECTOR_WIN,     // 2880K
  };

  // offset 0x0c (16 bytes):
  static const uint8_t GEOM[6][16]  = {
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0x80, 0x02, 0xFF, 0x01, 0x00, 0x08, 0x00, 0x02, 0x00 },  // 320K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xD0, 0x02, 0xFD, 0x02, 0x00, 0x09, 0x00, 0x02, 0x00 },  // 360K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xA0, 0x05, 0xF9, 0x03, 0x00, 0x09, 0x00, 0x02, 0x00 },  // 720K
    { 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x60, 0x09, 0xF9, 0x08, 0x00, 0x0F, 0x00, 0x02, 0x00 },  // 1200K
    { 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x40, 0x0B, 0xF0, 0x09, 0x00, 0x12, 0x00, 0x02, 0x00 },  // 1440K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0xF0, 0x00, 0x80, 0x16, 0xF0, 0x09, 0x00, 0x24, 0x00, 0x02, 0x00 },  // 2880K
  };

  // sizes in bytes
  static const int SIZES[6] = {
    512 *  8 * 40 * 2, // 327680 (320K)
    512 *  9 * 40 * 2, // 368640 (360K)
    512 *  9 * 80 * 2, // 737280 (720K)
    512 * 15 * 80 * 2, // 1228800 (1200K)
    512 * 18 * 80 * 2, // 1474560 (1440K)
    512 * 36 * 80 * 2, // 2949120 (2880K)
  };

  // FAT
  static const struct {
    uint8_t id;
    uint8_t fat1size;  // in 512 blocks
    uint8_t fat2size;  // in 512 blocks
  } FAT[6] = {
    { 0xff, 1,  8 },   // 320K
    { 0xfd, 2,  9 },   // 360K
    { 0xf9, 3, 10 },   // 720K
    { 0xf9, 8, 22 },   // 1200K
    { 0xf0, 9, 23 },   // 1440K
    { 0xf0, 9, 24 },   // 2880K
  };

  FileBrowser fb;
  fb.setDirectory("/SD");
  auto file = fb.openFile(filename, "wb");

  if (file) {

    auto sectbuf = new uint8_t[512];

    // initial fill with all 0xf6 up to entire size
    memset(sectbuf, 0xf6, 512);
    fseek(file, 0, SEEK_SET);
    for (int i = 0; i < SIZES[diskType]; i += 512)
      fwrite(sectbuf, 1, 512, file);

    // offset 0x00 (512 bytes) : write boot sector
    fseek(file, 0, SEEK_SET);
    fwrite(BOOTSECTOR[diskType], 1, 512, file);

    // offset 0x0c (16 bytes) : disk geometry
    fseek(file, 0x0c, SEEK_SET);
    fwrite(GEOM[diskType], 1, 16, file);

    // offset 0x27 (4 bytes) : Volume Serial Number
    const uint32_t volSN = rand();
    fseek(file, 0x27, SEEK_SET);
    fwrite(&volSN, 1, 4, file);

    // FAT
    memset(sectbuf, 0x00, 512);
    fseek(file, 0x200, SEEK_SET);
    for (int i = 0; i < (FAT[diskType].fat1size + FAT[diskType].fat2size); ++i)
      fwrite(sectbuf, 1, 512, file);
    sectbuf[0] = FAT[diskType].id;
    sectbuf[1] = 0xff;
    sectbuf[2] = 0xff;
    fseek(file, 0x200, SEEK_SET);
    fwrite(sectbuf, 1, 3, file);
    fseek(file, 0x200 + FAT[diskType].fat1size * 512, SEEK_SET);
    fwrite(sectbuf, 1, 3, file);

    fclose(file);

    delete [] sectbuf;
  }

  return file != nullptr;
}


void Machine::dumpMemory(char const * filename)
{
  constexpr int BLOCKLEN = 1024;
  FileBrowser fb;
  fb.setDirectory("/SD");
  auto file = fb.openFile(filename, "wb");
  if (file) {
    for (int i = 0; i < 1048576; i += BLOCKLEN)
      fwrite(s_memory + i, 1, BLOCKLEN, file);
    fclose(file);
  }
}


void Machine::dumpInfo(char const * filename)
{
  FileBrowser fb;
  fb.setDirectory("/SD");
  auto file = fb.openFile(filename, "wb");
  if (file) {
    // CPU state
    fprintf(file, " CS   DS   ES   SS\n");
    fprintf(file, "%04X %04X %04X %04X\n\n", i8086::CS(), i8086::DS(), i8086::ES(), i8086::SS());
    fprintf(file, " IP   AX   BX   CX   DX   SI   DI   BP   SP\n");
    fprintf(file, "%04X %04X %04X %04X %04X %04X %04X %04X %04X\n\n", i8086::IP(), i8086::AX(), i8086::BX(), i8086::CX(), i8086::DX(), i8086::SI(), i8086::DI(), i8086::BP(), i8086::SP());
    fprintf(file, "O D I T S Z A P C\n");
    fprintf(file, "%d %d %d %d %d %d %d %d %d\n\n", i8086::flagOF(), i8086::flagDF(), i8086::flagIF(), i8086::flagTF(), i8086::flagSF(), i8086::flagZF(), i8086::flagAF(), i8086::flagPF(), i8086::flagCF());
    fprintf(file, "CS+IP: %05X\n", i8086::CS() * 16 + i8086::IP());
    fprintf(file, "SS+SP: %05X\n\n", i8086::SS() * 16 + i8086::SP());
    fclose(file);
  }
}
