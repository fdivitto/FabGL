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

#include "fabgl.h"



namespace fabgl {


// PIC 8259 (Programmable Interrupt Controller)

// Limitations:
//   - only 8086 mode
//   - only single mode
//   - don't care about input level or edge
//   - don't care about buffered mode or unbuffered mode
//   - don't support special fully nested mode
//   - priority is fixed to IR0 = highest
//   - don't support Poll command
//   - don't support Special Mask



class PIC8259 {

public:

  void reset();

  void write(int addr, uint8_t value);
  uint8_t read(int addr);

  // Device->8259: a device reports interrupt to 8259
  bool signalInterrupt(int intnum);

  // 8259->CPU: 8259 reports interrupt to CPU
  bool pendingInterrupt()                               { return m_pendingInterrupt; }

  // 8259->CPU: 8259 reports interrupt number to CPU
  int pendingInterruptNum()                             { return m_baseVector | m_pendingIR; }

  // CPU->8259: CPU acks the pending interrupt to 8259
  void ackPendingInterrupt();


private:

  void performEOI();
  int getHighestPriorityBitNum(uint8_t value);
  void setPendingInterrupt();

  uint8_t m_state; // all zeros = ready, bit 0 = waiting for ICW2, bit 1 = waiting for ICW3, bit 2 = waiting for ICW4

  uint8_t m_baseVector; // bits 3..7 got from ICW2
  bool    m_autoEOI;

  uint8_t m_IRR;  // Interrupt Request Register
  uint8_t m_ISR;  // In-Service Register
  uint8_t m_IMR;  // Interrupt Mask Register

  // if true port 0 reads ISR, otherwise port 0 reads IRR
  bool    m_readISR;

  bool    m_pendingInterrupt;
  uint8_t m_pendingIR;

};


} // namespace fabgl
