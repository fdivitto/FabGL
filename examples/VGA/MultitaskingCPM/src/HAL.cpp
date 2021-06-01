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



#include <sys/time.h>

#include "driver/gpio.h"
#include "driver/dac.h"


#include "HAL.h"


#pragma GCC optimize ("O2")


HAL::HAL()
  : m_CPUSpeed(DEFAULTCPUSPEEDHZ),
    m_terminal(nullptr),
    m_LPTStream(nullptr),
    m_abortReason(AbortReason::NoAbort)
{
  for (int i = 0; i < 64; ++i)
    m_memBlock[i] = nullptr;

  for (int i = 0; i < MAXDRIVERS; ++i)
    m_mounts[i] = nullptr;

  m_serialStream[0] = nullptr;
  m_serialStream[1] = nullptr;

  dac_output_enable(DAC_CHANNEL_1);

  m_Z80.setCallbacks(this, readByte, writeByte, readWord, writeWord, readIO, writeIO);
  CPU_reset();
}


HAL::~HAL()
{
  for (int i = 0; i < MAXDRIVERS; ++i)
    if (m_mounts[i])
      free(m_mounts[i]);

  for (int i = 0; i < 64; ++i)
    if (m_memBlock[i])
      heap_caps_free(m_memBlock[i]);
}


int HAL::allocatedBlocks()
{
  int r = 0;
  for (int i = 0; i < 64; ++i)
    if (m_memBlock[i])
      ++r;
  return r;
}


// ret false if failed to alloc memory
bool HAL::checkMem(uint16_t addr)
{
  int block = addr >> 10;
  if (m_memBlock[block] == nullptr) {
    m_memBlock[block] = (uint32_t*) heap_caps_malloc(1024, MALLOC_CAP_32BIT);
    #if MSGDEBUG & DEBUG_HAL
    logf("Allocated block %d\r\n", block);
    #endif
    bool r = (m_memBlock[block] != nullptr);
    if (!r)
      m_abortReason = AbortReason::OutOfMemory;
    return r;
  }
  return true;
}


// will not free blocks that:
//   - remaining size is less than 1K
//   - addr is not aligned
void HAL::releaseMem(uint16_t startAddr, size_t endAddr)
{
  int block = startAddr >> 10;
  while (startAddr < endAddr && block < 64) {
    if (m_memBlock[block] && (block << 10) == startAddr && (endAddr - startAddr + 1) >= 1024) { // free only when addr is 1K aligned and remaining size is >=1K
      heap_caps_free(m_memBlock[block]);
      m_memBlock[block] = nullptr;
      #if MSGDEBUG & DEBUG_HAL
      logf("Free block %d\r\n", block);
      #endif
    }
    ++block;
    startAddr = (block << 10); // from here addr becomes 1K aligned
  }
}


// TODO: optimize
void HAL::fillMem(uint16_t addr, uint8_t value, size_t size)
{
  while (size--)
    writeByte(addr++, value);
}


// TODO: optimize
void HAL::copyMem(uint16_t destAddr, uint16_t srcAddr, size_t size)
{
  while (size--)
    writeByte(destAddr++, readByte(srcAddr++));
}


// TODO: optimize
void HAL::copyMem(uint16_t destAddr, void const * src, size_t size)
{
  auto s = (uint8_t const *) src;
  while (size--)
    writeByte(destAddr++, *s++);
}


// TODO: optimize
void HAL::copyMem(void * dst, uint16_t srcAddr, size_t size)
{
  auto d = (uint8_t*) dst;
  while (size--)
    *d++ = readByte(srcAddr++);
}


// TODO: optimize
void HAL::copyStr(char * dst, uint16_t srcAddr)
{
  while ((*dst++ = readByte(srcAddr++)))
    ;
}


// TODO: optimize
void HAL::copyStr(uint16_t dstAddr, char const * src)
{
  do {
    writeByte(dstAddr++, *src);
  } while (*src++);
}


// TODO: optimize
void HAL::copyStr(uint16_t dstAddr, uint16_t srcAddr)
{
  uint8_t s;
  do {
    s = readByte(srcAddr++);
    writeByte(dstAddr++, s);
  } while (s);
}


// TODO: optimize
size_t HAL::strLen(uint16_t addr)
{
  auto p = addr;
  while (readByte(addr))
    ++addr;
  return addr - p;
}


