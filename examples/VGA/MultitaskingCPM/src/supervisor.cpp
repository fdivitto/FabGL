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



#include "supervisor.h"


#pragma GCC optimize ("O2")


#define SESSIONTHREAD_STACK_SIZE    4500
#define SESSIONTHREAD_TASK_PRIORITY 5
#define SESSION_MIN_MEM             20000


#define UART_RX 34
#define UART_TX 2

#define UART_BAUD     115200
#define UART_CONF     SERIAL_8N1
#define UART_FLOWCTRL FlowControl::Software



Supervisor * Supervisor::s_singleton = nullptr;


Supervisor::Supervisor(BaseDisplayController * displayController)
  : m_displayController(displayController),
    m_activeSessionID(-1)
{
  if (s_singleton) {
    // no multiple instances allowed!
    abort();
  }
  s_singleton = this;

  for (int i = 0; i < MAXSESSIONS; ++i) {
    m_sessions[i].id         = i;
    m_sessions[i].thread     = nullptr;
    m_sessions[i].terminal   = nullptr;
  }
}


Supervisor::~Supervisor()
{
}

Terminal * Supervisor::createTerminal()
{
  Terminal * term = new Terminal;
  if (!term->begin(m_displayController)) {
    // failed to initialize terminal
    delete term;
    return nullptr;
  }
  term->connectLocally();      // to use Terminal.read(), available(), etc..
  return term;
}


void Supervisor::activateSession(int id)
{
  if (m_sessions[id].terminal == nullptr) {
    m_sessions[id].terminal = createTerminal();
    if (m_sessions[id].terminal == nullptr)
      return; // failed
  }

  auto trans = (m_activeSessionID == -1 ? TerminalTransition::None : (id < m_activeSessionID ? TerminalTransition::LeftToRight : TerminalTransition::RightToLeft));
  m_sessions[id].terminal->activate(trans);
  m_activeSessionID = id;

  if (m_sessions[id].thread == nullptr) {
    if (CoreUsage::busiestCore() == -1)
      xTaskCreate(&sessionThread, "", SESSIONTHREAD_STACK_SIZE, &m_sessions[id], SESSIONTHREAD_TASK_PRIORITY, &m_sessions[id].thread);
    else
      xTaskCreatePinnedToCore(&sessionThread, "", SESSIONTHREAD_STACK_SIZE, &m_sessions[id], SESSIONTHREAD_TASK_PRIORITY, &m_sessions[id].thread, CoreUsage::quietCore());
  }
}


int Supervisor::getSessionIDByTaskHandle(TaskHandle_t taskHandle)
{
  for (int i = 0; i < MAXSESSIONS; ++i)
    if (m_sessions[i].thread == taskHandle)
      return i;
  return -1;
}


void Supervisor::abortSession(int id, AbortReason abortReason)
{
  if (m_sessions[id].thread) {
    m_sessions[id].hal->abort(abortReason);
    // send a character to unlock waiting terminal
    m_sessions[id].terminal->localWrite(ASCII_CTRLC);
  }
}


void Supervisor::waitTermination()
{
  for (bool loop = true; loop; ) {
    loop = false;
    for (int i = 0; i < MAXSESSIONS; ++i)
      if (m_sessions[i].thread) {
        loop = true;
        break;
      }
    vTaskDelay(1000);
  }
}


int Supervisor::getOpenSessions()
{
  int r = 0;
  for (int i = 0; i < MAXSESSIONS; ++i)
    if (m_sessions[i].thread)
      ++r;
  return r;
}


void Supervisor::sessionThread(void * arg)
{

  Session * session = (Session*) arg;

  AbortReason abortReason = AbortReason::NoAbort;

  auto term = session->terminal;

  term->setTerminalType(TermType::ANSILegacy);
  term->setBackgroundColor(Color::Black);
  term->setForegroundColor(Color::BrightGreen);
  term->clear();
  term->enableCursor(true);

  while (true) {

    if (HAL::systemFree() < SESSION_MIN_MEM) {
      abortReason = AbortReason::OutOfMemory;
      break;
    }

    HAL hal;

    session->hal = &hal;

    hal.setTerminal(term);

    instance()->onNewSession(&hal);

    BIOS bios(&hal);
    BDOS bdos(&hal, &bios);
    CCP  ccp(&hal, &bdos);

    // initial path (needed to find "submit.com" at startup)
    bdos.setSearchPath("A:BIN");

    ccp.run();

    abortReason = hal.abortReason();

    break;
  }

  switch (abortReason) {
    case AbortReason::NoAbort:
      // should never reach this!
      break;
    case AbortReason::OutOfMemory:
      term->write("\r\n\nOut of memory, session aborted.\r\n");
      break;
    case AbortReason::GeneralFailure:
      term->write("\r\n\nGeneral failure, session aborted.\r\n");
      break;
    case AbortReason::AuxTerm:
      term->write("\r\n\nOpening UART terminal...\r\n");
      term->disconnectLocally();
      term->connectSerialPort(UART_BAUD, UART_CONF, UART_RX, UART_TX, UART_FLOWCTRL);
      vTaskDelete(NULL);
      break;
    case AbortReason::SessionClosed:
      term->write("\r\n\nSession closed.");
      break;
  }

  term->flush();

  term->end();
  delete term;

  session->terminal = nullptr;
  session->thread = nullptr;

  vTaskDelete(NULL);
}
