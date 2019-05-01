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



/**
 * @file
 *
 * @brief This file contains fabgl::PS2ControllerClass definition and the PS2Controller instance.
 */


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabglconf.h"


namespace fabgl {



/**
 * @brief The PS2 device controller class.
 *
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with up to two PS2 devices.<br>
 * The ULP coprocessor continuously monitor CLK and DATA lines for incoming data. Optionally can send commands to the PS2 devices.
 */
class PS2ControllerClass {

public:

  /**
  * @brief Initialize PS2 device controller.
  *
  * Initializes the PS2 controller assigning GPIOs to DAT and CLK lines.
  *
  * @param port0_clkGPIO The GPIO number of Clock line for PS/2 port 0.
  * @param port0_datGPIO The GPIO number of Data line for PS/2 port 0.
  * @param port1_clkGPIO The GPIO number of Clock line for PS/2 port 1 (GPIO_NUM_39 to disable).
  * @param port1_datGPIO The GPIO number of Data line for PS/2 port 1 (GPIO_NUM_39 to disable).
  */
  void begin(gpio_num_t port0_clkGPIO, gpio_num_t port0_datGPIO, gpio_num_t port1_clkGPIO = GPIO_NUM_39, gpio_num_t port1_datGPIO = GPIO_NUM_39);

  /**
   * @brief Get the number of scancodes available in the controller buffer.
   *
   * @param PS2Port PS2 port number (0 = port 0, 1 = port1).
   *
   * @return The number of scancodes available to read.
   */
  int dataAvailable(int PS2Port);

  bool waitData(int timeOutMS, int PS2Port);

  /**
   * @brief Get a scancode from the queue.
   *
   * @param PS2Port PS2 port number (0 = port 0, 1 = port1).
   *
   * @return The first scancode of the queue (-1 if no data is available).
   */
  int getData(int PS2Port);

  /**
   * @brief Send a command to the device.
   *
   * @param data Byte to send to the PS2 device.
   * @param PS2Port PS2 port number (0 = port 0, 1 = port1).
   */
  void sendData(uint8_t data, int PS2Port);

  /**
   * @brief Inject a byte into the RX buffer.
   *
   * Injects a byte as if it were actually sent by the device.
   *
   * @param value Byte to inject.
   * @param PS2Port PS2 port number (0 = port 0, 1 = port1).
   */
  void injectInRXBuffer(int value, int PS2Port);

private:

  static void IRAM_ATTR rtc_isr(void * arg);

  // address of next word to read in the circular buffer
  int                   m_readPos[2];

  // task that is waiting for TX ends
  volatile TaskHandle_t m_TXWaitTask[2];

  // task that is waiting for RX event
  volatile TaskHandle_t m_RXWaitTask[2];

};





} // end of namespace



extern fabgl::PS2ControllerClass PS2Controller;


