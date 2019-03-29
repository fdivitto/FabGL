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

#include "ps2device.h"



namespace fabgl {


#define PS2_CMD_SETLEDS                      0xED
#define PS2_CMD_ECHO                         0xEE
#define PS2_CMD_GETSET_CURRENT_SCANCODE_SET  0xF0   // keyboard specific
#define PS2_CMD_SET_REMOTE_MODE              0xF0   // mouse specific
#define PS2_CMD_IDENTIFY                     0xF2
#define PS2_CMD_SET_TYPEMATIC_RATE_AND_DELAY 0xF3   // keyboard specific
#define PS2_CMD_SET_SAMPLE_RATE              0xF3   // mouse specific
#define PS2_CMD_ENABLE_SCANNING              0xF4
#define PS2_CMD_DISABLE_SCANNING             0xF5
#define PS2_CMD_SET_DEFAULT_PARAMS           0xF6
#define PS2_CMD_RESEND_LAST_BYTE             0xFE
#define PS2_CMD_RESET                        0xFF
#define PS2_CMD_SET_STREAM_MODE              0xEA   // mouse specific
#define PS2_CMD_STATUS_REQUEST               0xE9   // mouse specific
#define PS2_CMD_SET_RESOLUTION               0xE8   // mouse specific
#define PS2_CMD_SET_SCALING                  0xE6   // mouse specific

#define PS2_REPLY_ERROR1                     0x00
#define PS2_REPLY_ERROR2                     0xFF
#define PS2_REPLY_SELFTEST_OK                0xAA
#define PS2_REPLY_ECHO                       0xEE
#define PS2_REPLY_ACK                        0xFA
#define PS2_REPLY_SELFTEST_FAILED1           0xFC
#define PS2_REPLY_SELFTEST_FAILED2           0xFD
#define PS2_REPLY_RESEND                     0xFE

#define PS2_CMD_RETRY_COUNT                     3
#define PS2_CMD_TIMEOUT                        10




void PS2DeviceClass::begin(int PS2Port)
{
  m_PS2Port = PS2Port;
}


bool PS2DeviceClass::sendCommand(uint8_t cmd, uint8_t expectedReply)
{
  for (int i = 0; i < PS2_CMD_RETRY_COUNT; ++i) {
    PS2Controller.sendData(cmd, m_PS2Port);
    if (getReplyCode(PS2_CMD_TIMEOUT) != expectedReply)
      continue;
    return true;
  }
  return false;
}


void PS2DeviceClass::requestToResendLastByte()
{
  PS2Controller.sendData(PS2_CMD_RESEND_LAST_BYTE, m_PS2Port);
}


bool PS2DeviceClass::send_cmdLEDs(bool numLock, bool capsLock, bool scrollLock)
{
  if (!sendCommand(PS2_CMD_SETLEDS, PS2_REPLY_ACK))
    return false;
  bool ret = sendCommand((scrollLock ? 1 : 0) | (numLock ? 2 : 0) | (capsLock ? 4 : 0), PS2_REPLY_ACK);
  return ret;
}


bool PS2DeviceClass::send_cmdEcho()
{
  return sendCommand(PS2_CMD_ECHO, PS2_REPLY_ECHO);
}


bool PS2DeviceClass::send_cmdGetScancodeSet(uint8_t * result)
{
  if (!sendCommand(PS2_CMD_GETSET_CURRENT_SCANCODE_SET, PS2_REPLY_ACK))
    return false;
  if (!sendCommand(0, PS2_REPLY_ACK))
    return false;
  *result = getReplyCode(PS2_CMD_TIMEOUT);
  return (*result >= 1 || *result <= 3);
}


bool PS2DeviceClass::send_cmdSetScancodeSet(uint8_t scancodeSet)
{
  if (!sendCommand(PS2_CMD_GETSET_CURRENT_SCANCODE_SET, PS2_REPLY_ACK))
    return false;
  return sendCommand(scancodeSet, PS2_REPLY_ACK);
}


// return value is always valid
bool PS2DeviceClass::send_cmdIdentify(PS2Device * result)
{
  *result = PS2Device::UnknownPS2Device;
  if (!send_cmdDisableScanning())
    return false;
  if (!sendCommand(PS2_CMD_IDENTIFY, PS2_REPLY_ACK))
    return false;
  int b1 = getReplyCode(PS2_CMD_TIMEOUT);
  int b2 = getReplyCode(PS2_CMD_TIMEOUT);
  if (b1 == -1 && b2 == -1)
    *result = PS2Device::OldATKeyboard;
  else if (b1 == 0x00 && b2 == -1)
    *result = PS2Device::MouseStandard;
  else if (b1 == 0x03 && b2 == -1)
    *result = PS2Device::MouseWithScrollWheel;
  else if (b1 == 0x04 && b2 == -1)
    *result = PS2Device::Mouse5Buttons;
  else if ((b1 == 0xAB && b2 == 0x41) || (b1 == 0xAB && b2 == 0xC1))
    *result = PS2Device::MF2KeyboardWithTranslation;
  else if (b1 == 0xAB && b2 == 0x83)
    *result = PS2Device::M2Keyboard;
  return send_cmdEnableScanning();
}


bool PS2DeviceClass::send_cmdDisableScanning()
{
  return sendCommand(PS2_CMD_DISABLE_SCANNING, PS2_REPLY_ACK);
}


bool PS2DeviceClass::send_cmdEnableScanning()
{
  return sendCommand(PS2_CMD_ENABLE_SCANNING, PS2_REPLY_ACK);
}


const int16_t REPEATRATES[32] = { 33,  37,  41,  45,  50,  54,  58,  62,  66,  75,  83,  91,
                                 100, 108, 125, 125, 133, 149, 166, 181, 200, 217, 232, 250,
                                 270, 303, 333, 370, 400, 434, 476, 500};


// repeatRateMS  :  33 ms ...  500 ms (in steps as above REPEATRATES table)
// repeatDelayMS : 250 ms ... 1000 ms (in steps of 250 ms)
bool PS2DeviceClass::send_cmdTypematicRateAndDelay(int repeatRateMS, int repeatDelayMS)
{
  if (!sendCommand(PS2_CMD_SET_TYPEMATIC_RATE_AND_DELAY, PS2_REPLY_ACK))
    return false;
  uint8_t byteToSend = 0b01011; // default repeat rate 10.9 characters per seconds (91ms)
  for (int i = 0; i < 32; ++i)
    if (REPEATRATES[i] >= repeatRateMS) {
      byteToSend = i;
      break;
    }
  byteToSend |= (repeatDelayMS / 250 - 1) << 5;
  return sendCommand(byteToSend, PS2_REPLY_ACK);
}


// sampleRate: valid values are 10, 20, 40, 60, 80, 100, and 200
bool PS2DeviceClass::send_cmdSetSampleRate(int sampleRate)
{
  if (!sendCommand(PS2_CMD_SET_SAMPLE_RATE, PS2_REPLY_ACK))
    return false;
  return sendCommand(sampleRate, PS2_REPLY_ACK);
}


// resolution:
//   0 = 1 count/mm
//   1 = 2 count/mm
//   2 = 4 count/mm
//   3 = 8 count/mm
bool PS2DeviceClass::send_cmdSetResolution(int resolution)
{
  if (!sendCommand(PS2_CMD_SET_RESOLUTION, PS2_REPLY_ACK))
    return false;
  return sendCommand(resolution, PS2_REPLY_ACK);
}


// scaling:
//   1 -> 1:1
//   2 -> 1:2
bool PS2DeviceClass::send_cmdSetScaling(int scaling)
{
  if (!sendCommand(PS2_CMD_SET_SCALING, PS2_REPLY_ACK))
    return false;
  return sendCommand(scaling, PS2_REPLY_ACK);
}


bool PS2DeviceClass::send_cmdSetDefaultParams()
{
  return sendCommand(PS2_CMD_SET_DEFAULT_PARAMS, PS2_REPLY_ACK);
}


bool PS2DeviceClass::send_cmdReset()
{
  if (!sendCommand(PS2_CMD_RESET, PS2_REPLY_ACK))
    return false;
  return getReplyCode(500) == PS2_REPLY_SELFTEST_OK; // timout 500ms should be enough for PS2 device reset and selft test
}


} // end of namespace
