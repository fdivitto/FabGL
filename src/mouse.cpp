/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
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


#include "Arduino.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "mouse.h"
#include "ps2controller.h"


fabgl::MouseClass Mouse;



namespace fabgl {


MouseClass::MouseClass()
  : m_mouseAvailable(false), m_mouseType(LegacyMouse), m_prevDeltaTime(0), m_wheelDelta(0), m_movementAcceleration(180), m_wheelAcceleration(60000)
{
}


void MouseClass::begin(int PS2Port)
{
  PS2DeviceClass::begin(PS2Port);
  reset();
}


void MouseClass::begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO)
{
  PS2Controller.begin(clkGPIO, dataGPIO);
  begin(0);
}


bool MouseClass::reset()
{
  // tries up to six times for mouse reset
  for (int i = 0; i < 6; ++i) {
    m_mouseAvailable = send_cmdReset();
    if (m_mouseAvailable)
      break;
    delay(500);
  }

  // negotiate compatibility
  if (m_mouseAvailable) {
    // try Intellimouse (three buttons + scrolling wheel, 4 bytes packet)
    if (send_cmdSetSampleRate(200) && send_cmdSetSampleRate(100) && send_cmdSetSampleRate(80) && identify() == PS2Device::MouseWithScrollWheel) {
      // Intellimouse ok!
      m_mouseType = Intellimouse;
    }
  }

  return m_mouseAvailable;
}


int MouseClass::getPacketSize()
{
  return (m_mouseType == Intellimouse ? 4 : 3);
}



int MouseClass::deltaAvailable()
{
  return dataAvailable() / getPacketSize();
}


bool MouseClass::getNextDelta(MouseDelta * delta, int timeOutMS, bool requestResendOnTimeOut)
{
  // receive packet
  int packetSize = getPacketSize();
  int rcv[packetSize];
  for (int i = 0; i < packetSize; ++i) {
    while (true) {
      rcv[i] = getData(timeOutMS);
      if (rcv[i] == -1 && requestResendOnTimeOut) {
        requestToResendLastByte();
        continue;
      }
      break;
    }
    if (rcv[i] < 0)
      return false;  // timeout
  }

  // decode packet
  delta->deltaX         = (int16_t)(rcv[0] & 0x10 ? 0xFF00 | rcv[1] : rcv[1]);
  delta->deltaY         = (int16_t)(rcv[0] & 0x20 ? 0xFF00 | rcv[2] : rcv[2]);
  delta->deltaZ         = (int8_t)(packetSize > 3 ? rcv[3] : 0);
  delta->buttons.left   = (rcv[0] & 0x01 ? 1 : 0);
  delta->buttons.middle = (rcv[0] & 0x04 ? 1 : 0);
  delta->buttons.right  = (rcv[0] & 0x02 ? 1 : 0);
  delta->overflowX      = (rcv[0] & 0x40 ? 1 : 0);
  delta->overflowY      = (rcv[0] & 0x80 ? 1 : 0);

  return true;
}


void MouseClass::setAbsolutePositionArea(int width, int height)
{
  m_area           = Size(width, height);
  m_position       = Point(width >> 1, height >> 1);
  m_buttons.left   = 0;
  m_buttons.middle = 0;
  m_buttons.right  = 0;
  m_wheelDelta     = 0;
}


// return if mouse position or buttons has been updated
bool MouseClass::updateAbsolutePosition(int maxEventsConsumeTimeMS)
{
  // consume deltas queue (up to maxEventsConsumeTimeMS milliseconds)
  bool r = false;
  TimeOut timeout;
  while (deltaAvailable() && !timeout.expired(maxEventsConsumeTimeMS)) {
    MouseDelta delta;
    r = getNextDelta(&delta, 0, false);
    updateAbsolutePosition(&delta);
  }
  return r;
}


void MouseClass::updateAbsolutePosition(MouseDelta * delta)
{
  const int maxDeltaTimeUS = 500000; // after 0.5s doesn't consider acceleration

  int dx = delta->deltaX;
  int dy = delta->deltaY;
  int dz = delta->deltaZ;

  int64_t now   = esp_timer_get_time();
  int deltaTime = now - m_prevDeltaTime; // time in microseconds

  if (deltaTime < maxDeltaTimeUS) {

    // calcualte movement acceleration
    if (dx != 0 || dy != 0) {
      int   deltaDist    = isqrt(dx * dx + dy * dy);                 // distance in mouse points
      float vel          = (float)deltaDist / deltaTime;             // velocity in mousepoints/microsecond
      float newVel       = vel + m_movementAcceleration * vel * vel; // new velocity
      int   newDeltaDist = newVel * deltaTime;                       // new distance
      dx = dx * newDeltaDist / deltaDist;
      dy = dy * newDeltaDist / deltaDist;
    }

    // calculate wheel acceleration
    if (dz != 0) {
      int   deltaDist    = abs(dz);                                  // distance in wheel points
      float vel          = (float)deltaDist / deltaTime;             // velocity in mousepoints/microsecond
      float newVel       = vel + m_wheelAcceleration * vel * vel;    // new velocity
      int   newDeltaDist = newVel * deltaTime;                       // new distance
      dz = dz * newDeltaDist / deltaDist;
    }

  }

  m_position.X    = tclamp((int)m_position.X + dx, 0, m_area.width  - 1);
  m_position.Y    = tclamp((int)m_position.Y - dy, 0, m_area.height - 1);
  m_wheelDelta   += dz;
  m_buttons       = delta->buttons;
  m_prevDeltaTime = now;
}


int MouseClass::wheelDelta()
{
  int ret = m_wheelDelta;
  m_wheelDelta = 0;
  return ret;
}


} // end of namespace
