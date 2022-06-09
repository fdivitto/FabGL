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
 * @brief This file contains fabgl::SerialPort definition.
 */


#include "freertos/FreeRTOS.h"

#include "soc/uart_struct.h"

#ifdef ARDUINO
  #include "Arduino.h"
  #include "Stream.h"
#endif

#include "fabutils.h"
#include "fabglconf.h"


namespace fabgl {



/** \ingroup Enumerations
 * @brief This enum defines various serial port flow control methods
 */
enum class FlowControl {
  None,              /**< No flow control */
  Software,          /**< Software flow control. Use XON and XOFF control characters */
  Hardware,          /**< Hardware flow control. Use RTS and CTS signals */
  Hardsoft,          /**< Hardware/software flow control. Use XON/XOFF and RTS/CTS */
};



class SerialPort {
public:

  typedef void (*RXCallback)(void * args, uint8_t value, bool fromISR);
  typedef bool (*RXReadyCallback)(void * args, bool fromISR);

  SerialPort();
  
  void setCallbacks(void * args, RXReadyCallback rxReadyCallback, RXCallback rxCallback);
  
  void connect(int uartIndex, uint32_t baud, int dataLength, char parity, float stopBits, int rxPin, int txPin, FlowControl flowControl, bool inverted, int rtsPin, int ctsPin);

  void setRTSStatus(bool value);
  
  void flowControl(bool enableRX);
  
  bool readyToSend();
  
  void updateFlowControlStatus();
  
  bool initialized() { return m_initialized; }
  
  bool CTSStatus()   { return m_ctsPin != GPIO_UNUSED ? gpio_get_level(m_ctsPin) == 0 : false; }
  
  bool RTSStatus()                     { return m_RTSStatus; }
  
  bool XOFFStatus()                    { return m_sentXOFF; }
  
  void send(uint8_t value);
  

private:

  int uartGetRXFIFOCount();
  void uartFlushTXFIFO();
  void uartFlushRXFIFO();
  static void IRAM_ATTR uart_isr(void * arg);
  //static void uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

  bool                      m_initialized;
  int                       m_idx;
  volatile uart_dev_t *     m_dev;
  
  // hardware flow pins
  gpio_num_t                m_rtsPin;
  gpio_num_t                m_ctsPin;
  bool                      m_RTSStatus;      // true = asserted (low)
  
  volatile FlowControl      m_flowControl;
  volatile bool             m_sentXOFF;       // true if XOFF has been sent or RTS is disabled (high)
  volatile bool             m_recvXOFF;       // true if XOFF has been received
  
  void *                    m_callbackArgs;
  RXReadyCallback           m_rxReadyCallback;
  RXCallback                m_rxCallback;

};




} // end of namespace


