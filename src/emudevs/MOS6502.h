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
 * @brief This file contains fabgl::MOS6502 definition.
 */



#include <stdint.h>



namespace fabgl {



/**
 * @brief MOS 6502 CPU emulator
 */
class MOS6502 {

public:

  // callbacks
  typedef int  (*ReadByteCallback)(void * context, int addr);
  typedef void (*WriteByteCallback)(void * context, int addr, int value);
  typedef int  (*Page0ReadByteCallback)(void * context, int addr);
  typedef void (*Page0WriteByteCallback)(void * context, int addr, int value);
  typedef int  (*Page1ReadByteCallback)(void * context, int addr);
  typedef void (*Page1WriteByteCallback)(void * context, int addr, int value);


  void setCallbacks(void * context, ReadByteCallback readByte, WriteByteCallback writeByte, Page0ReadByteCallback page0ReadByte, Page0WriteByteCallback page0WriteByte, Page1ReadByteCallback page1ReadByte, Page1WriteByteCallback page1WriteByte) {
    m_context        = context;
    m_readByte       = readByte;
    m_writeByte      = writeByte;
    m_page0ReadByte  = page0ReadByte;
    m_page0WriteByte = page0WriteByte;
    m_page1ReadByte  = page1ReadByte;
    m_page1WriteByte = page1WriteByte;
  }

  int reset();
  int IRQ();
	int NMI();

  void setPC(uint16_t addr) { m_PC = addr; }
  uint16_t getPC()          { return m_PC; }

  int step();

private:

  typedef void (MOS6502::*ADCSBC)(uint8_t);

  void OP_BINADC(uint8_t m);
  void OP_BINSBC(uint8_t m);

  void OP_BCDADC(uint8_t m);
  void OP_BCDSBC(uint8_t m);

  void setADCSBC();


  ADCSBC   m_OP_ADC;
  ADCSBC   m_OP_SBC;

  uint16_t m_PC;
  uint8_t  m_A;
  uint8_t  m_X;
  uint8_t  m_Y;
  uint8_t  m_SP;

  bool     m_carry;
  bool     m_zero;
  bool     m_intDisable;
  bool     m_decimal;
  bool     m_overflow;
  bool     m_negative;

  void *                 m_context;
  ReadByteCallback       m_readByte;
  WriteByteCallback      m_writeByte;
  Page0ReadByteCallback  m_page0ReadByte;
  Page0WriteByteCallback m_page0WriteByte;
  Page1ReadByteCallback  m_page1ReadByte;
  Page1WriteByteCallback m_page1WriteByte;

};

};  // namespace fabgl
