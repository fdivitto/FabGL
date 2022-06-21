/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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



#include <string.h>

#include "PC8250.h"



#define PC8250_IER_RX_INT     0x01
#define PC8250_IER_TX_INT     0x02
#define PC8250_IER_LINE_INT   0x04
#define PC8250_IER_MODEM_INT  0x08


#define PC8250_LCR_MBITSTOP   0x04
#define PC8250_LCR_PARITYEN   0x08
#define PC8250_LCR_PARITYEVEN 0x10
#define PC8250_LCR_DLAB       0x80


#define PC8250_MCR_DTR        0x01
#define PC8250_MCR_RTS        0x02
#define PC8250_MCR_OUT1       0x04
#define PC8250_MCR_OUT2       0x08
#define PC8250_MCR_LOOPBACK   0x10


#define PC8250_LSR_DR         0x01
#define PC8250_LSR_OE         0x02
#define PC8250_LSR_PE         0x04
#define PC8250_LSR_FE         0x08
#define PC8250_LSR_BI         0x10
#define PC8250_LSR_THRE       0x20
#define PC8250_LSR_TEMT       0x40


#define PC8250_MSR_DCTS       0x01
#define PC8250_MSR_DDSR       0x02
#define PC8250_MSR_TERI       0x04
#define PC8250_MSR_DDCD       0x08
#define PC8250_MSR_CTS        0x10
#define PC8250_MSR_DSR        0x20
#define PC8250_MSR_RI         0x40
#define PC8250_MSR_DCD        0x80



