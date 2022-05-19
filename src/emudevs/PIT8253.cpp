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


#include <string.h>

#include "PIT8253.h"



namespace fabgl {



PIT8253::PIT8253()
{
  FRC1Timer_init(PIT_FRC1_PRESCALER);
}


PIT8253::~PIT8253()
{
}


void PIT8253::reset()
{
  for (int i = 0; i < 3; ++i) {
    memset(&m_timer[i], 0, sizeof(TimerInfo));
    m_timer[i].mode      = 3;
    m_timer[i].RLMode    = 3;
    m_timer[i].latch     = -1;
    m_timer[i].LSBToggle = true;
  }
  m_lastTickTime = FRC1Timer();
  m_acc = 0;
}


void PIT8253::write(int reg, uint8_t value)
{
  // just in case ticks are needed
  tick();

  if (reg == 3) {

    // write control register
    
    int timerIndex = (value >> 6) & 0x03;
    auto & timer = m_timer[timerIndex];

    int RLMode = (value >> 4) & 0x03;

    if (RLMode == 0) {
      // counter latching operation (doesn't change BCD or mode)
      timer.latch     = timer.count;
      timer.LSBToggle = true;
      timer.ctrlSet   = false;
      //printf("%lld, PIT8253: write ctrl reg, %02X (timer=%d, latched=%04X)\n", esp_timer_get_time(), value, timerIndex, timer.latch);
    } else {
      // read / load
      timer.mode    = (value >> 1) & 0x07;
      timer.BCD     = (value & 1) == 1;
      timer.RLMode  = RLMode;
      timer.ctrlSet = true;
      if (RLMode == 3)
        timer.LSBToggle = true;
      //printf("%lld, PIT8253: write ctrl reg, %02X (timer=%d, mode=%d)\n", esp_timer_get_time(), value, timerIndex, timer.mode);
    }

    if (value >> 6 == 3)
      printf("8253, read back. Required 8254?\n");

  } else {

    // write timers registers
    
    int timerIndex = reg;
    auto & timer = m_timer[timerIndex];
    
    bool writeLSB = false;

    switch (timer.RLMode) {
      case 1:
        writeLSB = true;
        break;
      case 3:
        writeLSB = timer.LSBToggle;
        timer.LSBToggle = !timer.LSBToggle;
        break;
    }

    if (writeLSB) {
      // LSB
      //printf("%lld, PIT8253: timer %d write LSB, %02X\n", esp_timer_get_time(), timerIndex, value);
      timer.resetHolding = (timer.resetHolding & 0xFF00) | value;
    } else {
      // MSB
      //printf("%lld, PIT8253: timer %d write MSB, %02X\n", esp_timer_get_time(), timerIndex, value);
      timer.resetHolding = (timer.resetHolding & 0x00FF) | (((int)value) << 8);
      timer.resetCount   = timer.resetHolding;
      if (timer.ctrlSet) {
        timer.count   = (uint16_t)(timer.resetCount - 1);
        timer.ctrlSet = false;
      }
    }

    // OUT: with mode 0 it starts low, other modes it starts high
    changeOut(timerIndex, timer.mode != 0);

  }

}


uint8_t PIT8253::read(int reg)
{
  // just in case ticks are needed
  tick();

  uint8_t value = 0;

  if (reg < 3) {
    // read timers registers
    auto & timer = m_timer[reg];

    int readValue = timer.latch != -1 ? timer.latch : timer.count;

    bool readLSB = false;
    if (timer.RLMode == 1) {
      readLSB = true;
    } else if (timer.RLMode == 3) {
      readLSB = timer.LSBToggle;
      timer.LSBToggle = !timer.LSBToggle;
    }

    if (readLSB) {
      value = readValue & 0xFF;
    } else {
      value = (readValue >> 8) & 0xFF;
      timer.latch = -1;
    }
    //printf("read reg %d => %02X (%04X)\n", reg, value, readValue);
  }

  return value;
}


void PIT8253::setGate(int timerIndex, bool value)
{
  // just in case ticks are needed
  tick();
  
  auto & timer = m_timer[timerIndex];

  switch (timer.mode) {
    case 0:
    case 2:
    case 3:
      // running when gate is high
      timer.running = value;
      break;
    case 1:
    case 5:
      // switch to running when gate changes to high
      if (timer.gate == false && value == true)
        timer.running = true;
      break;
  }
  switch (timer.mode) {
    case 2:
    case 3:
      if (value == false)
        changeOut(timerIndex, true);
      break;
  }
  if (!timer.gate && value)
    timer.count = timer.resetCount;
  timer.gate = value;
  //printf("setGate(%d, %d) [gate=%d running=%d]\n", timerIndex, value, timer.gate, timer.running);
}


void PIT8253::changeOut(int timer, bool value)
{
  if (value != m_timer[timer].out) {
    //printf("timer %d, out = %d\n", timer, value);
    m_timer[timer].out = value;
    m_changeOut(m_context, timer);
  }
}


void PIT8253::tick()
{
  uint32_t now = FRC1Timer();
  int32_t diff = now - m_lastTickTime;
  if (diff < 0)
    diff = FRC1TimerMax - m_lastTickTime + now;

  constexpr uint32_t BITS = 10;
  constexpr uint32_t INC  = (PIT_TICK_FREQ << BITS) / PIT_FRC1_FREQUENCY;
  
  m_acc += INC * diff;
  int ticks = m_acc >> BITS;
  m_acc &= (1 << BITS) - 1;
  
  m_lastTickTime = now;
  
  if (ticks == 0)
    return;
    
  if (ticks > 65535) {
    //printf("Too much ticks! (%d)\n", ticks);
    m_acc += (ticks - 65535) << BITS;
    ticks = 65535;
  }

  for (int timerIndex = 0; timerIndex < 3; ++timerIndex) {

    auto & timer = m_timer[timerIndex];

    if (timer.running) {

      // modes 4 or 5: end of ending low pulse?
      if (timer.mode >= 4 && timer.out == false) {
        // mode 4, end of low pulse
        changeOut(timerIndex, true);
        timer.running = false;
        timer.count   = 65535;
        continue;
      }

      timer.count -= ticks;

      // in mode 3 each tick subtract 2 instead of 1
      if (timer.mode == 3)
        timer.count -= ticks;

      if (timer.count <= 0) {
        // count terminated
        timer.count += timer.resetCount == 0 ? 65536 : timer.resetCount;
        switch (timer.mode) {
          case 0:
          case 1:
            // at the end OUT goes high
            changeOut(timerIndex, true);
            break;
          case 2:
            changeOut(timerIndex, false);
            break;
          case 3:
            changeOut(timerIndex, !timer.out);
            break;
        }
      } else {
        // count running
        switch (timer.mode) {
          case 1:
          case 4:
          case 5:
            // start low pulse
            changeOut(timerIndex, false);
            break;
          case 2:
            changeOut(timerIndex, true);
            break;
        }
      }
      
      //printf("running: timer %d [count = %04X, ticks = %d]\n", timerIndex, timer.count, ticks);

    }

  }
  
}


} // namespace fabgl
