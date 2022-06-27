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
 
 
#if __has_include("esp32/rom/uart.h")
  #include "esp32/rom/uart.h"
#else
  #include "rom/uart.h"
#endif
#include "soc/uart_reg.h"
#include "soc/uart_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"
#include "soc/rtc.h"
#include "esp_intr_alloc.h"
 
#include "serialport.h"
 
 
 
 
namespace fabgl {



static const int URXD_IN_IDX[]    = { U0RXD_IN_IDX, U1RXD_IN_IDX, U2RXD_IN_IDX };
static const int UTXD_OUT_IDX[]   = { U0TXD_OUT_IDX, U1TXD_OUT_IDX, U2TXD_OUT_IDX };
static const int INTR_SRC[]       = { ETS_UART0_INTR_SOURCE, ETS_UART1_INTR_SOURCE, ETS_UART2_INTR_SOURCE };
static const uint32_t UART_BASE[] = { DR_REG_UART_BASE, DR_REG_UART1_BASE, DR_REG_UART2_BASE };



SerialPort::SerialPort()
  : m_initialized(false),
    m_RTSStatus(true),
    m_DTRStatus(true),
    m_flowControl(FlowControl::None),
    m_sentXOFF(false),
    m_recvXOFF(false),
    m_callbackArgs(nullptr),
    m_rxReadyCallback(nullptr),
    m_rxCallback(nullptr),
    m_lineStatusCallback(nullptr),
    m_parityError(false),
    m_framingError(false),
    m_overflowError(false),
    m_breakDetected(false)
{
}


void SerialPort::setCallbacks(void * args, RXReadyCallback rxReadyCallback, RXCallback rxCallback, LineStatusCallback lineStatusCallback)
{
  m_callbackArgs       = args;
  m_rxReadyCallback    = rxReadyCallback;
  m_rxCallback         = rxCallback;
  m_lineStatusCallback = lineStatusCallback;
}


// returns number of bytes received (in the UART rx fifo buffer)
int SerialPort::uartGetRXFIFOCount()
{
  return m_dev->status.rxfifo_cnt | ((int)(m_dev->mem_cnt_status.rx_cnt) << 8);
}


// flushes TX buffer of UART
void SerialPort::uartFlushTXFIFO()
{
  while (m_dev->status.txfifo_cnt || m_dev->status.st_utx_out)
    ;
}


// flushes RX buffer of UART
void SerialPort::uartFlushRXFIFO()
{
  while (uartGetRXFIFOCount() != 0 || m_dev->mem_rx_status.wr_addr != m_dev->mem_rx_status.rd_addr)
    m_dev->fifo.rw_byte;
}


void SerialPort::setRTSStatus(bool value)
{
  if (m_rtsPin != GPIO_UNUSED) {
    m_RTSStatus = value;
    gpio_set_level(m_rtsPin, !value); // low = asserted
  }
}


void SerialPort::setDTRStatus(bool value)
{
  if (m_dtrPin != GPIO_UNUSED) {
    m_DTRStatus = value;
    gpio_set_level(m_dtrPin, !value); // low = asserted
  }
}


void SerialPort::setSignals(int rxPin, int txPin, int rtsPin, int ctsPin, int dtrPin, int dsrPin, int dcdPin, int riPin)
{
  // RX (in)
  m_rxPin = int2gpio(rxPin);
  configureGPIO(m_rxPin, GPIO_MODE_INPUT);
  
  // TX (out)
  m_txPin = int2gpio(txPin);
  configureGPIO(m_txPin, GPIO_MODE_OUTPUT);

  // RTS (out)
  m_rtsPin = int2gpio(rtsPin);
  if (m_rtsPin != GPIO_UNUSED) {
    configureGPIO(m_rtsPin, GPIO_MODE_OUTPUT);
    setRTSStatus(true); // assert RTS
  }

  // DTR (out)
  m_dtrPin = int2gpio(dtrPin);
  if (m_dtrPin != GPIO_UNUSED) {
    configureGPIO(m_dtrPin, GPIO_MODE_OUTPUT);
    setDTRStatus(true); // assert DTR
  }

  // CTS (in)
  m_ctsPin = int2gpio(ctsPin);
  if (m_ctsPin != GPIO_UNUSED) {
    configureGPIO(m_ctsPin, GPIO_MODE_INPUT);
  }

  // DSR (in)
  m_dsrPin = int2gpio(dsrPin);
  if (m_dsrPin != GPIO_UNUSED) {
    configureGPIO(m_dsrPin, GPIO_MODE_INPUT);
  }

  // DCD (in)
  m_dcdPin = int2gpio(dcdPin);
  if (m_dcdPin != GPIO_UNUSED) {
    configureGPIO(m_dcdPin, GPIO_MODE_INPUT);
  }

  // RI (in)
  m_riPin = int2gpio(riPin);
  if (m_riPin != GPIO_UNUSED) {
    configureGPIO(m_riPin, GPIO_MODE_INPUT);
  }
}


// uart = 0, 1, 2
void SerialPort::setup(int uartIndex, uint32_t baud, int dataLength, char parity, float stopBits, FlowControl flowControl, bool inverted)
{
  if (!m_initialized) {
    // uart not configured, configure now

    m_idx = uartIndex;
    m_dev = (volatile uart_dev_t *) UART_BASE[m_idx];
    
    switch (m_idx) {
      case 0:
        #ifdef ARDUINO
        Serial.end();
        #endif
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART_RST);
        break;
      case 1:
        #ifdef ARDUINO
        Serial1.end();
        #endif
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART1_RST);
        break;
      case 2:
        #ifdef ARDUINO
        Serial2.end();
        #endif
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);
        break;
    }
    
    // flush
    uartFlushTXFIFO();
    uartFlushRXFIFO();

    // setup RX interrupt
    WRITE_PERI_REG(UART_CONF1_REG(m_idx), (1 << UART_RXFIFO_FULL_THRHD_S) |      // an interrupt for each character received
                                          (2 << UART_RX_TOUT_THRHD_S)     |      // actually not used
                                          (0 << UART_RX_TOUT_EN_S));             // timeout not enabled
    WRITE_PERI_REG(UART_INT_ENA_REG(m_idx), (1 << UART_RXFIFO_FULL_INT_ENA_S) |  // interrupt on FIFO full (1 character - see rxfifo_full_thrhd)
                                            (1 << UART_FRM_ERR_INT_ENA_S)     |  // interrupt on frame error
                                            (0 << UART_RXFIFO_TOUT_INT_ENA_S) |  // no interrupt on rx timeout (see rx_tout_en and rx_tout_thrhd)
                                            (1 << UART_PARITY_ERR_INT_ENA_S)  |  // interrupt on rx parity error
                                            (1 << UART_RXFIFO_OVF_INT_ENA_S)  |  // interrupt on rx overflow
                                            (1 << UART_BRK_DET_INT_ENA_S));        // interrupt on Break
    WRITE_PERI_REG(UART_INT_CLR_REG(m_idx), 0xffffffff);
    esp_intr_alloc_pinnedToCore(INTR_SRC[m_idx], 0, uart_isr, this, nullptr, CoreUsage::quietCore());

    // setup FIFOs size
    WRITE_PERI_REG(UART_MEM_CONF_REG(m_idx), (3 << UART_RX_SIZE_S) |    // RX: 3 * 128 = 384 bytes (this is the max for UART2)
                                             (1 << UART_TX_SIZE_S));    // TX: 1 * 128 = 128 bytes
  }

  m_flowControl = flowControl;
  
  m_inverted    = inverted;

  // baud rate
  setBaud(baud);

  // frame
  setFrame(dataLength, parity, stopBits);

  // TX/RX Pin logic
  gpio_matrix_in(m_rxPin, URXD_IN_IDX[m_idx], m_inverted);
  gpio_matrix_out(m_txPin, UTXD_OUT_IDX[m_idx], m_inverted, false);

  // Flow Control
  WRITE_PERI_REG(UART_FLOW_CONF_REG(m_idx), 0);
  
  m_initialized = true;

  // APB Change callback (TODO?)
  //addApbChangeCallback(this, uart_on_apb_change);
}


