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


#ifndef _PS2DEVICE_H_INCLUDED
#define _PS2DEVICE_H_INCLUDED



#include "freertos/FreeRTOS.h"

#include "fabglconf.h"
#include "ps2controller.h"


namespace fabgl {



/**
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



class PS2DeviceClass {

protected:

  void begin(int PS2Port);

  int dataAvailable()             { return PS2Controller.dataAvailable(m_PS2Port); }

  int getData(int timeOutMS)      { return PS2Controller.getData(timeOutMS, false, m_PS2Port); }
  int getReplyCode(int timeOutMS) { return PS2Controller.getData(timeOutMS, true, m_PS2Port); }

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

  int m_PS2Port;
};





} // end of namespace







#endif


