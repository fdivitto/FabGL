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
#define PC8250_LCR_SENDBREAK  0x40
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



#define PC8250_DEBUG 0


namespace fabgl {


PC8250::PC8250(int freq)
  : m_freq(freq),
    m_serialPort(nullptr)
{
}


PC8250::~PC8250()
{
}


void PC8250::reset()
{
  m_DLL       = 0;
  m_DLM       = 0;
  m_trigs     = 0;
  m_IER       = 0;
  m_LCR       = 0;
  m_MCR       = 0;
  m_LSR       = PC8250_LSR_THRE | PC8250_LSR_TEMT;
  m_MSR       = 0;
  m_SCR       = 0;
  m_IIR       = 1;
  m_overrun   = false;
  m_dataReady = false;
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
  
  obj->m_dataRecv = value;
  if (obj->m_dataReady)
    obj->m_overrun = true;
  obj->m_dataReady = true;
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
        
        #if PC8250_DEBUG
        printf("RD DLL %02X\n", ret);
        #endif
        
      } else {
      
        // Receiver Buffer Register (RBR)
        ret = m_MCR & PC8250_MCR_LOOPBACK ? m_THR : m_dataRecv;
        m_dataReady = false;
        m_LSR      &= ~PC8250_LSR_DR;       // reset DR flag
        m_trigs    &= ~PC8250_IER_RX_INT;   // reset interrupt triggered flag
        m_IIR       = 1;
        
        #if PC8250_DEBUG
        printf("RD RBR %02X\n", ret);
        #endif
        
      }
      break;
      
    // DLAB = 0 : Interrupt Enable Register (IER)
    // DLAB = 1 : Divisor Latch MSB Register (DLM)
    case 1:
      if (m_LCR & PC8250_LCR_DLAB) {
      
        // Divisor Latch MSB Register (DLM)
        ret = m_DLM;
        
        #if PC8250_DEBUG
        printf("RD DLM %02X\n", ret);
        #endif
        
      } else {
      
        // Interrupt Enable Register (IER)
        ret = m_IER;
        
        #if PC8250_DEBUG
        printf("RD IER %02X\n", ret);
        #endif
        
      }
      break;
      
    // Interrupt Identification Register (IIR)
    case 2:
      if (m_IIR == 1) {
        if (m_trigs & PC8250_IER_LINE_INT)
          m_IIR = 0b110;
        else if (m_trigs & PC8250_IER_RX_INT)
          m_IIR = 0b100;
        else if (m_trigs & PC8250_IER_TX_INT) {
          m_IIR = 0b010;
          m_trigs &= ~PC8250_IER_TX_INT;
        } else if (m_trigs & PC8250_IER_MODEM_INT)
          m_IIR = 0b000;
      } else if (!m_trigs)
        m_IIR = 1;
        
      ret = m_IIR;
        
      #if PC8250_DEBUG
      printf("RD IIR %02X\n", ret);
      #endif
      
      break;
        
    // Line Control Register (LCR)
    case 3:
      ret = m_LCR;
      
      #if PC8250_DEBUG
      printf("RD LCR %02X\n", ret);
      #endif
      
      break;
      
    // MODEM Control Register (MCR)
    case 4:
      ret = m_MCR;
      
      #if PC8250_DEBUG
      printf("RD MCR %02X (%s %s %s %s %s)\n", ret, ret & PC8250_MCR_DTR ? "DTR" : "", ret & PC8250_MCR_RTS ? "RTS" : "", ret & PC8250_MCR_OUT1 ? "OUT1(RI)" : "", ret & PC8250_MCR_OUT2 ? "OUT2(DCD)" : "", ret & PC8250_MCR_LOOPBACK ? "LOOPBACK" : "");
      #endif
      
      break;
      
