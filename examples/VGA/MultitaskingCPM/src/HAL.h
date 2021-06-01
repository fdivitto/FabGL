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

#include <stdio.h>

#include "emudevs/Z80.h"

#include "defs.h"
#include "fabgl.h"


#ifndef _ESP32_SOC_H_
  #undef HAS_WIFI
#endif


#ifdef HAS_WIFI
  #include <WiFi.h>
#endif


#define DEBUG_NONE   0
#define DEBUG_ERRORS 1
#define DEBUG_HAL    2
#define DEBUG_BIOS   4
#define DEBUG_BDOS   8
#define DEBUG_SCB    16
#define DEBUG_FULL   (DEBUG_ERRORS | DEBUG_HAL | DEBUG_BIOS | DEBUG_BDOS | DEBUG_SCB)

// one of above
#define MSGDEBUG     DEBUG_NONE
//#define MSGDEBUG     DEBUG_HAL
//#define MSGDEBUG     DEBUG_FULL
//#define MSGDEBUG     DEBUG_BDOS
//#define MSGDEBUG     (DEBUG_ERRORS | DEBUG_BDOS | DEBUG_BIOS)


#define DEFAULTCPUSPEEDHZ 0//1000000   // 0 = max


// this should never be different than 16
#define MAXDRIVERS 16


#define CPMMAXFILES   5


using fabgl::Terminal;
using fabgl::TerminalController;
using fabgl::Delegate;


enum class AbortReason {
  NoAbort,
  SessionClosed,
  OutOfMemory,
  GeneralFailure,
  AuxTerm,
};


// implements BDOS and BIOS
class HAL {

public:

  HAL();
  ~HAL();

  bool aborting()                   { return m_abortReason != AbortReason::NoAbort; }
  AbortReason abortReason()         { return m_abortReason; }
  void abort(AbortReason reason)    { m_abortReason = reason; }


  // Disk Drivers

  void mountDrive(int drive, char const * path);
  char const * getDriveMountPath(int drive)      { return drive < MAXDRIVERS ? m_mounts[drive] : nullptr; }


  // Terminal (keyboard and CRT), associated to PHYSICALDEV_CRT and PHYSICALDEV_KBD

  void setTerminal(Terminal * value)              { m_terminal = value; m_termCtrl.setTerminal(m_terminal); }
  Terminal * terminal()                           { return m_terminal; }

  int getTerminalColumns()                        { return m_terminal->getColumns(); }
  int getTerminalRows()                           { return m_terminal->getRows(); }
  void getTerminalCursorPos(int * col, int * row) { m_termCtrl.getCursorPos(col, row); }

  void setTerminalType(TermType value)            { m_termCtrl.setTerminalType(value); }


  // LPT (printer), associated to PHYSICALDEV_LPT

  void setLPT(Stream * value)                     { m_LPTStream = value; }


  // Aux (index 0 = first aux, 1 = second aux), associated to PHYSICALDEV_UART1 and PHYSICALDEV_UART2

  void setSerial(int index, Stream * value)       { m_serialStream[index] = value; }


  // Devices I/O

  void devOut(int device, uint8_t c);
  bool devOutAvailable(int device);
  uint8_t devIn(int device);
  bool devInAvailable(int device);


  // CPU

  void setCPUSpeed(uint32_t valueHz)              { m_CPUSpeed = valueHz; }

  uint8_t CPU_readRegByte(int reg)                { return m_Z80.readRegByte(reg); }
  void CPU_writeRegByte(int reg, uint8_t value)   { m_Z80.writeRegByte(reg, value); }

  uint16_t CPU_readRegWord(int reg)               { return m_Z80.readRegWord(reg); }
  void CPU_writeRegWord(int reg, uint16_t value)  { m_Z80.writeRegWord(reg, value); }

  uint16_t CPU_getPC()                            { return m_Z80.getPC(); }
  void CPU_setPC(uint16_t value)                  { m_Z80.setPC(value); }

  void CPU_reset()                                { m_Z80.reset(); }

  void CPU_setStackPointer(uint16_t value);
  void CPU_pushStack(uint16_t value);

  void CPU_exec(uint16_t addr, uint16_t exitAddr);
  void CPU_stop()                                 { m_execMainLoop = false; }

  Delegate<> onCPUStep;


  // Date/time

  void getDateTime(int * year, int * month, int * day, int * hour, int * minutes, int * seconds);
  void setDateTime(int year, int month, int day, int hour, int minutes, int seconds);


  // debug and logs

  static void logf(char const * format, ...);


  // RAM

  void releaseMem(uint16_t startAddr, size_t endAddr);

  int allocatedBlocks();

  static int systemFree() { return heap_caps_get_free_size(MALLOC_CAP_32BIT); }

  void fillMem(uint16_t addr, uint8_t value, size_t size);
  void copyMem(uint16_t destAddr, uint16_t srcAddr, size_t size);
  void copyMem(uint16_t destAddr, void const * src, size_t size);
  void copyMem(void * dst, uint16_t srcAddr, size_t size);
  void copyStr(char * dst, uint16_t srcAddr);
  void copyStr(uint16_t dstAddr, char const * src);
  void copyStr(uint16_t dstAddr, uint16_t srcAddr);
  size_t strLen(uint16_t addr);
  void moveMem(uint16_t destAddr, uint16_t srcAddr, size_t size);
  int compareMem(uint16_t s1, void const * s2, size_t size);
  uint16_t findChar(uint16_t addr, uint8_t c);

  uint8_t readByte(uint16_t addr);
  void writeByte(uint16_t addr, uint8_t value);

  uint16_t readWord(uint16_t addr);
  void writeWord(uint16_t addr, uint16_t value);



  // I/O ports

  uint8_t readIO(uint16_t addr);
  void writeIO(uint16_t addr, uint8_t value);


  // WIFI

  #ifdef HAS_WIFI
  static bool wifiConnected();
  #endif



private:

  bool checkMem(uint16_t addr);

  static int readByte(void * context, int address)                { return ((HAL*)context)->readByte(address); }
  static void writeByte(void * context, int address, int value)   { ((HAL*)context)->writeByte(address, value); }

  static int readWord(void * context, int addr)                   { return ((HAL*)context)->readWord(addr); }
  static void writeWord(void * context, int addr, int value)      { ((HAL*)context)->writeWord(addr, value); }

  static int readIO(void * context, int address)                  { return ((HAL*)context)->readIO(address); }
  static void writeIO(void * context, int address, int value)     { ((HAL*)context)->writeIO(address, value); }



  fabgl::Z80         m_Z80;

  uint32_t           m_CPUSpeed;  // 0 = max

  uint32_t *         m_memBlock[64];  // 64 blocks of 1K

  bool               m_execMainLoop;

  char *             m_mounts[MAXDRIVERS];

  Terminal *         m_terminal;
  TerminalController m_termCtrl;

  Stream *           m_serialStream[2];

  Stream *           m_LPTStream;

  AbortReason        m_abortReason;

};




