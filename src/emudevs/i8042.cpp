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


#include "i8042.h"



namespace fabgl {


// Controller status bits
#define STATUS_OBF                            0x01  // 0 : Output Buffer Full (0 = output buffer empty)
#define STATUS_IBF                            0x02  // 1 : Input Buffer Full (0 = input buffer empty)
#define STATUS_SYSFLAG                        0x04  // 2 : 0 = power on reset, 1 = diagnostic ok
#define STATUS_CMD                            0x08  // 3 : Command or Data, 0 = write to port 0 (0x60), 1 = write to port 1 (0x64)
#define STATUS_INH                            0x10  // 4 : Inhibit Switch, 0 = Keyboard inhibited, 1 = Keyboard not inhibited
#define STATUS_AOBF                           0x20  // 5 : Auxiliary Output Buffer Full, 0 = keyboard data, 1 = mouse data
#define STATUS_TIMEOUT                        0x40  // 6 : 1 = Timeout Error
#define STATUS_PARITY_ERR                     0x80  // 7 : 1 = Parity Error


// Controller commands
#define CTRLCMD_NONE                          0x00
#define CTRLCMD_GET_COMMAND_BYTE              0x20
#define CTRLCMD_READ_CONTROLLER_RAM_BEGIN     0x21
#define CTRLCMD_READ_CONTROLLER_RAM_END       0x3f
#define CTRLCMD_WRITE_COMMAND_BYTE            0x60
#define CTRLCMD_WRITE_CONTROLLER_RAM_BEGIN    0x61
#define CTRLCMD_WRITE_CONTROLLER_RAM_END      0x7f
#define CTRLCMD_DISABLE_MOUSE_PORT            0xa7
#define CTRLCMD_ENABLE_MOUSE_PORT             0xa8
#define CTRLCMD_TEST_MOUSE_PORT               0xa9
#define CTRLCMD_SELF_TEST                     0xaa
#define CTRLCMD_TEST_KEYBOARD_PORT            0xab
#define CTRLCMD_DISABLE_KEYBOARD              0xad
#define CTRLCMD_ENABLE_KEYBOARD               0xae
#define CTRLCMD_READ_INPUT_PORT               0xc0
#define CTRLCMD_READ_OUTPUT_PORT              0xd0
#define CTRLCMD_WRITE_OUTPUT_PORT             0xd1
#define CTRLCMD_WRITE_KEYBOARD_OUTPUT_BUFFER  0xd2
#define CTRLCMD_WRITE_MOUSE_OUTPUT_BUFFER     0xd3
#define CTRLCMD_WRITE_TO_MOUSE                0xd4
#define CTRLCMD_SYSTEM_RESET                  0xfe


// Command byte bits
#define CMDBYTE_ENABLE_KEYBOARD_IRQ           0x01  // 0 : 1 = Keyboard output buffer full causes interrupt (IRQ 1)
#define CMDBYTE_ENABLE_MOUSE_IRQ              0x02  // 1 : 1 = Mouse output buffer full causes interrupt (IRQ 12)
#define CMDBYTE_SYSFLAG                       0x04  // 2 : 1 = System flag after successful controller self-test
#define CMDBYTE_UNUSED1                       0x08  // 3 : unused (must be 0)
#define CMDBYTE_DISABLE_KEYBOARD              0x10  // 4 : 1 = Disable keyboard by forcing the keyboard clock low
#define CMDBYTE_DISABLE_MOUSE                 0x20  // 5 : 1 = Disable mouse by forcing the mouse serial clock line low
#define CMDBYTE_STD_SCAN_CONVERSION           0x40  // 6 : 1 = Standard Scan conversion
#define CMDBYTE_UNUSED2                       0x80  // 7 : unused (must be 0)


i8042::i8042()
{
  m_mutex = xSemaphoreCreateMutex();
}


i8042::~i8042()
{
  vSemaphoreDelete(m_mutex);
}


void i8042::init()
{
  // because mouse is optional, don't re-try if it is not found (to speed-up boot)
  Mouse::quickCheckHardware();

  // keyboard configured on port 0, and optionally mouse on port 1
  m_PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::NoVirtualKeys);
  m_keyboard = m_PS2Controller.keyboard();
  m_mouse    = m_PS2Controller.mouse();

