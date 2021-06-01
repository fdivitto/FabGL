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


#include "PIC8259.h"



namespace fabgl {



#define ICW1_IC4   0x01    // 1 = ICW4 needed
#define ICW1_SNGL  0x02    // 1 = single mode, 0 = cascade mode
#define ICW1_LTIM  0x08    // 1 = level trig imput, 0 = edge trig input

#define ICW4_PM    0x01    // 1 = x86, 0 = 8085
#define ICW4_AEOI  0x02    // 1 = auto EOI, 0 = normal EOI
#define ICW4_MS    0x04    // valid when ICW4_BUF = 1:  0 = buffered mode slave, 1 = buffered mode master
#define ICW4_BUF   0x08    // 1 = buffered mode, 0 = non buffered mode
#define ICW4_SFNM  0x10    // 1 = special fully nested mode

#define OCW2_EOI   0x20    // 1 = EOI command
#define OCW2_SL    0x40    // 1 = with EOI means Specific, with R means Set Priority
#define OCW2_R     0x80    // 1 = with EOI means Rotate, with SL means Set Priority

#define OCW3_RIS   0x01    // 1 = with OCW3_RR means Read IS register, 0 = with OCW3_RR means Read IR register
#define OCW3_RR    0x02    // 1 = OCW3_RIS specifies which register to read, 0 = no action
#define OCW3_POLL  0x04    // 1 = poll command
#define OCW3_SMM   0x08    // valid when OCW3_ESMM = 1: 0 = reset special mask, 1 = set special mask

#define PORT0_ICW1 0x10    // 1 = presenting ICW1 on port 0, 0 = presenting OCW2 or OCW3 on port 0
#define PORT0_OCW3 0x08    // 1 = presenting OCW3 on port 0, 0 = presenting OCW2 on port 0


// m_state meaning
#define STATE_READY         0x00 // waiting for ICW1/OCW2/OCW3 on port 0, OCW1 on port 1
#define STATE_WAITING_ICW2  0X01 // waiting for ICW2 on port 1
#define STATE_WAITING_ICW3  0X02 // waiting for ICW3 on port 2
#define STATE_WAITING_ICW4  0X04 // waiting for ICW4 on port 3


void PIC8259::reset()
{
  m_state               = STATE_READY;
  m_readISR             = false;
	m_IRR                 = 0x00;
	m_IMR                 = 0xff;
	m_ISR                 = 0x00;
  m_autoEOI             = false;
  m_baseVector          = 0x00;
  m_pendingInterrupt    = false;
  m_pendingIR           = 0;
}


void PIC8259::write(int addr, uint8_t value)
{
  switch (addr) {

    case 0:
      if (value & PORT0_ICW1) {
        // getting ICW1
        //printf("ICW1 <= %02X\n", value);
        //printf("  LTIM=%d ADI=%d SNGL=%d IC4=%d\n", (bool)(value & 0x08), (bool)(value & 0x04), (bool)(value & 0x02), (bool)(value & 0x01));
        m_state  = STATE_WAITING_ICW2;
        m_state |= value & ICW1_IC4 ? STATE_WAITING_ICW4 : 0;
        m_state |= value & ICW1_SNGL ? 0 : STATE_WAITING_ICW3;
      } else if (value & PORT0_OCW3) {
        // getting OCW3
        //printf("OCW3 <= %02X\n", value);
        //printf("  ESMM=%d SMM=%d P=%d RR=%d RIS=%d\n", (bool)(value & 0x40), (bool)(value & 0x20), (bool)(value & 0x04), (bool)(value & 0x02), (bool)(value & 0x01));
        if (value & OCW3_RR)
          m_readISR = value & OCW3_RIS;
      } else {
        // getting OCW2
        //printf("OCW2 <= %02X\n", value);
        //printf("  R=%d SL=%d EOI=%d L210=%d\n", (bool)(value & 0x80), (bool)(value & 0x40), (bool)(value & 0x20), value & 7);
        if (value & OCW2_EOI) {
          // perform EOI
          performEOI();
        }
      }
      break;

    case 1:
      if (m_state & STATE_WAITING_ICW2) {
        // getting ICW2
        //printf("ICW2 <= %02X\n", value);
        m_baseVector = value & 0xf8;
        m_state &= ~STATE_WAITING_ICW2;
      } else if (m_state & STATE_WAITING_ICW3) {
        // getting ICW3
        //printf("ICW3 <= %02X\n", value);
        // not supported, nothing to do
        m_state &= ~STATE_WAITING_ICW3;
      } else if (m_state & STATE_WAITING_ICW4) {
        // getting ICW4
        //printf("ICW4 <= %02X\n", value);
        //printf("  SFNM=%d BUF=%d M/S=%d AEOI=%d uPM=%d\n", (bool)(value & 0x10), (bool)(value & 0x08), (bool)(value & 0x04), (bool)(value & 0x02), (bool)(value & 0x01));
        m_autoEOI = value & ICW4_AEOI;
        m_state &= ~STATE_WAITING_ICW4;
      } else {
        // getting OCW1
        //printf("OCW1 <= %02X\n", value);
        m_IMR = value;
      }
      break;
  }
}


uint8_t PIC8259::read(int addr)
{
  switch (addr) {

    case 0:
      //printf("%02X <= %s\n", m_readISR ? m_ISR : m_IRR, m_readISR ? "ISR" : "IRR");
      return m_readISR ? m_ISR : m_IRR;

    case 1:
      //printf("%02X <= IMR\n", m_IMR);
      return m_IMR;

    default:
      return 0;

  }
}


// ret: 0..7
// 256 = no bit set
int PIC8259::getHighestPriorityBitNum(uint8_t value)
{
  if (value) {
    for (int i = 0; i < 8; ++i)
      if ((value >> i) & 1)
        return i;
  }
  return 256;
}


// set as pending interrupt the higher priority bit in M_IRR
void PIC8259::setPendingInterrupt()
{
  int highestInt   = getHighestPriorityBitNum(m_IRR & ~m_IMR);
  int servicingInt = getHighestPriorityBitNum(m_ISR);
  //printf("PIC8259::setPendingInterrupt(), highestInt=%d, servicingInt=%d\n", highestInt, servicingInt);
  if (highestInt < servicingInt) {
    // interrupt in IRR has higher priority than the servicing one
    m_pendingInterrupt    = true;
    m_pendingIR           = highestInt;
  }
}


// Device->8259: a device reports interrupt to 8259
bool PIC8259::signalInterrupt(int intnum)
{
  //printf("PIC8259::signalInterrupt(%d)\n", intnum);
  bool success = false;
  intnum &= 7;
  // already servicing?
  if ((m_ISR & (1 << intnum)) == 0) {
    // no, request interrupt
    m_IRR |= 1 << intnum;
    setPendingInterrupt();
    success = true;
  }
  return success;
}


// CPU->8259: CPU acks the pending interrupt to 8259
void PIC8259::ackPendingInterrupt()
{
  //printf("PIC8259::ackPendingInterrupt(), pending IR = %d\n", m_pendingIR);
  int pendmsk = 1 << m_pendingIR;
  m_ISR |= pendmsk;  // set interrupt bit in ISR
  m_IRR &= ~pendmsk; // reset interrupt bit in IRR
  m_pendingInterrupt = false;
  //printf("   ISR=%02X   IRR=%02X\n", m_ISR, m_IRR);
}


// application reports EOI (of the higher interrupt)
void PIC8259::performEOI()
{
  //printf("PIC8259::performEOI()\n");
  // reset servicing int
  int servicingInt = getHighestPriorityBitNum(m_ISR);
  m_ISR &= ~(1 << servicingInt);
  // is there another interrupt to ask for ack?
  setPendingInterrupt();
}


} // namespace fabgl
