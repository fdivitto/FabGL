/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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

#include <stdint.h>


class MOS6502 {

public:
	
	MOS6502(void * context);

  int callReset();
  int callIRQ();
	int callNMI();

  void go(int addr) { m_PC = addr; }

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

  void *   m_context;

};
