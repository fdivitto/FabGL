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



#include "BIOS.h"
#include "BDOS.h"


#pragma GCC optimize ("O2")


const PhysicalDevice chrtbl[CHRTBL_DEVICES + 1] = {
  // PHYSICALDEV_CRT: terminal out (display)
  { { 'C', 'R', 'T', ' ', ' ', ' ' }, PHYSICALDEVICE_FLAG_OUTPUT | PHYSICALDEVICE_FLAG_SERIAL, 0 },
  // PHYSICALDEV_KBD: terminal in (keyboard)
  { { 'K', 'B', 'D', ' ', ' ', ' ' }, PHYSICALDEVICE_FLAG_INPUT | PHYSICALDEVICE_FLAG_SERIAL, 0 },
  // PHYSICALDEV_LPT: printer
  { { 'L', 'P', 'T', ' ', ' ', ' ' }, PHYSICALDEVICE_FLAG_OUTPUT | PHYSICALDEVICE_FLAG_SERIAL, 0 },
  // PHYSICALDEV_UART1: serial 1
  { { 'U', 'A', 'R', 'T', '1', ' ' }, PHYSICALDEVICE_FLAG_INOUT | PHYSICALDEVICE_FLAG_SERIAL | PHYSICALDEVICE_FLAG_SOFTBAUD, 15 },
  // PHYSICALDEV_UART2: serial 2
  { { 'U', 'A', 'R', 'T', '2', ' ' }, PHYSICALDEVICE_FLAG_INOUT | PHYSICALDEVICE_FLAG_SERIAL | PHYSICALDEVICE_FLAG_SOFTBAUD, 15 },
  // mark for ending device
  { { 0, 0, 0, 0, 0, 0 }, 0, 0 },
};




///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// DateTime


// convert decimal to packed BCD (v in range 0..99)
inline uint8_t byteToBCD(uint8_t v)
{
  return (v % 10) | ((v / 10) << 4);
}


// convert packed BCD to decimal
inline uint8_t BCDtoByte(uint8_t v)
{
  return (v & 0x0F) + (v >> 4) * 10;
}


// Returns number of days since civil DATEBASE.  Negative values indicate days prior to DATEBASE.
// Preconditions:  y-m-d represents a date in the civil (Gregorian) calendar
//                 m is in [1, 12]
//                 d is in [1, last_day_of_month(y, m)]
// ref: http://howardhinnant.github.io/date_algorithms.html
int32_t DateTime::days_from_civil(int32_t y, uint32_t m, uint32_t d)
{
  if (y > 0 && m > 0 && d > 0) {
    y -= m <= 2;
    const int32_t  era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = (uint32_t)(y - era * 400);
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)(doe) - DATEBASE;
  }
  return 0;
}