void SerialPort::setBaud(int value)
{
  uint32_t clk_div = (getApbFrequency() << 4) / value;
  WRITE_PERI_REG(UART_CLKDIV_REG(m_idx), ((clk_div >> 4)  << UART_CLKDIV_S) |
                                         ((clk_div & 0xf) << UART_CLKDIV_FRAG_S));
}


void SerialPort::setFrame(int dataLength, char parity, float stopBits)
{
  uint32_t config0 = (1 << UART_TICK_REF_ALWAYS_ON_S) | ((dataLength - 5) << UART_BIT_NUM_S);
  if (parity == 'E')
    config0 |= (1 << UART_PARITY_EN_S);
  else if (parity == 'O')
    config0 |= (1 << UART_PARITY_EN_S) | (1 << UART_PARITY_S);
  if (stopBits == 1.0)
    config0 |= 1 << UART_STOP_BIT_NUM_S;
  else if (stopBits == 1.5)
    config0 |= 2 << UART_STOP_BIT_NUM_S;
  else if (stopBits >= 2.0) {
    config0 |= 1 << UART_STOP_BIT_NUM_S;
    SET_PERI_REG_BITS(UART_RS485_CONF_REG(m_idx), UART_DL1_EN_V, 1, UART_DL1_EN_S); // additional 1 stop bit
    if (stopBits >= 3.0)
      SET_PERI_REG_BITS(UART_RS485_CONF_REG(m_idx), UART_DL0_EN_V, 1, UART_DL1_EN_S); // additional 1 stop bit
  }
  WRITE_PERI_REG(UART_CONF0_REG(m_idx), config0);
}


