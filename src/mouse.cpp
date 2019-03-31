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
  : m_mouseAvailable(false), m_mouseType(LegacyMouse)
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
  delta->deltaX       = (int16_t)(rcv[0] & 0x10 ? 0xFF00 | rcv[1] : rcv[1]);
  delta->deltaY       = (int16_t)(rcv[0] & 0x20 ? 0xFF00 | rcv[2] : rcv[2]);
  delta->deltaZ       = (int8_t)(packetSize > 3 ? rcv[3] : 0);
  delta->leftButton   = (rcv[0] & 0x01 ? 1 : 0);
  delta->middleButton = (rcv[0] & 0x04 ? 1 : 0);
  delta->rightButton  = (rcv[0] & 0x02 ? 1 : 0);
  delta->overflowX    = (rcv[0] & 0x40 ? 1 : 0);
  delta->overflowY    = (rcv[0] & 0x80 ? 1 : 0);

  return true;
}



} // end of namespace
