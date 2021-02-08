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


namespace fabgl {



// PIT 8253 (Programmable Interval Timers)

class PIT8253 {

public:

  typedef void (*ChangeOut)(void * context, int timerIndex);
  typedef void (*Tick)(void * context, int ticks);

  PIT8253();
  ~PIT8253();

  void setCallbacks(void * context, ChangeOut changeOut, Tick tick) {
    m_context   = context;
    m_changeOut = changeOut;
    m_tick      = tick;
  }

  void runAutoTick(int freq, int updatesPerSec);


  void reset();

  void tick(int ticks);

  void write(int reg, uint8_t value);
  uint8_t read(int reg);

  bool readOut(int index) { return m_timer[index].out; }


private:

  struct TimerInfo {
    bool    BCD;          // BCD mode
    int32_t mode;         // Timer mode
    int32_t RLMode;       // Read/Load mode
    int32_t resetHolding; // Holding area for timer reset count
    int32_t resetCount;   // Reload value when count is zero
    int32_t count;        // Current timer counter
    int32_t latch;        // Latched timer count (-1 = not latched)
    bool    LSBToggle;    // true: Read load LSB, false: Read load MSB
    bool    out;
  };


  static void autoTickTask(void * pvParameters);

  void changeOut(int timer, bool value);

  void unsafeTick(int ticks);

  TimerInfo         m_timer[3];

  SemaphoreHandle_t m_mutex;

  // callbacks
  void *            m_context;
  ChangeOut         m_changeOut;
  Tick              m_tick;

  // autotick support
  TaskHandle_t      m_taskHandle;
  int32_t           m_autoTickFreq;
  int32_t           m_updatesPerSec;

};


} // namespace fabgl
