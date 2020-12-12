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


#include "VIA6522.h"
#include "machine.h"



/////////////////////////////////////////////////////////////////////////////////////////////
// VIA (6522 - Versatile Interface Adapter)


MOS6522::MOS6522(Machine * machine, int tag, VIAPortIO portOut, VIAPortIO portIn)
  : m_machine(machine),
    m_tag(tag),
    m_portOut(portOut),
    m_portIn(portIn)
{
  reset();
}


void MOS6522::reset()
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
  memset(m_regs, 0, 16);
}


#if DEBUG6522
void MOS6522::dump()
{
  for (int i = 0; i < 16; ++i)
    printf("%02x ", m_regs[i]);
}
#endif


// reg must be 0..0x0f (not checked)
void MOS6522::writeReg(int reg, int value)
{
  #if DEBUG6522
  printf("VIA %d, writeReg 0x%02x = 0x%02x\n", m_tag, reg, value);
  #endif

  m_regs[reg] = value;

  switch (reg) {

    // ORB: Output Register B
    case VIA_REG_ORB_IRB:
      m_regs[VIA_REG_ORB_IRB] = value | (m_regs[VIA_REG_ORB_IRB] & ~m_regs[VIA_REG_DDRB]);
      m_portOut(this, Port_PB);
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_I_CB1;
      m_IFR &= ~VIA_I_CB2;
      break;

    // ORA: Output Register A
    case VIA_REG_ORA_IRA:
      m_regs[VIA_REG_ORA_IRA] = value | (m_regs[VIA_REG_ORA_IRA] & ~m_regs[VIA_REG_DDRA]);
      m_portOut(this, Port_PA);
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_I_CA1;
      m_IFR &= ~VIA_I_CA2;
      break;

    // DDRB: Data Direction Register B
    case VIA_REG_DDRB:
      m_regs[VIA_REG_DDRB] = value;
      break;

    // DDRA: Data Direction Register A
    case VIA_REG_DDRA:
      m_regs[VIA_REG_DDRA] = value;
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
      m_IFR &= ~VIA_I_T1;
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
      m_IFR &= ~VIA_I_T1;
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
      m_IFR &= ~VIA_I_T2;
      m_timer2Triggered = false;
      break;

    // SR: Shift Register
    case VIA_REG_SR:
      break;

    // ACR: Auxliary Control Register
    case VIA_REG_ACR:
      m_ACR = value;
      break;

    // PCR: Peripheral Control Register
    case VIA_REG_PCR:
    {
      m_regs[VIA_REG_PCR] = value;
      // CA2 control
      switch ((m_regs[VIA_REG_PCR] >> 1) & 0b111) {
        case 0b110:
          // manual output - low
          m_CA2 = 0;
          m_portOut(this, Port_CA2);
          break;
        case 0b111:
          // manual output - high
          m_CA2 = 1;
          m_portOut(this, Port_CA2);
          break;
        default:
          break;
      }
      // CB2 control
      switch ((m_regs[VIA_REG_PCR] >> 5) & 0b111) {
        case 0b110:
          // manual output - low
          m_CB2 = 0;
          m_portOut(this, Port_CB2);
          break;
        case 0b111:
          // manual output - high
          m_CB2 = 1;
          m_portOut(this, Port_CB2);
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
      if (value & VIA_I_CTRL) {
        // set 0..6 bits
        m_IER |= value & 0x7f;
      } else {
        // reset 0..6 bits
        m_IER &= ~value & 0x7f;
      }
      break;

    // ORA: Output Register A - no Handshake
    case VIA_REG_ORA_IRA_NH:
      m_regs[VIA_REG_ORA_IRA] = value | (m_regs[VIA_REG_ORA_IRA] & ~m_regs[VIA_REG_DDRA]);
      m_portOut(this, Port_PA);
      break;

  };
}


// reg must be 0..0x0f (not checked)
int MOS6522::readReg(int reg)
{
  #if DEBUG6522
  printf("VIA %d, readReg 0x%02x\n", m_tag, reg);
  #endif

  switch (reg) {

    // IRB: Input Register B
    case VIA_REG_ORB_IRB:
      // clear CB1 and CB2 interrupt flags
      m_IFR &= ~VIA_I_CB1;
      m_IFR &= ~VIA_I_CB2;
      // Input from Port B
      m_portIn(this, Port_PB);
      return m_regs[VIA_REG_ORB_IRB];

    // IRA: Input Register A
    case VIA_REG_ORA_IRA:
      // clear CA1 and CA2 interrupt flags
      m_IFR &= ~VIA_I_CA1;
      m_IFR &= ~VIA_I_CA2;
      // Input from Port A
      m_portIn(this, Port_PA);
      return m_regs[VIA_REG_ORA_IRA];

    // DDRB: Data Direction Register B
    case VIA_REG_DDRB:
      return m_regs[VIA_REG_DDRB];

    // DDRA: Data Direction Register A
    case VIA_REG_DDRA:
      return m_regs[VIA_REG_DDRA];

    // T1C-L: T1 Low-Order Counter
    case VIA_REG_T1_C_LO:
      // clear T1 interrupt flag
      m_IFR &= ~VIA_I_T1;
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
      m_IFR &= ~VIA_I_T2;
      // read T2 low order counter
      return m_timer2Counter & 0xff;

    // T2C-H: T2 High-Order Counter
    case VIA_REG_T2_C_HI:
      // read T2 high order counter
      return m_timer2Counter >> 8;

    // SR: Shift Register
    case VIA_REG_SR:
      return m_regs[reg];

    // ACR: Auxiliary Control Register
    case VIA_REG_ACR:
      return m_ACR;

    // PCR: Peripheral Control Register
    case VIA_REG_PCR:
      return m_regs[VIA_REG_PCR];

    // IFR: Interrupt Flag Register
    case VIA_REG_IFR:
      return m_IFR | (m_IFR & m_IER ? 0x80 : 0);

    // IER: Interrupt Enable Register
    case VIA_REG_IER:
      return m_IER | 0x80;

    // IRA: Input Register A - no handshake
    case VIA_REG_ORA_IRA_NH:
      // Input from Port A (no handshake)
      m_portIn(this, Port_PA);
      return m_regs[VIA_REG_ORA_IRA];

  }
  return 0;
}


// ret. true on interrupt
bool MOS6522::tick(int cycles)
{
  // handle Timer 1
  m_timer1Counter -= cycles;
  if (m_timer1Counter <= 0) {
    if (m_ACR & VIA_ACR_T1_FREERUN) {
      // free run, reload from latch
      m_timer1Counter += (m_timer1Latch - 1) + 3;  // +2 delay before next start
      m_IFR |= VIA_I_T1;  // set interrupt flag
    } else if (!m_timer1Triggered) {
      // one shot
      m_timer1Counter += 0xFFFF;
      m_timer1Triggered = true;
      m_IFR |= VIA_I_T1;  // set interrupt flag
    } else
      m_timer1Counter = (uint16_t)m_timer1Counter;  // restart from <0xffff
  }

  // handle Timer 2
  if ((m_ACR & VIA_ACR_T2_COUNTPULSES) == 0) {
    m_timer2Counter -= cycles;
    if (m_timer2Counter <= 0 && !m_timer2Triggered) {
      m_timer2Counter += 0xFFFF;
      m_timer2Triggered = true;
      m_IFR |= VIA_I_T2;  // set interrupt flag
    }
  }

  // handle CA1 (RESTORE key)
  if (m_CA1 != m_CA1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_regs[VIA_REG_PCR] & 1) && m_CA1) || (!(m_regs[VIA_REG_PCR] & 1) && !m_CA1)) {
      m_IFR |= VIA_I_CA1;
    }
    m_CA1_prev = m_CA1;  // set interrupt flag
  }

/*
  // handle CB1
  if (m_CB1 != m_CB1_prev) {
    // (interrupt on low->high transition) OR (interrupt on high->low transition)
    if (((m_regs[VIA_REG_PCR] & 0x10) && m_CB1) || (!(m_regs[VIA_REG_PCR] & 0x10) && !m_CB1)) {
      m_IFR |= VIA_I_CB1;  // set interrupt flag
    }
    m_CB1_prev = m_CB1;
  }
*/

  return m_IER & m_IFR & 0x7f;

}