// Returns year/month/day triple in civil calendar
// Preconditions:  z is number of days since DATEBASE
// ref: http://howardhinnant.github.io/date_algorithms.html
void DateTime::civil_from_days(int32_t z, int * ret_y, int * ret_m, int * ret_d)
{
  z += DATEBASE;
  const int32_t  era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t  y   = (int32_t)(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp  = (5 * doy + 2) / 153;
  *ret_d             = doy - (153 * mp + 2) / 5 + 1;
  *ret_m             = mp + (mp < 10 ? 3 : -9);
  *ret_y             = y + (*ret_m <= 2);
}


void DateTime::set(int year, int month, int day, int hour, int minutes, int seconds)
{
  days_since1978 = days_from_civil(year, month, day);
  hour_BCD       = byteToBCD(hour);
  minutes_BCD    = byteToBCD(minutes);
  seconds_BCD    = byteToBCD(seconds);
}


void DateTime::get(int * year, int * month, int * day, int * hour, int * minutes, int * seconds)
{
  *hour    = BCDtoByte(hour_BCD);
  *minutes = BCDtoByte(minutes_BCD);
  *seconds = BCDtoByte(seconds_BCD);
  civil_from_days(days_since1978, year, month, day);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIOS



BIOS::BIOS(HAL * hal)
  : m_HAL(hal)
{
  // System Control Block (SCB)
  // becasue SCB is shared among BIOS and BDOS we reset it here
  m_HAL->fillMem(SCB_PAGEADDR, 0, 256);

  // store chrtbl into RAM
  m_HAL->copyMem(CHRTBL_ADDR, chrtbl, CHRTBL_DEVICES * sizeof(PhysicalDevice) + 1);

  // SCB jump to LIST call
  m_HAL->writeByte(SCB_ADDR + SCB_BIOSPRINTCALL_3B + 0, 0xC3);  // JP
  m_HAL->writeWord(SCB_ADDR + SCB_BIOSPRINTCALL_3B + 1, BIOS_ENTRY + 3 * BIOS_LIST);
}


BIOS::~BIOS()
{
}


// logicalDevice:
//   0 = console input
//   1 = console output
//   2 = aux input
//   3 = aux output
//   4 = list
// physicalDevice:
//   0..11 = one of chrtbl[]
// returns true if the specified logical device is mapped to the specified physical device
bool BIOS::isPhysicalDeviceAssigned(int logicalDevice, int physicalDevice)
{
  uint16_t scbFieldAddr = SCB_ADDR + SCB_REDIRECTIONVECTS_W + logicalDevice * 2;
  uint16_t devmap = m_HAL->readWord(scbFieldAddr);
  return devmap & (1 << (15 - physicalDevice));
}


void BIOS::assignPhysicalDevice(int logicalDevice, int physicalDevice)
{
  uint16_t scbFieldAddr = SCB_ADDR + SCB_REDIRECTIONVECTS_W + logicalDevice * 2;
  uint16_t devmap = m_HAL->readWord(scbFieldAddr);
  devmap |= (1 << (15 - physicalDevice));
  m_HAL->writeWord(scbFieldAddr, devmap);
}


// logical device input status
// from CP/M Plus System Guide: "An input status routine should return true if any selected device is ready."
bool BIOS::devInAvailable(int device)
{
  uint16_t devmap = m_HAL->readWord(SCB_ADDR + SCB_REDIRECTIONVECTS_W + device * 2);
  for (int physicalDevice = 0; physicalDevice < 12; ++physicalDevice)
    if ((devmap & (1 << (15 - physicalDevice))) && m_HAL->devInAvailable(physicalDevice))
      return true;
  return false;
}


// logical device output status
// from CP/M Plus System Guide: "An output status routine should return true only if all selected devices are ready."
bool BIOS::devOutAvailable(int device)
{
  uint16_t devmap = m_HAL->readWord(SCB_ADDR + SCB_REDIRECTIONVECTS_W + device * 2);
  for (int physicalDevice = 0; physicalDevice < 12; ++physicalDevice)
    if ((devmap & (1 << (15 - physicalDevice))) && m_HAL->devInAvailable(physicalDevice) == false)
      return false;
  return true;
}


// logical device input
// from CP/M Plus System Guide: "An input character should be read from the first ready device whose corresponding bit is set."
// this method must block on console input without data available
uint8_t BIOS::devIn(int device)
{
  uint16_t devmap = m_HAL->readWord(SCB_ADDR + SCB_REDIRECTIONVECTS_W + device * 2);
  for (int physicalDevice = 0; physicalDevice < 12; ++physicalDevice)
    if (devmap & (1 << (15 - physicalDevice)))  // do not call devInAvailable(), otherwise keyboard input never blocks
      return m_HAL->devIn(physicalDevice);
  return 0x1A;  // EOF
}


// logical device output
// from CP/M Plus System Guide: "You should send an output character to all of the devices whose corresponding bit is set."
void BIOS::devOut(int device, uint8_t c)
{
  uint16_t devmap = m_HAL->readWord(SCB_ADDR + SCB_REDIRECTIONVECTS_W + device * 2);
  for (int physicalDevice = 0; physicalDevice < 12; ++physicalDevice)
    if (devmap & (1 << (15 - physicalDevice)))
      m_HAL->devOut(physicalDevice, c);
}


void BIOS::BIOS_call(int func, uint16_t * BC, uint16_t * DE, uint16_t * HL, uint16_t * AF)
{
  m_HAL->CPU_writeRegWord(Z80_BC, *BC);
  m_HAL->CPU_writeRegWord(Z80_DE, *DE);
  m_HAL->CPU_writeRegWord(Z80_HL, *HL);
  m_HAL->CPU_writeRegWord(Z80_AF, *AF);

  // has the BIOS been changed? (we don't care about PAGE0_WSTARTADDR, but just BIOS_ENTRY jumps)
  if (m_HAL->readWord(BIOS_ENTRY + func * 3 + 1) != BIOS_RETS + func) {

    // yes, you can call BDOS using CPU execution

    // set return address (it is important that is it inside TPA, some RSX check it)
    uint16_t retAddr = BDOS_ENTRY;               // as if calling from BDOS_ENTRY
    m_HAL->CPU_pushStack(retAddr);          // RET will interrupt execution

    // save program counter
    uint32_t prevPC = m_HAL->CPU_getPC();

    m_HAL->CPU_exec(BIOS_ENTRY + func * 3, retAddr);

    // restore program counter
    m_HAL->CPU_setPC(prevPC);

  } else {

    // no, you can directly call BDOS function here

    processBIOS(func);
  }

  *BC = m_HAL->CPU_readRegWord(Z80_BC);
  *DE = m_HAL->CPU_readRegWord(Z80_DE);
  *HL = m_HAL->CPU_readRegWord(Z80_HL);
  *AF = m_HAL->CPU_readRegWord(Z80_AF);
}


void BIOS::BIOS_call_CONOUT(uint8_t c)
{
  uint16_t BC = c, DE = 0, HL = 0, AF = 0;
  BIOS_call(BIOS_CONOUT, &BC, &DE, &HL, &AF);
}


uint8_t BIOS::BIOS_call_CONIN()
{
  uint16_t BC = 0, DE = 0, HL = 0, AF = 0;
  BIOS_call(BIOS_CONIN, &BC, &DE, &HL, &AF);
  return m_HAL->CPU_readRegByte(Z80_A);
}


uint8_t BIOS::BIOS_call_CONST()
{
  uint16_t BC = 0, DE = 0, HL = 0, AF = 0;
  BIOS_call(BIOS_CONST, &BC, &DE, &HL, &AF);
  return m_HAL->CPU_readRegByte(Z80_A);
}


void BIOS::BIOS_call_LIST(uint8_t c)
{
  uint16_t BC = c, DE = 0, HL = 0, AF = 0;
  BIOS_call(BIOS_LIST, &BC, &DE, &HL, &AF);
  m_HAL->CPU_readRegByte(Z80_A);
}


void BIOS::BIOS_call_WBOOT()
{
  uint16_t BC = 0, DE = 0, HL = 0, AF = 0;
  BIOS_call(BIOS_WBOOT, &BC, &DE, &HL, &AF);
}


// ret = true, program end
void BIOS::processBIOS(int func)
{
  #if MSGDEBUG & DEBUG_BIOS
  m_HAL->logf("BIOS %d: ", func);
  #endif
  switch (func) {
    // BOOT 0 (0x00)
    case 0:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("BOOT\r\n");
      #endif
      exec_BOOT();
      break;

    // WBOOT 1 (0x01)
    case 1:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("WBOOT\r\n");
      #endif
      exec_WBOOT();
      break;

    // CONST 2 (0x02)
    case 2:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("CONST\r\n");
      #endif
      exec_CONST();
      break;

    // CONIN 3 (0x03)
    case 3:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("CONIN\r\n");
      #endif
      exec_CONIN();
      break;

    // CONOUT 4 (0x04)
    case 4:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("CONOUT\r\n");
      #endif
      exec_CONOUT();
      break;

    // LIST 5 (0x05)
    case 5:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("LIST\r\n");
      #endif
      exec_LIST();
      break;

    // AUXOUT 6 (0x06)
    case 6:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("AUXOUT\r\n");
      #endif
      exec_AUXOUT();
      break;

    // AUXIN 7 (0x07)
    case 7:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("AUXIN\r\n");
      #endif
      exec_AUXIN();
      break;

    // SELDSK 9 (0x09)
    case 9:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("SELDSK\r\n");
      #endif
      exec_SELDSK();
      break;

    // LISTST 15 (0x0F)
    case 15:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("LISTST\r\n");
      #endif
      exec_LISTST();
      break;

    // CONOST 17 (0x11)
    case 17:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("CONOST\r\n");
      #endif
      exec_CONOST();
      break;

    // AUXIST 18 (0x12)
    case 18:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("AUXIST\r\n");
      #endif
      exec_AUXIST();
      break;

    // AUXOST 19 (0x13)
    case 19:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("AUXOST\r\n");
      #endif
      exec_AUXOST();
      break;

    // DEVTBL 20 (0x14)
    case 20:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("DEVTBL\r\n");
      #endif
      exec_DEVTBL();
      break;

    // DEVINI 21 (0x15)
    case 21:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("DEVINI\r\n");
      #endif
      exec_DEVINI();
      break;

    // TIME 26 (0x1A)
    case 26:
      #if MSGDEBUG & DEBUG_BIOS
      m_HAL->logf("Time\r\n");
      #endif
      exec_TIME();
      break;

    default:
      #if MSGDEBUG & DEBUG_ERRORS
      m_HAL->logf("Unsupp BIOS %02xh\r\n", func);
      #endif
      break;
  }
}


// 0 (0x00)
void BIOS::exec_BOOT()
{
  exec_WBOOT();
}


// 1 (0x01)
void BIOS::exec_WBOOT()
{
  m_HAL->CPU_stop();

  // restore BIOS WBOOT address
  m_HAL->writeWord(PAGE0_WSTARTADDR, BIOS_ENTRY + 3);

  // restore BDOS entry from SCB
  m_HAL->writeWord(PAGE0_OSBASE, m_HAL->readWord(SCB_ADDR + SCB_TOPOFUSERTPA_W));
}


// 2 (0x02)
void BIOS::exec_CONST()
{
  uint8_t ret = devInAvailable(LOGICALDEV_CONIN) ? 0xFF : 0x00;
  m_HAL->CPU_writeRegByte(Z80_A, ret);
  #if MSGDEBUG & DEBUG_BIOS
  m_HAL->logf("  A = 0x%02X\r\n", ret);
  #endif
}


// 3 (0x03)
void BIOS::exec_CONIN()
{
  uint8_t ret = devIn(LOGICALDEV_CONIN);
  m_HAL->CPU_writeRegByte(Z80_A, ret);
  #if MSGDEBUG & DEBUG_BIOS
  m_HAL->logf("  A = 0x%02X (%c)\r\n", ret, ret > 31 && ret < 127 ? ret : ' ');
  #endif
}


// 4 (0x04)
void BIOS::exec_CONOUT()
{
  uint8_t c = m_HAL->CPU_readRegByte(Z80_C);
  devOut(LOGICALDEV_CONOUT, c);
  #if MSGDEBUG & DEBUG_BIOS
  m_HAL->logf("  C = 0x%02X (%c)\r\n", c, c > 31 && c < 127 ? c : ' ');
  #endif
}


// 5 (0x05)
void BIOS::exec_LIST()
{
  devOut(LOGICALDEV_LIST, m_HAL->CPU_readRegByte(Z80_C));
}


// 6 (0x06)
void BIOS::exec_AUXOUT()
{
  devOut(LOGICALDEV_AUXOUT, m_HAL->CPU_readRegByte(Z80_C));
}


// 7 (0x07)
void BIOS::exec_AUXIN()
{
  m_HAL->CPU_writeRegByte(Z80_A, devIn(LOGICALDEV_AUXIN));
}


// 9 (0x09)
void BIOS::exec_SELDSK()
{
  int drive = m_HAL->CPU_readRegByte(Z80_C);
  if (drive < MAXDRIVERS && m_HAL->getDriveMountPath(drive)) {
    m_HAL->CPU_writeRegWord(Z80_HL, DPH_ADDR);
  } else
    m_HAL->CPU_writeRegWord(Z80_HL, 0);
}


// 15 (0x0F)
void BIOS::exec_LISTST()
{
  m_HAL->CPU_writeRegByte(Z80_A, devOutAvailable(LOGICALDEV_LIST) ? 0xFF : 0);
}


// 17 (0x11)
void BIOS::exec_CONOST()
{
  m_HAL->CPU_writeRegByte(Z80_A, devOutAvailable(LOGICALDEV_CONOUT) ? 0xFF : 0);
}


// 18 (0x12)
void BIOS::exec_AUXIST()
{
  m_HAL->CPU_writeRegByte(Z80_A, devInAvailable(LOGICALDEV_AUXIN) ? 0xFF : 0);
}


// 19 (0x13)
void BIOS::exec_AUXOST()
{
  m_HAL->CPU_writeRegByte(Z80_A, devOutAvailable(LOGICALDEV_AUXOUT) ? 0xFF : 0);
}


// 20 (0x14)
void BIOS::exec_DEVTBL()
{
  m_HAL->CPU_writeRegWord(Z80_HL, CHRTBL_ADDR);
}


// 21 (0x15)
void BIOS::exec_DEVINI()
{
  // initializes baud rate of specified device
  // TODO:
  // int device = m_HAL->CPU_readRegByte(Z80_C);
}


// get datetime from HAL and set SCB fields
void BIOS::updateSCBFromHALDateTime()
{
  int year, month, day, hour, minutes, seconds;
  m_HAL->getDateTime(&year, &month, &day, &hour, &minutes, &seconds);
  DateTime dt(year, month, day, hour, minutes, seconds);
  m_HAL->copyMem(SCB_ADDR + SCB_DATEDAYS_W, &dt, sizeof(DateTime));
}


// get datetime from SCB fields and update HAL datetime
void BIOS::updateHALDateTimeFromSCB()
{
  DateTime dt;
  m_HAL->copyMem(&dt, SCB_ADDR + SCB_DATEDAYS_W, sizeof(DateTime));
  int year, month, day, hour, minutes, seconds;
  dt.get(&year, &month, &day, &hour, &minutes, &seconds);
  m_HAL->setDateTime(year, month, day, hour, minutes, seconds);
}


// 26 (0x1A)
void BIOS::exec_TIME()
{
  int op = m_HAL->CPU_readRegByte(Z80_C);
  if (op == 0) {
    // update date/time to SCB (get datetime)
    updateSCBFromHALDateTime();
  } else if (op == 0xFF) {
    // update date/time from SCB (set datetime)
    updateHALDateTimeFromSCB();
  }
}
