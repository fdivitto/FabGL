/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
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


#include "freertos/FreeRTOS.h"

#include "mouse.h"
#include "comdrivers/ps2controller.h"
#include "displaycontroller.h"


#pragma GCC optimize ("O2")



namespace fabgl {


bool Mouse::s_quickCheckHardware = false;


Mouse::Mouse()
  : m_mouseAvailable(false),
    m_mouseType(LegacyMouse),
    m_mouseUpdateTask(nullptr),
    m_absoluteUpdate(false),
    m_prevDeltaTime(0),
    m_movementAcceleration(180),
    m_wheelAcceleration(60000),
    m_absoluteQueue(nullptr),
    m_updateDisplayController(nullptr),
    m_uiApp(nullptr)
{
}


Mouse::~Mouse()
{
  terminateAbsolutePositioner();
  vTaskDelete(m_mouseUpdateTask);
  vQueueDelete(m_deltaAvail);
}


void Mouse::begin(int PS2Port)
{
  if (s_quickCheckHardware)
    PS2Device::quickCheckHardware();
  PS2Device::begin(PS2Port);
  reset();
  m_deltaAvail = xQueueCreate(1, sizeof(MouseDeltaRaw));
  xTaskCreate(&mouseUpdateTask, "", 1600, this, 5, &m_mouseUpdateTask);
}


void Mouse::begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO)
{
  PS2Controller * PS2 = PS2Controller::instance();
  PS2->begin(clkGPIO, dataGPIO);
  PS2->setMouse(this);
  begin(0);
}


