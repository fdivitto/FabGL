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
 * @brief This file contains fabgl::VIA6522 definition.
 */



#include <stdlib.h>
#include <stdint.h>

#include "fabgl.h"


#define DEBUG6522 0


namespace fabgl {


/////////////////////////////////////////////////////////////////////////////////////////////
// VIA (6522 - Versatile Interface Adapter)


// VIA registers
#define VIA_REG_ORB_IRB         0x0
#define VIA_REG_ORA_IRA         0x1
#define VIA_REG_DDRB            0x2
#define VIA_REG_DDRA            0x3
#define VIA_REG_T1_C_LO         0x4
#define VIA_REG_T1_C_HI         0x5
#define VIA_REG_T1_L_LO         0x6
#define VIA_REG_T1_L_HI         0x7
#define VIA_REG_T2_C_LO         0x8
#define VIA_REG_T2_C_HI         0x9
#define VIA_REG_SR              0xa
#define VIA_REG_ACR             0xb   // Auxiliary Control Register
#define VIA_REG_PCR             0xc   // Peripherical Control Register
#define VIA_REG_IFR             0xd   // Interrupt Flag Register
#define VIA_REG_IER             0xe   // Interrupt Enable Register
#define VIA_REG_ORA_IRA_NH      0xf

// IER: VIA interrupt enable/disable bit mask
#define VIA_IER_CA2             0x01
#define VIA_IER_CA1             0x02
#define VIA_IER_SR              0x04
#define VIA_IER_CB2             0x08
#define VIA_IER_CB1             0x10
#define VIA_IER_T2              0x20
#define VIA_IER_T1              0x40
#define VIA_IER_CTRL            0x80  // 0 = Logic 1 in bits 0-6 disables the corresponding interrupt, 1 = Logic 1 in bits 0-6 enables the corresponding interrupt

// VIA, ACR flags
#define VIA_ACR_T2_COUNTPULSES  0x20
#define VIA_ACR_T1_FREERUN      0x40
#define VIA_ACR_T1_OUTENABLE    0x80


/** \ingroup Enumerations
 * @brief I/O port
 */
enum class VIA6522Port {
  PA,  /**< (8 bit) */
  PB,  /**< (8 bit) */
  CA1, /**< (1 bit) */
  CA2, /**< (1 bit) */
  CB1, /**< (1 bit) */
  CB2, /**< (1 bit) */
};


#if DEBUG6522
static const char * VIAREG2STR[] = { "ORB_IRB", "ORA_IRA", "DDRB", "DDRA", "T1_C_LO", "T1_C_HI", "T1_L_LO", "T1_L_HI", "T2_C_LO", "T2_C_HI", "SR", "ACR", "PCR", "IFR", "IER", "ORA_IRA_NH" };
#endif


/**
 * @brief VIA 6522 emulator
 */
class VIA6522 {
public:

  // callbacks
  typedef void (*PortOutputCallback)(void * context, VIA6522 * via, VIA6522Port port);
  typedef void (*PortInputCallback)(void * context, VIA6522 * via, VIA6522Port port);

  VIA6522(int tag);

  void setCallbacks(void * context, PortInputCallback portIn, PortOutputCallback portOut) {
    m_context = context;
    m_portIn  = portIn;
    m_portOut = portOut;
  }

  void reset();

  void writeReg(int reg, int value);
  int readReg(int reg);

  bool tick(int cycles);

  // set/get "external" state of PA
  uint8_t PA()                       { return m_PA; }
  void setPA(int value);
  void setBitPA(int bit, bool value);
  void openBitPA(int bit);

  // set/get "external" state of PB
  uint8_t PB()                       { return m_PB; }
  void setPB(int value);
  void setBitPB(int bit, bool value);
  void openBitPB(int bit);

  uint8_t CA1()                      { return m_CA1; }
  void setCA1(int value)             { m_CA1_prev = m_CA1; m_CA1 = value; }

  uint8_t CA2()                      { return m_CA2; }
  void setCA2(int value)             { m_CA2_prev = m_CA2; m_CA2 = value; }

  uint8_t CB1()                      { return m_CB1; }
  void setCB1(int value)             { m_CB1_prev = m_CB1; m_CB1 = value; }

  uint8_t CB2()                      { return m_CB2; }
  void setCB2(int value)             { m_CB2_prev = m_CB2; m_CB2 = value; }

  uint8_t DDRA()                     { return m_DDRA; }

  uint8_t DDRB()                     { return m_DDRB; }

  uint8_t tag()                      { return m_tag; }


private:

  uint8_t       m_tag;

  // timers
  int           m_timer1Counter;
  uint16_t      m_timer1Latch;
  int           m_timer2Counter;
  uint8_t       m_timer2Latch;     // timer 2 latch is 8 bits
  bool          m_timer1Triggered;
  bool          m_timer2Triggered;

  // CA1, CA2
  uint8_t       m_CA1;
  uint8_t       m_CA1_prev;
  uint8_t       m_CA2;
  uint8_t       m_CA2_prev;

  // CB1, CB2
  uint8_t       m_CB1;
  uint8_t       m_CB1_prev;
  uint8_t       m_CB2;
  uint8_t       m_CB2_prev;

  // PA, PB
  uint8_t       m_DDRA;
  uint8_t       m_DDRB;
  uint8_t       m_PA;     // what actually there is out of 6522 port A
  uint8_t       m_PB;     // what actually there is out of 6522 port B
  uint8_t       m_IRA;    // input register A
  uint8_t       m_IRB;    // input register B
  uint8_t       m_ORA;    // output register A
  uint8_t       m_ORB;    // output register B

  uint8_t       m_IFR;
  uint8_t       m_IER;
  uint8_t       m_ACR;
  uint8_t       m_PCR;
  uint8_t       m_SR;

  // callbacks
  void *             m_context;
  PortInputCallback  m_portIn;       // input callback
  PortOutputCallback m_portOut;      // output callback

  #if DEBUG6522
  int           m_tick;
  #endif
};


};  // namespace fabgl