// enable/disable RX sending XON/XOFF and/or setting RTS
void SerialPort::flowControl(bool enableRX)
{
  bool isrEnabled = m_dev->int_ena.rxfifo_full;
  CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(m_idx), UART_RXFIFO_FULL_INT_ENA_M);
  
  if (enableRX) {
    // resume RX
    if (m_sentXOFF) {
      if (m_flowControl == FlowControl::Software || m_flowControl == FlowControl::Hardsoft)
        send(ASCII_XON);
      if (m_flowControl == FlowControl::Hardware || m_flowControl == FlowControl::Hardsoft)
        setRTSStatus(true);            // assert RTS
      isrEnabled = true;
      m_sentXOFF = false;
    }
  } else {
    // suspend RX
    if (!m_sentXOFF) {
      if (m_flowControl == FlowControl::Software || m_flowControl == FlowControl::Hardsoft)
        send(ASCII_XOFF);
      if (m_flowControl == FlowControl::Hardware || m_flowControl == FlowControl::Hardsoft)
        setRTSStatus(false);           // disable RTS
      // note: here we don't disable FIFO interrupts. These are disabled in ISR when input queue is full
      m_sentXOFF = true;
    }
  }
  
  if (isrEnabled)
    SET_PERI_REG_MASK(UART_INT_ENA_REG(m_idx), UART_RXFIFO_FULL_INT_ENA_M); // enable RX FIFO full interrupt
}


// check if TX is enabled looking for XOFF received or reading CTS
bool SerialPort::readyToSend()
{
  //Serial.printf("readyToSend m_recvXOFF=%d\n", m_recvXOFF);
  if ((m_flowControl == FlowControl::Software || m_flowControl == FlowControl::Hardsoft) && m_recvXOFF)
    return false; // TX disabled (received XOFF)
  if ((m_flowControl == FlowControl::Hardware || m_flowControl == FlowControl::Hardsoft) && CTSStatus() == false)
    return false; // TX disabled (CTS=high, not active)
  return true;  // TX enabled
}


void SerialPort::send(uint8_t value)
{
  while (m_dev->status.txfifo_cnt == 0x7F)
    ;
  WRITE_PERI_REG(UART_FIFO_AHB_REG(m_idx), value);
}


void SerialPort::sendBreak(bool value)
{
  constexpr int MATRIX_DETACH_OUT_SIG = 0x100;

  while (m_dev->status.txfifo_cnt == 0x7F)
    ;
  if (value) {
    gpio_matrix_out(m_txPin, MATRIX_DETACH_OUT_SIG, m_inverted, false);
    configureGPIO(m_txPin, GPIO_MODE_OUTPUT);
    gpio_set_level(m_txPin, 0);
  } else {
    gpio_matrix_out(m_txPin, UTXD_OUT_IDX[m_idx], m_inverted, false);
  }
}