  m_STATUS      = STATUS_SYSFLAG | STATUS_INH;
  m_DBBOUT      = 0;
  m_DBBIN       = 0;
  m_commandByte = CMDBYTE_ENABLE_KEYBOARD_IRQ | CMDBYTE_ENABLE_MOUSE_IRQ | CMDBYTE_SYSFLAG | CMDBYTE_STD_SCAN_CONVERSION | CMDBYTE_DISABLE_MOUSE;

  m_executingCommand = CTRLCMD_NONE;
  m_writeToMouse     = false;
  m_mousePacketIdx   = -1;

  m_mouseIntTrigs = 0;
  m_keybIntTrigs  = 0;
}


uint8_t i8042::read(int address)
{
  AutoSemaphore autoSemaphore(m_mutex);
  switch (address) {

    // 0 = read 8042 output register (DBBOUT) and set OBF = 0 and AOBF = 0
    // this is port 0x60 as seen from CPU side
    case 0:
      m_STATUS &= ~(STATUS_OBF | STATUS_AOBF);
      //printf("i8042.read(%02X) => %02X\n", address, m_DBBOUT);
      return m_DBBOUT;

    // 1 = read 8042 status register (STATUS)
    // this is port 0x64 as seen from CPU side
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

    // 0 = write 8042 input register (DBBIN), set STATUS_CMD = 0 and STATUS_IBF = 1
    // this is port 0x60 as seen from CPU side
    case 0:
      //printf("i8042.write(%02X, %02X)\n", address, value);
      m_DBBIN = value;
      m_STATUS = (m_STATUS & ~STATUS_CMD) | STATUS_IBF;
      break;

    // 1 = write 8042 input register (DBBIN), set F1 = 1 and STATUS_IBF = 1
    // this is port 0x64 as seen from CPU side
    case 1:
      //printf("i8042.write(%02X, %02X)\n", address, value);
      m_DBBIN = value;
      m_STATUS |= STATUS_CMD | STATUS_IBF;
      break;

  }
}


void i8042::tick()
{
  AutoSemaphore autoSemaphore(m_mutex);

  // something to receive from keyboard?
  if ((m_STATUS & STATUS_OBF) == 0 && m_keyboard->scancodeAvailable()) {
    if (m_commandByte & CMDBYTE_STD_SCAN_CONVERSION) {
      // transform "set 2" scancodes to "set 1"
      uint8_t scode = Keyboard::convScancodeSet2To1(m_keyboard->getNextScancode());  // "set 1" code (0xf0 doesn't change!)
      m_DBBOUT = (m_DBBOUT == 0xf0 ? (0x80 | scode) : scode);
      if (scode != 0xf0) {
        m_STATUS |= STATUS_OBF;
        // IR1 (IRQ9) triggered when non break code or when code+break has been received
        ++m_keybIntTrigs;
      }
    } else {
      // no transform
      m_DBBOUT = m_keyboard->getNextScancode();
      m_STATUS |= STATUS_OBF;
      ++m_keybIntTrigs;
    }
  }

  // something to receive from mouse?
  if ((m_STATUS & STATUS_OBF) == 0 && (m_mousePacketIdx > -1 || m_mouse->packetAvailable())) {
    if (m_mousePacketIdx == -1)
      m_mouse->getNextPacket(&m_mousePacket);
    m_DBBOUT = m_mousePacket.data[++m_mousePacketIdx];
    if (m_mousePacketIdx == m_mouse->getPacketSize() - 1)
      m_mousePacketIdx = -1;
    m_STATUS |= STATUS_OBF | STATUS_AOBF;
    ++m_mouseIntTrigs;
  }

  // something to execute?
  if (m_STATUS & STATUS_CMD) {
    m_STATUS &= ~(STATUS_IBF | STATUS_CMD);
    execCommand();
  }

  // something to execute (with parameters)?
  if ((m_STATUS & STATUS_IBF) && m_executingCommand != CTRLCMD_NONE) {
    m_STATUS &= ~STATUS_IBF;
    execCommand();
  }

  // something to send?
  if (m_STATUS & STATUS_IBF) {
    m_STATUS &= ~(STATUS_IBF | STATUS_PARITY_ERR);
    if (m_writeToMouse)
      m_mouse->sendCommand(m_DBBIN);
    else
      m_keyboard->sendCommand(m_DBBIN);
    m_writeToMouse = false;
    m_STATUS |= STATUS_PARITY_ERR * m_keyboard->parityError();
  }

  // are there interrupts to trig?
  if (m_keybIntTrigs && trigKeyboardInterrupt())
    --m_keybIntTrigs;
  if (m_mouseIntTrigs && trigMouseInterrupt())
    --m_mouseIntTrigs;
}


