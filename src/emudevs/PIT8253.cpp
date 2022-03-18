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
  m_lastTickTime = esp_timer_get_time();
}


void PIT8253::write(int reg, uint8_t value)
{
  // just in case ticks are needed
  tick();

  int timerIndex;

  if (reg == 3) {

    // write control register
    timerIndex = (value >> 6) & 0x03;

    int RLMode = (value >> 4) & 0x03;

    if (RLMode == 0) {
      // counter latching operation (doesn't change BCD or mode)
      m_timer[timerIndex].latch     = m_timer[timerIndex].count;
      m_timer[timerIndex].LSBToggle = true;
      m_timer[timerIndex].ctrlSet   = false;
      //printf("%lld, PIT8253: write ctrl reg, %02X (timer=%d, latched=%04X)\n", esp_timer_get_time(), value, timerIndex, m_timer[timerIndex].latch);
    } else {
      // read / load
      m_timer[timerIndex].mode    = (value >> 1) & 0x07;
      m_timer[timerIndex].BCD     = (value & 1) == 1;
      m_timer[timerIndex].RLMode  = RLMode;
      m_timer[timerIndex].ctrlSet = true;
      if (RLMode == 3)
        m_timer[timerIndex].LSBToggle = true;
      //printf("%lld, PIT8253: write ctrl reg, %02X (timer=%d, mode=%d)\n", esp_timer_get_time(), value, timerIndex, m_timer[timerIndex].mode);
    }

    if (value >> 6 == 3)
      printf("8253, read back. Required 8254?\n");

  } else {

    // write timers registers
    timerIndex = reg;
    bool writeLSB = false;

    switch (m_timer[timerIndex].RLMode) {
      case 1:
        writeLSB = true;
        break;
      case 3:
        writeLSB = m_timer[timerIndex].LSBToggle;
        m_timer[timerIndex].LSBToggle = !m_timer[timerIndex].LSBToggle;
        break;
    }

    if (writeLSB) {
      // LSB
      //printf("%lld, PIT8253: timer %d write LSB, %02X\n", esp_timer_get_time(), timerIndex, value);
      m_timer[timerIndex].resetHolding = (m_timer[timerIndex].resetHolding & 0xFF00) | value;
    } else {
      // MSB
      //printf("%lld, PIT8253: timer %d write MSB, %02X\n", esp_timer_get_time(), timerIndex, value);
      m_timer[timerIndex].resetHolding = (m_timer[timerIndex].resetHolding & 0x00FF) | (((int)value) << 8);
      m_timer[timerIndex].resetCount   = m_timer[timerIndex].resetHolding;
      if (m_timer[timerIndex].ctrlSet) {
        m_timer[timerIndex].count   = (uint16_t)(m_timer[timerIndex].resetCount - 1);
        m_timer[timerIndex].ctrlSet = false;
      }
    }

    // OUT: with mode 0 it starts low, other modes it starts high
    changeOut(timerIndex, m_timer[timerIndex].mode != 0);

  }

}


uint8_t PIT8253::read(int reg)
{
  // just in case ticks are needed
  tick();

  uint8_t value = 0;

  if (reg < 3) {
    // read timers registers
    int timerIndex = reg;

    int readValue = m_timer[timerIndex].latch != -1 ? m_timer[timerIndex].latch : m_timer[timerIndex].count;

    bool readLSB = false;
    if (m_timer[timerIndex].RLMode == 1) {
      readLSB = true;
    } else if (m_timer[timerIndex].RLMode == 3) {
      readLSB = m_timer[timerIndex].LSBToggle;
      m_timer[timerIndex].LSBToggle = !m_timer[timerIndex].LSBToggle;
    }

    if (readLSB) {
      value = readValue & 0xFF;
    } else {
      value = (readValue >> 8) & 0xFF;
      m_timer[timerIndex].latch = -1;
    }
    //printf("read reg %d => %02X (%04X)\n", reg, value, readValue);
  }

  return value;
}


void PIT8253::setGate(int timerIndex, bool value)
{
  // just in case ticks are needed
  tick();

  switch (m_timer[timerIndex].mode) {
    case 0:
    case 2:
    case 3:
      // running when gate is high
      m_timer[timerIndex].running = value;
      break;
    case 1:
    case 5:
      // switch to running when gate changes to high
      if (m_timer[timerIndex].gate == false && value == true)
        m_timer[timerIndex].running = true;
      break;
  }
  switch (m_timer[timerIndex].mode) {
    case 2:
    case 3:
      if (value == false)
        changeOut(timerIndex, true);
      break;
  }
  if (!m_timer[timerIndex].gate && value)
    m_timer[timerIndex].count = m_timer[timerIndex].resetCount;
  m_timer[timerIndex].gate = value;
  //printf("setGate(%d, %d) [gate=%d running=%d]\n", timerIndex, value, m_timer[timerIndex].gate, m_timer[timerIndex].running);
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
  uint64_t now = esp_timer_get_time();
  int ticks = (int) ((now - m_lastTickTime) * 1000 / (1000000000 / PIT_TICK_FREQ));
  //printf("ticks=%d (now=%lld last=%lld now-last=%lld)\n", ticks, now, m_lastTickTime, now-m_lastTickTime);
  if (ticks == 0)
    return;
  m_lastTickTime = now;

  if (ticks > 65535)
    printf("Too much ticks! (%d)\n", ticks);

  for (int timerIndex = 0; timerIndex < 3; ++timerIndex) {

    if (m_timer[timerIndex].running) {

      // modes 4 or 5, end of ending low pulse?
      if (m_timer[timerIndex].mode >= 4 && m_timer[timerIndex].out == false) {
        // mode 4, end of low pulse
        changeOut(timerIndex, true);
        m_timer[timerIndex].running = false;
        m_timer[timerIndex].count   = 65535;
        continue;
      }

      m_timer[timerIndex].count -= ticks;

      // in mode 3 each tick subtract 2 instead of 1
      if (m_timer[timerIndex].mode == 3)
        m_timer[timerIndex].count -= ticks;

      if (m_timer[timerIndex].count <= 0) {
        // count terminated
        if (m_timer[timerIndex].resetCount == 0) {
          m_timer[timerIndex].count += 65536;
        } else {
          m_timer[timerIndex].count += m_timer[timerIndex].resetCount;
        }
        switch (m_timer[timerIndex].mode) {
          case 0:
            // at the end OUT goes high
            changeOut(timerIndex, true);
            break;
          case 1:
            // at the end OUT goes high
            changeOut(timerIndex, true);
            break;
          case 2:
            changeOut(timerIndex, false);
            break;
          case 3:
            changeOut(timerIndex, !m_timer[timerIndex].out);
            break;
        }
      } else {
        // count running
        switch (m_timer[timerIndex].mode) {
          case 1:
            // OUT is low while running
            changeOut(timerIndex, false);
            break;
          case 2:
            changeOut(timerIndex, true);
            break;
          case 4:
          case 5:
            // start low pulse
            changeOut(timerIndex, false);
            break;
        }
      }

    }

    //if (m_timer[timerIndex].running) printf("running: timer %d [count = %04X, ticks = %d]\n", timerIndex, m_timer[timerIndex].count, ticks);
  }
}


} // namespace fabgl
