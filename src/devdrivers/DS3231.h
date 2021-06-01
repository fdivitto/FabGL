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


/**
 * @file
 *
 * @brief This file contains the DS3231 (Real Time Clock) device driver.
 */



#ifdef ARDUINO


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "comdrivers/tsi2c.h"



namespace fabgl {


/**
 * @brief Represents date and time
 */
struct DateTime {
  int8_t  seconds;      /**< 0..59             */
  int8_t  minutes;      /**< 0..59             */
  int8_t  hours;        /**< 0..23             */
  int8_t  dayOfWeek;    /**< 1..7 (1 = sunday) */
  int8_t  dayOfMonth;   /**< 1..31             */
  int8_t  month;        /**< 1..12             */
  int16_t year;         /**< 1900..9999 (2000..2099 for DS3231, and 1900..2038 using timestamp() */

  DateTime() { }
  DateTime(int seconds_, int minutes_, int hours_, int dayOfMonth_, int month_, int year_);

  /**
   * @brief Calculates Unix timestamp
   *
   * @param timezone Timezone (hours)
   *
   * @return Unix timestamp
   */
  time_t timestamp(int timezone);

private:
  void calcDayOfWeek();
};


/**
 * @brief DS3231 Real Time Clock driver
 *
 * Example:
 *
 *     fabgl::I2C    I2C;
 *     fabgl::DS3231 DS3231;
 *
 *     void setup() {
 *       I2C.begin(GPIO_NUM_4, GPIO_NUM_15);  // 4 = SDA, 15 = SCL (WARN: disconnect VGA connector!!)
 *       DS3231.begin(&I2C);
 *
 *       auto dt = DS3231.datetime();
 *       Serial.printf("%02d/%02d/%d  %02d:%02d:%02d\n", dt.dayOfMonth, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds);
 *       Serial.printf("temp = %0.3f C\n", DS3231.temperature());
 *     }
 *
 */
class DS3231 {

public:

  DS3231();

  /**
   * @brief Initializes DS3231 driver
   *
   * @param i2c Pointer to I2C handler
   */
  void begin(I2C * i2c);

  /**
   * @brief Determines if DS3231 is reachable
   *
   * @return True if the device has been correctly initialized.
   */
  bool available() { return m_available; }

  /**
   * @brief Determines the validity of datetime
   *
   * After calling setDateTime() the datetime becomes valid.
   *
   * @return False indicates that the oscillator either is stopped or was stopped for some period and may be used to judge the validity of the timekeeping data.
   */
  bool dateTimeValid() { return m_dateTimeValid; }

  /**
   * @brief Queries DS3231 for current date and time
   *
   * @return A DateTime structure containing current date and time
   */
  DateTime const & datetime();

  /**
   * @brief Sets current date and time
   *
   * This methods also resets the invalid date flag.
   *
   * @param value Date and time to set
   */
  bool setDateTime(DateTime const & value);

  /**
   * @brief Forces DS3231 to read current temperature
   *
   * @return Current temperature in Celsius Degrees
   */
  double temperature();

  /**
   * @brief Enables or disables DS3231 oscillator
   *
   * @param value If true oscillator is enabled (date and time are always maintained by external battery)
   */
  void clockEnabled(bool value);

private:

  uint8_t readReg(int reg);

  bool writeReg(int reg, int value);

  I2C *    m_i2c;
  bool     m_available;
  bool     m_dateTimeValid;

  DateTime m_datetime;


};



} // fabgl namespace


#endif  // #ifdef ARDUINO