void i8042::execCommand()
{
  uint8_t cmd = m_executingCommand == CTRLCMD_NONE ? m_DBBIN : m_executingCommand;

  switch (cmd) {

    case CTRLCMD_GET_COMMAND_BYTE:
      m_DBBOUT = m_commandByte;
      m_STATUS |= STATUS_OBF;
      break;

    case CTRLCMD_WRITE_COMMAND_BYTE:
      if (m_executingCommand) {
        // data received
        updateCommandByte(m_DBBIN);
        m_executingCommand = CTRLCMD_NONE;
      } else {
        // wait for data
        m_executingCommand = CTRLCMD_WRITE_COMMAND_BYTE;
      }
      break;

    case CTRLCMD_DISABLE_MOUSE_PORT:
      enableMouse(false);
      break;

    case CTRLCMD_ENABLE_MOUSE_PORT:
      enableMouse(true);
      break;

    case CTRLCMD_TEST_MOUSE_PORT:
      m_DBBOUT = m_mouse->isMouseAvailable() ? 0x00 : 0x02;
      m_STATUS |= STATUS_OBF;
      break;

    case CTRLCMD_SELF_TEST:
      m_DBBOUT = 0x55;  // no errors!
      m_STATUS |= STATUS_OBF;
      break;

    case CTRLCMD_TEST_KEYBOARD_PORT:
      m_DBBOUT = m_keyboard->isKeyboardAvailable() ? 0x00 : 0x02;
      m_STATUS |= STATUS_OBF;
      break;

    case CTRLCMD_DISABLE_KEYBOARD:
      updateCommandByte(m_commandByte | CMDBYTE_DISABLE_KEYBOARD);
      break;

    case CTRLCMD_ENABLE_KEYBOARD:
      updateCommandByte(m_commandByte & ~CMDBYTE_DISABLE_KEYBOARD);
      break;

    case CTRLCMD_WRITE_TO_MOUSE:
      m_writeToMouse = 1;
      break;

    case CTRLCMD_SYSTEM_RESET:
      esp_restart();
      break;

    default:
      printf("8042: unsupported controller command %02X\n", cmd);
      break;
  }
}


void i8042::enableMouse(bool value)
{
  AutoSemaphore autoSemaphore(m_mutex);
  updateCommandByte(value ? (m_commandByte & ~CMDBYTE_DISABLE_MOUSE) : (m_commandByte | CMDBYTE_DISABLE_MOUSE));
}


void i8042::updateCommandByte(uint8_t newValue)
{
  // disable keyboard bit changed?
  if ((newValue ^ m_commandByte) & CMDBYTE_DISABLE_KEYBOARD) {
    if (newValue & CMDBYTE_DISABLE_KEYBOARD) {
      m_keyboard->suspendPort();
    } else {
      m_keyboard->resumePort();
    }
  }

  // disable mouse bit changed?
  if ((newValue ^ m_commandByte) & CMDBYTE_DISABLE_MOUSE) {
    if (newValue & CMDBYTE_DISABLE_MOUSE) {
      m_mouse->suspendPort();
    } else {
      m_mouse->resumePort();
    }
  }

  m_commandByte = newValue;

}


bool i8042::trigKeyboardInterrupt()
{
  return m_commandByte & CMDBYTE_ENABLE_KEYBOARD_IRQ ? m_keyboardInterrupt(m_context) : true;
}


bool i8042::trigMouseInterrupt()
{
  return m_commandByte & CMDBYTE_ENABLE_MOUSE_IRQ ? m_mouseInterrupt(m_context) : true;
}





} // namespace fabgl
