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


#include "i8042.h"



namespace fabgl {


// STATUS bits
#define STATUS_OBF        0x01
#define STATUS_IBF        0x02
#define STATUS_F0         0x04
#define STATUS_F1         0x08
#define STATUS_INH        0x10
#define STATUS_TX_TIMEOUT 0x20
#define STATUS_RX_TIMEOUT 0x40
#define STATUS_PARITY_ERR 0x80


i8042::i8042()
{
  m_mutex = xSemaphoreCreateMutex();
}


i8042::~i8042()
{
  vSemaphoreDelete(m_mutex);
}


void i8042::init(PIC8259 * pic8259)
{
  m_PIC8259 = pic8259;

  m_PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::NoVirtualKeys);
  m_keyboard = m_PS2Controller.keyboard();

  m_STATUS = STATUS_F0 | STATUS_INH;
  m_DBBOUT = 0;
  m_DBBIN  = 0;
}


uint8_t i8042::read(int address)
{
  AutoSemaphore autoSemaphore(m_mutex);
  switch (address) {

    // 0 = read 8042 output register (DBBOUT) and set OBF = 0
    case 0:
      m_STATUS &= ~STATUS_OBF;
      //printf("i8042.read(%02X) => %02X\n", address, m_DBBOUT);
      return m_DBBOUT;

    // 1 = read 8042 status register (STATUS)
    case 1:
      //printf("i8042.read(%02X) => %02X\n", address, m_STATUS);
      return m_STATUS;

    default:
      return 0;

  }
}


void i8042::write(int address, uint8_t value)
{
  AutoSemaphore autoSemaphore(m_mutex);

  switch (address) {

    // 0 = write 8042 input register (DBBIN), set F1 = 0 and IBF = 1
    case 0:
      //printf("i8042.write(%02X, %02X)\n", address, value);
      m_DBBIN = value;
      m_STATUS = (m_STATUS & ~STATUS_F1) | STATUS_IBF;
      break;

    // 1 = write 8042 input register (DBBIN), set F1 = 1 and IBF = 1
    case 1:
      //printf("i8042.write(%02X, %02X)\n", address, value);
      m_DBBIN = value;
      m_STATUS |= STATUS_F1 | STATUS_IBF;
      break;

  }
}


void i8042::tick()
{
  AutoSemaphore autoSemaphore(m_mutex);

  // something to receive?
  if ((m_STATUS & STATUS_OBF) == 0 && m_keyboard->scancodeAvailable()) {
    // transform "set 2" scancodes to "set 1"
    uint8_t scode = Keyboard::convScancodeSet2To1(m_keyboard->getNextScancode());  // "set 1" code (0xf0 doesn't change!)
    //printf("i8042:tick(), received scode = %02X\n", scode);
    m_DBBOUT = (m_DBBOUT == 0xf0 ? (0x80 | scode) : scode);
    if (scode != 0xf0) {
      m_STATUS |= STATUS_OBF;
      // IR1 (IRQ9) triggered when non break code or when code+break has been received
      m_PIC8259->signalInterrupt(1);
    }
  }

  // something to send?
  if (m_STATUS & STATUS_IBF) {
    //printf("i8042:tick(), send scode = %02X\n", m_DBBIN);
    m_STATUS &= ~(STATUS_IBF | STATUS_TX_TIMEOUT | STATUS_PARITY_ERR);
    m_keyboard->sendCommand(m_DBBIN);
    m_STATUS |= STATUS_PARITY_ERR * m_keyboard->parityError();
  }
}






} // namespace fabgl
