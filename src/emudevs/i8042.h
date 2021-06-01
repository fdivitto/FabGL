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


#pragma once

#include "fabgl.h"



namespace fabgl {


// 8042 PS/2 Keyboard Controller. Actually it is emulated how it is seen in the IBM AT

class i8042 {

public:

  typedef bool (*InterruptCallback)(void * context);

  i8042();
  ~i8042();

  void init();

  void setCallbacks(void * context, InterruptCallback keyboardInterrupt, InterruptCallback mouseInterrupt) {
    m_context           = context;
    m_keyboardInterrupt = keyboardInterrupt;
    m_mouseInterrupt    = mouseInterrupt;
  }

  void tick();

  uint8_t read(int address);
  void write(int address, uint8_t value);

  Keyboard * keyboard()  { return m_keyboard; }
  Mouse * mouse()        { return m_mouse; }

  void enableMouse(bool value);

private:

  void execCommand();
  void updateCommandByte(uint8_t newValue);
  bool trigKeyboardInterrupt();
  bool trigMouseInterrupt();

  PS2Controller     m_PS2Controller;
  Keyboard *        m_keyboard;
  Mouse *           m_mouse;

  void *            m_context;
  InterruptCallback m_keyboardInterrupt;
  InterruptCallback m_mouseInterrupt;

  uint8_t           m_STATUS;
  uint8_t           m_DBBOUT;
  uint8_t           m_DBBIN;
  uint8_t           m_commandByte;
  bool              m_writeToMouse; // if True next byte on port 0 (0x60) is transferred to mouse port
  MousePacket       m_mousePacket;
  int               m_mousePacketIdx;

  // used when a command requires a parameter
  uint8_t           m_executingCommand; // 0 = none

  SemaphoreHandle_t m_mutex;

  int               m_mouseIntTrigs;
  int               m_keybIntTrigs;

};











} // namespace fabgl