    // Line Status Register (LSR)
    case 5:
      if ((m_MCR & PC8250_MCR_LOOPBACK) == 0) {
        checkByteReceived();
        checkOverflowError();
        checkParityError();
        checkFramingError();
        checkBreakDetected();
      }
      ret = m_LSR;
      // reset OE, PE, FE, BI flags
      m_LSR  &= ~(PC8250_LSR_OE | PC8250_LSR_PE | PC8250_LSR_FE | PC8250_LSR_BI);
      // reset line interrupt triggered flag
      m_trigs &= ~PC8250_IER_LINE_INT;
      m_IIR    = 1;
      
      #if PC8250_DEBUG
      printf("RD LSR %02X\n", ret);
      #endif
      
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
      m_IIR    = 1;
      
      #if PC8250_DEBUG
      printf("RD MSR %02X (%s %s %s %s %s %s %s %s)\n", ret, ret & PC8250_MSR_DCTS ? "DCTS" : "", ret & PC8250_MSR_DDSR ? "DDSR" : "", ret & PC8250_MSR_TERI ? "TERI" : "", ret & PC8250_MSR_DDCD ? "DDCD" : "", ret & PC8250_MSR_CTS ? "CTS" : "", ret & PC8250_MSR_DSR ? "DSR" : "", ret & PC8250_MSR_RI ? "RI(OUT1)" : "", ret & PC8250_MSR_DCD ? "DCD(OUT2)" : "");
      #endif
      
      break;
      
    // Scratch Register
    case 7:
      ret = m_SCR;
      
      #if PC8250_DEBUG
      printf("RD SCR %02X\n", ret);
      #endif
      
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
        
        #if PC8250_DEBUG
        printf("WR THR = %02X ", value);
        #endif
        
        if (m_MCR & PC8250_MCR_LOOPBACK) {
          // loopback mode
          
          #if PC8250_DEBUG
          printf("  loopback ");
          #endif
          
          m_THR = value;
          if (m_LSR & PC8250_LSR_DR) {
          
            #if PC8250_DEBUG
            printf("  overrun ");
            #endif
            
            m_overrun = true;
            
          }
          m_LSR   |= PC8250_LSR_DR;                // set DR flag
          m_trigs |= m_IER & PC8250_IER_RX_INT;    // set int triggered flag
        } else {
          // normal mode
          m_serialPort->send(value);
        }
        
        #if PC8250_DEBUG
        printf("\n");
        #endif
        
        m_trigs &= ~PC8250_IER_TX_INT;         // reset interrupt triggered flag
        m_IIR    = 1;
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
        m_trigs |= m_IER & PC8250_IER_TX_INT;
        
