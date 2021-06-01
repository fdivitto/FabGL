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



#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "fabglconf.h"
#include "ps2controller.h"


/**
 * @file
 *
 * @brief This file contains fabgl::PS2Device definition.
 */


namespace fabgl {



/** \ingroup Enumerations
 * @brief Represents the type of device attached to PS/2 port.
 */
enum class PS2DeviceType {
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
 * PS2Device connects to one port of the PS2 Controller class (fabgl::PS2Controller).<br>
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with the PS2 device.<br>
 */
class PS2Device {

public:

  /**
   * @brief Identifies the device attached to the PS2 port.
   *
   * @return The identification ID sent by keyboard.
   */
  PS2DeviceType identify() { PS2DeviceType result; send_cmdIdentify(&result); return result; };

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

  bool parityError();

  bool syncError();

  bool CLKTimeOutError();

  /**
   * @brief Sends a raw command to the PS/2 device and wait for reply
   *
   * @param cmd The command byte
   * @param expectedReply Expected reply from PS/2 device
   *
   * @return True on success
   */
  bool sendCommand(uint8_t cmd, uint8_t expectedReply);

  /**
   * @brief Sends a raw command to the PS/2 device
   *
   * @param cmd The command byte
   */
  void sendCommand(uint8_t cmd);

  /**
   * @brief Suspends PS/2 port driving the CLK line Low
   *
   * Use resumePort() to release CLK line.
   */
  void suspendPort();

  /**
   * @brief Resumes PS/2 port releasing CLK line
   *
   * Use suspendPort() to suspend.
   */
  void resumePort();

  uint16_t deviceID()    { return m_deviceID; }


protected:

  PS2Device();
  ~PS2Device();

  void quickCheckHardware();

  void begin(int PS2Port);

  int dataAvailable();
  int getData(int timeOutMS);

  void requestToResendLastByte();

  bool send_cmdLEDs(bool numLock, bool capsLock, bool scrollLock);
  bool send_cmdEcho();
  bool send_cmdGetScancodeSet(uint8_t * result);
  bool send_cmdSetScancodeSet(uint8_t scancodeSet);
  bool send_cmdIdentify(PS2DeviceType * result);
  bool send_cmdDisableScanning();
  bool send_cmdEnableScanning();
  bool send_cmdTypematicRateAndDelay(int repeatRateMS, int repeatDelayMS);
  bool send_cmdSetSampleRate(int sampleRate);
  bool send_cmdSetDefaultParams();
  bool send_cmdReset();
  bool send_cmdSetResolution(int resolution);
  bool send_cmdSetScaling(int scaling);

private:

  int16_t           m_PS2Port;
  int16_t           m_cmdTimeOut;
  int16_t           m_cmdSubTimeOut;
  uint16_t          m_deviceID;       // read by send_cmdIdentify()
};



struct PS2DeviceLock {
  PS2DeviceLock(PS2Device * PS2Device) : m_PS2Device(PS2Device) { m_PS2Device->lock(-1); }
  ~PS2DeviceLock() { m_PS2Device->unlock(); }

  PS2Device * m_PS2Device;
};







} // end of namespace



