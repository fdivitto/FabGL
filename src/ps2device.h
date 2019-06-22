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


#pragma once



#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "fabglconf.h"
#include "ps2controller.h"


/**
 * @file
 *
 * @brief This file contains fabgl::PS2DeviceClass definition.
 */


namespace fabgl {



/** \ingroup Enumerations
 * @brief Represents the type of device attached to PS/2 port.
 */
enum PS2Device {
  UnknownPS2Device,             /**< Unknown device or unable to connect to the device */
  OldATKeyboard,                /**< Old AT keyboard */
  MouseStandard,                /**< Standard mouse */
  MouseWithScrollWheel,         /**< Mouse with Scroll Wheel */
  Mouse5Buttons,                /**< Mouse with 5 buttons */
  MF2KeyboardWithTranslation,   /**< Standard MF2 keyboard with translation */
  M2Keyboard,                   /**< Standard MF2 keyboard. This is the most common value returned by USB/PS2 modern keyboards */
};



/**
 * @brief Base class for PS2 devices (like mouse or keyboard).
 *
 * PS2DeviceClass connects to one port of the PS2 Controller class (fabgl::PS2ControllerClass).<br>
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with the PS2 device.<br>
 *
 * Applications should not use PS2DeviceClass directly: use instead fabgl::MouseClass or fabgl::KeyboardClass.
 */
class PS2DeviceClass {

public:

  /**
   * @brief Identifies the device attached to the PS2 port.
   *
   * @return The identification ID sent by keyboard.
   */
  PS2Device identify() { PS2Device result; send_cmdIdentify(&result); return result; };

  /**
   * @brief Gets exclusive access to the device.
   *
   * @param timeOutMS Timeout in milliseconds to wait before fail.
   *
   * @return True if the device has been locked.
   */
  bool lock(int timeOutMS);

  /**
   * @brief Releases device from exclusive access.
   */
  void unlock();

protected:

  PS2DeviceClass();
  ~PS2DeviceClass();

  void begin(int PS2Port);

  int dataAvailable();
  int getData(int timeOutMS);

  void requestToResendLastByte();

  bool sendCommand(uint8_t cmd, uint8_t expectedReply);

  bool send_cmdLEDs(bool numLock, bool capsLock, bool scrollLock);
  bool send_cmdEcho();
  bool send_cmdGetScancodeSet(uint8_t * result);
  bool send_cmdSetScancodeSet(uint8_t scancodeSet);
  bool send_cmdIdentify(PS2Device * result);
  bool send_cmdDisableScanning();
  bool send_cmdEnableScanning();
  bool send_cmdTypematicRateAndDelay(int repeatRateMS, int repeatDelayMS);
  bool send_cmdSetSampleRate(int sampleRate);
  bool send_cmdSetDefaultParams();
  bool send_cmdReset();
  bool send_cmdSetResolution(int resolution);
  bool send_cmdSetScaling(int scaling);

private:

  int               m_PS2Port;
  SemaphoreHandle_t m_deviceLock;
};



struct PS2DeviceLock {
  PS2DeviceLock(PS2DeviceClass * PS2Device) : m_PS2Device(PS2Device) { m_PS2Device->lock(-1); }
  ~PS2DeviceLock() { m_PS2Device->unlock(); }

  PS2DeviceClass * m_PS2Device;
};







} // end of namespace



