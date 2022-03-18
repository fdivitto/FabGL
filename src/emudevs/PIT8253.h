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


namespace fabgl {



// PIT (timers) frequency in Hertz
#define PIT_TICK_FREQ        1193182



// PIT 8253 (Programmable Interval Timers)

class PIT8253 {

public:

  typedef void (*ChangeOut)(void * context, int timerIndex);

  struct TimerInfo {
    bool    BCD;          // BCD mode
    int32_t mode;         // Timer mode
    int32_t RLMode;       // Read/Load mode
    int32_t resetHolding; // Holding area for timer reset count
    int32_t resetCount;   // Reload value when count is zero
    int32_t count;        // Current timer counter
    int32_t latch;        // Latched timer count (-1 = not latched)
    bool    LSBToggle;    // true: Read load LSB, false: Read load MSB
    bool    out;          // out state
    bool    gate;         // date (1 = timer running)
    bool    running;      // counting down in course
    bool    ctrlSet;      // control word set
  };

  PIT8253();
  ~PIT8253();

  void setCallbacks(void * context, ChangeOut changeOut) {
    m_context   = context;
    m_changeOut = changeOut;
  }

  void reset();

  void tick();

  void write(int reg, uint8_t value);
  uint8_t read(int reg);

  bool getOut(int timerIndex)  { return m_timer[timerIndex].out; }
  bool getGate(int timerIndex) { return m_timer[timerIndex].gate; }

  void setGate(int timerIndex, bool value);

  TimerInfo const & timerInfo(int timerIndex) { return m_timer[timerIndex]; }


private:

  void changeOut(int timer, bool value);


  TimerInfo         m_timer[3];

  // callbacks
  void *            m_context;
  ChangeOut         m_changeOut;
  uint64_t          m_lastTickTime;

};


} // namespace fabgl
