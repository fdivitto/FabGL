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


#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"

#include "fabgl.h"


#if FABGL_ESP_IDF_VERSION <= FABGL_ESP_IDF_VERSION_VAL(3, 3, 5)
typedef nvs_handle nvs_handle_t;
#endif


namespace fabgl {


// RTC MC146818 emulator

// On the PC/AT there are following connections:
//    /IRQ   -> IRQ8 (INT 70h)
//    CKFS   -> 5V  (hence CKOUT has the same frequency as OSC1)
//    PS     -> 5V
//    /RESET -> 5V
//    OSC1   -> 32768Hz clock
//    OSC2   -> NC
//    CKOUT  -> NC
//    SQW    -> NC

// I/O Access
//   port 0x70 (W)   : Register address port (bits 0-6)
//   port 0x71 (R/W) : Register read or write


class MC146818 {

public:

  typedef bool (*InterruptCallback)(void * context);

  MC146818();
  ~MC146818();

  void init(char const * NVSNameSpace);

  void setCallbacks(void * context, InterruptCallback interruptCallback) {
    m_context           = context;
    m_interruptCallback = interruptCallback;
  }

  void reset();

  void commit();

  uint8_t read(int address);
  void write(int address, uint8_t value);

  uint8_t & reg(int address) { return m_regs[address]; }

  void updateTime();

private:

  void enableTimers();

  void stopPeriodicTimer();
  void stopEndUpdateTimer();

  static void periodIntTimerFunc(void * args);
  static void endUpdateIntTimerFunc(void * args);


  nvs_handle_t       m_nvs;

  uint8_t            m_regs[64];
  uint8_t            m_regSel;

  void *             m_context;
  InterruptCallback  m_interruptCallback;

  esp_timer_handle_t m_periodicIntTimerHandle;
  esp_timer_handle_t m_endUpdateIntTimerHandle;

};







}   // fabgl namespace
