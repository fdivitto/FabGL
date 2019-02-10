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


#ifndef _PS2CONTROLLER_H_INCLUDED
#define _PS2CONTROLLER_H_INCLUDED


/**
 * @file
 *
 * @brief This file contains fabgl::PS2ControllerClass definition and the PS2Controller instance.
 */


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "fabglconf.h"


namespace fabgl {



/**
 * @brief The PS2 device controller class.
 *
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with the PS2 device.<br>
 * The ULP coprocessor continuously monitor CLK and DATA lines for incoming data. Optionally can send commands to the PS2 device.
 */
class PS2ControllerClass {

public:

  /**
  * @brief Initialize PS2 device controller.
  *
  * @param clkGPIO The GPIO number of Clock line
  * @param datGPIO The GPIO number of Data line
  */
  void begin(gpio_num_t clkGPIO, gpio_num_t datGPIO);

  /**
   * @brief Get the number of scancodes available in the controller buffer.
   *
   * @return The number of scancodes available to read.
   */
  int dataAvailable();

  /**
   * @brief Get a scancode from the queue.
   *
   * @param timeOutMS Timeout in milliseconds. -1 means no timeout (infinite time).
   * @param isReply Set true when waiting for a reply from a command sent to the device.
   *
   * @return The first scancode of the queue (-1 if no data is available in the timeout period).
   */
  int getData(int timeOutMS = -1, bool isReply = false);

  /**
   * @brief Send a command to the device.
   *
   * @param data Byte to send to the PS2 device.
   */
  void sendData(uint8_t data);


private:

  static void IRAM_ATTR rtc_isr(void * arg);

  // address of next word to read in the circular buffer
  int                   m_readPos;

  // address of next word to read in the circular buffer. Set by sendData() and used by getReplyData()
  volatile int          m_replyReadPos;

  // task that is waiting for TX ends
  volatile TaskHandle_t m_TXWaitTask;

  // task that is waiting for RX event
  volatile TaskHandle_t m_RXWaitTask;

};





} // end of namespace



extern fabgl::PS2ControllerClass PS2Controller;



#endif

