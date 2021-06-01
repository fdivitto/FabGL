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


#ifdef ARDUINO


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp32-hal.h"

#include "fabglconf.h"



/**
 * @file
 *
 * @brief This file contains fabgl::I2C definition.
 */


namespace fabgl {


struct I2CJobInfo {
  int32_t   frequency;
  uint8_t * buffer;
  uint8_t   address;
  uint16_t  size;
  uint16_t  timeout;
  uint32_t  readCount;
  i2c_err_t lastError;
};


/**
 * @brief I2C class allows multiple tasks to communicate with I2C devices, serializing read/write jobs.
 *
 * A single instance of I2C class can be shared among multiple tasks or timers (not interrupts).
 *
 * Example:
 *
 *     fabgl::I2C I2C;
 *
 *     void setup() {
 *       I2C.begin(GPIO_NUM_4, GPIO_NUM_15);  // 4 = SDA, 15 = SCL (WARN: disconnect VGA connector!!)
 *     }
 */
class I2C {

public:

  /**
   * @brief I2C class constructor
   *
   * @param bus I2C bus to use. Allowed 0 or 1 (default is 0).
   */
  I2C(int bus = 0);

  ~I2C();

  /**
   * @brief Initializes I2C instance associating GPIOs to I2C signals.
   *
   * @param SDAGPIO Pin to use for SDA signal
   * @param SCLGPIO Pin to use for SCK signal
   *
   * Example:
   *
   *     // 4 = SDA, 15 = SCL (WARN: disconnect VGA connector!!)
   *     I2C.begin(GPIO_NUM_4, GPIO_NUM_15);
   */
  bool begin(gpio_num_t SDAGPIO, gpio_num_t SCLGPIO);

  void end();

  /**
   * @brief Sends a buffer to I2C bus.
   *
   * This is a thread-safe operation. Multiple tasks can call this method concurrently.
   *
   * @param address I2C address of the destination slave device
   * @param buffer Buffer to send.
   * @param size Number of bytes to send. Maximum size is the return value of getMaxBufferLength().
   * @param frequency Clock frequency
   * @param timeOutMS Operation timeout in milliseconds
   *
   * @return True on success
   */
  bool write(int address, uint8_t * buffer, int size, int frequency = 100000, int timeOutMS = 50);

  /**
   * @brief Receives a buffer from I2C bus
   *
   * This is a thread-safe operation. Multiple tasks can call this methods concurrently.
   *
   * @param address I2C address of the source slave device
   * @param buffer Buffer where to put received data (must have space for at least "size" bytes)
   * @param size Number of bytes to receive. Maximum size is the return value of getMaxBufferLength().
   * @param frequency Clock frequency
   * @param timeOutMS Operation timeout in milliseconds
   *
   * @return Number of actually read bytes
   */
  int read(int address, uint8_t * buffer, int size, int frequency = 100000, int timeOutMS = 50);

  /**
   * @brief Returns maximum size of read and write buffers
   *
   * @return Maximum size in bytes
   */
  int getMaxBufferLength() { return 128; }


private:

  static void commTaskFunc(void * pvParameters);


  i2c_t *            m_i2c;

  uint8_t            m_bus;
  gpio_num_t         m_SDAGPIO;
  gpio_num_t         m_SCLGPIO;

  TaskHandle_t       m_commTaskHandle;

  EventGroupHandle_t m_eventGroup;

  I2CJobInfo         m_jobInfo;
};










} // end of namespace



#endif // #ifdef ARDUINO

