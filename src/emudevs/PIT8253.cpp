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


#include "PIT8253.h"



namespace fabgl {



PIT8253::PIT8253()
{
  m_mutex = xSemaphoreCreateMutex();
}


PIT8253::~PIT8253()
{
  vSemaphoreDelete(m_mutex);
}


// because vTaskDelay has a resolution of 1ms, updatesPerSec should be <=1000
void PIT8253::runAutoTick(int freq, int updatesPerSec)
{
  m_autoTickFreq  = freq;
  m_updatesPerSec = updatesPerSec;
  xTaskCreatePinnedToCore(&autoTickTask, "", 2048, this, 5, &m_taskHandle, CoreUsage::busiestCore());
}


void PIT8253::autoTickTask(void * pvParameters)
{
  auto p = (PIT8253*)pvParameters;

  const int ticks = p->m_autoTickFreq / p->m_updatesPerSec;
  const int delay_ms = 1000 / p->m_updatesPerSec / portTICK_PERIOD_MS;
  //const int delay_us = 1000000 / p->m_updatesPerSec;

  while (true) {
    p->tick(ticks);
    vTaskDelay(delay_ms);
    //ets_delay_us(delay_us);
  }
}


void PIT8253::reset()
{
  AutoSemaphore autoSemaphore(m_mutex);
  m_timer[0] = { false, 3, 3, 0, 0, 0, -1, true, false, false, false };
  m_timer[1] = { false, 3, 3, 0, 0, 0, -1, true, false, false, false };
  m_timer[2] = { false, 3, 3, 0, 0, 0, -1, true, false, false, false };
}


void PIT8253::write(int reg, uint8_t value)
{
  AutoSemaphore autoSemaphore(m_mutex);

  int timerIndex;

  if (reg == 3) {

    // write control register
    timerIndex = (value >> 6) & 0x03;

    int RLMode = (value >> 4) & 0x03;
    if (RLMode == 0) {
      // counter latching operation (doesn't change BCD or mode)
      m_timer[timerIndex].latch = m_timer[timerIndex].count;
      m_timer[timerIndex].LSBToggle = true;
    } else {
      // read/load
      m_timer[timerIndex].mode = (value >> 1) & 0x07;
      m_timer[timerIndex].BCD  = (value & 1) == 1;
      m_timer[timerIndex].RLMode = RLMode;
      if (RLMode == 3)
        m_timer[timerIndex].LSBToggle = true;
    }

    //printf("PIT8253: write ctrl reg, %02X (timer=%d, mode=%d)\n", value, timerIndex, m_timer[timerIndex].mode);
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
      //printf("PIT8253: write LSB, %02X\n", value);
      m_timer[timerIndex].resetHolding = (m_timer[timerIndex].resetHolding & 0xFF00) | value;
    } else {
      // MSB
      //printf("PIT8253: write MSB, %02X\n", value);
      m_timer[timerIndex].resetHolding = (m_timer[timerIndex].resetHolding & 0x00FF) | (((int)value) << 8);
      m_timer[timerIndex].resetCount = m_timer[timerIndex].resetHolding;
      if (m_timer[timerIndex].mode == 0) {
        m_timer[timerIndex].count = (uint16_t)(m_timer[timerIndex].resetCount - 1);
      }
    }

    // OUT: with mode 0 it starts low, other modes it starts high
    changeOut(timerIndex, m_timer[timerIndex].mode != 0);

  }

}


uint8_t PIT8253::read(int reg)
{
  AutoSemaphore autoSemaphore(m_mutex);

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
  }

  // this avoids that two timer reads returns the same value (causing division by zero) maybe because the update time is still not expired.
  // Unfortunately makes the timers less precise!
  unsafeTick(1);

  return value;
}


void PIT8253::setGate(int timerIndex, bool value)
{
  AutoSemaphore autoSemaphore(m_mutex);
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
  m_timer[timerIndex].gate = value;
}


void PIT8253::changeOut(int timer, bool value)
{
  if (value != m_timer[timer].out) {
    //printf("timer %d, out = %d\n", timer, value);
    m_timer[timer].out = value;
    m_changeOut(m_context, timer);
  }
}


void PIT8253::tick(int ticks)
{
  AutoSemaphore autoSemaphore(m_mutex);
  unsafeTick(ticks);
  m_tick(m_context, ticks);
}


void PIT8253::unsafeTick(int ticks)
{
  for (int timerIndex = 0; timerIndex < 3; ++timerIndex) {

    if (m_timer[timerIndex].running) {

      // modes 4 or 5, end of ending low pulse?
      if (m_timer[timerIndex].mode >= 4 && m_timer[timerIndex].out == false) {
        // mode 4, end of low pulse
        changeOut(timerIndex, true);
        m_timer[timerIndex].running = false;
        m_timer[timerIndex].count = 65535;
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

  }

  //printf("timer 0 count = %d\n", m_timer[0].count);

}


} // namespace fabgl