        #if PC8250_DEBUG
        printf("WR IER = %02X (%d ms)\n", value, (int)esp_timer_get_time()/1000);
        #endif
      }
      break;
      
    case 2:
      #if PC8250_DEBUG
      printf("WR IIR - UNSUPPORTED\n");
      #endif
      break;
      
    // Line Control Register (LCR)
    case 3:
      if ((m_LCR & PC8250_LCR_SENDBREAK) != (value & PC8250_LCR_SENDBREAK)) {
        // send break changed
        if (m_MCR & PC8250_MCR_LOOPBACK) {
          if (value & PC8250_LCR_SENDBREAK) {
            m_LSR   |= PC8250_LSR_BI | PC8250_LSR_DR;
            m_trigs |= m_IER & PC8250_IER_LINE_INT;
            m_THR    = 0;
          }
        } else {
          m_serialPort->sendBreak(value & PC8250_LCR_SENDBREAK);
        }
      }
      m_LCR = value;
      setFrame();
      
      #if PC8250_DEBUG
      printf("WR LCR = %02X (%d ms)\n", value, (int)esp_timer_get_time()/1000);
      if (m_LCR & PC8250_LCR_SENDBREAK) printf("  SEND BREAK\n");
      #endif
      
      break;
      
    // MODEM Control Register (MCR)
    case 4:
      m_MCR = value & 0x1f;
      if (m_MCR & PC8250_MCR_LOOPBACK) {
        checkCTSChanged();
        checkDSRChanged();
        checkRIChangeLow();
        checkDCDChanged();
      } else {
        m_serialPort->setDTRStatus(m_MCR & PC8250_MCR_DTR);
        m_serialPort->setRTSStatus(m_MCR & PC8250_MCR_RTS);
      }
      
      #if PC8250_DEBUG
      printf("WR MCR = %02X (%s %s %s %s %s)\n", value, value & PC8250_MCR_DTR ? "DTR" : "", value & PC8250_MCR_RTS ? "RTS" : "", value & PC8250_MCR_OUT1 ? "OUT1(RI)" : "", value & PC8250_MCR_OUT2 ? "OUT2(DCD)" : "", value & PC8250_MCR_LOOPBACK ? "LOOPBACK" : "");
      #endif
      
      break;
      
    case 5:
      #if PC8250_DEBUG
      printf("WR LSR - NOT SUPPORTED\n");
      #endif
      break;
    
    // MODEM Status Register (MSR)
    case 6:
      #if PC8250_DEBUG
      printf("WR MSR = %02X (%s %s %s %s %s %s %s %s)\n", value, value & PC8250_MSR_DCTS ? "DCTS" : "", value & PC8250_MSR_DDSR ? "DDSR" : "", value & PC8250_MSR_TERI ? "TERI" : "",value & PC8250_MSR_DDCD ? "DDCD" : "", value & PC8250_MSR_CTS ? "CTS" : "", value & PC8250_MSR_DSR ? "DSR" : "", value & PC8250_MSR_RI ? "RI(OUT1)" : "", value & PC8250_MSR_DCD ? "DCD(OUT2)" : "");
      #endif
      
      m_MSR = (m_MSR & 0xf0) | (value & 0x0f);
      if (m_MSR & 0x0f)
        m_trigs |= m_IER & PC8250_IER_MODEM_INT;
      break;
      
    // Scratch Register
    case 7:
      m_SCR = value;
      
      #if PC8250_DEBUG
      printf("WR SCR = %02X\n", value);
      #endif
      
      break;
  
  }
}


void PC8250::setBaud()
{
  int div  = m_DLL | ((int)m_DLM << 8);
  int baud = m_freq / 16 / (div ? div : 1);
  m_serialPort->setBaud(baud);
  
  #if PC8250_DEBUG
  printf("BAUD = %d\n", baud);
  #endif
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


void PC8250::checkBreakDetected()
{
  if (m_serialPort->breakDetected()) {
    m_LSR   |= PC8250_LSR_BI | PC8250_LSR_DR;
    m_trigs |= m_IER & PC8250_IER_LINE_INT;
    m_THR    = 0;
  }
}


void PC8250::checkByteReceived()
{
  if (m_dataReady && (m_MCR & PC8250_MCR_LOOPBACK) == 0) {
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
  if ((bool)(m_MSR & PC8250_MSR_RI) != value && !value) {
    m_MSR   &= ~PC8250_MSR_RI;                // reset RI
    m_MSR   |= PC8250_MSR_TERI;               // set TERI
    m_trigs |= m_IER & PC8250_IER_MODEM_INT;  // set int triggered flag
  }
  m_MSR = (m_MSR & ~PC8250_MSR_RI) | (value ? PC8250_MSR_RI : 0);
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

    // check for line status error or break interrupt?
    if (m_IER & PC8250_IER_LINE_INT) {
      checkOverflowError();
      checkParityError();
      checkFramingError();
      checkBreakDetected();
    }
    
    // check for MODEM changes interrupt?
    if ((m_IER & PC8250_IER_MODEM_INT) && !(m_MCR & PC8250_MCR_LOOPBACK)) {
      checkCTSChanged();
      checkDSRChanged();
      checkRIChangeLow();
      checkDCDChanged();
    }

    // set INTR pin? (callback)
    if (m_trigs && (m_LCR & PC8250_LCR_DLAB) == 0) {
      if (m_interruptCallback(this, m_context)) {
        #if PC8250_DEBUG
        printf("INTR\n");
        #endif
      }
    }
      
  }
}




};  // fabgl namespace
