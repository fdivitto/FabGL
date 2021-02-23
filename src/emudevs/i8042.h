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


#pragma once

#include "fabgl.h"
#include "emudevs/PIC8259.h"



namespace fabgl {


// 8042 PS/2 Keyboard Controller. Actually it is emulated how it is seen in the IBM AT

class i8042 {

public:

  i8042();
  ~i8042();

  void init(PIC8259 * pic8259);

  void tick();

  uint8_t read(int address);
  void write(int address, uint8_t value);

  Keyboard * keyboard() { return m_keyboard; }

private:

  PS2Controller     m_PS2Controller;
  Keyboard        * m_keyboard;
  PIC8259         * m_PIC8259;

  uint8_t           m_STATUS;
  uint8_t           m_DBBOUT;
  uint8_t           m_DBBIN;

  SemaphoreHandle_t m_mutex;

};











} // namespace fabgl
