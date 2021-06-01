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

#include "defs.h"
#include "HAL.h"


#define BIOS_BOOT    0
#define BIOS_WBOOT   1
#define BIOS_CONST   2
#define BIOS_CONIN   3
#define BIOS_CONOUT  4
#define BIOS_LIST    5
#define BIOS_AUXOUT  6
#define BIOS_AUXIN   7
#define BIOS_SELDSK  9
#define BIOS_LISTST 15
#define BIOS_CONOST 17
#define BIOS_AUXIST 18
#define BIOS_AUXOST 19
#define BIOS_DEVTBL 20
#define BIOS_DEVINI 21



struct DateTime {
  uint16_t days_since1978;
  uint8_t  hour_BCD;
  uint8_t  minutes_BCD;
  uint8_t  seconds_BCD;

  DateTime() : days_since1978(0), hour_BCD(0), minutes_BCD(0), seconds_BCD(0) { }
  DateTime(int year, int month, int day, int hour, int minutes, int seconds) { set(year, month, day, hour, minutes, seconds); }

  void set(int year, int month, int day, int hour, int minutes, int seconds);
  void get(int * year, int * month, int * day, int * hour, int * minutes, int * seconds);

private:

  // 722389 = number of days between 0000-03-01 and 1978-01-01 (CP/M base)
  static constexpr int32_t DATEBASE = 722389;

  static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d);
  static void civil_from_days(int32_t z, int * ret_y, int * ret_m, int * ret_d);

} __attribute__ ((packed));




class BIOS {

public:

  BIOS(HAL * hal);
  ~BIOS();

  void processBIOS(int func);  

  // logical to physical device mapping
  bool isPhysicalDeviceAssigned(int logicalDevice, int physicalDevice);
  void assignPhysicalDevice(int logicalDevice, int physicalDevice);


  // date/time

  void updateSCBFromHALDateTime();
  void updateHALDateTimeFromSCB();


  // BIOS calls helpers from host (not CPU code)

  void BIOS_call(int func, uint16_t * BC, uint16_t * DE, uint16_t * HL, uint16_t * AF);
  void BIOS_call_CONOUT(uint8_t c);
  uint8_t BIOS_call_CONIN();
  uint8_t BIOS_call_CONST();
  void BIOS_call_LIST(uint8_t c);
  void BIOS_call_WBOOT();

private:

  void exec_BOOT();                     //  0 (0x00)
  void exec_WBOOT();                    //  1 (0x01)
  void exec_CONST();                    //  2 (0x02)
  void exec_CONIN();                    //  3 (0x03)
  void exec_CONOUT();                   //  4 (0x04)
  void exec_LIST();                     //  5 (0x05)
  void exec_AUXOUT();                   //  6 (0x06)
  void exec_AUXIN();                    //  7 (0x07)
  void exec_SELDSK();                   //  9 (0x09)
  void exec_LISTST();                   // 15 (0x0F)
  void exec_CONOST();                   // 17 (0x11)
  void exec_AUXIST();                   // 18 (0x12)
  void exec_AUXOST();                   // 19 (0x13)
  void exec_DEVTBL();                   // 20 (0x14)
  void exec_DEVINI();                   // 21 (0x15)
  void exec_TIME();                     // 26 (0x1A)


  // logical devices I/O

  bool devInAvailable(int device);
  bool devOutAvailable(int device);
  uint8_t devIn(int device);
  void devOut(int device, uint8_t c);

  

  HAL * m_HAL;

};
