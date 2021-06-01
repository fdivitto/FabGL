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


#include "VIA6522.h"


#pragma GCC optimize ("O2")

#if FABGL_ESP_IDF_VERSION > FABGL_ESP_IDF_VERSION_VAL(3, 3, 5)
  #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif



namespace fabgl {


/////////////////////////////////////////////////////////////////////////////////////////////
// VIA (6522 - Versatile Interface Adapter)


VIA6522::VIA6522(int tag)
  : m_tag(tag)
{
  #if DEBUG6522
  m_tick = 0;
  #endif
}


void VIA6522::reset()
{
  m_timer1Counter   = 0x0000;
  m_timer1Latch     = 0x0000;
  m_timer2Counter   = 0x0000;
  m_timer2Latch     = 0x00;
  m_CA1             = 0;
  m_CA1_prev        = 0;
  m_CA2             = 0;
  m_CA2_prev        = 0;
  m_CB1             = 0;
  m_CB1_prev        = 0;
  m_CB2             = 0;
  m_CB2_prev        = 0;
  m_IFR             = 0;
  m_IER             = 0;
  m_ACR             = 0;
  m_timer1Triggered = false;
  m_timer2Triggered = false;
  m_DDRA            = 0;
  m_DDRB            = 0;
  m_PCR             = 0;
  m_PA              = 0xff;
  m_PB              = 0xff;
  m_SR              = 0;
  m_IRA             = 0xff;
  m_IRB             = 0xff;
  m_ORA             = 0;
  m_ORB             = 0;
}


void VIA6522::setPA(int value)
{
  m_PA = value;
  m_IRA = m_PA; // IRA is exactly what there is on port A
}


void VIA6522::setBitPA(int bit, bool value)
{
  uint8_t newPA = (m_PA & ~(1 << bit)) | ((int)value << bit);
  setPA(newPA);
}


void VIA6522::openBitPA(int bit)
{
  uint8_t mask = (1 << bit);
  if (m_DDRA & mask) {
    // pin configured as output, set value of ORA
    setBitPA(bit, m_ORA & mask);
  } else {
    // pin configured as input, pull it up
    setBitPA(bit, true);
  }
}


void VIA6522::setPB(int value)
{
  m_PB = value;
  m_IRB = (m_PB & ~m_DDRB) | (m_ORB & m_DDRB);  // IRB contains PB for inputs, and ORB for outputs
}


void VIA6522::setBitPB(int bit, bool value)
{
  uint8_t newPB = (m_PB & ~(1 << bit)) | ((int)value << bit);
  setPB(newPB);
}


void VIA6522::openBitPB(int bit)
{
  uint8_t mask = (1 << bit);
  if (m_DDRB & mask) {
    // pin configured as output, set value of ORB
    setBitPB(bit, m_ORB & mask);
  } else {
    // pin configured as input, pull it up
    setBitPB(bit, true);
  }
}


// reg must be 0..0x0f (not checked)
void VIA6522::writeReg(int reg, int value)
{
  #if DEBUG6522
  printf("tick %d VIA %d writeReg(%s, 0x%02x)\n", m_tick, m_tag, VIAREG2STR[reg], value);
  #endif

  switch (reg) {

    // ORB: Output Register B
    case VIA_REG_ORB_IRB:
      m_ORB = value;
      m_PB  = (m_ORB & m_DDRB) | (m_PB & ~m_DDRB);
      m_IRB = (m_PB & ~m_DDRB) | (m_ORB & m_DDRB);  // IRB contains PB for inputs, and ORB for outputs
      m_portOut(m_context, this, VIA6522Port::PB);
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_IER_CB1;
      m_IFR &= ~VIA_IER_CB2;
      break;

    // ORA: Output Register A
    case VIA_REG_ORA_IRA:
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_IER_CA1;
      m_IFR &= ~VIA_IER_CA2;
      // not break!
    // ORA: Output Register A - no Handshake
    case VIA_REG_ORA_IRA_NH:
      m_ORA = value;
      m_PA  = (m_ORA & m_DDRA) | (m_PA & ~m_DDRA);
      m_IRA = m_PA;                                   // IRA is exactly what there is on port A
      m_portOut(m_context, this, VIA6522Port::PA);
      break;

    // DDRB: Data Direction Register B
    case VIA_REG_DDRB:
      m_DDRB = value;
      m_PB   = (m_ORB & m_DDRB) | (m_PB & ~m_DDRB);   // refresh Port B status
      m_IRB  = (m_PB & ~m_DDRB) | (m_ORB & m_DDRB);   // IRB contains PB for inputs, and ORB for outputs
      break;

    // DDRA: Data Direction Register A
    case VIA_REG_DDRA:
      m_DDRA = value;
      m_PA   = (m_ORA & m_DDRA) | (m_PA & ~m_DDRA);   // refresh Port A status
      m_IRA  = m_PA;                                  // IRA is exactly what there is on port A
      break;

    // T1C-L: T1 Low-Order Latches
    case VIA_REG_T1_C_LO:
      m_timer1Latch = (m_timer1Latch & 0xff00) | value;
      break;

    // T1C-H: T1 High-Order Counter
    case VIA_REG_T1_C_HI:
      m_timer1Latch = (m_timer1Latch & 0x00ff) | (value << 8);
      // timer1: write into high order counter
      // timer1: transfer low order latch into low order counter
      m_timer1Counter = (m_timer1Latch & 0x00ff) | (value << 8);
      // clear T1 interrupt flag
      m_IFR &= ~VIA_IER_T1;
      m_timer1Triggered = false;
      break;

    // T1L-L: T1 Low-Order Latches
    case VIA_REG_T1_L_LO:
      m_timer1Latch = (m_timer1Latch & 0xff00) | value;
      break;

    // T1L-H: T1 High-Order Latches
    case VIA_REG_T1_L_HI:
      m_timer1Latch = (m_timer1Latch & 0x00ff) | (value << 8);
      // clear T1 interrupt flag
      m_IFR &= ~VIA_IER_T1;
      break;

    // T2C-L: T2 Low-Order Latches
    case VIA_REG_T2_C_LO:
      m_timer2Latch = value;
      break;

    // T2C-H: T2 High-Order Counter
    case VIA_REG_T2_C_HI:
      // timer2: copy low order latch into low order counter
      m_timer2Counter = (value << 8) | m_timer2Latch;
      // clear T2 interrupt flag
      m_IFR &= ~VIA_IER_T2;
      m_timer2Triggered = false;
      break;

    // SR: Shift Register
    case VIA_REG_SR:
      m_SR = value;
      break;

    // ACR: Auxliary Control Register
    case VIA_REG_ACR:
      m_ACR = value;
      break;

    // PCR: Peripheral Control Register
    case VIA_REG_PCR:
    {
      m_PCR = value;
      // CA2 control
      switch ((m_PCR >> 1) & 0b111) {
        case 0b110:
          // manual output - low
          m_CA2 = 0;
          m_portOut(m_context, this, VIA6522Port::CA2);
          break;
        case 0b111:
          // manual output - high
          m_CA2 = 1;
          m_portOut(m_context, this, VIA6522Port::CA2);
          break;
        default:
          break;
      }
      // CB2 control
      switch ((m_PCR >> 5) & 0b111) {
        case 0b110:
          // manual output - low
          m_CB2 = 0;
          m_portOut(m_context, this, VIA6522Port::CB2);
          break;
        case 0b111:
          // manual output - high
          m_CB2 = 1;
          m_portOut(m_context, this, VIA6522Port::CB2);
          break;
        default:
          break;
      }
      break;
    }

    // IFR: Interrupt Flag Register
    case VIA_REG_IFR:
      // reset each bit at 1
      m_IFR &= ~value & 0x7f;
      break;

    // IER: Interrupt Enable Register
    case VIA_REG_IER:
      if (value & VIA_IER_CTRL) {
        // set 0..6 bits
        m_IER |= value & 0x7f;
      } else {
        // reset 0..6 bits
        m_IER &= ~value & 0x7f;
      }
      break;

  };
}


// reg must be 0..0x0f (not checked)
int VIA6522::readReg(int reg)
{
  #if DEBUG6522
  printf("tick %d VIA %d readReg(%s)\n", m_tick, m_tag, VIAREG2STR[reg]);
  #endif

  switch (reg) {

    // IRB: Input Register B
    case VIA_REG_ORB_IRB:
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_IER_CB1;
      m_IFR &= ~VIA_IER_CB2;
      // get updated PB status
      m_portIn(m_context, this, VIA6522Port::PB);
      return m_IRB;

    // IRA: Input Register A
    case VIA_REG_ORA_IRA:
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_IER_CA1;
      m_IFR &= ~VIA_IER_CA2;
    // IRA: Input Register A - no handshake
    case VIA_REG_ORA_IRA_NH:
      // get updated PA status
      m_portIn(m_context, this, VIA6522Port::PA);
      return m_IRA;

    // DDRB: Data Direction Register B
    case VIA_REG_DDRB:
      return m_DDRB;

    // DDRA: Data Direction Register A
    case VIA_REG_DDRA:
      return m_DDRA;

    // T1C-L: T1 Low-Order Counter
    case VIA_REG_T1_C_LO:
      // clear T1 interrupt flag
      m_IFR &= ~VIA_IER_T1;
      // read T1 low order counter
      return m_timer1Counter & 0xff;

    // T1C-H: T1 High-Order Counter
    case VIA_REG_T1_C_HI:
      // read T1 high order counter
      return m_timer1Counter >> 8;

    // T1L-L: T1 Low-Order Latches
    case VIA_REG_T1_L_LO:
      // read T1 low order latch
      return m_timer1Latch & 0xff;

    // T1L-H: T1 High-Order Latches
    case VIA_REG_T1_L_HI:
      // read T1 high order latch
      return m_timer1Latch >> 8;

    // T2C-L: T2 Low-Order Counter
    case VIA_REG_T2_C_LO:
      // clear T2 interrupt flag
      m_IFR &= ~VIA_IER_T2;
      // read T2 low order counter
      return m_timer2Counter & 0xff;

    // T2C-H: T2 High-Order Counter
    case VIA_REG_T2_C_HI:
      // read T2 high order counter
      return m_timer2Counter >> 8;

    // SR: Shift Register
    case VIA_REG_SR:
      return m_SR;

    // ACR: Auxiliary Control Register
    case VIA_REG_ACR:
      return m_ACR;

    // PCR: Peripheral Control Register
    case VIA_REG_PCR:
      return m_PCR;

    // IFR: Interrupt Flag Register
    case VIA_REG_IFR:
      return m_IFR | (m_IFR & m_IER ? 0x80 : 0);

    // IER: Interrupt Enable Register
    case VIA_REG_IER:
      return m_IER | 0x80;

  }
  return 0;
}


// ret. true on interrupt
bool VIA6522::tick(int cycles)
{
  #if DEBUG6522
  m_tick += cycles;
  #endif

  // handle Timer 1
  m_timer1Counter -= cycles;
  if (m_timer1Counter <= 0) {
    if (m_ACR & VIA_ACR_T1_FREERUN) {
      // free run, reload from latch
      m_timer1Counter += (m_timer1Latch - 1) + 3;  // +2 delay before next start
      m_IFR |= VIA_IER_T1;  // set interrupt flag
    } else if (!m_timer1Triggered) {
      // one shot
      m_timer1Counter += 0xFFFF;
      m_timer1Triggered = true;
      m_IFR |= VIA_IER_T1;  // set interrupt flag
    } else
      m_timer1Counter = (uint16_t)m_timer1Counter;  // restart from <0xffff
  }

  // handle Timer 2
  if ((m_ACR & VIA_ACR_T2_COUNTPULSES) == 0) {
    m_timer2Counter -= cycles;
    if (m_timer2Counter <= 0 && !m_timer2Triggered) {
      m_timer2Counter += 0xFFFF;
      m_timer2Triggered = true;
      m_IFR |= VIA_IER_T2;  // set interrupt flag
    }
  }

  // handle CA1 (RESTORE key)
  if (m_CA1 != m_CA1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_PCR & 1) && m_CA1) || (!(m_PCR & 1) && !m_CA1)) {
      m_IFR |= VIA_IER_CA1;
    }
    m_CA1_prev = m_CA1;  // set interrupt flag
  }

  // handle CB1
  if (m_CB1 != m_CB1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_PCR & 0x10) && m_CB1) || (!(m_PCR & 0x10) && !m_CB1)) {
      m_IFR |= VIA_IER_CB1;  // set interrupt flag
    }
    m_CB1_prev = m_CB1;
  }

  return m_IER & m_IFR & 0x7f;

}



};  // namespace fabgl
