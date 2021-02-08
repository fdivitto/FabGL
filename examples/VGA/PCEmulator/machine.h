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


#pragma once


#include "fabgl.h"
#include "emudevs/graphicsadapter.h"
#include "emudevs/i8086.h"
#include "emudevs/PIC8259.h"
#include "emudevs/PIT8253.h"

#include "bios.h"


using fabgl::GraphicsAdapter;
using fabgl::PS2Controller;
using fabgl::PIC8259;
using fabgl::PIT8253;


class Machine {

public:
  Machine();
  ~Machine();

  void run();

  uint32_t ticksCounter() { return m_ticksCounter; }

private:


  static void runTask(void * pvParameters);

  void init();

  void tick();

  void setCGAMode();
  void setCGA6845Register(uint8_t value);

  void setHGCMode();
  void setHGC6845Register(uint8_t value);

  void queryKeyboard();

  static void writePort(void * context, int address, uint8_t value);
  static uint8_t readPort(void * context, int address);

  static void writeVideoMemory8(void * context, int address, uint8_t value);
  static void writeVideoMemory16(void * context, int address, uint16_t value);

  static bool interrupt(void * context, int num);

  static void PITChangeOut(void * context, int timerIndex);
  static void PITTick(void * context, int timerIndex);



  GraphicsAdapter          m_graphicsAdapter;
  PS2Controller            m_PS2Controller;

  uint8_t                  m_keyInputBuffer;
  bool                     m_keyInputFull;
  uint8_t *                m_curScancode;
  fabgl::VirtualKeyItem    m_vkItem;

  FILE *                   m_disk[2];

  static uint8_t *         s_memory;
  static uint8_t *         s_videoMemory;

  PIC8259                  m_PIC8259;

  // pin connections of PIT8253 on the IBM XT:
  //    gate-0 = gate-1 = +5V
  //    gate-2 = TIM2GATESPK
  //    clk-0  = clk-1  = clk-2 = 1193182 Hz
  //    out-0  = IRQ0
  //    out-1  = RAM refresh
  //    out-2  = speaker
  PIT8253                  m_PIT8253;

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

};


