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
#include "emudevs/graphicsadapter.h"
#include "emudevs/i8086.h"
#include "emudevs/PIC8259.h"
#include "emudevs/PIT8253.h"
#include "emudevs/i8042.h"
#include "emudevs/MC146818.h"

#include "bios.h"


#define RAM_SIZE             1048576    // must correspond to bios MEMSIZE
#define VIDEOMEMSIZE         65536

// PIT (timers) frequency in Hertz
#define PIT_TICK_FREQ        1193182

// number of times PIT is updated every second
#define PIT_UPDATES_PER_SEC  500


using fabgl::GraphicsAdapter;
using fabgl::PIC8259;
using fabgl::PIT8253;
using fabgl::i8042;
using fabgl::MC146818;


class Machine {

public:
  Machine();
  ~Machine();

  void setDriveA(char const * filename) { m_diskImageFile[1] = filename; }
  void setDriveC(char const * filename) { m_diskImageFile[0] = filename; }

  void run();

  uint32_t ticksCounter()    { return m_ticksCounter; }

  i8042 * getI8042()         { return &m_i8042; }

  MC146818 * getMC146818()   { return &m_MC146818; }

  uint8_t * memory()         { return s_memory; }

private:


  static void runTask(void * pvParameters);

  void init();

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
  static void PITTick(void * context, int timerIndex);

  static bool MC146818Interrupt(void * context);

  static bool keyboardInterrupt(void * context);
  static bool mouseInterrupt(void * context);

  void speakerSetFreq();
  void speakerEnableDisable();


  GraphicsAdapter          m_graphicsAdapter;

  BIOS                     m_BIOS;

  FILE *                   m_disk[2];
  char const *             m_diskImageFile[2];

  static uint8_t *         s_memory;
  static uint8_t *         s_videoMemory;

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

};