void IRAM_ATTR SerialPort::uart_isr(void * arg)
{
  auto ser = (SerialPort*) arg;
  auto dev = ser->m_dev;
  auto idx = ser->m_idx;

  // look for overflow or RX errors
  if (dev->int_st.rxfifo_ovf || dev->int_st.frm_err || dev->int_st.parity_err) {
    // save (or) error states
    ser->m_parityError   |= dev->int_st.parity_err;
    ser->m_framingError  |= dev->int_st.frm_err;
    ser->m_overflowError |= dev->int_st.rxfifo_ovf;
    if (ser->m_lineStatusCallback)
      ser->m_lineStatusCallback(ser->m_callbackArgs, ser->m_parityError, ser->m_framingError, ser->m_overflowError, ser->m_breakDetected, true);
    // reset RX-FIFO, because hardware bug rxfifo_rst cannot be used, so just flush
    ser->uartFlushRXFIFO();
    // clear interrupt flags
    SET_PERI_REG_MASK(UART_INT_CLR_REG(idx), UART_RXFIFO_OVF_INT_CLR_M | UART_FRM_ERR_INT_CLR_M | UART_PARITY_ERR_INT_CLR_M);
    return;
  }
  
  // Break detection
  if (dev->int_st.brk_det) {
    // save (or) break state
    ser->m_breakDetected |= dev->int_st.brk_det;
    if (ser->m_lineStatusCallback)
      ser->m_lineStatusCallback(ser->m_callbackArgs, ser->m_parityError, ser->m_framingError, ser->m_overflowError, ser->m_breakDetected, true);
    // clear interrupt flags
    SET_PERI_REG_MASK(UART_INT_CLR_REG(idx), UART_BRK_DET_INT_CLR_M);
    return;
  }

  // flow control?
  if (ser->m_flowControl != FlowControl::None) {
    // send XOFF/XON or set RTS looking at RX FIFO occupation
    int count = ser->uartGetRXFIFOCount();
    if (count > FABGLIB_TERMINAL_FLOWCONTROL_RXFIFO_MAX_THRESHOLD)
      ser->flowControl(false); // suspend RX
    else if (count < FABGLIB_TERMINAL_FLOWCONTROL_RXFIFO_MIN_THRESHOLD)
      ser->flowControl(true);  // resume RX
  }

  // main receive loop
  while (ser->uartGetRXFIFOCount() != 0 || dev->mem_rx_status.wr_addr != dev->mem_rx_status.rd_addr) {
    // look for enough room in input queue
    if (ser->m_flowControl != FlowControl::None && !ser->m_rxReadyCallback(ser->m_callbackArgs, true)) {
      ser->flowControl(false);  // suspend RX
      // block further interrupts
      CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(idx), UART_RXFIFO_FULL_INT_ENA_M);
      break;
    }
    // get char
    uint8_t r = READ_PERI_REG(UART_FIFO_REG(idx));
    // XON/XOFF?
    if ((r == ASCII_XOFF || r == ASCII_XON) && (ser->m_flowControl == FlowControl::Software || ser->m_flowControl == FlowControl::Hardsoft)) {
      ser->m_recvXOFF = (r == ASCII_XOFF);
    } else {
      // add to input queue
      ser->m_rxCallback(ser->m_callbackArgs, r, true);
    }
  }

  // clear interrupt flag
  SET_PERI_REG_MASK(UART_INT_CLR_REG(idx), UART_RXFIFO_FULL_INT_CLR_M);
}



////////////////////////////////////////////////////////////////////////////////////////////////
// SerialPortTerminalConnector


SerialPortTerminalConnector::SerialPortTerminalConnector()
  : m_terminal(nullptr),
    m_serialPort(nullptr),
    m_serialPortRXEnabled(true)
{
}


SerialPortTerminalConnector::SerialPortTerminalConnector(SerialPort * serialPort, Terminal * terminal)
  : SerialPortTerminalConnector()
{
  connect(serialPort, terminal);
}


void SerialPortTerminalConnector::connect(SerialPort * serialPort, Terminal * terminal)
{
  m_serialPort = serialPort;
  m_serialPort->setCallbacks(this, rxReadyCallback, rxCallback);
  
  m_terminal = terminal;
  m_terminal->connectKeyboard();
  
  m_terminal->onReceive = [&](uint8_t c) {
    // resume RX?
    if (!m_serialPort->readyToReceive() && m_terminal->availableForWrite())
      m_serialPort->flowControl(true);
  };
  
  m_terminal->onReadyToSend = [&](bool * readyToSend) {
    // suspend/resume TX?
    if (*readyToSend)
      *readyToSend = m_serialPort->readyToSend();
  };
  
  m_terminal->onSend = [&](uint8_t c) {
    m_serialPort->send(c);
  };
}


void SerialPortTerminalConnector::rxCallback(void * args, uint8_t value, bool fromISR)
{
  auto obj = (SerialPortTerminalConnector *) args;
  if (obj->m_serialPortRXEnabled)
    obj->m_terminal->write(value, fromISR);
}


bool SerialPortTerminalConnector::rxReadyCallback(void * args, bool fromISR)
{
  auto obj = (SerialPortTerminalConnector *) args;
  return obj->m_terminal->availableForWrite(fromISR) > 0;
}


} // end of namespace