// TODO: optimize
// return 0 if not found
uint16_t HAL::findChar(uint16_t addr, uint8_t c)
{
  uint8_t s;
  while ((s = readByte(addr))) {
    if (s == c)
      return addr;
    ++addr;
  }
  return 0;
}


// TODO: optimize
void HAL::moveMem(uint16_t destAddr, uint16_t srcAddr, size_t size)
{
  if (destAddr < srcAddr) {
    copyMem(destAddr, srcAddr, size);
  } else {
    destAddr += size - 1;
    srcAddr  += size - 1;
    while (size--)
      writeByte(destAddr--, readByte(srcAddr--));
  }
}


// TODO: optimize
int HAL::compareMem(uint16_t s1, void const * s2, size_t size)
{
  auto p1 = s1;
  auto p2 = (uint8_t const *) s2;
  while (size--) {
    auto v1 = readByte(p1++);
    if (v1 != *p2)
      return v1 - *p2;
    else
      ++p2;
  }
  return 0;
}


void HAL::logf(char const * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    Serial.write(buf);
  }
  va_end(ap);
}


// drive: 0 = A, 15 = P
void HAL::mountDrive(int drive, char const * path)
{
  m_mounts[drive] = strdup(path);
}


void HAL::CPU_setStackPointer(uint16_t value)
{
  CPU_writeRegWord(Z80_SP, value);
}


void HAL::CPU_pushStack(uint16_t value)
{
  uint16_t sp = CPU_readRegWord(Z80_SP) - 2;
  CPU_writeRegWord(Z80_SP, sp);
  writeWord(sp, value);
}


// exec code at addr while m_execMainLoop is true (by BIOS call or RET)
// exit loop when:
//   - m_execMainLoop becomes false (calling HAL::stop())
//   - when pc = exitAddr (so you can push exitAddr into the stack to exit loop on RET)
void HAL::CPU_exec(uint16_t addr, uint16_t exitAddr)
{
  CPU_setPC(addr);

  m_execMainLoop = true;  // may be set False by a reset

  int loopCount = 0;

  while (m_execMainLoop && !aborting()) {

    // HALT?
    if (readByte(m_Z80.getPC()) == 0x76)
      break;

    onCPUStep();

    if (!m_execMainLoop)
      break;

    /*int cycles = */m_Z80.step();

    /*
    if (m_CPUSpeed != 0) {
      timeval t;
      gettimeofday(&t, NULL);
      int64_t us = t.tv_sec * 1000000 + t.tv_usec + cycles * 1000000 / m_CPUSpeed;
      while (true) {
        gettimeofday(&t, NULL);
        int64_t ct = t.tv_sec * 1000000 + t.tv_usec;
        if (ct > us)
          break;
      }
    }
    */

    // exit when pc = exitAddr
    if (CPU_getPC() == exitAddr)
      break;

    if ((++loopCount % 30000) == 0)
      vTaskDelay(1);
  }
}


uint8_t HAL::readByte(uint16_t addr)
{
  if (!checkMem(addr))
    return 0;

  uint32_t val32 = *(m_memBlock[addr >> 10] + ((addr >> 2) & 0b11111111));  // 32 bit aligned pointer
  uint8_t value = ((uint8_t*)&val32)[addr & 0b11];

  #if MSGDEBUG
  if (addr >= SCB_PAGEADDR && addr < SCB_PAGEADDR + SCB_SIZE) {
    int field = static_cast<int>(addr) - static_cast<int>(SCB_ADDR);
    #if MSGDEBUG & DEBUG_SCB
    logf("readByte(): SCB %d (0x%02X) => 0x%02X\r\n", field, field, value);
    #endif
    #if MSGDEBUG & DEBUG_ERRORS
    if (!isSupportedSCBField(field))
      logf("readByte(): unsupp SCB %d (0x%02X)\r\n", field, field);
    #endif
  }
  #endif

  return value;
}


