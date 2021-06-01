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


#include "defs.h"
#include "HAL.h"
#include "BDOS.h"


#define DEFAULT_TERMINAL_TYPE TermType::ANSILegacy


// command processor (CLI)
class CCP {
public:
  CCP(HAL * hal, BDOS * bdos);
  ~CCP();

  void run();

private:

  bool internalCommand(uint16_t cmdlineAddr, size_t cmdlen, uint16_t tailAddr);

  bool iscmd(char const * cmd, size_t cmdlen, uint16_t cmdlineAddr);

  void consoleOutChar(char c);
  void consoleOut(char const * str, size_t maxChars = 0);
  void consoleOut(uint16_t str, size_t maxChars = 0);
  void consoleOutFmt(const char * format, ...);

  bool cmd_DIR(uint16_t paramsAddr);
  bool cmd_LS(uint16_t paramsAddr);
  bool cmd_ERASE(uint16_t paramsAddr);
  bool cmd_HELP(uint16_t paramsAddr);
  bool cmd_CD(uint16_t paramsAddr);
  bool cmd_RENAME(uint16_t paramsAddr);
  bool cmd_TYPE(uint16_t paramsAddr);
  bool cmd_PATH(uint16_t paramsAddr);
  bool cmd_MKDIR(uint16_t paramsAddr);
  bool cmd_RMDIR(uint16_t paramsAddr);
  bool cmd_COPY(uint16_t paramsAddr);
  bool cmd_TERM(uint16_t paramsAddr);
  bool cmd_INFO(uint16_t paramsAddr);
  bool cmd_REBOOT(uint16_t paramsAddr);
  bool cmd_EMU(uint16_t paramsAddr);
  bool cmd_KEYB(uint16_t paramsAddr);
  bool cmd_EXIT(uint16_t paramsAddr);
  bool cmd_DINFO(uint16_t paramsAddr);
  bool cmd_WIFISCAN(uint16_t paramsAddr);
  bool cmd_WIFI(uint16_t paramsAddr);
  bool cmd_PING(uint16_t paramsAddr);
  bool cmd_TELNET(uint16_t paramsAddr);
  bool cmd_FORMAT(uint16_t paramsAddr);

  bool wifiErrorMsg();

  HAL *              m_HAL;
  BDOS *             m_BDOS;
  bool               m_exitSystem;
  TermType           m_defaultTerminalType;
  TerminalController m_termCtrl;

};

