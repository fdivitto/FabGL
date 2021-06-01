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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabgl.h"
#include "HAL.h"
#include "BIOS.h"
#include "BDOS.h"
#include "CCP.h"


#define MAXSESSIONS 12


using fabgl::BaseDisplayController;


struct Session {
  int          id;
  TaskHandle_t thread;
  Terminal *   terminal;
  HAL *        hal;
};


class Supervisor {

public:

  Supervisor(BaseDisplayController * displayController);
  ~Supervisor();

  static Supervisor * instance() { return s_singleton; }

  void activateSession(int id);

  void abortSession(int id, AbortReason abortReason);

  int getSessionIDByTaskHandle(TaskHandle_t taskHandle);

  void waitTermination();

  int getOpenSessions();

  Delegate<HAL *> onNewSession;

private:

  static void sessionThread(void * arg);

  Terminal * createTerminal();

  static Supervisor *     s_singleton;

  BaseDisplayController * m_displayController;
  Session                 m_sessions[MAXSESSIONS];
  int                     m_activeSessionID;

};
