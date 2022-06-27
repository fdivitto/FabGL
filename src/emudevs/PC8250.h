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


#pragma once


/**
 * @file
 *
 * @brief This file contains fabgl::PC8250 definition.
 */



#include <stdlib.h>
#include <stdint.h>

#include "fabgl.h"



#define PC8250_DEFAULT_FREQ_HZ 1843200



namespace fabgl {


// PC8250 Universal Asynchronous Receiver/Transmitter with FIFOs

class PC8250 {
public:

  typedef bool (*InterruptCallback)(PC8250 * source, void * context);
  
  PC8250(int freq = PC8250_DEFAULT_FREQ_HZ);
  ~PC8250();
  
  void setCallbacks(void * context, InterruptCallback interruptCallback) {
    m_context           = context;
    m_interruptCallback = interruptCallback;
  }
  
  void setSerialPort(SerialPort * value);
  
  bool assigned()            { return m_serialPort != nullptr; }
  
  void reset();
  
  void tick();

  void write(int reg, uint8_t value);
  
  uint8_t read(int reg);

  bool getOut1();
  
  bool getOut2();


private:

  static void rxCallback(void * args, uint8_t value, bool fromISR);
  static bool rxReadyCallback(void * args, bool fromISR);

  void trigInterrupt(bool value);
  void setBaud();
  void setFrame();
  
  // line checks
  void checkOverflowError();
  void checkParityError();
  void checkFramingError();
  void checkBreakDetected();
  
  // rx check
  void checkByteReceived();
  
  // MODEM checks
  void checkCTSChanged();
  void checkDSRChanged();
  void checkRIChangeLow();
  void checkDCDChanged();
    
    
  int32_t           m_freq;
  
  SerialPort *      m_serialPort;
  
  // callbacks
  void *            m_context;
  InterruptCallback m_interruptCallback;
  
  // Transmitter Holding Register (THR)
  uint8_t           m_THR;
  
  // Interrupt Identification Register (IIR)
  uint8_t           m_IIR;
  
  // Interrupt Enable Register (IER)
  //   0 = Receiver Data Available interrupt enabled
  //   1 = Transmitter Holding Register Empty interrupt enabled
  //   2 = Receiver Line Status interrupt enabled
  //   3 = MODEM Status interrupt enabled
  uint8_t           m_IER;
  
  // interrupt triggered flags (not an actual 8250 register)
  //   0 = Receiver Data Available interrupt triggered
  //   1 = Transmitter Holding Register Empty interrupt triggered
  //   2 = Receiver Line Status interrupt triggered
  //   3 = MODEM Status interrupt triggered
  uint8_t           m_trigs;
  
  // Line Control Register (LCR):
  //   0, 1 = number of bits: 00 = 5 bits, 01 = 6 bits, 10 = 7 bits, 11 = 8 bits
  //   2    = number of stop bits: 0 = 1 stop bit, 1 = if 5 bits then 1.5 stop bits, 2 stop bits otherwise
  //   3    = parity enable: 1 = parity enabled
  //   4    = parity type: 0 = odd, 1 = even
  //   5    = stick parity (UNSUPPORTED)
  //   6    = send break
  //   7    = Divisor Latch Access Bit (DLAB)
  uint8_t           m_LCR;
  
  // MODEM Control Register (MCR)
  //   0 = DTR out (connected to DSR in loopback mode)
  //   1 = RTS out (connected to CTS in loopback mode)
  //   2 = OUT1 out (connected to RI in loopback mode)
  //   3 = OUT2 out (connected to DCD in loopback mode)
  //   4 = loopback mode (1 = enabled)
  uint8_t           m_MCR;
  
  // Line Status Register (LSR)
  //   0 = Data Ready (DR), 1 = data available
  //   1 = Overrun Error (OE), 1 = error
  //   2 = Parity Error (PE), 1 = error
  //   3 = Framing Error (FE), 1 = error
  //   4 = Break Interrupt (BI), 1 = break received
  //   5 = Transmitter Holding Register Empty (THRE), 1 = transmitter holding register empty
  //   6 = Transmitter Empty indicator (TEMT), 1 = transmitter shift register empty
  uint8_t           m_LSR;
  
  // MODEM Status Register (MSR)
  //   0 = Delta CTS (DCTS), 1 = CTS changed since last read
  //   1 = Delta DSR (DDSR), 1 = DSR changed since last read
  //   2 = Trailing Edge of RI (TERI), 1 = RI has changed from hi to low
  //   3 = Delta DCD (DDCD), 1 = DCD changed since last read
  //   4 = CTS in status (connected to RTS in loopback mode)
  //   5 = DSR in status (connected to DTR in loopback mode)
  //   6 = RI in status (connected to OUT1 in loopback mode)
  //   7 = DCD in status (connected to OUT2 in loopback mode)
  uint8_t           m_MSR;
  
  // Scratch Register (SCR)
  uint8_t           m_SCR;
  
  // Divisor Latch low byte (DLL)
  uint8_t           m_DLL;
  
  // Divisor Latch high byte (DLM)
  uint8_t           m_DLM;
  
  // bytes lost flag (not an actual 8250 register)
  volatile bool     m_overrun;
  
  // a byte has been received
  volatile bool     m_dataReady;
  volatile uint8_t  m_dataRecv;
  
};





};  // fabgl namespace
