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
#include "emudevs/graphicsadapter.h"
#include "emudevs/i8086.h"
#include "emudevs/PIC8259.h"
#include "emudevs/PIT8253.h"
#include "emudevs/i8042.h"
#include "emudevs/MC146818.h"
#include "devdrivers/MCP23S17.h"

#include "bios.h"


#define RAM_SIZE             1048576    // must correspond to bios MEMSIZE
#define VIDEOMEMSIZE         65536



using fabgl::GraphicsAdapter;
using fabgl::PIC8259;
using fabgl::PIT8253;
using fabgl::i8042;
using fabgl::MC146818;
using fabgl::MCP23S17;


#ifdef FABGL_EMULATED
typedef void (*StepCallback)(void *);
#endif


typedef void (*SysReqCallback)();



class Machine {

public:
  Machine();
  ~Machine();

  void setBaseDirectory(char const * value)    { m_baseDir = value; }

  void setDriveImage(int drive, char const * filename, int cylinders = 0, int heads = 0, int sectors = 0);

  void setBootDrive(int drive)                 { m_bootDrive = drive; }

  void setSysReqCallback(SysReqCallback value) { m_sysReqCallback = value; }

  void run();

  void trigReset()                     { m_reset = true; }

  uint32_t ticksCounter()              { return m_ticksCounter; }

  i8042 * getI8042()                   { return &m_i8042; }

  MC146818 * getMC146818()             { return &m_MC146818; }

  uint8_t * memory()                   { return s_memory; }

  uint8_t * videoMemory()              { return s_videoMemory; }

  uint8_t * frameBuffer()              { return m_frameBuffer; }

  GraphicsAdapter * graphicsAdapter()  { return &m_graphicsAdapter; }

  FILE * disk(int index)               { return m_disk[index]; }
  char const * diskFilename(int index) { return m_diskFilename[index]; }
  uint64_t diskSize(int index)         { return m_diskSize[index]; }
  uint16_t diskCylinders(int index)    { return m_diskCylinders[index]; }
  uint8_t diskHeads(int index)         { return m_diskHeads[index]; }
  uint8_t diskSectors(int index)       { return m_diskSectors[index]; }

  void dumpMemory(char const * filename);
  void dumpInfo(char const * filename);

  #ifdef FABGL_EMULATED
  void setStepCallback(StepCallback value)  { m_stepCallback = value; }
  #endif

private:


  static void runTask(void * pvParameters);

  void init();
  void reset();

  void tick();

  void setCGAMode();
  void setCGA6845Register(uint8_t value);

  void setHGCMode();
  void setHGC6845Register(uint8_t value);

  static void writePort(void * context, int address, uint8_t value);
  static uint8_t readPort(void * context, int address);

  static void writeVideoMemory8(void * context, int address, uint8_t value);
  static void writeVideoMemory16(void * context, int address, uint16_t value);
  static uint8_t readVideoMemory8(void * context, int address);
  static uint16_t readVideoMemory16(void * context, int address);

  static bool interrupt(void * context, int num);

  static void PITChangeOut(void * context, int timerIndex);

  static bool MC146818Interrupt(void * context);

  static bool keyboardInterrupt(void * context);
  static bool mouseInterrupt(void * context);
  static bool resetMachine(void * context);
  static bool sysReq(void * context);

  void speakerSetFreq();
  void speakerEnableDisable();

  void autoDetectDriveGeometry(int drive);


  #ifdef FABGL_EMULATED
  StepCallback             m_stepCallback;
  #endif

  bool                     m_reset;

  GraphicsAdapter          m_graphicsAdapter;

  BIOS                     m_BIOS;

  // 0, 1 = floppy
  // >= 2 = hard disk
  char *                   m_diskFilename[DISKCOUNT];
  FILE *                   m_disk[DISKCOUNT];
  uint64_t                 m_diskSize[DISKCOUNT];
  uint16_t                 m_diskCylinders[DISKCOUNT];
  uint8_t                  m_diskHeads[DISKCOUNT];
  uint8_t                  m_diskSectors[DISKCOUNT];

  static uint8_t *         s_memory;
  static uint8_t *         s_videoMemory;
  uint8_t *                m_frameBuffer;

  // 8259 Programmable Interrupt Controllers
  PIC8259                  m_PIC8259A;  // master
  PIC8259                  m_PIC8259B;  // slave

  // 8253 Programmable Interval Timers
  // pin connections of PIT8253 on the IBM XT:
  //    gate-0 = gate-1 = +5V
  //    gate-2 = TIM2GATESPK
  //    clk-0  = clk-1  = clk-2 = 1193182 Hz
  //    out-0  = IRQ0
  //    out-1  = RAM refresh
  //    out-2  = speaker
  PIT8253                  m_PIT8253;

  // 8042 PS/2 Keyboard Controller
  i8042                    m_i8042;

  TaskHandle_t             m_taskHandle;

  uint32_t                 m_ticksCounter;

  // CGA
  uint8_t                  m_CGA6845SelectRegister;
  uint8_t                  m_CGA6845[18];
  uint16_t                 m_CGAMemoryOffset;
  uint8_t                  m_CGAModeReg;
  uint8_t                  m_CGAColorReg;
  uint16_t                 m_CGAVSyncQuery;

  // Hercules
  uint8_t                  m_HGC6845SelectRegister;
  uint8_t                  m_HGC6845[18];
  uint16_t                 m_HGCMemoryOffset;
  uint8_t                  m_HGCModeReg;
  uint8_t                  m_HGCSwitchReg;
  uint16_t                 m_HGCVSyncQuery;

  // speaker/audio
  bool                     m_speakerDataEnable;
  SoundGenerator           m_soundGen;
  SineWaveformGenerator    m_sinWaveGen;

  // CMOS & RTC
  MC146818                 m_MC146818;

  // extended I/O (MCP23S17)
  MCP23S17                 m_MCP23S17;
  uint8_t                  m_MCP23S17Sel;

  uint8_t                  m_bootDrive;

  SysReqCallback           m_sysReqCallback;

  char const *             m_baseDir;

};


