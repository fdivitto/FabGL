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

#include "fabutils.h"
#include "fabglconf.h"
#include "terminal.h"


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


/**
 * @brief SerialPort class handles UART setup and communications
 */
class SerialPort {
public:

  typedef void (*RXCallback)(void * args, uint8_t value, bool fromISR);
  typedef bool (*RXReadyCallback)(void * args, bool fromISR);
  typedef void (*LineStatusCallback)(void * args, bool parityError, bool framingError, bool overflowError, bool breakDetected, bool fromISR);

  SerialPort();
  
  void setCallbacks(void * args, RXReadyCallback rxReadyCallback, RXCallback rxCallback, LineStatusCallback lineStatusCallback = nullptr);

  /**
   * @brief Associates GPIOs to serial port signals
   *
   * This method must be called before setup(). setSignals() can be called just one time.
   *
   * @param rxPin UART RX pin GPIO number.
   * @param txPin UART TX pin GPIO number.
   * @param rtsPin RTS signal GPIO number (-1 = not used)
   * @param ctsPin CTS signal GPIO number (-1 = not used)
   * @param dtrPin DTR signal GPIO number (-1 = not used)
   * @param dsrPin DSR signal GPIO number (-1 = not used)
   * @param dcdPin DCD signal GPIO number (-1 = not used)
   * @param riPin RI signal GPIO number (-1 = not used)
   *
   * Example:
   *
   *     serialPort.setSignals(34, 2);  // rx = GPIO 34, tx = GPIO 2
   *     serialPort.setup(2, 115200, 8, 'N', 1, FlowControl::Software);
   */
  void setSignals(int rxPin, int txPin, int rtsPin = -1, int ctsPin = -1, int dtrPin = -1, int dsrPin = -1, int dcdPin = -1, int riPin = -1);
  
  /**
   * @brief Configures and activates specified UART
   *
   * You may call this method whenever a parameters needs to be changed. setSignals() must be called before.
   *
   * @param uartIndex UART device to use (0, 1, 2)
   * @param baud Baud rate.
   * @param dataLength Data word length. 5 = 5 bits, 6 = 6 bits, 7 = 7 bits, 8 = 8 bits
   * @param parity Parity. 'N' = none, 'E' = even, 'O' = odd
   * @param stopBits Number of stop bits. 1 = 1 bit, 1.5 = 1.5 bits, 2 = 2 bits, 3 = 3 bits
   * @param flowControl Flow control.
   * @param inverted If true RX and TX signals are inverted.
   *
   * Example:
   *
   *     serialPort.setSignals(34, 2);  // rx = GPIO 34, tx = GPIO 2
   *     serialPort.setup(2, 115200, 8, 'N', 1, FlowControl::Software);
   */
  void setup(int uartIndex, uint32_t baud, int dataLength, char parity, float stopBits, FlowControl flowControl, bool inverted = false);
  
  /**
   * @brief Changes baud rate
   *
   * This method can be called after setup to change current baud rate
   *
   * @param value New baud rate
   */
  void setBaud(int value);
  
  /**
   * @brief Changes frame structure
   *
   * This method can be called after setup to change frame structure
   *
   * @param dataLength Data word length. 5 = 5 bits, 6 = 6 bits, 7 = 7 bits, 8 = 8 bits
   * @param parity Parity. 'N' = none, 'E' = even, 'O' = odd
   * @param stopBits Number of stop bits. 1 = 1 bit, 1.5 = 1.5 bits, 2 = 2 bits, 3 = 3 bits
   */
  void setFrame(int dataLength, char parity, float stopBits);
  
  /**
   * @brief Allows/disallows host to send data
   *
   * @param enableRX If True host can send data (sent XON and/or RTS asserted)
   */
  void flowControl(bool enableRX);
  
  /**
   * @brief Checks if TX is enabled looking for XOFF received or reading CTS
   *
   * @return True if sending is allowed
   */
  bool readyToSend();
  
  /**
   * @brief Reports whether RX is active
   *
   * @return False if XOFF has been sent or RTS is not asserted
   */
  bool readyToReceive()                { return !m_sentXOFF; }
  
  /**
   * @brief Checks if the object is initialized
   *
   * @return True if serial port has been initialized
   */
  bool initialized()                   { return m_initialized; }
  
  /**
   * @brief Reports current CTS signal status
   *
   * @return True if CTS is asserted (low voltage, host is ready to receive data)
   */
  bool CTSStatus()                     { return m_ctsPin != GPIO_UNUSED ? gpio_get_level(m_ctsPin) == 0 : true; }

  /**
   * @brief Sets RTS signal status
   *
   * RTS signal is handled automatically when flow control has been setup
   *
   * @param value True to assert RTS (low voltage)
   */
  void setRTSStatus(bool value);