void HAL::writeByte(uint16_t addr, uint8_t value)
{
  if (!checkMem(addr))
    return;

  uint32_t * ptr32 = m_memBlock[addr >> 10] + ((addr >> 2) & 0b11111111); // 32 bit aligned pointer
  uint32_t val32 = *ptr32;
  ((uint8_t*)&val32)[addr & 0b11] = value;
  *ptr32 = val32;

  #if MSGDEBUG
  if (addr >= SCB_PAGEADDR && addr < SCB_PAGEADDR + SCB_SIZE) {
    int field = static_cast<int>(addr) - static_cast<int>(SCB_ADDR);
    #if MSGDEBUG & DEBUG_SCB
    logf("writeByte(): SCB %d (0x%02x) <= 0x%02X\r\n", field, field, value);
    #endif
    #if MSGDEBUG & DEBUG_ERRORS
    if (!isSupportedSCBField(field))
      logf("writeByte(): unsupp SCB %d (0x%02x)\r\n", field, field);
    #endif
  }
  #endif
}


uint16_t HAL::readWord(uint16_t addr)
{
  return readByte(addr) | (readByte(addr + 1) << 8);
}


void HAL::writeWord(uint16_t addr, uint16_t value)
{
  writeByte(addr, value & 0xFF);
  writeByte(addr + 1, value >> 8);
}



uint8_t HAL::readIO(uint16_t addr)
{
  #if MSGDEBUG & DEBUG_HAL
  logf("readIO(%04x)\r\n", addr);
  #endif
  return 0;
}


void HAL::writeIO(uint16_t addr, uint8_t value)
{
  #if MSGDEBUG & DEBUG_HAL
  logf("writeIO(%04x, %02x)\r\n", addr, value);
  #endif
  if (addr == 0x50) {
    dac_output_voltage(DAC_CHANNEL_1, value);
  }
}


// physical device output
void HAL::devOut(int device, uint8_t c)
{
  switch (device) {
    case PHYSICALDEV_CRT:
      m_terminal->write(c);
      break;
    case PHYSICALDEV_LPT:
      m_LPTStream->write(c);
      break;
    case PHYSICALDEV_UART1:
      if (m_serialStream[0])
        m_serialStream[0]->write(c);
      break;
    case PHYSICALDEV_UART2:
      if (m_serialStream[1])
        m_serialStream[1]->write(c);
      break;
  }
}


// physical device output status
bool HAL::devOutAvailable(int device)
{
  switch (device) {
    case PHYSICALDEV_CRT:
      return m_terminal != nullptr;
    case PHYSICALDEV_LPT:
      return m_LPTStream != nullptr;
    case PHYSICALDEV_UART1:
      return m_serialStream[0] != nullptr;
    case PHYSICALDEV_UART2:
      return m_serialStream[1] != nullptr;
    default:
      return false;
  }
}


// physical device input
// this method must block on keyboard input without data available
uint8_t HAL::devIn(int device)
{
  switch (device) {
    case PHYSICALDEV_KBD:
      return m_terminal->read(-1);
    case PHYSICALDEV_UART1:
      return m_serialStream[0] ? m_serialStream[0]->read() : 0x1A;  // 1Ah = EOF
    case PHYSICALDEV_UART2:
      return m_serialStream[1] ? m_serialStream[1]->read() : 0x1A;  // 1Ah = EOF
    default:
      return 0x1A;  // EOF
  }
}


// physical device input status
bool HAL::devInAvailable(int device)
{
  switch (device) {
    case PHYSICALDEV_KBD:
      return m_terminal && m_terminal->available() != 0;
    case PHYSICALDEV_UART1:
      return m_serialStream[0] && m_serialStream[0]->available();
    case PHYSICALDEV_UART2:
      return m_serialStream[1] && m_serialStream[1]->available();
    default:
      return false;
  }
}


void HAL::getDateTime(int * year, int * month, int * day, int * hour, int * minutes, int * seconds)
{
  auto t   = time(nullptr);
  auto tm  = *localtime(&t);
  *year    = 1900 + tm.tm_year;
  *month   = 1 + tm.tm_mon;
  *day     = tm.tm_mday;
  *hour    = tm.tm_hour;
  *minutes = tm.tm_min;
  *seconds = fabgl::tmin<int>(tm.tm_sec, 59); // [0, 61] (until C99), [0, 60] (since C99)
}


void HAL::setDateTime(int year, int month, int day, int hour, int minutes, int seconds)
{
  // not implemented
  #if MSGDEBUG & DEBUG_ERRORS
  logf("unimplemented setting system datetime\r\n");
  #endif
}


#ifdef HAS_WIFI

bool HAL::wifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

#endif