bool Mouse::reset()
{
  if (s_quickCheckHardware) {
    m_mouseAvailable = send_cmdReset();
  } else {
    // tries up to three times for mouse reset
    for (int i = 0; i < 3; ++i) {
      m_mouseAvailable = send_cmdReset();
      if (m_mouseAvailable)
        break;
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }

  // negotiate compatibility and default parameters
  if (m_mouseAvailable) {
    // try Intellimouse (three buttons + scroll wheel, 4 bytes packet)
    if (send_cmdSetSampleRate(200) && send_cmdSetSampleRate(100) && send_cmdSetSampleRate(80) && identify() == PS2DeviceType::MouseWithScrollWheel) {
      // Intellimouse ok!
      m_mouseType = Intellimouse;
    }

    setSampleRate(60);
  }

  return m_mouseAvailable;
}


int Mouse::getPacketSize()
{
  return (m_mouseType == Intellimouse ? 4 : 3);
}


bool Mouse::deltaAvailable()
{
  return uxQueueMessagesWaiting(m_deltaAvail) > 0;
}


bool Mouse::getNextDelta(MouseDelta * delta, int timeOutMS, bool requestResendOnTimeOut)
{
  MouseDeltaRaw draw;
  if (!xQueueReceive(m_deltaAvail, &draw, msToTicks(timeOutMS)))
    return false;

  // the bit 4 of first byte must be always 1
  if ((draw.data[0] & 8) == 0) {
    return false;
  }

  m_prevStatus = m_status;

  // decode packet
  m_status.buttons.left   = (draw.data[0] & 0x01 ? 1 : 0);
  m_status.buttons.middle = (draw.data[0] & 0x04 ? 1 : 0);
  m_status.buttons.right  = (draw.data[0] & 0x02 ? 1 : 0);
  if (delta) {
    delta->deltaX    = (int16_t)(draw.data[0] & 0x10 ? 0xFF00 | draw.data[1] : draw.data[1]);
    delta->deltaY    = (int16_t)(draw.data[0] & 0x20 ? 0xFF00 | draw.data[2] : draw.data[2]);
    delta->deltaZ    = (int8_t)(getPacketSize() > 3 ? draw.data[3] : 0);
    delta->overflowX = (draw.data[0] & 0x40 ? 1 : 0);
    delta->overflowY = (draw.data[0] & 0x80 ? 1 : 0);
    delta->buttons   = m_status.buttons;
  }
  
  return true;
}


void Mouse::setupAbsolutePositioner(int width, int height, bool createAbsolutePositionsQueue, BitmappedDisplayController * updateDisplayController, uiApp * app)
{
  m_area                  = Size(width, height);
  m_status.X              = width >> 1;
  m_status.Y              = height >> 1;
  m_status.wheelDelta     = 0;
  m_status.buttons.left   = 0;
  m_status.buttons.middle = 0;
  m_status.buttons.right  = 0;
  m_prevStatus            = m_status;

  m_updateDisplayController = updateDisplayController;

  m_uiApp = app;

  if (createAbsolutePositionsQueue && m_absoluteQueue == nullptr) {
    m_absoluteQueue = xQueueCreate(FABGLIB_MOUSE_EVENTS_QUEUE_SIZE, sizeof(MouseStatus));
  }

  if (m_updateDisplayController) {
    // setup initial position
    m_updateDisplayController->setMouseCursorPos(m_status.X, m_status.Y);
  }

  m_absoluteUpdate = (m_updateDisplayController || createAbsolutePositionsQueue || m_uiApp);
}


void Mouse::terminateAbsolutePositioner()
{
  m_absoluteUpdate = false;
  m_updateDisplayController = nullptr;
  m_uiApp = nullptr;
  if (m_absoluteQueue) {
    vQueueDelete(m_absoluteQueue);
    m_absoluteQueue = nullptr;
  }
}


void Mouse::updateAbsolutePosition(MouseDelta * delta)
{
  const int maxDeltaTimeUS = 500000; // after 0.5s doesn't consider acceleration

  int dx = delta->deltaX;
  int dy = delta->deltaY;
  int dz = delta->deltaZ;

  int64_t now   = esp_timer_get_time();
  int deltaTime = now - m_prevDeltaTime; // time in microseconds

  if (deltaTime < maxDeltaTimeUS) {

    // calculate movement acceleration
    if (dx != 0 || dy != 0) {
      int   deltaDist    = isqrt(dx * dx + dy * dy);                 // distance in mouse points
      float vel          = (float)deltaDist / deltaTime;             // velocity in mousepoints/microsecond
      float newVel       = vel + m_movementAcceleration * vel * vel; // new velocity
      int   newDeltaDist = newVel * deltaTime;                       // new distance
      dx = dx * newDeltaDist / deltaDist;
      dy = dy * newDeltaDist / deltaDist;
    }

    // calculate wheel acceleration
    if (dz != 0) {
      int   deltaDist    = abs(dz);                                  // distance in wheel points
      float vel          = (float)deltaDist / deltaTime;             // velocity in mousepoints/microsecond
      float newVel       = vel + m_wheelAcceleration * vel * vel;    // new velocity
      int   newDeltaDist = newVel * deltaTime;                       // new distance
      dz = dz * newDeltaDist / deltaDist;
    }

  }

  m_status.X           = tclamp((int)m_status.X + dx, 0, m_area.width  - 1);
  m_status.Y           = tclamp((int)m_status.Y - dy, 0, m_area.height - 1);
  m_status.wheelDelta  = dz;
  m_prevDeltaTime      = now;
}


void Mouse::mouseUpdateTask(void * arg)
{
  constexpr int MAX_TIME_BETWEEN_DATA_US = 500000;  // maximum time between data composing a delta frame

  Mouse * mouse = (Mouse*) arg;

  int64_t prevDataTime = 0;

  while (true) {

    MouseDeltaRaw draw;
    int drawlen = 0;
    while (drawlen < mouse->getPacketSize()) {
      int r = mouse->getData(-1);
      if (mouse->parityError() || mouse->syncError()) {
        drawlen = 0;
        continue;
      }
      int64_t now = esp_timer_get_time();
      if (drawlen > 0 && prevDataTime > 0 && (now - prevDataTime) > MAX_TIME_BETWEEN_DATA_US) {
        // too much time elapsed since last byte, start a new delta
        drawlen = 0;
      }
      if (r > -1) {
        draw.data[drawlen++] = r;
        prevDataTime = now;
      }
    }

    xQueueSendToBack(mouse->m_deltaAvail, &draw, portMAX_DELAY);

    if (mouse->m_absoluteUpdate) {
      MouseDelta delta;
      if (mouse->getNextDelta(&delta)) {
        mouse->updateAbsolutePosition(&delta);

        // VGA Controller
        if (mouse->m_updateDisplayController)
          mouse->m_updateDisplayController->setMouseCursorPos(mouse->m_status.X, mouse->m_status.Y);

        // queue (if you need availableStatus() or getNextStatus())
        if (mouse->m_absoluteQueue) {
          xQueueSend(mouse->m_absoluteQueue, &mouse->m_status, 0);
        }

        if (mouse->m_uiApp) {
          // generate uiApp events
          if (mouse->m_prevStatus.X != mouse->m_status.X || mouse->m_prevStatus.Y != mouse->m_status.Y) {
            // X and Y movement: UIEVT_MOUSEMOVE
            uiEvent evt = uiEvent(nullptr, UIEVT_MOUSEMOVE);
            evt.params.mouse.status = mouse->m_status;
            evt.params.mouse.changedButton = 0;
            mouse->m_uiApp->postEvent(&evt);
          }
          if (mouse->m_status.wheelDelta != 0) {
            // wheel movement: UIEVT_MOUSEWHEEL
            uiEvent evt = uiEvent(nullptr, UIEVT_MOUSEWHEEL);
            evt.params.mouse.status = mouse->m_status;
            evt.params.mouse.changedButton = 0;
            mouse->m_uiApp->postEvent(&evt);
          }
          if (mouse->m_prevStatus.buttons.left != mouse->m_status.buttons.left) {
            // left button: UIEVT_MOUSEBUTTONDOWN, UIEVT_MOUSEBUTTONUP
            uiEvent evt = uiEvent(nullptr, mouse->m_status.buttons.left ? UIEVT_MOUSEBUTTONDOWN : UIEVT_MOUSEBUTTONUP);
            evt.params.mouse.status = mouse->m_status;
            evt.params.mouse.changedButton = 1;
            mouse->m_uiApp->postEvent(&evt);
          }
          if (mouse->m_prevStatus.buttons.middle != mouse->m_status.buttons.middle) {
            // middle button: UIEVT_MOUSEBUTTONDOWN, UIEVT_MOUSEBUTTONUP
            uiEvent evt = uiEvent(nullptr, mouse->m_status.buttons.middle ? UIEVT_MOUSEBUTTONDOWN : UIEVT_MOUSEBUTTONUP);
            evt.params.mouse.status = mouse->m_status;
            evt.params.mouse.changedButton = 2;
            mouse->m_uiApp->postEvent(&evt);
          }
          if (mouse->m_prevStatus.buttons.right != mouse->m_status.buttons.right) {
            // right button: UIEVT_MOUSEBUTTONDOWN, UIEVT_MOUSEBUTTONUP
            uiEvent evt = uiEvent(nullptr, mouse->m_status.buttons.right ? UIEVT_MOUSEBUTTONDOWN : UIEVT_MOUSEBUTTONUP);
            evt.params.mouse.status = mouse->m_status;
            evt.params.mouse.changedButton = 3;
            mouse->m_uiApp->postEvent(&evt);
          }
        }

      }

    }

  }
}


int Mouse::availableStatus()
{
  return m_absoluteQueue ? uxQueueMessagesWaiting(m_absoluteQueue) : 0;
}


MouseStatus Mouse::getNextStatus(int timeOutMS)
{
  MouseStatus status;
  if (m_absoluteQueue)
    xQueueReceive(m_absoluteQueue, &status, msToTicks(timeOutMS));
  return status;
}


void Mouse::emptyQueue()
{
  while (getData(0) != -1)
    ;
  if (m_absoluteQueue)
    xQueueReset(m_absoluteQueue);
}



} // end of namespace