  /**
   * @brief Reports current RTS signal status
   *
   * @return True if RTS is asserted (low voltage, terminal is ready to receive data)
   */
  bool RTSStatus()                     { return m_RTSStatus; }

  /**
   * @brief Sets DTR signal status
   *
   * @param value True to assert DTR (low voltage)
   */
  void setDTRStatus(bool value);

  /**
   * @brief Reports current DTR signal status
   *
   * @return True if DTR is asserted (low voltage)
   */
  bool DTRStatus()                     { return m_DTRStatus; }

  /**
   * @brief Reports current DSR signal status
   *
   * @return True if DSR is asserted (low voltage)
   */
  bool DSRStatus()                     { return m_dsrPin != GPIO_UNUSED ? gpio_get_level(m_dsrPin) == 0 : true; }

  /**
   * @brief Reports current DCD signal status
   *
   * @return True if DCD is asserted (low voltage)
   */
  bool DCDStatus()                     { return m_dcdPin != GPIO_UNUSED ? gpio_get_level(m_dcdPin) == 0 : true; }

  /**
   * @brief Reports current RI signal status
   *
   * @return True if RI is asserted (low voltage)
   */
  bool RIStatus()                      { return m_riPin != GPIO_UNUSED ? gpio_get_level(m_riPin) == 0 : false; }

  /**
   * @brief Sends a byte
   *
   * @param value Byte to send
   */
  void send(uint8_t value);
  
  /**
   * @brief Activates or disattivaes Break
   *
   * @param If True Break is active (keep TX low)
   */
  void sendBreak(bool value);
  
  /**
   * @brief Gets parity error status
   *
   * Calling this method resets the error.
   *
   * @return Parity error status
   */
  bool parityError()   { auto r = m_parityError; m_parityError = false; return r; }
  
  /**
   * @brief Gets framing error status
   *
   * Calling this method resets the error.
   *
   * @return Framing error status
   */
  bool framingError()  { auto r = m_framingError; m_framingError = false; return r; }
  
  /**
   * @brief Gets overflow error status
   *
   * Calling this method resets the error.
   *
   * @return Overflow error status
   */
  bool overflowError() { auto r = m_overflowError; m_overflowError = false; return r; }

  /**
   * @brief Gets Break detected flag status
   *
   * Calling this method resets the status to False.
   *
   * @return Break detected status
   */
  bool breakDetected() { auto r = m_breakDetected; m_breakDetected = false; return r; }


private:

  int uartGetRXFIFOCount();
  void uartFlushTXFIFO();
  void uartFlushRXFIFO();
  static void IRAM_ATTR uart_isr(void * arg);
  //static void uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

  bool                      m_initialized;
  int                       m_idx;
  volatile uart_dev_t *     m_dev;
  
  bool                      m_inverted;
  
  gpio_num_t                m_rxPin;
  gpio_num_t                m_txPin;
  gpio_num_t                m_rtsPin;
  gpio_num_t                m_ctsPin;
  gpio_num_t                m_dtrPin;
  gpio_num_t                m_dsrPin;
  gpio_num_t                m_dcdPin;
  gpio_num_t                m_riPin;
  
  bool                      m_RTSStatus;      // true = asserted (low)
  bool                      m_DTRStatus;      // true = asserted (low)
  
  volatile FlowControl      m_flowControl;
  volatile bool             m_sentXOFF;       // true if XOFF has been sent or RTS is disabled (high)
  volatile bool             m_recvXOFF;       // true if XOFF has been received
  
  void *                    m_callbackArgs;
  RXReadyCallback           m_rxReadyCallback;
  RXCallback                m_rxCallback;
  LineStatusCallback        m_lineStatusCallback;
  
  volatile bool             m_parityError;
  volatile bool             m_framingError;
  volatile bool             m_overflowError;
  volatile bool             m_breakDetected;

};



/**
 * @brief SerialPortTerminalConnector is an helper class used to connect Terminal and SerialPort
 */
class SerialPortTerminalConnector {
public:
  SerialPortTerminalConnector();
  SerialPortTerminalConnector(SerialPort * serialPort, Terminal * terminal);
  void connect(SerialPort * serialPort, Terminal * terminal);

  /**
   * @brief Disables/Enables serial port RX
   *
   * This method temporarily disables RX from serial port, discarding all incoming data.
   *
   * @param value If True RX is disabled. If False RX is re-enabled.
   */
  void disableSerialPortRX(bool value)         { m_serialPortRXEnabled = !value; }

private:
  static void rxCallback(void * args, uint8_t value, bool fromISR);
  static bool rxReadyCallback(void * args, bool fromISR);
  
  Terminal *    m_terminal;
  SerialPort *  m_serialPort;

  // if false all inputs from UART are discarded
  volatile bool m_serialPortRXEnabled;
};



} // end of namespace


