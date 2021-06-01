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



#ifdef ARDUINO


#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"

#include "DS3231.h"



#define DS3231_ADDR 0x68
#define DS3231_FREQ 400000


namespace fabgl {



////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////



DateTime::DateTime(int seconds_, int minutes_, int hours_, int dayOfMonth_, int month_, int year_)
  : seconds(seconds_),
    minutes(minutes_),
    hours(hours_),
    dayOfMonth(dayOfMonth_),
    month(month_),
    year(year_)
{
  calcDayOfWeek();
}


// 1 = sunday
void DateTime::calcDayOfWeek()
{
  static const int8_t t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  int y = year - (month < 3);
  dayOfWeek = 1 + (y + y / 4 - y / 100 + y / 400 + t[month - 1] + dayOfMonth) % 7;
}


// has 2038 unix bug!!
time_t DateTime::timestamp(int timezone)
{
  tm m;
  m.tm_sec   = seconds;
  m.tm_min   = minutes;
  m.tm_hour  = hours;
  m.tm_mday  = dayOfMonth;
  m.tm_mon   = month - 1;
  m.tm_year  = year - 1900;
  m.tm_isdst = 0;
  return mktime(&m) - 3600 * timezone;
}


////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////


DS3231::DS3231()
  : m_i2c(nullptr),
    m_available(false),
    m_dateTimeValid(false)
{
}


void DS3231::begin(I2C * i2c)
{
  m_i2c = i2c;

  // check oscillator stop flag
  uint8_t status = readReg(0x0F);
  if (m_available)
    m_dateTimeValid = (status & 0x80) == 0;
}


uint8_t DS3231::readReg(int reg)
{
  uint8_t b = reg;
  m_i2c->write(DS3231_ADDR, &b, 1, DS3231_FREQ);
  m_available = m_i2c->read(DS3231_ADDR, &b, 1, DS3231_FREQ);
  return b;
}


bool DS3231::writeReg(int reg, int value)
{
  uint8_t buf[2] = { (uint8_t)reg, (uint8_t)value };
  return m_i2c->write(DS3231_ADDR, buf, 2, DS3231_FREQ);
}


DateTime const & DS3231::datetime()
{
  // read 7 registers starting from reg 0
  uint8_t b = 0;
  m_i2c->write(DS3231_ADDR, &b, 1, DS3231_FREQ);
  uint8_t buf[7];
  m_available = m_i2c->read(DS3231_ADDR, buf, 7, DS3231_FREQ);

  // decode
  if (m_available) {
    m_datetime.seconds = (buf[0] & 0x0f) + ((buf[0] & 0x70) >> 4) * 10;
    m_datetime.minutes = (buf[1] & 0x0f) + ((buf[1] & 0x70) >> 4) * 10;
    m_datetime.hours   = (buf[2] & 0x0f) + ((buf[2] & 0x10) >> 4) * 10;
    if (buf[2] & (1 << 6)) {
      // 12 hours mode (convert to 24)
      if (buf[2] & (1 << 5))
        m_datetime.hours += 12;
    } else {
      // 24 hours mode
      m_datetime.hours += ((buf[2] & 0x20) >> 4) * 10;
    }
    m_datetime.dayOfWeek  = buf[3] & 7;
    m_datetime.dayOfMonth = (buf[4] & 0x0f) + ((buf[4] & 0x30) >> 4) * 10;
    m_datetime.month      = (buf[5] & 0x0f) + ((buf[5] & 0x10) >> 4) * 10;
    m_datetime.year       = (buf[6] & 0x0f) + ((buf[6] & 0xf0) >> 4) * 10 + 2000;
  }
  return m_datetime;
}


bool DS3231::setDateTime(DateTime const & value)
{
  // write 7 registers starting from reg 0
  uint8_t buf[8];
  buf[0] = 0; // starting reg address
  buf[1] = (value.seconds % 10) | ((value.seconds / 10) << 4);
  buf[2] = (value.minutes % 10) | ((value.minutes / 10) << 4);
  buf[3] = (value.hours % 10) | ((value.hours / 10) << 4);  // bit6=0 -> 24 hours mode
  buf[4] = value.dayOfWeek;
  buf[5] = (value.dayOfMonth % 10) | ((value.dayOfMonth / 10) << 4);;
  buf[6] = (value.month % 10) | ((value.month / 10) << 4);
  buf[7] = ((value.year - 2000) % 10) | (((value.year - 2000) / 10) << 4);
  m_i2c->write(DS3231_ADDR, buf, 8, DS3231_FREQ);

  // update oscillator stop flag (make date no more invalid)
  uint8_t st = readReg(0x0f);
  writeReg(0x0f, st & 0x7f);

  return m_available;
}


double DS3231::temperature()
{
  if ((readReg(0x0f) & (0b100)) == 0) {
    // not busy, force "convert temperature"
    uint8_t r = readReg(0x0e) | 0x20;
    writeReg(0x0e, r);
    // wait for not busy
    vTaskDelay(2 / portTICK_PERIOD_MS);
    while ((readReg(0x0f) & (0b100)) != 0 && m_available) {
      vTaskDelay(1);
    }
  }

  // read 2 registers starting from reg 0x11
  uint8_t b = 0x11;
  m_i2c->write(DS3231_ADDR, &b, 1, DS3231_FREQ);
  uint8_t buf[2];
  m_available = m_i2c->read(DS3231_ADDR, buf, 2, DS3231_FREQ);

  return (int8_t)buf[0] + 0.25 * (buf[1] >> 6);
}


void DS3231::clockEnabled(bool value)
{
  uint8_t r = readReg(0x0e);
  writeReg(0x0e, value ? (0x7f & r) : (0x80 | r));
}


} // fdv namespace

#endif // #ifdef ARDUINO