namespace fabgl {


PC8250::PC8250(int freq)
  : m_freq(freq),
    m_serialPort(nullptr)
{
  m_rxQueue = xQueueCreate(16, 1);
}


PC8250::~PC8250()
{
  vQueueDelete(m_rxQueue);
}


void PC8250::reset()
{
  m_DLL     = 0;
  m_DLM     = 0;
  m_trigs   = 0;
  m_IER     = 0;
  m_LCR     = 0;
  m_MCR     = 0;
  m_LSR     = PC8250_LSR_THRE | PC8250_LSR_TEMT;
  m_MSR     = 0;
  m_SCR     = 0;
  m_overrun = false;
}


void PC8250::setSerialPort(SerialPort * value)
{
  m_serialPort = value;
  m_serialPort->setCallbacks(this, rxReadyCallback, rxCallback);
  m_serialPort->setup(2, 115200, 8, 'N', 1, fabgl::FlowControl::None);
}


void PC8250::rxCallback(void * args, uint8_t value, bool fromISR)
{
  auto obj = (PC8250 *) args;
  bool r = fromISR ? xQueueSendToBackFromISR(obj->m_rxQueue, &value, nullptr) : xQueueSendToBack(obj->m_rxQueue, &value, 0);
  if (!r)
    obj->m_overrun = true;
}


bool PC8250::rxReadyCallback(void * args, bool fromISR)
{
  return true;
}


uint8_t PC8250::read(int reg)
{
  uint8_t ret = 0;
  
  switch(reg & 7) {
  
    // DLAB = 0 : Receiver Buffer Register (RBR)
    // DLAB = 1 : Divisor Latch LSB Register (DLL)
    case 0:
      if (m_LCR & PC8250_LCR_DLAB) {
      
        // Divisor Latch LSB Register (DLL)
        ret = m_DLL;
        
      } else {
      
        // Receiver Buffer Register (RBR)
        //printf("RD RBR ");
        if (xQueueReceive(m_rxQueue, &ret, 0)) {
          m_LSR   &= ~PC8250_LSR_DR;       // reset DR flag
          m_trigs &= ~PC8250_IER_RX_INT;   // reset interrupt triggered flag
          //printf("%02X\n", ret);
        } else {
          //printf("--\n");
        }
        
      }
      break;
      
    // DLAB = 0 : Interrupt Enable Register (IER)
    // DLAB = 1 : Divisor Latch MSB Register (DLM)
    case 1:
      if (m_LCR & PC8250_LCR_DLAB) {
      
        // Divisor Latch MSB Register (DLM)
        ret = m_DLM;
        
      } else {
      
        // Interrupt Enable Register (IER)
        ret = m_IER;
        //printf("RD IER %02X\n", ret);
        
      }
      break;
      
    // Interrupt Identification Register (IIR)
    case 2:
      if (m_trigs & PC8250_IER_LINE_INT)
        ret = 0b110;
      else if (m_trigs & PC8250_IER_RX_INT)
        ret = 0b100;
      else if (m_trigs & PC8250_IER_TX_INT)
        ret = 0b010;
      else if (m_trigs & PC8250_IER_MODEM_INT)
        ret = 0b000;
      else
        ret = 0b001;
      //printf("RD IIR %02X\n", ret);
      break;
        
    // Line Control Register (LCR)
    case 3:
      ret = m_LCR;
      //printf("RD LCR %02X\n", ret);
      break;
      
    // MODEM Control Register (MCR)
    case 4:
      ret = m_MCR;
      //printf("RD MCR %02X\n", ret);
      break;
      
    // Line Status Register (LSR)
    case 5:
      checkByteReceived();
      checkOverflowError();
      checkParityError();
      checkFramingError();
      ret = m_LSR;
      // reset OE, PE, FE flags
      m_LSR  &= ~(PC8250_LSR_OE | PC8250_LSR_PE | PC8250_LSR_FE);
      // reset interrupt triggered flag
      m_trigs &= ~PC8250_IER_LINE_INT;
      //printf("RD LSR %02X\n", ret);
      break;
      
    // MODEM Status Register (MSR)
    case 6:
      checkCTSChanged();
      checkDSRChanged();
      checkRIChangeLow();
      checkDCDChanged();
      ret = m_MSR;
      // reset DCTS, DDSR, TERI, DDCD flags
      m_MSR   &= ~(PC8250_MSR_DCTS | PC8250_MSR_DDSR | PC8250_MSR_TERI | PC8250_MSR_DDCD);
      // reset interrupt triggered flag
      m_trigs &= ~PC8250_IER_MODEM_INT;
      //printf("RD MSR %02X (%s %s %s %s %s %s %s %s)\n", ret, ret & PC8250_MSR_DCTS ? "DCTS" : "", ret & PC8250_MSR_DDSR ? "DDSR" : "", ret & PC8250_MSR_TERI ? "TERI" : "", ret & PC8250_MSR_DDCD ? "DDCD" : "", ret & PC8250_MSR_CTS ? "CTS" : "", ret & PC8250_MSR_DSR ? "DSR" : "", ret & PC8250_MSR_RI ? "RI(OUT1)" : "", ret & PC8250_MSR_DCD ? "DCD(OUT2)" : "");
      break;
      
    // Scratch Register
    case 7:
      ret = m_SCR;
      //printf("RD SCR %02X\n", ret);
      break;
  
  }
  
  return ret;
}


void PC8250::write(int reg, uint8_t value)
{
  switch(reg & 7) {
  
    // DLAB = 0 : Transmitter Holding Register (THR)
    // DLAB = 1 : Divisor Latch LSB Register (DLL)
    case 0:
      if (m_LCR & PC8250_LCR_DLAB) {
      
        // Divisor Latch LSB Register (DLL)
        m_DLL = value;
        setBaud();
        
      } else {
      
        // Transmitter Holding Register (THR)
        //printf("WR THR = %02X ", value);
        if (m_MCR & PC8250_MCR_LOOPBACK) {
          // loopback mode
          //printf("  loopback ");
          if ((m_LSR & PC8250_LSR_DR) || !xQueueSendToBack(m_rxQueue, &value, 0)) {
            //printf("  overrun ");
            m_overrun = true;
          }
          m_LSR |= PC8250_LSR_DR;
        } else {
          // normal mode
          m_serialPort->send(value);
        }
        //printf("\n");
        m_trigs &= ~PC8250_IER_TX_INT;         // reset interrupt triggered flag
        m_trigs |= m_IER & PC8250_IER_TX_INT;  // set interrupt triggered flag (of interrupt enabled)
        
      }
      break;
      
    // DLAB = 0 : Interrupt Enable Register (IER)
    // DLAB = 1 : Divisor Latch MSB Register (DLM)
    case 1:
      if (m_LCR & PC8250_LCR_DLAB) {
        // Divisor Latch MSB Register (DLM)
        m_DLM = value;
        setBaud();
      } else {
        // Interrupt Enable Register (IER)
        m_IER = value & 0x0f;
        //printf("WR IER = %02X\n", value);
      }
      break;
      
    case 2:
      //printf("WR IIR - UNSUPPORTED\n");
      break;
      
    // Line Control Register (LCR)
    case 3:
      m_LCR = value;
      setFrame();
      //printf("WR LCR = %02X\n", value);
      //if (m_LCR & 0x40) printf("  SEND BREAK!!\n");
      break;
      
    // MODEM Control Register (MCR)
    case 4:
      m_MCR = value & 0x1f;
      m_serialPort->setDTRStatus(m_MCR & PC8250_MCR_DTR);
      m_serialPort->setRTSStatus(m_MCR & PC8250_MCR_RTS);
      //printf("WR MCR = %02X (%s %s %s %s)\n", value, value & PC8250_MCR_DTR ? "DTR" : "", value & PC8250_MCR_RTS ? "RTS" : "", value & PC8250_MCR_OUT1 ? "OUT1(RI)" : "", value & PC8250_MCR_OUT2 ? "OUT2(DCD)" : "");
      break;
      
    case 5:
      //printf("WR LSR - NOT SUPPORTED\n");
      break;
    
    // MODEM Status Register (MSR)
    case 6:
      m_MSR = (m_MSR & 0xf0) | (value & 0x0f);
      if (m_MSR & 0x0f)
        m_trigs |= m_IER & PC8250_IER_MODEM_INT;
      //printf("WR MSR = %02X (%s %s %s %s %s %s %s %s)\n", value, value & PC8250_MSR_DCTS ? "DCTS" : "", value & PC8250_MSR_DDSR ? "DDSR" : "", value & PC8250_MSR_TERI ? "TERI" : "",value & PC8250_MSR_DDCD ? "DDCD" : "", value & PC8250_MSR_CTS ? "CTS" : "", value & PC8250_MSR_DSR ? "DSR" : "", value & PC8250_MSR_RI ? "RI(OUT1)" : "", value & PC8250_MSR_DCD ? "DCD(OUT2)" : "");
      break;
      
    // Scratch Register
    case 7:
      m_SCR = value;
      //printf("WR SCR = %02X\n", value);
      break;
  
  }
}


void PC8250::setBaud()
{
  int div  = m_DLL | ((int)m_DLM << 8);
  int baud = m_freq / 16 / (div ? div : 1);
  m_serialPort->setBaud(baud);
  //printf("BAUD = %d\n", baud);
}


void PC8250::setFrame()
{
  int dataLength = (m_LCR & 0b11) + 5;
  char parity    = m_LCR & PC8250_LCR_PARITYEN ? (m_LCR & PC8250_LCR_PARITYEVEN ? 'E' : 'O') : 'N';
  float stopBits = m_LCR & PC8250_LCR_MBITSTOP ? (dataLength == 5 ? 1.5 : 2) : 1;
  m_serialPort->setFrame(dataLength, parity, stopBits);
}


bool PC8250::getOut1()
{
  return m_MCR & PC8250_MCR_OUT1;
}


bool PC8250::getOut2()
{
  return m_MCR & PC8250_MCR_OUT2;
}


void PC8250::checkOverflowError()
{
  if (m_serialPort->overflowError() || m_overrun) {
    m_overrun  = false;
    m_LSR     |= PC8250_LSR_OE;                // set OE flag
    m_trigs   |= m_IER & PC8250_IER_LINE_INT;  // set int triggered flag
  }
}


void PC8250::checkParityError()
{
  if (m_serialPort->parityError()) {
    m_LSR   |= PC8250_LSR_PE;                // set PE flag
    m_trigs |= m_IER & PC8250_IER_LINE_INT;  // set int triggered flag
  }
}


void PC8250::checkFramingError()
{
  if (m_serialPort->framingError()) {
    m_LSR   |= PC8250_LSR_FE;                // set FE flag
    m_trigs |= m_IER & PC8250_IER_LINE_INT;  // set int triggered flag
  }
}


void PC8250::checkByteReceived()
{
  if (uxQueueMessagesWaiting(m_rxQueue)) {
    m_LSR   |= PC8250_LSR_DR;                // set DR flag
    m_trigs |= m_IER & PC8250_IER_RX_INT;    // set int triggered flag
  }
}


void PC8250::checkCTSChanged()
{
  bool value = m_MCR & PC8250_MCR_LOOPBACK ? m_MCR & PC8250_MCR_RTS : m_serialPort->CTSStatus();
  if ((bool)(m_MSR & PC8250_MSR_CTS) != value) {
    m_MSR   ^= PC8250_MSR_CTS;                // invert CTS
    m_MSR   |= PC8250_MSR_DCTS;               // set DCTS
    m_trigs |= m_IER & PC8250_IER_MODEM_INT;  // set int triggered flag
  }
}


void PC8250::checkDSRChanged()
{
  bool value = m_MCR & PC8250_MCR_LOOPBACK ? m_MCR & PC8250_MCR_DTR : m_serialPort->DSRStatus();
  if ((bool)(m_MSR & PC8250_MSR_DSR) != value) {
    m_MSR   ^= PC8250_MSR_DSR;                // invert DSR
    m_MSR   |= PC8250_MSR_DDSR;               // set DDSR
    m_trigs |= m_IER & PC8250_IER_MODEM_INT;  // set int triggered flag
  }
}


void PC8250::checkRIChangeLow()
{
  bool value = m_MCR & PC8250_MCR_LOOPBACK ? m_MCR & PC8250_MCR_OUT1 : m_serialPort->RIStatus();
  if ((bool)(m_MSR & PC8250_MSR_RI) != value) {
    m_MSR   ^= PC8250_MSR_RI;                // invert RI
    m_MSR   |= PC8250_MSR_TERI;               // set TERI
    m_trigs |= m_IER & PC8250_IER_MODEM_INT;  // set int triggered flag
  }
  //m_MSR = (m_MSR & ~PC8250_MSR_RI) | (value ? PC8250_MSR_RI : 0);
    
}


void PC8250::checkDCDChanged()
{
  bool value = m_MCR & PC8250_MCR_LOOPBACK ? m_MCR & PC8250_MCR_OUT2 : m_serialPort->DCDStatus();
  if ((bool)(m_MSR & PC8250_MSR_DCD) != value) {
    m_MSR   ^= PC8250_MSR_DCD;                // invert DCD
    m_MSR   |= PC8250_MSR_DDCD;               // set DDCD
    m_trigs |= m_IER & PC8250_IER_MODEM_INT;  // set int triggered flag
  }
}


void PC8250::tick()
{
  // check for interrupts?
  if (m_IER) {
  
    // check for received byte interrupt?
    if (m_IER & PC8250_IER_RX_INT)
      checkByteReceived();

    // check for line status error interrupt?
    if (m_IER & PC8250_IER_LINE_INT) {
      checkOverflowError();
      checkParityError();
      checkFramingError();
    }
    
    // check for MODEM changes interrupt?
    if (m_IER & PC8250_IER_MODEM_INT) {
      checkCTSChanged();
      checkDSRChanged();
      checkRIChangeLow();
      checkDCDChanged();
    }

    // set INTR pin? (callback)
    if (m_trigs) {
      if (m_interruptCallback(this, m_context))
        ;//printf("INTR\n");
    }
      
  }
}




};  // fabgl namespace
