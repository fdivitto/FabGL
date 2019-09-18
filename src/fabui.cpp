/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
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
#include "freertos/timers.h"

#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "fabutils.h"
#include "fabui.h"
#include "canvas.h"
#include "mouse.h"
#include "keyboard.h"
#include "images/bitmaps.h"



namespace fabgl {



// debug only!
/*
void dumpEvent(uiEvent * event)
{
  static int idx = 0;
  static const char * TOSTR[] = { "UIEVT_NULL", "UIEVT_DEBUGMSG", "UIEVT_APPINIT", "UIEVT_GENPAINTEVENTS", "UIEVT_PAINT", "UIEVT_ACTIVATE",
                                  "UIEVT_DEACTIVATE", "UIEVT_MOUSEMOVE", "UIEVT_MOUSEWHEEL", "UIEVT_MOUSEBUTTONDOWN",
                                  "UIEVT_MOUSEBUTTONUP", "UIEVT_SETPOS", "UIEVT_SETSIZE", "UIEVT_RESHAPEWINDOW",
                                  "UIEVT_MOUSEENTER", "UIEVT_MOUSELEAVE", "UIEVT_MAXIMIZE", "UIEVT_MINIMIZE", "UIEVT_RESTORE",
                                  "UIEVT_SHOW", "UIEVT_HIDE", "UIEVT_SETFOCUS", "UIEVT_KILLFOCUS", "UIEVT_KEYDOWN", "UIEVT_KEYUP",
                                  "UIEVT_TIMER", "UIEVT_DBLCLICK", "UIEVT_DBLCLICK", "UIEVT_EXITMODAL", "UIEVT_DESTROY", "UIEVT_CLOSE",
                                  "UIEVT_QUIT",
                                };
  Serial.printf("#%d ", idx++);
  Serial.write(TOSTR[event->id]);
  if (event->dest && event->dest->objectType().uiFrame)
    Serial.printf(" dst=\"%s\"(%p) ", ((uiFrame*)(event->dest))->title(), event->dest);
  else
    Serial.printf(" dst=%p ", event->dest);
  switch (event->id) {
    case UIEVT_DEBUGMSG:
      Serial.write(event->params.debugMsg);
      break;
    case UIEVT_MOUSEMOVE:
      Serial.printf("X=%d Y=%d", event->params.mouse.status.X, event->params.mouse.status.Y);
      break;
    case UIEVT_MOUSEWHEEL:
      Serial.printf("delta=%d", event->params.mouse.status.wheelDelta);
      break;
    case UIEVT_MOUSEBUTTONDOWN:
    case UIEVT_MOUSEBUTTONUP:
    case UIEVT_DBLCLICK:
      Serial.printf("btn=%d", event->params.mouse.changedButton);
      break;
    case UIEVT_PAINT:
    case UIEVT_GENPAINTEVENTS:
    case UIEVT_RESHAPEWINDOW:
      Serial.printf("rect=%d,%d,%d,%d", event->params.rect.X1, event->params.rect.Y1, event->params.rect.X2, event->params.rect.Y2);
      break;
    case UIEVT_SETPOS:
      Serial.printf("pos=%d,%d", event->params.pos.X, event->params.pos.Y);
      break;
    case UIEVT_SETSIZE:
      Serial.printf("size=%d,%d", event->params.size.width, event->params.size.height);
      break;
    case UIEVT_KEYDOWN:
    case UIEVT_KEYUP:
      Serial.printf("VK=%s ", Keyboard.virtualKeyToString(event->params.key.VK));
      if (event->params.key.LALT) Serial.write(" +LALT");
      if (event->params.key.RALT) Serial.write(" +RALT");
      if (event->params.key.CTRL) Serial.write(" +CTRL");
      if (event->params.key.SHIFT) Serial.write(" +SHIFT");
      if (event->params.key.GUI) Serial.write(" +GUI");
      break;
    case UIEVT_TIMER:
      Serial.printf("handle=%p", event->params.timerHandle);
      break;
    default:
      break;
  }
  Serial.write("\n");
}
*/



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiObject


uiObject::uiObject()
{
}


uiObject::~uiObject()
{
}


// uiObject
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvtHandler


uiEvtHandler::uiEvtHandler(uiApp * app)
  : m_app(app)
{
  objectType().uiEvtHandler = true;
}


uiEvtHandler::~uiEvtHandler()
{
}


void uiEvtHandler::processEvent(uiEvent * event)
{
  switch (event->id) {

    default:
      break;

  }
}


// uiEvtHandler
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiApp


uiApp::uiApp()
  : uiEvtHandler(nullptr),
    m_rootWindow(nullptr),
    m_activeWindow(nullptr),
    m_focusedWindow(nullptr),
    m_capturedMouseWindow(nullptr),
    m_freeMouseWindow(nullptr),
    m_modalWindow(nullptr),
    m_combineMouseMoveEvents(false),
    m_caretWindow(nullptr),
    m_caretTimer(nullptr),
    m_caretInvertState(-1),
    m_lastMouseUpTimeMS(0)
{
  objectType().uiApp = true;
  setApp(this);
}


uiApp::~uiApp()
{
}


int uiApp::run()
{
  m_eventsQueue = xQueueCreate(FABGLIB_UI_EVENTS_QUEUE_SIZE, sizeof(uiEvent));

  // setup absolute events from mouse
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), false, true, this);

  // setup keyboard
  Keyboard.setUIApp(this);

  // root window always stays at 0, 0 and cannot be moved
  m_rootWindow = new uiFrame(nullptr, "", Point(0, 0), Size(Canvas.getWidth(), Canvas.getHeight()), false);
  m_rootWindow->setApp(this);

  m_rootWindow->windowStyle().borderSize     = 0;
  m_rootWindow->frameStyle().backgroundColor = RGB(3, 3, 3);

  m_rootWindow->frameProps().resizeable = false;
  m_rootWindow->frameProps().moveable   = false;

  // setup mouse cursor (otherwise it has to wait mouse first moving to show mouse pointer)
  VGAController.setMouseCursor(m_rootWindow->windowStyle().defaultCursor);

  showWindow(m_rootWindow, true);

  m_activeWindow = m_rootWindow;

  // generate UIEVT_APPINIT event
  uiEvent evt = uiEvent(this, UIEVT_APPINIT);
  postEvent(&evt);

  int exitCode = 0;

  // dispatch events
  while (true) {
    uiEvent event;
    if (getEvent(&event, -1)) {

      // debug
      //dumpEvent(&event);

      preprocessEvent(&event);

      if (event.dest)
        event.dest->processEvent(&event);

      if (event.id == UIEVT_QUIT) {
        exitCode = event.params.exitCode;
        break;
      }
    }
  }

  showCaret(nullptr);

  VGAController.setMouseCursor(nullptr);

  Canvas.setBrushColor(m_rootWindow->frameStyle().backgroundColor);
  Canvas.clear();

  delete m_rootWindow;
  m_rootWindow = nullptr;

  Keyboard.setUIApp(nullptr);

  Mouse.terminateAbsolutePositioner();

  vQueueDelete(m_eventsQueue);
  m_eventsQueue = nullptr;

  return exitCode;
}


void uiApp::processEvents()
{
  uiEvent event;
  while (getEvent(&event, 0)) {

    preprocessEvent(&event);

    if (event.dest)
      event.dest->processEvent(&event);
  }
}


void uiApp::quit(int exitCode)
{
  for (auto child = m_rootWindow->lastChild(); child; child = child->prev())
    destroyWindow(child);
  uiEvent evt = uiEvent(nullptr, UIEVT_QUIT);
  evt.params.exitCode = exitCode;
  postEvent(&evt);
}


void uiApp::preprocessEvent(uiEvent * event)
{
  if (event->dest == nullptr) {
    // events with no destination
    switch (event->id) {
      case UIEVT_MOUSEMOVE:
      case UIEVT_MOUSEWHEEL:
      case UIEVT_MOUSEBUTTONDOWN:
      case UIEVT_MOUSEBUTTONUP:
        preprocessMouseEvent(event);
        break;
      case UIEVT_KEYDOWN:
      case UIEVT_KEYUP:
        preprocessKeyboardEvent(event);
        break;
      default:
        break;
    }
  } else {
    // events with destination
    switch (event->id) {
      case UIEVT_TIMER:
        if (event->params.timerHandle == m_caretTimer) {
          blinkCaret();
          event->dest = nullptr;  // do not send this event to the root window
        }
        break;
      case UIEVT_PAINT:
        blinkCaret(true);
        break;
      default:
        break;
    }
  }
  if (m_modalWindow != nullptr)
    filterModalEvent(event);
}


// decide if an event with destination to non-modal windows can pass when a modal window is active
void uiApp::filterModalEvent(uiEvent * event)
{
  if (event->dest != nullptr && event->dest->objectType().uiWindow && event->dest != m_modalWindow && !m_modalWindow->isChild((uiWindow*)event->dest)) {
    switch (event->id) {
      case UIEVT_MOUSEMOVE:
      case UIEVT_MOUSEWHEEL:
      case UIEVT_MOUSEBUTTONDOWN:
      case UIEVT_MOUSEBUTTONUP:
      case UIEVT_MOUSEENTER:
      case UIEVT_DBLCLICK:
        // block these events
        event->dest = nullptr;
        if (event->id == UIEVT_MOUSEENTER) {
          // a little hack to set the right mouse pointer exiting from modal window
          VGAController.setMouseCursor(m_modalWindow->windowStyle().defaultCursor);
        }
        break;

      default:
        break;
    }
  }
}


// look for destination window at event X, Y coordinates, then set "dest" field and modify mouse X, Y coordinates (convert to window coordinates)
// generate UIEVT_MOUSEENTER and UIEVT_MOUSELEAVE events
void uiApp::preprocessMouseEvent(uiEvent * event)
{
  // combine a sequence of UIEVT_MOUSEMOVE events?
  if (m_combineMouseMoveEvents && event->id == UIEVT_MOUSEMOVE) {
    uiEvent nextEvent;
    while (peekEvent(&nextEvent, 0) && nextEvent.id == UIEVT_MOUSEMOVE)
      getEvent(event, -1);
  }

  Point mousePos = Point(event->params.mouse.status.X, event->params.mouse.status.Y);

  // search for window under the mouse or mouse capturing window
  // insert UIEVT_MOUSELEAVE when mouse capture is finished over a non-capturing window
  uiWindow * oldFreeMouseWindow = m_freeMouseWindow;
  Point winMousePos = mousePos;
  if (m_capturedMouseWindow) {
    // mouse captured, just go back up to m_rootWindow
    for (uiWindow * cur = m_capturedMouseWindow; cur != m_rootWindow; cur = cur->parent())
      winMousePos = winMousePos.sub(cur->pos());
    event->dest = m_capturedMouseWindow;
    // left mouse button UP?
    if (event->id == UIEVT_MOUSEBUTTONUP && event->params.mouse.changedButton == 1) {
      // mouse up will end mouse capture, check that mouse is still inside
      if (!m_capturedMouseWindow->rect(uiOrigin::Window).contains(winMousePos)) {
        // mouse is not inside, post mouse leave and enter events
        uiEvent evt = uiEvent(m_capturedMouseWindow, UIEVT_MOUSELEAVE);
        postEvent(&evt);
        m_freeMouseWindow = oldFreeMouseWindow = nullptr;
      }
    }
  } else {
    m_freeMouseWindow = screenToWindow(winMousePos);  // translates winMousePos
    event->dest = m_freeMouseWindow;
  }
  event->params.mouse.status.X = winMousePos.X;
  event->params.mouse.status.Y = winMousePos.Y;

  // insert UIEVT_MOUSEENTER and UIEVT_MOUSELEAVE events
  if (oldFreeMouseWindow != m_freeMouseWindow) {
    if (m_freeMouseWindow) {
      uiEvent evt = uiEvent(m_freeMouseWindow, UIEVT_MOUSEENTER);
      insertEvent(&evt);
    }
    if (oldFreeMouseWindow) {
      uiEvent evt = uiEvent(oldFreeMouseWindow, UIEVT_MOUSELEAVE);
      insertEvent(&evt);
    }
  }

  // double click?
  if (event->id == UIEVT_MOUSEBUTTONUP && event->params.mouse.changedButton == 1) {
    int curTime = esp_timer_get_time() / 1000;  // uS -> MS
    if (m_lastMouseUpPos == mousePos && curTime - m_lastMouseUpTimeMS <= m_appProps.doubleClickTime) {
      // post double click message
      uiEvent evt = *event;
      evt.id = UIEVT_DBLCLICK;
      postEvent(&evt);
    }
    m_lastMouseUpTimeMS = curTime;
    m_lastMouseUpPos    = mousePos;
  }
}


void uiApp::preprocessKeyboardEvent(uiEvent * event)
{
  // keyboard events go to focused window
  if (m_focusedWindow) {
    event->dest = m_focusedWindow;
  }
  // keyboard events go also to active window (if not focused)
  if (m_focusedWindow != m_activeWindow) {
    uiEvent evt = *event;
    evt.dest = m_activeWindow;
    insertEvent(&evt);
  }
}


// allow a window to capture mouse. window = nullptr to end mouse capture
void uiApp::captureMouse(uiWindow * window)
{
  m_capturedMouseWindow = window;
  if (m_capturedMouseWindow)
    suspendCaret(true);
  else
    suspendCaret(false);
}


// convert screen coordinates to window coordinates (the most visible window)
// return the window under specified absolute coordinates
uiWindow * uiApp::screenToWindow(Point & point)
{
  uiWindow * win = m_rootWindow;
  while (win->hasChildren()) {
    uiWindow * child = win->lastChild();
    for (; child; child = child->prev()) {
      if (child->state().visible && win->clientRect(uiOrigin::Window).contains(point) && child->rect(uiOrigin::Parent).contains(point)) {
        win   = child;
        point = point.sub(child->pos());
        break;
      }
    }
    if (child == nullptr)
      break;
  }
  return win;
}


void uiApp::init()
{
}


bool uiApp::postEvent(uiEvent const * event)
{
  return xQueueSendToBack(m_eventsQueue, event, 0) == pdTRUE;
}


bool uiApp::insertEvent(uiEvent const * event)
{
  return xQueueSendToFront(m_eventsQueue, event, 0) == pdTRUE;
}


void uiApp::postDebugMsg(char const * msg)
{
  uiEvent evt = uiEvent(nullptr, UIEVT_DEBUGMSG);
  evt.params.debugMsg = msg;
  postEvent(&evt);
}


bool uiApp::getEvent(uiEvent * event, int timeOutMS)
{
  return xQueueReceive(m_eventsQueue, event,  timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS)) == pdTRUE;
}


bool uiApp::peekEvent(uiEvent * event, int timeOutMS)
{
  return xQueuePeek(m_eventsQueue, event,  timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS)) == pdTRUE;
}


void uiApp::processEvent(uiEvent * event)
{
  uiEvtHandler::processEvent(event);

  switch (event->id) {

    case UIEVT_APPINIT:
      init();
      break;

    case UIEVT_TIMER:
      onTimer(event->params.timerHandle);
      break;

    default:
      break;

  }
}


uiWindow * uiApp::setActiveWindow(uiWindow * value)
{
  // move on top of the children
  uiWindow * prev = m_activeWindow;

  if (value != m_activeWindow) {

    // is "value" window activable? If not turn "value" to the first activabe parent
    while (!value->m_windowProps.activable) {
      value = value->m_parent;
      if (!value)
        return prev; // no parent is activable
    }
    if (value == m_activeWindow)
      return prev;  // already active, nothing to do

    // changed active window, disable focus
    setFocusedWindow(nullptr);

    // ...and caret (setFocusedWindow() may not disable caret)
    showCaret(nullptr);

    m_activeWindow = value;

    if (prev) {
      // deactivate previous window
      uiEvent evt = uiEvent(prev, UIEVT_DEACTIVATE);
      postEvent(&evt);
    }

    if (m_activeWindow) {
      // activate window
      uiEvent evt = uiEvent(m_activeWindow, UIEVT_ACTIVATE);
      postEvent(&evt);
    }
  }

  return prev;
}


// value = nullptr              -> kill focus on old focused window
// value = focusable window     -> kill focus on old focused window, set focus on new window
// value = non-focusable window -> kill focus on old focused window (same of nullptr)
uiWindow * uiApp::setFocusedWindow(uiWindow * value)
{
  uiWindow * prev = m_focusedWindow;

  if (value && !value->isFocusable())
    value = nullptr;

  if (m_focusedWindow != value) {

    if (prev) {
      uiEvent evt = uiEvent(prev, UIEVT_KILLFOCUS);
      evt.params.newFocused = value;
      postEvent(&evt);
    }

    m_focusedWindow = value;

    // changed focus, disable caret
    showCaret(nullptr);

    if (m_focusedWindow) {
      uiEvent evt = uiEvent(m_focusedWindow, UIEVT_SETFOCUS);
      evt.params.oldFocused = prev;
      postEvent(&evt);
    }

  }

  return prev;
}


// delta = 1, go next focused index
// delta = -1, go previous focused index
uiWindow * uiApp::moveFocus(int delta)
{
  uiWindow * old = m_focusedWindow;
  uiWindow * parent = old ? old->parent() : m_activeWindow;
  if (parent && parent->hasChildren()) {
    uiWindow * proposed = old;
    int startingIndex = old ? old->focusIndex() + delta : 0;
    int newIndex = startingIndex;
    do {
      int maxIndex;
      uiWindow * child = parent->getChildWithFocusIndex(newIndex, &maxIndex);
      if (child) {
        proposed = child;
        break;
      }
      if (delta > 0)
        newIndex = (newIndex >= maxIndex ? 0 : newIndex + delta);
      else
        newIndex = (newIndex <= 0 ? maxIndex : newIndex + delta);
    } while (newIndex != startingIndex);
    setFocusedWindow(proposed);
  }
  return old;
}


void uiApp::repaintWindow(uiWindow * window)
{
  repaintRect(window->rect(uiOrigin::Screen));
}


void uiApp::repaintRect(Rect const & rect)
{
  uiEvent evt = uiEvent(m_rootWindow, UIEVT_GENPAINTEVENTS);
  evt.params.rect = rect;
  postEvent(&evt);
}


// move to position (x, y) relative to the parent window
void uiApp::moveWindow(uiWindow * window, int x, int y)
{
  reshapeWindow(window, Rect(x, y, x + window->size().width - 1, y + window->size().height - 1));
}


void uiApp::resizeWindow(uiWindow * window, int width, int height)
{
  reshapeWindow(window, window->rect(uiOrigin::Parent).resize(width, height));
}


void uiApp::resizeWindow(uiWindow * window, Size size)
{
  reshapeWindow(window, window->rect(uiOrigin::Parent).resize(size));
}


// coordinates relative to the parent window
void uiApp::reshapeWindow(uiWindow * window, Rect const & rect)
{
  uiEvent evt = uiEvent(window, UIEVT_RESHAPEWINDOW);
  evt.params.rect = rect;
  postEvent(&evt);
}


void uiApp::showWindow(uiWindow * window, bool value)
{
  window->m_state.visible = value;  // set now so setFocusedWindow can focus a just made visible window
  uiEvent evt = uiEvent(window, value ? UIEVT_SHOW : UIEVT_HIDE);
  postEvent(&evt);
}


ModalWindowState * uiApp::initModalWindow(uiWindow * window)
{
  showWindow(window, true);

  auto state = new ModalWindowState;
  state->window            = window;
  state->modalResult       = -1;
  state->prevFocusedWindow = setFocusedWindow(nullptr);
  state->prevActiveWindow  = setActiveWindow(window);
  state->prevModal         = m_modalWindow;

  return state;
}


// ret:
//   false = EXIT or CLOSE received, modal window should close (call endModalWindow)
//   true  = other processModalWindowEvents required, continue outer loop
bool uiApp::processModalWindowEvents(ModalWindowState * state, int timeout)
{
  // a new inner event loop...
  uiEvent event;
  while (getEvent(&event, timeout)) {

    if (m_modalWindow != state->window && event.dest == state->window) {
      // becomes modal when first message arrives
      m_modalWindow = state->window;
    }
    if (event.id == UIEVT_EXITMODAL) {
      // clean exit using exitModal() method
      state->modalResult = event.params.modalResult;
      return false;
    } else if (event.id == UIEVT_CLOSE) {
      // exit using Close button (default return value remains -1)
      return false;
    }

    preprocessEvent(&event);

    if (event.dest)
      event.dest->processEvent(&event);

  }
  return true;
}


// ret: modal result
int uiApp::endModalWindow(ModalWindowState * state)
{
  m_modalWindow = state->prevModal;
  setActiveWindow(state->prevActiveWindow);
  showWindow(state->window, false);
  setFocusedWindow(state->prevFocusedWindow);
  int result = state->modalResult;
  free(state);
  return result;
}


int uiApp::showModalWindow(uiWindow * window)
{
  auto state = initModalWindow(window);
  while (processModalWindowEvents(state, -1))
    ;
  return endModalWindow(state);
}


void uiApp::maximizeWindow(uiWindow * window, bool value)
{
  uiEvent evt = uiEvent(window, value ? UIEVT_MAXIMIZE : UIEVT_RESTORE);
  postEvent(&evt);
}


void uiApp::minimizeWindow(uiWindow * window, bool value)
{
  uiEvent evt = uiEvent(window, value ? UIEVT_MINIMIZE : UIEVT_RESTORE);
  postEvent(&evt);
}


void uiApp::timerFunc(TimerHandle_t xTimer)
{
  uiEvtHandler * dest = (uiEvtHandler*) pvTimerGetTimerID(xTimer);
  uiEvent evt = uiEvent(dest, UIEVT_TIMER);
  evt.params.timerHandle = xTimer;
  dest->app()->postEvent(&evt);
}


// return handler to pass to deleteTimer()
uiTimerHandle uiApp::setTimer(uiEvtHandler * dest, int periodMS)
{
  TimerHandle_t h = xTimerCreate("", pdMS_TO_TICKS(periodMS), pdTRUE, dest, &uiApp::timerFunc);
  xTimerStart(h, 0);
  return h;
}


void uiApp::killTimer(uiTimerHandle handle)
{
  xTimerDelete(handle, portMAX_DELAY);
}


// window = nullptr -> disable caret
// "window" must be focused window (and top-level window, otherwise caret is painted wrongly)
void uiApp::showCaret(uiWindow * window)
{
  if (m_caretWindow != window) {
    if (window && window == m_focusedWindow) {
      // enable caret
      m_caretWindow = window;
      m_caretTimer  = setTimer(m_rootWindow, m_appProps.caretBlinkingTime);
      m_caretInvertState = 0;
      blinkCaret();
    } else if (m_caretTimer) {
      // disable caret
      suspendCaret(true);
      killTimer(m_caretTimer);
      m_caretTimer  = nullptr;
      m_caretWindow = NULL;
    }
  }
}


void uiApp::suspendCaret(bool value)
{
  if (m_caretTimer) {
    if (value) {
      if (m_caretInvertState != -1) {
        xTimerStop(m_caretTimer, 0);
        blinkCaret(true); // force off
        m_caretInvertState = -1;
      }
    } else {
      if (m_caretInvertState == -1) {
        xTimerStart(m_caretTimer, 0);
        m_caretInvertState = 0;
        blinkCaret();
      }
    }
  }
}


// just to force blinking
void uiApp::setCaret(bool value)
{
  blinkCaret(!value);
}


void uiApp::setCaret(Point const & pos)
{
  setCaret(m_caretRect.move(pos));
}


void uiApp::setCaret(Rect const & rect)
{
  blinkCaret(true);
  m_caretRect = rect;
  blinkCaret();
}


void uiApp::blinkCaret(bool forceOFF)
{
  if (m_caretWindow && m_caretInvertState != -1 && (forceOFF == false || m_caretInvertState == 1)) {
    Canvas.resetPaintOptions();
    Canvas.setOrigin(m_rootWindow->pos());
    Canvas.setClippingRect(m_caretWindow->clientRect(uiOrigin::Screen));
    Rect aRect = m_caretWindow->transformRect(m_caretRect, m_rootWindow);
    Canvas.invertRectangle(aRect);
    m_caretInvertState = m_caretInvertState ? 0 : 1;
  }
}


// this is the unique way to manually destroy a window
void uiApp::destroyWindow(uiWindow * window)
{
  if (window) {
    // first destroy children
    for (auto child = window->lastChild(); child; child = child->prev())
      destroyWindow(child);
    // is this window used for something?
    if (m_caretWindow == window)
      showCaret(nullptr);
    if (m_focusedWindow == window)
      setFocusedWindow(nullptr);
    if (m_activeWindow == window)
      setActiveWindow(nullptr);
    // to send Hide event and repaint area
    showWindow(window, false);
    // to actualy detach from parent and destroy the object
    uiEvent evt = uiEvent(window, UIEVT_DESTROY);
    postEvent(&evt);
  }
}


void uiApp::cleanWindowReferences(uiWindow * window)
{
  if (m_capturedMouseWindow == window)
    m_capturedMouseWindow = nullptr;
  if (m_freeMouseWindow == window)
    m_freeMouseWindow = nullptr;
  if (m_activeWindow == window)
    m_activeWindow = nullptr;
  if (m_focusedWindow == window)
    m_focusedWindow = nullptr;
  if (m_modalWindow == window)
    m_modalWindow = nullptr;
  if (m_caretWindow == window)
    m_caretWindow = nullptr;
}


uiMessageBoxResult uiApp::messageBox(char const * title, char const * text, char const * button1Text, char const * button2Text, char const * button3Text, uiMessageBoxIcon icon)
{
  auto font = Canvas.getPresetFontInfoFromHeight(14, false);
  const int titleHeight = title && strlen(title) ? font->height : 0;
  const int textExtent = Canvas.textExtent(font, text);
  const int button1Extent = button1Text ? Canvas.textExtent(font, button1Text) + 10 : 0;
  const int button2Extent = button2Text ? Canvas.textExtent(font, button2Text) + 10 : 0;
  const int button3Extent = button3Text ? Canvas.textExtent(font, button3Text) + 10 : 0;
  const int buttonsWidth  = imax(imax(imax(button1Extent, button2Extent), button3Extent), 40);
  int totButtons = 0;
  if (button1Extent)
    ++totButtons;
  if (button2Extent)
    ++totButtons;
  if (button3Extent)
    ++totButtons;
  const int buttonsHeight = font->height + 6;
  auto bitmap = (icon == uiMessageBoxIcon::Question ? &questionBitmap :
                (icon == uiMessageBoxIcon::Info ?     &infoBitmap :
                (icon == uiMessageBoxIcon::Warning ?  &warnBitmap :
                (icon == uiMessageBoxIcon::Error ?    &errorBitmap : nullptr))));
  const int bitmapWidth  = bitmap ? bitmap->width : 0;
  const int bitmapHeight = bitmap ? bitmap->height : 0;
  constexpr int buttonsSpace = 10;
  const int bitmapSpace = bitmap ? 8 : 0;
  const int textHeight = imax(font->height, bitmapHeight);
  const int requiredWidth  = imin(imax(bitmapWidth + bitmapSpace + textExtent + 10, buttonsWidth * totButtons + (2 + buttonsSpace) * totButtons), Canvas.getWidth());
  const int requiredHeight = textHeight + buttonsHeight + titleHeight + font->height * 3;
  const int frameX = (Canvas.getWidth() - requiredWidth) / 2;
  const int frameY = (Canvas.getHeight() - requiredHeight) / 2;

  auto mainFrame = new uiFrame(m_rootWindow, title, Point(frameX, frameY), Size(requiredWidth, requiredHeight), false);
  mainFrame->frameProps().resizeable        = false;
  mainFrame->frameProps().hasMaximizeButton = false;
  mainFrame->frameProps().hasMinimizeButton = false;

  int x = (requiredWidth - bitmapWidth - bitmapSpace - textExtent) / 2;
  if (bitmap) {
    int y = font->height + titleHeight + (textHeight - bitmapHeight) / 2;
    new uiImage(mainFrame, bitmap, Point(x, y));
    x += bitmapWidth + bitmapSpace;
  }

  int y = font->height + titleHeight + (textHeight - font->height) / 2;
  new uiLabel(mainFrame, text, Point(x, y));

  // setup panel (where buttons are positioned)

  y += textHeight + titleHeight;
  auto panel = new uiPanel(mainFrame, Point(0, y), Size(mainFrame->size().width, mainFrame->size().height - y));
  panel->windowStyle().borderColor = RGB(2, 2, 2);
  panel->panelStyle().backgroundColor = mainFrame->frameStyle().backgroundColor;

  // setup buttons

  y = (panel->size().height - buttonsHeight) / 2;
  x = mainFrame->windowStyle().borderSize + requiredWidth - buttonsWidth * totButtons - buttonsSpace * totButtons;  // right aligned

  auto button1 = button1Text ? new uiButton(panel, button1Text, Point(x, y), Size(buttonsWidth, buttonsHeight)) : nullptr;
  if (button1) {
    button1->onClick = [&]() { mainFrame->exitModal(1); };
    x += buttonsWidth + buttonsSpace;
  }

  auto button2 = button2Text ? new uiButton(panel, button2Text, Point(x, y), Size(buttonsWidth, buttonsHeight)) : nullptr;
  if (button2) {
    button2->onClick = [&]() { mainFrame->exitModal(2); };
    x += buttonsWidth + buttonsSpace;
  }

  auto button3 = button3Text ? new uiButton(panel, button3Text, Point(x, y), Size(buttonsWidth, buttonsHeight)) : nullptr;
  if (button3) {
    button3->onClick = [&]() { mainFrame->exitModal(3); };
    x += buttonsWidth + buttonsSpace;
  }

  // focus on fist button
  mainFrame->onShow = [&]() {
    setFocusedWindow(button1);
  };

  // run

  int modalResult = showModalWindow(mainFrame);
  destroyWindow(mainFrame);

  switch (modalResult) {
    case 1:
      return uiMessageBoxResult::Button1;
    case 2:
      return uiMessageBoxResult::Button2;
    case 3:
      return uiMessageBoxResult::Button3;
    default:
      return uiMessageBoxResult::Cancel;
  }
}


uiMessageBoxResult uiApp::inputBox(char const * title, char const * text, char * inOutString, int maxLength, char const * button1Text, char const * button2Text)
{
  auto font = Canvas.getPresetFontInfoFromHeight(14, false);
  const int titleHeight   = title && strlen(title) ? font->height : 0;
  const int textExtent    = Canvas.textExtent(font, text);
  const int editExtent    = imin(maxLength * Canvas.textExtent(font, "M"), m_rootWindow->clientSize().width / 2 - textExtent);
  const int button1Extent = button1Text ? Canvas.textExtent(font, button1Text) + 10 : 0;
  const int button2Extent = button2Text ? Canvas.textExtent(font, button2Text) + 10 : 0;
  const int buttonsWidth  = imax(imax(button1Extent, button2Extent), 40);
  int totButtons = 0;
  if (button1Extent)
    ++totButtons;
  if (button2Extent)
    ++totButtons;
  const int buttonsHeight    = font->height + 6;
  const int textHeight       = font->height;
  constexpr int buttonsSpace = 10;
  const int requiredWidth    = imin(imax(editExtent + textExtent + 10, buttonsWidth * totButtons + (2 + buttonsSpace) * totButtons), Canvas.getWidth());
  const int requiredHeight   = textHeight + buttonsHeight + titleHeight + font->height * 3;
  const int frameX           = (Canvas.getWidth() - requiredWidth) / 2;
  const int frameY           = (Canvas.getHeight() - requiredHeight) / 2;

  auto mainFrame = new uiFrame(m_rootWindow, title, Point(frameX, frameY), Size(requiredWidth, requiredHeight), false);
  mainFrame->frameProps().resizeable        = false;
  mainFrame->frameProps().hasMaximizeButton = false;
  mainFrame->frameProps().hasMinimizeButton = false;
  mainFrame->onKeyUp = [&](uiKeyEventInfo key) {
    if (key.VK == VK_RETURN)
      mainFrame->exitModal(1);
    else if (key.VK == VK_ESCAPE)
      mainFrame->exitModal(0);
  };

  int x = 10;
  int y = font->height + titleHeight + (textHeight - font->height) / 2;
  new uiLabel(mainFrame, text, Point(x, y));

  auto edit = new uiTextEdit(mainFrame, inOutString, Point(x + textExtent + 5, y - 4), Size(editExtent - 15, textHeight + 6));

  // setup panel (where buttons are positioned)

  y += textHeight + titleHeight;
  auto panel = new uiPanel(mainFrame, Point(0, y), Size(mainFrame->size().width, mainFrame->size().height - y));
  panel->windowStyle().borderColor = RGB(2, 2, 2);
  panel->panelStyle().backgroundColor = mainFrame->frameStyle().backgroundColor;

  // setup buttons

  y = (panel->size().height - buttonsHeight) / 2;
  x = mainFrame->windowStyle().borderSize + requiredWidth - buttonsWidth * totButtons - buttonsSpace * totButtons;  // right aligned

  auto button1 = button1Text ? new uiButton(panel, button1Text, Point(x, y), Size(buttonsWidth, buttonsHeight)) : nullptr;
  if (button1) {
    button1->onClick = [&]() { mainFrame->exitModal(1); };
    x += buttonsWidth + buttonsSpace;
  }

  auto button2 = button2Text ? new uiButton(panel, button2Text, Point(x, y), Size(buttonsWidth, buttonsHeight)) : nullptr;
  if (button2) {
    button2->onClick = [&]() { mainFrame->exitModal(2); };
    x += buttonsWidth + buttonsSpace;
  }

  // focus on edit
  mainFrame->onShow = [&]() {
    setFocusedWindow(edit);
  };

  // run

  int modalResult = showModalWindow(mainFrame);
  destroyWindow(mainFrame);

  switch (modalResult) {
    case 1:
    {
      int len = imin(maxLength, strlen(edit->text()));
      memcpy(inOutString, edit->text(), len);
      inOutString[len] = 0;
      return uiMessageBoxResult::Button1;
    }
    case 2:
      return uiMessageBoxResult::Button2;
    default:
      return uiMessageBoxResult::Cancel;
  }
}


void uiApp::enableKeyboardAndMouseEvents(bool value)
{
  if (value) {
    Keyboard.setUIApp(this);
    Mouse.setUIApp(this);
    VGAController.setMouseCursor(m_rootWindow->windowStyle().defaultCursor);
  } else {
    Keyboard.setUIApp(nullptr);
    Mouse.setUIApp(nullptr);
    VGAController.setMouseCursor(nullptr);
  }
}


// uiApp
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiWindow


uiWindow::uiWindow(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiEvtHandler(parent ? parent->app() : nullptr),
    m_parent(parent),
    m_pos(pos),
    m_size(size),
    m_mouseDownPos(Point(-1, -1)),
    m_isMouseOver(false),
    m_next(nullptr),
    m_prev(nullptr),
    m_firstChild(nullptr),
    m_lastChild(nullptr)
{
  objectType().uiWindow = true;

  m_state.visible   = false;
  m_state.maximized = false;
  m_state.minimized = false;
  m_state.active    = false;

  if (parent)
    parent->addChild(this);

  if (visible)
    app()->showWindow(this, true);

  m_focusIndex = prev() ? prev()->m_focusIndex + 1 : 0;
}


uiWindow::~uiWindow()
{
  freeChildren();
}


void uiWindow::freeChildren()
{
  for (uiWindow * next, * cur = m_firstChild; cur; cur = next) {
    next = cur->m_next;
    delete cur;
  }
  m_firstChild = m_lastChild = nullptr;
}


void uiWindow::addChild(uiWindow * child)
{
  if (m_firstChild) {
    // append after last child
    m_lastChild->m_next = child;
    child->m_prev = m_lastChild;
    m_lastChild = child;
  } else {
    // there are no children
    m_firstChild = m_lastChild = child;
  }
}


// insert child over (one position after) underlyingChild
// underlyingChild = nullptr, first position
void uiWindow::insertAfter(uiWindow * child, uiWindow * underlyingChild)
{
  if (!hasChildren()) {
    // this is the first child, just add
    addChild(child);
    return;
  }
  child->m_prev = underlyingChild;
  if (underlyingChild) {
    // insert before underlyingChild
    child->m_next = underlyingChild->m_next;
    if (child->m_next)
      child->m_next->m_prev = child;
    underlyingChild->m_next = child;
    if (m_lastChild == underlyingChild)
      m_lastChild = child;
  } else {
    // insert at first position (before m_firstChild)
    m_firstChild->m_prev = child;
    child->m_next = m_firstChild;
    m_firstChild = child;
  }
}


void uiWindow::removeChild(uiWindow * child, bool freeChild)
{
  if (child) {
    if (child == m_firstChild)
      m_firstChild = child->m_next;
    else
      child->m_prev->m_next = child->m_next;

    if (child == m_lastChild)
      m_lastChild = child->m_prev;
    else
      child->m_next->m_prev = child->m_prev;

    if (freeChild) {
      delete child;
      app()->cleanWindowReferences(child);
    } else
      child->m_prev = child->m_next = nullptr;

  }
}


// move to the last position (top window)
void uiWindow::moveChildOnTop(uiWindow * child)
{
  removeChild(child, false);
  addChild(child);
}


// move child over (one position after) underlyingChild
// underlyingChild = nullptr, first position
void uiWindow::moveAfter(uiWindow * child, uiWindow * underlyingChild)
{
  removeChild(child, false);
  insertAfter(child, underlyingChild);
}


void uiWindow::bringOnTop()
{
  parent()->moveChildOnTop(this);
}


void uiWindow::bringAfter(uiWindow * insertionPoint)
{
  parent()->moveAfter(this, insertionPoint);
}


// return true if "window" is a child (or descendant) of this window
bool uiWindow::isChild(uiWindow * window)
{
  for (auto child = firstChild(); child; child = child->next())
    if (child == window || (child->hasChildren() && child->isChild(window)))
      return true;
  return false;
}


// transform a rect relative to this window to a rect relative to the specified base window
Rect uiWindow::transformRect(Rect const & rect, uiWindow * baseWindow)
{
  Rect r = rect;
  for (uiWindow * win = this; win != baseWindow; win = win->m_parent)
    r = r.translate(win->m_pos);
  return r;
}


// rect is based on window coordinates
void uiWindow::repaint(Rect const & rect)
{
  app()->repaintRect(transformRect(rect, app()->rootWindow()));
}


void uiWindow::repaint()
{
  app()->repaintRect(rect(uiOrigin::Screen));
}


Rect uiWindow::rect(uiOrigin origin)
{
  switch (origin) {
    case uiOrigin::Screen:
      return transformRect(Rect(0, 0, m_size.width - 1, m_size.height - 1), app()->rootWindow());

    case uiOrigin::Parent:
      return Rect(m_pos.X, m_pos.Y, m_pos.X + m_size.width - 1, m_pos.Y + m_size.height - 1);

    case uiOrigin::Window:
      return Rect(0, 0, m_size.width - 1, m_size.height - 1);
  }
  return Rect();
}


Rect uiWindow::clientRect(uiOrigin origin)
{
  int bSize = hasFocus() ? m_windowStyle.focusedBorderSize : m_windowStyle.borderSize;
  return rect(origin).shrink(bSize);
}


Size uiWindow::clientSize()
{
  return clientRect(uiOrigin::Window).size();
}


Point uiWindow::clientPos()
{
  return clientRect(uiOrigin::Window).pos();
}


void uiWindow::beginPaint(uiEvent * paintEvent, Rect const & clippingRect)
{
  Rect srect = rect(uiOrigin::Screen);
  Canvas.setOrigin(srect.X1, srect.Y1);
  Canvas.setClippingRect( clippingRect.intersection(paintEvent->params.rect) );
  Canvas.resetGlyphOptions();
  Canvas.resetPaintOptions();
}


void uiWindow::processEvent(uiEvent * event)
{
  uiEvtHandler::processEvent(event);

  switch (event->id) {

    case UIEVT_DESTROY:
      m_parent->removeChild(this);
      break;

    case UIEVT_CLOSE:
      // for default a Close request just hides the window
      app()->showWindow(this, false);
      break;

    case UIEVT_ACTIVATE:
      {
        m_state.active = true;
        uiWindow * winToRepaint = this;
        // move this window and parent windows on top (last position), and select the window to actually repaint
        for (uiWindow * child = this; child->parent() != nullptr; child = child->parent()) {
          if (child != child->parent()->lastChild()) {
            child->parent()->moveChildOnTop(child);
            winToRepaint = child;
          }
        }
        winToRepaint->repaint();
        break;
      }

    case UIEVT_DEACTIVATE:
      m_state.active = false;
      repaint();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1) {
        m_mouseDownPos    = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
        m_posAtMouseDown  = m_pos;
        m_sizeAtMouseDown = m_size;
        // activate window? setActiveWindow() will activate the right window (maybe a parent)
        if (!m_state.active)
          app()->setActiveWindow(this);
        // focus window?
        app()->setFocusedWindow(this);
        // capture mouse
        app()->captureMouse(this);
      }
      break;

    case UIEVT_MOUSEBUTTONUP:
      // end capture mouse if left button is up
      if (event->params.mouse.changedButton == 1) {
        app()->captureMouse(nullptr);
        // generate UIEVT_CLICK. The check is required to avoid onclick event when mouse is captured and moved out of button area
        if (rect(uiOrigin::Window).contains(event->params.mouse.status.X, event->params.mouse.status.Y)) {
          uiEvent evt = *event;
          evt.id = UIEVT_CLICK;
          app()->postEvent(&evt);
        }
      }
      break;

    case UIEVT_CLICK:
      onClick();
      break;

    case UIEVT_DBLCLICK:
      onDblClick();
      break;

    case UIEVT_SHOW:
      repaint();
      break;

    case UIEVT_HIDE:
      repaint();
      break;

    case UIEVT_MAXIMIZE:
      if (!m_state.minimized)
        m_savedScreenRect = rect(uiOrigin::Parent);
      m_state.maximized = true;
      m_state.minimized = false;
      app()->reshapeWindow(this, m_parent->clientRect(uiOrigin::Window));
      break;

    case UIEVT_MINIMIZE:
      if (!m_state.maximized)
        m_savedScreenRect = rect(uiOrigin::Parent);
      m_state.maximized = false;
      m_state.minimized = true;
      app()->resizeWindow(this, minWindowSize());
      break;

    case UIEVT_RESTORE:
      m_state.maximized = false;
      m_state.minimized = false;
      app()->reshapeWindow(this, m_savedScreenRect);
      break;

    case UIEVT_RESHAPEWINDOW:
      reshape(event->params.rect);
      break;

    case UIEVT_GENPAINTEVENTS:
      generatePaintEvents(event->params.rect);
      break;

    case UIEVT_MOUSEENTER:
      m_isMouseOver = true;
      VGAController.setMouseCursor(m_windowStyle.defaultCursor);
      break;

    case UIEVT_MOUSELEAVE:
      m_isMouseOver = false;
      break;

    case UIEVT_KEYDOWN:
      // only non-focusable windows can make focusable its children
      if (!m_windowProps.focusable) {
        // move focused child
        if (event->params.key.VK == VK_TAB) {
          if (event->params.key.SHIFT)
            app()->moveFocus(-1);
          else
            app()->moveFocus(1);
        }
      }
      break;

    case UIEVT_PAINT:
      beginPaint(event, rect(uiOrigin::Window));
      paintWindow();
      break;

    case UIEVT_SETFOCUS:
    case UIEVT_KILLFOCUS:
      repaint();  // to update border
      break;

    default:
      break;
  }
}


void uiWindow::paintWindow()
{
  // border
  int bSize = hasFocus() ? m_windowStyle.focusedBorderSize : m_windowStyle.borderSize;
  if (bSize > 0) {
    Canvas.setPenColor(hasFocus() ? m_windowStyle.focusedBorderColor : (state().active ? m_windowStyle.activeBorderColor : m_windowStyle.borderColor));
    for (int i = 0; i < bSize; ++i)
      Canvas.drawRectangle(i, i, m_size.width - 1 - i, m_size.height - 1 - i);
  }
}


// given a relative paint rect generate a set of UIEVT_PAINT events
void uiWindow::generatePaintEvents(Rect const & paintRect)
{
  app()->setCaret(false);
  Stack<Rect> rects;
  rects.push(paintRect);
  while (!rects.isEmpty()) {
    Rect thisRect = rects.pop();
    bool noIntesections = true;
    for (uiWindow * win = lastChild(); win; win = win->prev()) {
      Rect winRect = clientRect(uiOrigin::Window).intersection(win->rect(uiOrigin::Parent));
      if (win->state().visible && thisRect.intersects(winRect)) {
        noIntesections = false;
        removeRectangle(rects, thisRect, winRect);
        Rect newRect = thisRect.intersection(winRect).translate(-win->pos().X, -win->pos().Y);
        win->generatePaintEvents(newRect);
        break;
      }
    }
    if (noIntesections) {
      uiEvent evt = uiEvent(nullptr, UIEVT_PAINT);
      evt.dest = this;
      evt.params.rect = thisRect;
      // process event now. insertEvent() may dry events queue. On the other side, this may use too much stack!
      processEvent(&evt);
    }
  }
}


// insert/post UIEVT_PAINT, UIEVT_SETPOS and UIEVT_SETSIZE events in order to modify window bounding rect
// rect: new window rectangle based on parent coordinates
// handle anchors of its children
void uiWindow::reshape(Rect const & r)
{
  // new rect based on root window coordiantes
  Rect newRect = parent()->transformRect(r, app()->rootWindow());

  // old rect based on root window coordinates
  Rect oldRect = rect(uiOrigin::Screen);

  if (oldRect == newRect)
    return;

  // set here because generatePaintEvents() requires updated window pos() and size()
  m_pos  = Point(r.X1, r.Y1);
  m_size = r.size();

  if (!oldRect.intersects(newRect)) {
    // old position and new position do not intersect, just repaint old rect
    app()->rootWindow()->generatePaintEvents(oldRect);
  } else {
    Stack<Rect> rects;
    removeRectangle(rects, oldRect, newRect); // remove newRect from oldRect
    while (!rects.isEmpty())
      app()->rootWindow()->generatePaintEvents(rects.pop());
  }

  // generate set position event
  uiEvent evt = uiEvent(this, UIEVT_SETPOS);
  evt.params.pos = pos();
  app()->postEvent(&evt);

  // generate set size event
  evt = uiEvent(this, UIEVT_SETSIZE);
  evt.params.size = size();
  app()->postEvent(&evt);

  // handle children's anchors
  int dx = newRect.width() - oldRect.width();
  int dy = newRect.height() - oldRect.height();
  if (dx != 0 || dy != 0) {
    for (auto child = firstChild(); child; child = child->next()) {
      Rect childRect = child->rect(uiOrigin::Parent);
      Rect newChildRect = childRect;
      if (dx) {
        if (!child->m_anchors.left && !child->m_anchors.right) {
          // TODO: due the integer division the window may not stay at center when resizing by odd values. "ofs" is just a bad workaround
          int ofs = dx > 0 ? imax(1, dx / 2) : imin(-1, dx / 2);
          newChildRect.X1 += ofs;
          newChildRect.X2 += ofs;
        } else if (!child->m_anchors.left)
          newChildRect.X1 += dx;
        if (child->m_anchors.right)
          newChildRect.X2 += dx;
      }
      if (dy) {
        if (!child->m_anchors.top && !child->m_anchors.bottom) {
          // TODO: due the integer division the window may not stay at center when resizing by odd values. "ofs" is just a bad workaround
          int ofs = dy > 0 ? imax(1, dy / 2) : imin(-1, dy / 2);
          newChildRect.Y1 += ofs;
          newChildRect.Y2 += ofs;
        } else if (!child->m_anchors.top)
          newChildRect.Y1 += dy;
        if (child->m_anchors.bottom)
          newChildRect.Y2 += dy;
      }
      if (newChildRect != childRect) {
        //app()->reshapeWindow(child, newChildRect);
        child->m_pos.X = newChildRect.X1;
        child->m_pos.Y = newChildRect.Y1;
        child->m_size.width  = newChildRect.width();
        child->m_size.height = newChildRect.height();
        // generate set position event
        uiEvent evt = uiEvent(child, UIEVT_SETPOS);
        evt.params.pos = child->pos();
        app()->postEvent(&evt);
        // generate set size event
        evt = uiEvent(child, UIEVT_SETSIZE);
        evt.params.size = child->size();
        app()->postEvent(&evt);
      }
    }
  }

  app()->rootWindow()->generatePaintEvents(newRect);
}


void uiWindow::exitModal(int modalResult)
{
  uiEvent evt = uiEvent(this, UIEVT_EXITMODAL);
  evt.params.modalResult = modalResult;
  app()->postEvent(&evt);
}


bool uiWindow::hasFocus()
{
  return app()->focusedWindow() == this;
}


bool uiWindow::isFocusable()
{
  return windowProps().focusable && state().visible;
}


uiWindow * uiWindow::getChildWithFocusIndex(int focusIndex, int * maxIndex)
{
  *maxIndex = -1;
  for (auto child = m_firstChild; child; child = child->m_next) {
    if (child->isFocusable()) {
      *maxIndex = imax(*maxIndex, child->m_focusIndex);
      if (child->m_focusIndex == focusIndex)
        return child;
    }
  }
  return nullptr;
}


// uiWindow
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame


uiFrame::uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible)
  : uiWindow(parent, pos, size, visible),
    m_title(nullptr),
    m_titleLength(0),
    m_mouseDownFrameItem(uiFrameItem::None),
    m_mouseMoveFrameItem(uiFrameItem::None),
    m_lastReshapingBox(Rect(0, 0, 0, 0))
{
  objectType().uiFrame = true;
  setTitle(title);
}


uiFrame::~uiFrame()
{
  free(m_title);
}


void uiFrame::setTitle(char const * value)
{
  m_titleLength = strlen(value);
  m_title = (char*) realloc(m_title, m_titleLength + 1);
  strcpy(m_title, value);
}


void uiFrame::setTitleFmt(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    setTitle(buf);
  }
  va_end(ap);
}


int uiFrame::titleBarHeight()
{
  return m_frameStyle.titleFont->height + 3;
}


Rect uiFrame::titleBarRect()
{
  Rect r = uiWindow::clientRect(uiOrigin::Window);
  r.Y2 = r.Y1 + titleBarHeight() - 1;
  return r;
}


Rect uiFrame::clientRect(uiOrigin origin)
{
  Rect r = uiWindow::clientRect(origin);

  // title bar
  if (m_titleLength > 0)
    r.Y1 += titleBarHeight();

  return r;
}


Size uiFrame::minWindowSize()
{
  Size r = Size(0, 0);
  if (m_frameProps.resizeable && !state().minimized && m_titleLength == 0) {
    r.width  += CORNERSENSE * 2;
    r.height += CORNERSENSE * 2;
  }
  r.width  += windowStyle().borderSize * 2;
  r.height += windowStyle().borderSize * 2;
  if (m_titleLength > 0) {
    int barHeight = titleBarHeight();  // titleBarHeight is also the button width
    r.height += barHeight;
    if (m_frameProps.hasCloseButton || m_frameProps.hasMaximizeButton || m_frameProps.hasMinimizeButton)
      r.width += barHeight * 3;
    r.width += barHeight * 4;  // additional space to let some characters visible
  }
  return r;
}


// buttonIndex:
//   0 = close button
//   1 = maximize button
//   2 = minimize button
Rect uiFrame::getBtnRect(int buttonIndex)
{
  int btnSize = titleBarHeight();  // horiz and vert size of each button
  Rect barRect = titleBarRect();
  Rect btnRect = Rect(barRect.X2 - btnSize - CORNERSENSE / 2, barRect.Y1,
                      barRect.X2 - CORNERSENSE / 2, barRect.Y2);
  while (buttonIndex--)
    btnRect = btnRect.translate(-btnSize, 0);
  return btnRect;
}


void uiFrame::paintFrame()
{
  Rect bkgRect = uiWindow::clientRect(uiOrigin::Window);
  // title bar
  if (m_titleLength > 0) {
    int barHeight = titleBarHeight();
    // title bar background
    RGB titleBarBrushColor = state().active ? m_frameStyle.activeTitleBackgroundColor : m_frameStyle.titleBackgroundColor;
    Canvas.setBrushColor(titleBarBrushColor);
    Canvas.fillRectangle(titleBarRect());
    // close, maximize and minimze buttons
    int btnX = paintButtons(bkgRect);
    // title
    Canvas.setPenColor(state().active ? m_frameStyle.activeTitleColor : m_frameStyle.titleColor);
    Canvas.setGlyphOptions(GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
    Canvas.drawTextWithEllipsis(m_frameStyle.titleFont, 1 + bkgRect.X1, 1 + bkgRect.Y1, m_title, btnX);
    // adjust background rect
    bkgRect.Y1 += barHeight;
  }
  // background
  if (!state().minimized && bkgRect.width() > 0 && bkgRect.height() > 0) {
    Canvas.setBrushColor(m_frameStyle.backgroundColor);
    Canvas.fillRectangle(bkgRect);
  }
}


// return the X coordinate where button start
int uiFrame::paintButtons(Rect const & bkgRect)
{
  int buttonsX = bkgRect.X2;
  if (m_frameProps.hasCloseButton) {
    // close button
    Rect r = getBtnRect(0);
    buttonsX = r.X1;
    if (m_mouseMoveFrameItem == uiFrameItem::CloseButton) {
      Canvas.setBrushColor(m_frameStyle.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_frameStyle.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_frameStyle.activeButtonColor : m_frameStyle.buttonColor);
    r = r.shrink(4);
    Canvas.drawLine(r.X1, r.Y1, r.X2, r.Y2);
    Canvas.drawLine(r.X2, r.Y1, r.X1, r.Y2);
  }
  if (m_frameProps.hasMaximizeButton) {
    // maximize/restore button
    Rect r = getBtnRect(1);
    buttonsX = r.X1;
    if (m_mouseMoveFrameItem == uiFrameItem::MaximizeButton) {
      Canvas.setBrushColor(m_frameStyle.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_frameStyle.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_frameStyle.activeButtonColor : m_frameStyle.buttonColor);
    r = r.shrink(4);
    if (state().maximized || state().minimized) {
      // draw restore (from maximize or minimize) button
      r = r.shrink(1).translate(-1, +1);
      Canvas.drawRectangle(r);
      r = r.translate(+2, -2);
      Canvas.moveTo(r.X1, r.Y1 + 2);
      Canvas.lineTo(r.X1, r.Y1);
      Canvas.lineTo(r.X2, r.Y1);
      Canvas.lineTo(r.X2, r.Y2);
      Canvas.lineTo(r.X2 - 2, r.Y2);
    } else
      Canvas.drawRectangle(r);
  }
  if (m_frameProps.hasMinimizeButton && !state().minimized) {
    // minimize button
    Rect r = getBtnRect(2);
    buttonsX = r.X1;
    if (m_mouseMoveFrameItem == uiFrameItem::MinimizeButton) {
      Canvas.setBrushColor(m_frameStyle.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_frameStyle.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_frameStyle.activeButtonColor : m_frameStyle.buttonColor);
    r = r.shrink(4);
    int h = (r.Y2 - r.Y1 + 1) / 2;
    Canvas.drawLine(r.X1, r.Y1 + h, r.X2, r.Y1 + h);
  }
  return buttonsX;
}


void uiFrame::processEvent(uiEvent * event)
{
  uiWindow::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiWindow::clientRect(uiOrigin::Window));
      paintFrame();
      onPaint();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1) {
        m_mouseDownFrameItem = getFrameItemAt(event->params.mouse.status.X, event->params.mouse.status.Y);
        app()->combineMouseMoveEvents(true);
      }
      break;

    case UIEVT_MOUSEBUTTONUP:
      if (event->params.mouse.changedButton == 1) {
        int mouseX = event->params.mouse.status.X;
        int mouseY = event->params.mouse.status.Y;

        // this actually moves or resizes the window in case of non-realtime mode
        movingCapturedMouse(mouseX, mouseY, false);

        // this sets the right mouse cursor in case of end of capturing
        movingFreeMouse(mouseX, mouseY);

        // handle buttons clicks
        handleButtonsClick(mouseX, mouseY, false);

        app()->combineMouseMoveEvents(false);
      }
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this)
        movingCapturedMouse(event->params.mouse.status.X, event->params.mouse.status.Y, true);
      else
        movingFreeMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_MOUSELEAVE:
      if (m_mouseMoveFrameItem == uiFrameItem::CloseButton)
        repaint(getBtnRect(0));
      if (m_mouseMoveFrameItem == uiFrameItem::MaximizeButton)
        repaint(getBtnRect(1));
      if (m_mouseMoveFrameItem == uiFrameItem::MinimizeButton)
        repaint(getBtnRect(2));
      m_mouseMoveFrameItem = uiFrameItem::None;
      break;

    case UIEVT_DBLCLICK:
      handleButtonsClick(event->params.mouse.status.X, event->params.mouse.status.Y, true);
      break;

    case UIEVT_SHOW:
      onShow();
      break;

    case UIEVT_HIDE:
      onHide();
      break;

    case UIEVT_SETSIZE:
      onResize();
      break;

    case UIEVT_TIMER:
      onTimer(event->params.timerHandle);
      break;

    case UIEVT_KEYDOWN:
      onKeyDown(event->params.key);
      break;

    case UIEVT_KEYUP:
      onKeyUp(event->params.key);
      break;

    default:
      break;
  }
}


uiFrameItem uiFrame::getFrameItemAt(int x, int y)
{
  Point p = Point(x, y);

  if (m_titleLength > 0) {
    if (m_frameProps.hasCloseButton && getBtnRect(0).contains(p))
      return uiFrameItem::CloseButton;    // on Close Button area

    if (m_frameProps.hasMaximizeButton && getBtnRect(1).contains(p))
      return uiFrameItem::MaximizeButton; // on maximize button area

    if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(p))
      return uiFrameItem::MinimizeButton; // on minimize button area
  }

  if (m_frameProps.resizeable && !state().maximized && !state().minimized) {

    int w = size().width;
    int h = size().height;

    // on top center, resize
    if (Rect(CORNERSENSE, 0, w - CORNERSENSE, windowStyle().borderSize).contains(p))
      return uiFrameItem::TopCenterResize;

    // on left center side, resize
    if (Rect(0, CORNERSENSE, windowStyle().borderSize, h - CORNERSENSE).contains(p))
      return uiFrameItem::CenterLeftResize;

    // on right center side, resize
    if (Rect(w - windowStyle().borderSize, CORNERSENSE, w - 1, h - CORNERSENSE).contains(p))
      return uiFrameItem::CenterRightResize;

    // on bottom center, resize
    if (Rect(CORNERSENSE, h - windowStyle().borderSize, w - CORNERSENSE, h - 1).contains(p))
      return uiFrameItem::BottomCenterResize;

    // on top left side, resize
    if (Rect(0, 0, CORNERSENSE, CORNERSENSE).contains(p))
      return uiFrameItem::TopLeftResize;

    // on top right side, resize
    if (Rect(w - CORNERSENSE, 0, w - 1, CORNERSENSE).contains(p))
      return uiFrameItem::TopRightResize;

    // on bottom left side, resize
    if (Rect(0, h - CORNERSENSE, CORNERSENSE, h - 1).contains(p))
      return uiFrameItem::BottomLeftResize;

    // on bottom right side, resize
    if (Rect(w - CORNERSENSE, h - CORNERSENSE, w - 1, h - 1).contains(p))
      return uiFrameItem::BottomRightResize;

  }

  // on title bar, moving area
  if (m_titleLength > 0 && m_frameProps.moveable && !state().maximized && titleBarRect().contains(p))
    return uiFrameItem::MoveArea;

  return uiFrameItem::None;
}


void uiFrame::movingCapturedMouse(int mouseX, int mouseY, bool mouseIsDown)
{
  int dx = mouseX - mouseDownPos().X;
  int dy = mouseY - mouseDownPos().Y;

  Size minSize = minWindowSize();

  Rect newRect = rect(uiOrigin::Parent);

  switch (m_mouseDownFrameItem) {

    case uiFrameItem::MoveArea:
      newRect = newRect.move(pos().X + dx, pos().Y + dy);
      break;

    case uiFrameItem::CenterRightResize:
      newRect = newRect.resize(imax(sizeAtMouseDown().width + dx, minSize.width), newRect.height());
      break;

    case uiFrameItem::CenterLeftResize:
      {
        Rect r = newRect;
        r.X1 = pos().X + dx;
        newRect.X1 = r.X1 - imax(0, minSize.width - r.size().width);
        break;
      }

    case uiFrameItem::TopLeftResize:
      {
        Rect r = newRect;
        r.X1 = pos().X + dx;
        newRect.X1 = r.X1 - imax(0, minSize.width - r.size().width);
        r.Y1 = pos().Y + dy;
        newRect.Y1 = r.Y1 - imax(0, minSize.height - r.size().height);
        break;
      }

    case uiFrameItem::TopCenterResize:
      {
        Rect r = newRect;
        r.Y1 = pos().Y + dy;
        newRect.Y1 = r.Y1 - imax(0, minSize.height - r.size().height);
        break;
      }

    case uiFrameItem::TopRightResize:
      {
        Rect r = newRect;
        r.X2 = pos().X + sizeAtMouseDown().width + dx;
        newRect.X2 = r.X2 + imax(0, minSize.width - r.size().width);
        r.Y1 = pos().Y + dy;
        newRect.Y1 = r.Y1 - imax(0, minSize.height - r.size().height);
        break;
      }

    case uiFrameItem::BottomLeftResize:
      {
        Rect r = newRect;
        r.X1 = pos().X + dx;
        newRect.X1 = r.X1 - imax(0, minSize.width - r.size().width);
        r.Y2 = pos().Y + sizeAtMouseDown().height + dy;
        newRect.Y2 = r.Y2 + imax(0, minSize.height - r.size().height);
        break;
      }

    case uiFrameItem::BottomCenterResize:
      newRect = newRect.resize(newRect.width(), imax(sizeAtMouseDown().height + dy, minSize.height));
      break;

    case uiFrameItem::BottomRightResize:
      newRect = newRect.resize(imax(sizeAtMouseDown().width + dx, minSize.width), imax(sizeAtMouseDown().height + dy, minSize.height));
      break;

    default:
      return; // no action
  }

  // reshape to newRect or draw the reshaping box)
  if (mouseIsDown == false || app()->appProps().realtimeReshaping) {
    m_lastReshapingBox = Rect();
    app()->reshapeWindow(this, newRect);
  } else
    drawReshapingBox(newRect);
}


void uiFrame::drawReshapingBox(Rect boxRect)
{
  int clientOffsetY = clientRect(uiOrigin::Window).Y1;
  Canvas.setOrigin(parent()->rect(uiOrigin::Screen).pos());
  Canvas.setClippingRect(parent()->clientRect(uiOrigin::Window));
  PaintOptions popt;
  popt.NOT = true;
  Canvas.setPaintOptions(popt);
  if (m_lastReshapingBox != Rect()) {
    Canvas.drawRectangle(m_lastReshapingBox);
    if (m_titleLength > 0)
      Canvas.drawLine(m_lastReshapingBox.X1, m_lastReshapingBox.Y1 + clientOffsetY, m_lastReshapingBox.X2, m_lastReshapingBox.Y1 + clientOffsetY);
  }
  if (boxRect != Rect()) {
    Canvas.drawRectangle(boxRect);
    if (m_titleLength > 0)
      Canvas.drawLine(boxRect.X1, boxRect.Y1 + clientOffsetY, boxRect.X2, boxRect.Y1 + clientOffsetY);
  }
  Canvas.setPaintOptions(PaintOptions());
  m_lastReshapingBox = boxRect;
}


void uiFrame::movingFreeMouse(int mouseX, int mouseY)
{
  uiFrameItem prevSensPos = m_mouseMoveFrameItem;

  m_mouseMoveFrameItem = getFrameItemAt(mouseX, mouseY);

  if ((m_mouseMoveFrameItem == uiFrameItem::CloseButton || prevSensPos == uiFrameItem::CloseButton) && m_mouseMoveFrameItem != prevSensPos)
    repaint(getBtnRect(0));

  if ((m_mouseMoveFrameItem == uiFrameItem::MaximizeButton || prevSensPos == uiFrameItem::MaximizeButton) && m_mouseMoveFrameItem != prevSensPos)
    repaint(getBtnRect(1));

  if ((m_mouseMoveFrameItem == uiFrameItem::MinimizeButton || prevSensPos == uiFrameItem::MinimizeButton) && m_mouseMoveFrameItem != prevSensPos)
    repaint(getBtnRect(2));

  CursorName cur = windowStyle().defaultCursor;

  switch (m_mouseMoveFrameItem) {

    case uiFrameItem::TopLeftResize:
      cur = CursorName::CursorResize2;
      break;

    case uiFrameItem::TopCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiFrameItem::TopRightResize:
      cur = CursorName::CursorResize1;
      break;

    case uiFrameItem::CenterLeftResize:
      cur = CursorName::CursorResize4;
      break;

    case uiFrameItem::CenterRightResize:
      cur = CursorName::CursorResize4;
      break;

    case uiFrameItem::BottomLeftResize:
      cur = CursorName::CursorResize1;
      break;

    case uiFrameItem::BottomCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiFrameItem::BottomRightResize:
      cur = CursorName::CursorResize2;
      break;

    default:
      break;
  }

  VGAController.setMouseCursor(cur);
}


void uiFrame::handleButtonsClick(int x, int y, bool doubleClick)
{
  if (m_titleLength > 0) {
    if (m_frameProps.hasCloseButton && getBtnRect(0).contains(x, y) && getBtnRect(0).contains(mouseDownPos())) {
      // generate UIEVT_CLOSE event
      uiEvent evt = uiEvent(this, UIEVT_CLOSE);
      app()->postEvent(&evt);
    } else if (m_frameProps.hasMaximizeButton && ((getBtnRect(1).contains(x, y) && getBtnRect(1).contains(mouseDownPos())) ||
                                                  (doubleClick && titleBarRect().contains(x, y)))) {
      // maximimize or restore on:
      //   - click on maximize/restore button
      //   - double click on the title bar
      app()->maximizeWindow(this, !state().maximized && !state().minimized);  // used also for "restore" from minimized
    } else if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(x, y) && getBtnRect(2).contains(mouseDownPos())) {
      app()->minimizeWindow(this, !state().minimized);
    } else
      return;
    // this avoids the button remains selected (background colored) when window change size
    m_mouseMoveFrameItem = uiFrameItem::None;
  }
}


// uiFrame
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiControl


uiControl::uiControl(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiWindow(parent, pos, size, visible)
{
  objectType().uiControl = true;
  windowProps().activable = false;
}


uiControl::~uiControl()
{
}


void uiControl::processEvent(uiEvent * event)
{
  uiWindow::processEvent(event);
}



// uiControl
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiButton


uiButton::uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, uiButtonKind kind, bool visible)
  : uiControl(parent, pos, size, visible),
    m_text(nullptr),
    m_textExtent(0),
    m_down(false),
    m_kind(kind)
{
  objectType().uiButton = true;

  windowProps().focusable = true;

  windowStyle().borderSize         = 1;
  windowStyle().focusedBorderSize  = 2;
  windowStyle().borderColor        = RGB(1, 1, 1);
  windowStyle().focusedBorderColor = RGB(0, 0, 3);

  setText(text);
}


uiButton::~uiButton()
{
  free(m_text);
}


void uiButton::setText(char const * value)
{
  int len = strlen(value);
  m_text = (char*) realloc(m_text, len + 1);
  strcpy(m_text, value);

  m_textExtent = Canvas.textExtent(m_buttonStyle.textFont, value);
}


void uiButton::paintButton()
{
  Rect bkgRect = uiControl::clientRect(uiOrigin::Window);
  // background
  RGB bkColor = m_down ? m_buttonStyle.downBackgroundColor : m_buttonStyle.backgroundColor;
  if (app()->capturedMouseWindow() == this)
    bkColor = m_buttonStyle.mouseDownBackgroundColor;
  else if (isMouseOver())
    bkColor = m_buttonStyle.mouseOverBackgroundColor;
  Canvas.setBrushColor(bkColor);
  Canvas.fillRectangle(bkgRect);
  // content (text and bitmap)
  paintContent(bkgRect);
}


void uiButton::paintContent(Rect const & rect)
{
  Bitmap const * bitmap = m_down ? m_buttonStyle.downBitmap : m_buttonStyle.bitmap;
  int textHeight        = m_buttonStyle.textFont->height;
  int bitmapWidth       = bitmap ? bitmap->width : 0;
  int bitmapHeight      = bitmap ? bitmap->height : 0;
  int bitmapTextSpace   = bitmap ? m_buttonStyle.bitmapTextSpace : 0;

  int x = rect.X1 + (rect.size().width - m_textExtent - bitmapTextSpace - bitmapWidth) / 2;
  int y = rect.Y1 + (rect.size().height - imax(textHeight, bitmapHeight)) / 2;

  if (bitmap) {
    Canvas.drawBitmap(x, y, bitmap);
    x += bitmapWidth + bitmapTextSpace;
    y += (imax(textHeight, bitmapHeight) - textHeight) / 2;
  }
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
  Canvas.setPenColor(m_buttonStyle.textColor);
  Canvas.drawText(m_buttonStyle.textFont, x, y, m_text);
}


void uiButton::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintButton();
      break;

    case UIEVT_CLICK:
      trigger();
      break;

    case UIEVT_MOUSEENTER:
      repaint();  // to update background color
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1)
        repaint();
      break;

    case UIEVT_MOUSELEAVE:
      repaint();  // to update background
      break;

    case UIEVT_KEYUP:
      if (event->params.key.VK == VK_RETURN || event->params.key.VK == VK_KP_ENTER || event->params.key.VK == VK_SPACE) {
        trigger();
        onClick();
      }
      break;

    default:
      break;
  }
}


// action to perfom on mouse up or keyboard space/enter
void uiButton::trigger()
{
  if (m_kind == uiButtonKind::Switch) {
    m_down = !m_down;
    onChange();
  }
  repaint();
}


void uiButton::setDown(bool value)
{
  if (value != m_down) {
    m_down = value;
    repaint();
  }
}



// uiButton
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
// uiTextEdit


uiTextEdit::uiTextEdit(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible),
    m_text(nullptr),
    m_textLength(0),
    m_textSpace(0),
    m_viewX(0),
    m_cursorCol(0),
    m_selCursorCol(0)
{
  objectType().uiTextEdit = true;

  windowProps().focusable = true;

  windowStyle().defaultCursor = CursorName::CursorTextInput;
  windowStyle().borderColor   = RGB(1, 1, 1);
  windowStyle().borderSize    = 1;

  setText(text);
}


uiTextEdit::~uiTextEdit()
{
  free(m_text);
}


void uiTextEdit::setText(char const * value)
{
  m_textLength = strlen(value);
  checkAllocatedSpace(m_textLength);
  strcpy(m_text, value);
}


void uiTextEdit::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintTextEdit();
      if (m_textEditProps.hasCaret)
        app()->setCaret(true); // force blinking (previous painting may cover caret)
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1) {
        int col = getColFromMouseX(event->params.mouse.status.X);
        moveCursor(col, col);
        repaint();
      }
      break;

    case UIEVT_MOUSEBUTTONUP:
      break;

    case UIEVT_MOUSEENTER:
      repaint();  // to update background color
      break;

    case UIEVT_MOUSELEAVE:
      repaint();  // to update background and border
      break;

    case UIEVT_MOUSEMOVE:
      // dragging mouse? select
      if (app()->capturedMouseWindow() == this)
        moveCursor(getColFromMouseX(event->params.mouse.status.X), m_selCursorCol);
      break;

    case UIEVT_SETFOCUS:
      if (m_textEditProps.hasCaret) {
        updateCaret();
        app()->showCaret(this);
      }
      repaint();
      break;

    case UIEVT_KILLFOCUS:
      if (m_textEditProps.hasCaret)
        app()->showCaret(NULL);
      moveCursor(0, 0);
      repaint();
      break;

    case UIEVT_KEYDOWN:
      handleKeyDown(event->params.key);
      break;

    case UIEVT_DBLCLICK:
      selectWordAt(event->params.mouse.status.X);
      break;

    default:
      break;
  }
}


void uiTextEdit::handleKeyDown(uiKeyEventInfo key)
{
  if (m_textEditProps.allowEdit) {
    switch (key.VK) {

      case VK_BACKSPACE:
        if (m_cursorCol != m_selCursorCol)
          removeSel();  // there is a selection, same behavior of VK_DELETE
        else if (m_cursorCol > 0) {
          // remove character at left
          moveCursor(m_cursorCol - 1, m_cursorCol - 1);
          removeSel();
        }
        break;

      case VK_DELETE:
      case VK_KP_DELETE:
        removeSel();
        break;

      default:
      {
        // normal keys
        int c = Keyboard.virtualKeyToASCII(key.VK);
        if (c >= 0x20 && c != 0x7F) {
          if (m_cursorCol != m_selCursorCol)
            removeSel();  // there is a selection, same behavior of VK_DELETE
          insert(c);
        }
        break;
      }
    }
  }

  switch (key.VK) {

    case VK_LEFT:
    case VK_KP_LEFT:
    {
      // LEFT                : cancel selection and move cursor by one character
      // SHIFT + LEFT        : move cursor and select
      // CTRL + LEFT         : cancel selection and move cursor by one word
      // SHIFT + CTRL + LEFT : move cursor by one word and select
      int newCurCol = key.CTRL ? getWordPosAtLeft() : m_cursorCol - 1;
      moveCursor(newCurCol, (key.SHIFT ? m_selCursorCol : newCurCol));
      break;
    }

    case VK_RIGHT:
    case VK_KP_RIGHT:
    {
      // RIGHT                : cancel selection and move cursor by one character
      // SHIFT + RIGHT        : move cursor and select
      // CTRL + RIGHT         : cancel selection and move cursor by one word
      // SHIFT + CTRL + RIGHT : move cursor by one word and select
      int newCurCol = key.CTRL ? getWordPosAtRight() : m_cursorCol + 1;
      moveCursor(newCurCol, (key.SHIFT ? m_selCursorCol : newCurCol));
      break;
    }

    case VK_HOME:
    case VK_KP_HOME:
      // SHIFT + HOME, select up to Home
      // HOME, move cursor to home
      moveCursor(0, (key.SHIFT ? m_selCursorCol : 0));
      break;

    case VK_END:
    case VK_KP_END:
      // SHIFT + END, select up to End
      // END, move cursor to End
      moveCursor(m_textLength, (key.SHIFT ? m_selCursorCol : m_textLength));
      break;

    default:
    {
      if (key.CTRL) {
        // keys with CTRL
        switch (key.VK) {
          case VK_a:
            // CTRL+A, select all
            moveCursor(m_textLength, 0);
            break;
          default:
            break;
        }
      }
      break;
    }
  }
}


Rect uiTextEdit::getEditRect()
{
  return uiControl::clientRect(uiOrigin::Window);
}


void uiTextEdit::paintTextEdit()
{
  m_contentRect = getEditRect();
  // background
  RGB bkColor = hasFocus() ? m_textEditStyle.focusedBackgroundColor : (isMouseOver() ? m_textEditStyle.mouseOverBackgroundColor : m_textEditStyle.backgroundColor);
  Canvas.setBrushColor(bkColor);
  Canvas.fillRectangle(m_contentRect);
  // content
  paintContent();
}


// get width of specified characted
// return glyph data of the specified character
uint8_t const * uiTextEdit::getCharInfo(char ch, int * width)
{
  uint8_t const * chptr;
  if (m_textEditStyle.textFont->chptr) {
    // variable width
    chptr = m_textEditStyle.textFont->data + m_textEditStyle.textFont->chptr[(int)(ch)];
    *width = *chptr++;
  } else {
    // fixed width
    chptr = m_textEditStyle.textFont->data + ch;
    *width = m_textEditStyle.textFont->width;
  }
  return chptr;
}


void uiTextEdit::paintContent()
{
  m_contentRect = m_contentRect.shrink(2);
  Canvas.setClippingRect(Canvas.getClippingRect().intersection(m_contentRect));
  Canvas.setPenColor(m_textEditStyle.textColor);

  GlyphOptions glyphOpt = GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0);
  if (m_selCursorCol != m_cursorCol)
    glyphOpt.FillBackground(true);
  Canvas.setGlyphOptions(glyphOpt);

  for (int x = m_contentRect.X1 + m_viewX, y = m_contentRect.Y1, col = 0, fontWidth; m_text[col]; ++col, x += fontWidth) {
    uint8_t const * chptr = getCharInfo(m_text[col], &fontWidth);
    if (m_selCursorCol != m_cursorCol && (col == m_selCursorCol || col == m_cursorCol)) {
      glyphOpt.invert = !glyphOpt.invert;
      Canvas.setGlyphOptions(glyphOpt);
    }
    if (x >= m_contentRect.X1 && x <= m_contentRect.X2)
      Canvas.drawGlyph(x, y, fontWidth, m_textEditStyle.textFont->height, chptr, 0);
  }
}


// returns the X coordinate where is character "col"
// return value is < m_contentRect.X1 if "col" is at left of visible area
// return value is > m_contentRect.X2 if "col" is at the right of visible area
int uiTextEdit::charColumnToWindowX(int col)
{
  int x = m_contentRect.X1 + m_viewX;
  for (int curcol = 0, fontWidth; m_text[curcol]; ++curcol, x += fontWidth) {
    getCharInfo(m_text[curcol], &fontWidth);
    if (curcol == col)
      break;
  }
  return x;
}


// update caret coordinates from current pos (m_cursorCol)
void uiTextEdit::updateCaret()
{
  if (m_textEditProps.hasCaret) {
    int x = charColumnToWindowX(m_cursorCol);
    app()->setCaret(Rect(x, m_contentRect.Y1, x, m_contentRect.Y1 + m_textEditStyle.textFont->height));
  }
}


// col (cursor position):
//     0 up to m_textLength. For example having a m_text="1234", min col is 0, max col is 4 (passing last char).
// selCol (selection position):
//     0 up to m_textLength
void uiTextEdit::moveCursor(int col, int selCol)
{
  col    = iclamp(col, 0, m_textLength);
  selCol = iclamp(selCol, 0, m_textLength);

  if (col == m_cursorCol && selCol == m_selCursorCol)
    return; // nothing to do

  bool doRepaint = false;

  // there was a selection, now there is no selection
  if (m_cursorCol != m_selCursorCol && col == selCol)
    doRepaint = true;

  m_cursorCol    = col;
  m_selCursorCol = selCol;

  if (m_cursorCol != m_selCursorCol)
    doRepaint = true;

  // need to scroll?
  int x = charColumnToWindowX(m_cursorCol);

  int prevCharWidth = 0;
  if (col > 0)
    getCharInfo(m_text[col - 1], &prevCharWidth);

  int charWidth;
  getCharInfo(m_text[col < m_textLength ? col : col - 1], &charWidth);

  if (x - prevCharWidth < m_contentRect.X1) {
    // scroll right
    m_viewX += m_contentRect.X1 - (x - prevCharWidth);
    doRepaint = true;
  } else if (x + charWidth > m_contentRect.X2) {
    // scroll left
    m_viewX -= (x + charWidth - m_contentRect.X2);
    doRepaint = true;
  }

  updateCaret();

  if (doRepaint)
    repaint();
}


// return column (that is character index in m_text[]) from specified mouseX
int uiTextEdit::getColFromMouseX(int mouseX)
{
  int col = 0;
  for (int x = m_contentRect.X1 + m_viewX, fontWidth; m_text[col]; ++col, x += fontWidth) {
    getCharInfo(m_text[col], &fontWidth);
    if (mouseX < x || (mouseX >= x && mouseX < x + fontWidth))
      break;
  }
  return col;
}


// requiredLength does NOT include ending zero
void uiTextEdit::checkAllocatedSpace(int requiredLength)
{
  ++requiredLength; // add ending zero
  if (m_textSpace < requiredLength) {
    if (m_textSpace == 0) {
      // first time allocates exact space
      m_textSpace = requiredLength;
    } else {
      // next times allocate double
      while (m_textSpace < requiredLength)
        m_textSpace *= 2;
    }
    m_text = (char*) realloc(m_text, m_textSpace);
  }
}


// insert specified char at m_cursorCol
void uiTextEdit::insert(char c)
{
  ++m_textLength;
  checkAllocatedSpace(m_textLength);
  memmove(m_text + m_cursorCol + 1, m_text + m_cursorCol, m_textLength - m_cursorCol);
  m_text[m_cursorCol] = c;
  moveCursor(m_cursorCol + 1, m_cursorCol + 1);
  onChange();
  repaint();
}


// remove from m_cursorCol to m_selCursorCol
void uiTextEdit::removeSel()
{
  if (m_textLength > 0) {
    if (m_cursorCol > m_selCursorCol)
      iswap(m_cursorCol, m_selCursorCol);
    int count = imax(1, m_selCursorCol - m_cursorCol);
    if (m_cursorCol < m_textLength) {
      memmove(m_text + m_cursorCol, m_text + m_cursorCol + count, m_textLength - m_cursorCol - count + 1);
      m_textLength -= count;
      moveCursor(m_cursorCol, m_cursorCol);
      onChange();
      repaint();
    }
  }
}


// return starting position of next word at left of m_cursorCol
int uiTextEdit::getWordPosAtLeft()
{
  int col = m_cursorCol - 1;
  while (col > 0 && (!isspace(m_text[col - 1]) || isspace(m_text[col])))
    --col;
  return imax(0, col);
}


// return starting position of next word at right of m_cursorCol
int uiTextEdit::getWordPosAtRight()
{
  int col = m_cursorCol + 1;
  while (col < m_textLength && (!isspace(m_text[col - 1]) || isspace(m_text[col])))
    ++col;
  return imin(m_textLength, col);
}


// if mouseX is at space, select all space at left and right
// if mouseX is at character, select all characters at left and right
void uiTextEdit::selectWordAt(int mouseX)
{
  int col = getColFromMouseX(mouseX), left = col, right = col;
  bool lspc = isspace(m_text[col]);  // look for spaces?
  while (left > 0 && (bool)isspace(m_text[left - 1]) == lspc)
    --left;
  while (right < m_textLength && (bool)isspace(m_text[right]) == lspc)
    ++right;
  moveCursor(left, right);
}



// uiTextEdit
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
// uiLabel


uiLabel::uiLabel(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible),
    m_text(nullptr),
    m_textExtent(0)
{
  objectType().uiLabel = true;

  windowProps().focusable = false;
  windowStyle().borderSize = 0;
  m_autoSize = (size.width == 0 && size.height == 0);
  setText(text);
}


uiLabel::~uiLabel()
{
  free(m_text);
}


void uiLabel::setText(char const * value)
{
  int len = strlen(value);
  m_text = (char*) realloc(m_text, len + 1);
  strcpy(m_text, value);
  update();
}


void uiLabel::setTextFmt(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    setText(buf);
  }
  va_end(ap);
}


void uiLabel::update()
{
  m_textExtent = Canvas.textExtent(m_labelStyle.textFont, m_text);
  if (m_autoSize)
    app()->resizeWindow(this, m_textExtent, m_labelStyle.textFont->height);
  repaint();
}


void uiLabel::paintLabel()
{
  Rect r = uiControl::clientRect(uiOrigin::Window);
  Canvas.setBrushColor(m_labelStyle.backgroundColor);
  Canvas.fillRectangle(r);
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
  Canvas.setPenColor(m_labelStyle.textColor);
  int x = r.X1;
  int y = r.Y1 + (r.height() - m_labelStyle.textFont->height) / 2;
  Canvas.drawText(m_labelStyle.textFont, x, y, m_text);
}


void uiLabel::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintLabel();
      break;

    default:
      break;
  }
}


// uiLabel
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
// uiImage
// TODO? bitmap is not copied, but just referenced


uiImage::uiImage(uiWindow * parent, Bitmap const * bitmap, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible),
    m_bitmap(nullptr)
{
  objectType().uiImage = true;

  windowProps().focusable = false;
  windowStyle().borderSize = 0;
  m_autoSize = (size.width == 0 && size.height == 0);
  setBitmap(bitmap);
}


uiImage::~uiImage()
{
}


void uiImage::setBitmap(Bitmap const * bitmap)
{
  m_bitmap = bitmap;

  if (m_autoSize && bitmap)
    app()->resizeWindow(this, bitmap->width, bitmap->height);
}


void uiImage::paintImage()
{
  Rect r = uiControl::clientRect(uiOrigin::Window);
  Canvas.setBrushColor(m_imageStyle.backgroundColor);
  Canvas.fillRectangle(r);
  if (m_bitmap) {
    int x = r.X1 + (r.X2 + 1 - m_bitmap->width) / 2;
    int y = r.Y1 + (r.Y2 + 1 - m_bitmap->height) / 2;
    Canvas.drawBitmap(x, y, m_bitmap);
  }
}


void uiImage::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintImage();
      break;

    default:
      break;
  }
}


// uiImage
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiPanel


uiPanel::uiPanel(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible)
{
  objectType().uiPanel = true;

  windowProps().focusable = false;
  windowStyle().borderSize = 1;
  windowStyle().borderColor = RGB(1, 1, 1);
}


uiPanel::~uiPanel()
{
}


void uiPanel::paintPanel()
{
  Rect bkgRect = uiControl::clientRect(uiOrigin::Window);
  // background
  Canvas.setBrushColor(m_panelStyle.backgroundColor);
  Canvas.fillRectangle(bkgRect);
}


void uiPanel::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintPanel();
      break;

    default:
      break;
  }
}


// uiPanel
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiPaintBox


uiPaintBox::uiPaintBox(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiScrollableControl(parent, pos, size, visible)
{
  objectType().uiPaintBox = true;

  windowProps().focusable = false;
  windowStyle().borderSize  = 1;
  windowStyle().borderColor = RGB(1, 1, 1);
}


uiPaintBox::~uiPaintBox()
{
}


void uiPaintBox::paintPaintBox()
{
  Rect bkgRect = uiScrollableControl::clientRect(uiOrigin::Window);

  // background
  Canvas.setBrushColor(m_paintBoxStyle.backgroundColor);
  Canvas.fillRectangle(bkgRect);

  onPaint(bkgRect);
}


void uiPaintBox::processEvent(uiEvent * event)
{
  uiScrollableControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiScrollableControl::clientRect(uiOrigin::Window));
      paintPaintBox();
      break;

    default:
      break;
  }
}


// uiPaintBox
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiScrollableControl


uiScrollableControl::uiScrollableControl(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible),
    m_HScrollBarPosition(0),
    m_HScrollBarVisible(0),
    m_HScrollBarRange(0),
    m_VScrollBarPosition(0),
    m_VScrollBarVisible(0),
    m_VScrollBarRange(0),
    m_mouseOverItem(uiScrollBarItem::None),
    m_scrollTimer(nullptr)
{
  objectType().uiScrollableControl = true;
}


uiScrollableControl::~uiScrollableControl()
{
  if (m_scrollTimer)
    app()->killTimer(m_scrollTimer);
}


// position: The position of the scrollbar in scroll units.
// visible: The size of the visible portion of the scrollbar, in scroll units.
// range: The maximum position of the scrollbar
void uiScrollableControl::setScrollBar(uiOrientation orientation, int position, int visible, int range, bool repaintScrollbar)
{
  position = iclamp(position, 0, range - visible);
  switch (orientation) {
    case uiOrientation::Vertical:
    {
      bool changedPos = (m_VScrollBarPosition != position);
      if (m_VScrollBarVisible != visible || m_VScrollBarRange != range || changedPos) {
        m_VScrollBarVisible  = visible;
        m_VScrollBarRange    = range;
        m_VScrollBarPosition = position;
        if (repaintScrollbar)
          repaintScrollBar(orientation);
        if (changedPos)
          onChangeVScrollBar();
      }
      break;
    }
    case uiOrientation::Horizontal:
    {
      bool changedPos = (m_HScrollBarPosition != position);
      if (m_HScrollBarVisible != visible || m_HScrollBarRange != range || changedPos) {
        m_HScrollBarVisible  = visible;
        m_HScrollBarRange    = range;
        m_HScrollBarPosition = position;
        if (repaintScrollbar)
          repaintScrollBar(orientation);
        if (changedPos)
          onChangeHScrollBar();
      }
      break;
    }
  };
}


void uiScrollableControl::repaintScrollBar(uiOrientation orientation)
{
  repaint( orientation == uiOrientation::Vertical ? getVScrollBarRects() : getHScrollBarRects() );
}


void uiScrollableControl::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintScrollableControl();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1) {
        m_mouseDownHScrollBarPosition = m_HScrollBarPosition;
        m_mouseDownVScrollBarPosition = m_VScrollBarPosition;
        if (m_mouseOverItem == uiScrollBarItem::LeftButton || m_mouseOverItem == uiScrollBarItem::RightButton ||
            m_mouseOverItem == uiScrollBarItem::TopButton || m_mouseOverItem == uiScrollBarItem::BottomButton) {
          // start the timer to repeat buttons
          m_scrollTimer = app()->setTimer(this, 250);
          handleButtonsScroll();
        } else {
          handlePageScroll();
        }
        app()->combineMouseMoveEvents(true);
      }
      break;

    case UIEVT_MOUSEBUTTONUP:
      if (event->params.mouse.changedButton == 1) {
        app()->combineMouseMoveEvents(false);
        if (m_scrollTimer) {
          app()->killTimer(m_scrollTimer);
          m_scrollTimer = nullptr;
        }
      }
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this)
        handleCapturedMouseMove(event->params.mouse.status.X, event->params.mouse.status.Y);
      else
        handleFreeMouseMove(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_MOUSEWHEEL:
      if (m_VScrollBarRange)
        setScrollBar(uiOrientation::Vertical, m_VScrollBarPosition + event->params.mouse.status.wheelDelta, m_VScrollBarVisible, m_VScrollBarRange);
      break;

    case UIEVT_TIMER:
      if (event->params.timerHandle == m_scrollTimer)
        handleButtonsScroll();
      break;

    default:
      break;
  }
}


void uiScrollableControl::handleButtonsScroll()
{
  switch (m_mouseOverItem) {
    case uiScrollBarItem::LeftButton:
      setScrollBar(uiOrientation::Horizontal, m_HScrollBarPosition - 1, m_HScrollBarVisible, m_HScrollBarRange);
      break;
    case uiScrollBarItem::RightButton:
      setScrollBar(uiOrientation::Horizontal, m_HScrollBarPosition + 1, m_HScrollBarVisible, m_HScrollBarRange);
      break;
    case uiScrollBarItem::TopButton:
      setScrollBar(uiOrientation::Vertical, m_VScrollBarPosition - 1, m_VScrollBarVisible, m_VScrollBarRange);
      break;
    case uiScrollBarItem::BottomButton:
      setScrollBar(uiOrientation::Vertical, m_VScrollBarPosition + 1, m_VScrollBarVisible, m_VScrollBarRange);
      break;
    default:
      break;
  }
}


void uiScrollableControl::handlePageScroll()
{
  switch (m_mouseOverItem) {
    case uiScrollBarItem::PageLeft:
      setScrollBar(uiOrientation::Horizontal, m_HScrollBarPosition - m_HScrollBarVisible, m_HScrollBarVisible, m_HScrollBarRange);
      break;
    case uiScrollBarItem::PageRight:
      setScrollBar(uiOrientation::Horizontal, m_HScrollBarPosition + m_HScrollBarVisible, m_HScrollBarVisible, m_HScrollBarRange);
      break;
    case uiScrollBarItem::PageUp:
      setScrollBar(uiOrientation::Vertical, m_VScrollBarPosition - m_VScrollBarVisible, m_VScrollBarVisible, m_VScrollBarRange);
      break;
    case uiScrollBarItem::PageDown:
      setScrollBar(uiOrientation::Vertical, m_VScrollBarPosition + m_VScrollBarVisible, m_VScrollBarVisible, m_VScrollBarRange);
      break;
    default:
      break;
  }
}


void uiScrollableControl::handleFreeMouseMove(int mouseX, int mouseY)
{
  auto prev = m_mouseOverItem;
  m_mouseOverItem = getItemAt(mouseX, mouseY);
  if (m_mouseOverItem != prev) {
    if (m_VScrollBarRange)
      repaintScrollBar(uiOrientation::Vertical);
    if (m_HScrollBarRange)
      repaintScrollBar(uiOrientation::Horizontal);
  }
}


void uiScrollableControl::handleCapturedMouseMove(int mouseX, int mouseY)
{
  if (m_mouseOverItem == uiScrollBarItem::HBar) {
    // dragging horizontal bar
    int offset = mouseX - mouseDownPos().X;
    int newPos = m_mouseDownHScrollBarPosition + offset * m_HScrollBarRange / m_HBarArea;
    setScrollBar(uiOrientation::Horizontal, newPos, m_HScrollBarVisible, m_HScrollBarRange);
  } else if (m_mouseOverItem == uiScrollBarItem::VBar) {
    // dragging vertical bar
    int offset = mouseY - mouseDownPos().Y;
    int newPos = m_mouseDownVScrollBarPosition + offset * m_VScrollBarRange / m_VBarArea;
    setScrollBar(uiOrientation::Vertical, newPos, m_VScrollBarVisible, m_VScrollBarRange);
  }
}


uiScrollBarItem uiScrollableControl::getItemAt(int x, int y)
{
  if (m_HScrollBarRange) {
    Rect lbtn, rbtn, bar;
    Rect box = getHScrollBarRects(&lbtn, &rbtn, &bar);
    if (lbtn.contains(x, y))
      return uiScrollBarItem::LeftButton;
    if (rbtn.contains(x, y))
      return uiScrollBarItem::RightButton;
    if (bar.contains(x, y))
      return uiScrollBarItem::HBar;
    if (box.contains(x, y))
      return x < bar.X1 ? uiScrollBarItem::PageLeft : uiScrollBarItem::PageRight;
  }
  if (m_VScrollBarRange) {
    Rect tbtn, bbtn, bar;
    Rect box = getVScrollBarRects(&tbtn, &bbtn, &bar);
    if (tbtn.contains(x, y))
      return uiScrollBarItem::TopButton;
    if (bbtn.contains(x, y))
      return uiScrollBarItem::BottomButton;
    if (bar.contains(x, y))
      return uiScrollBarItem::VBar;
    if (box.contains(x, y))
      return y < bar.Y1 ? uiScrollBarItem::PageUp: uiScrollBarItem::PageDown;
  }
  return uiScrollBarItem::None;
}


Rect uiScrollableControl::getVScrollBarRects(Rect * topButton, Rect * bottomButton, Rect * bar)
{
  Rect cArea = uiControl::clientRect(uiOrigin::Window);
  const int sbSize = m_scrollableControlStyle.scrollBarSize;
  Rect box = Rect(cArea.X2 + 1 - sbSize, cArea.Y1, cArea.X2, cArea.Y2 - (m_HScrollBarRange ? sbSize : 0));
  if (topButton && bottomButton && bar) {
    // buttons
    *topButton    = Rect(box.X1 + 2, box.Y1 + 2, box.X2 - 2, box.Y1 + sbSize - 2);
    *bottomButton = Rect(box.X1 + 2, box.Y2 - sbSize + 2, box.X2 - 2, box.Y2 - 2);
    // the bar
    int barAreaY1 = topButton->Y2 + 2;
    int barAreaY2 = bottomButton->Y1 - 2;
    m_VBarArea = barAreaY2 - barAreaY1 + 1;
    int barOffsetY = m_VScrollBarPosition * m_VBarArea / m_VScrollBarRange;
    int barHeight  = m_VScrollBarVisible * m_VBarArea / m_VScrollBarRange;
    *bar = Rect(box.X1 + 1, barAreaY1 + barOffsetY, box.X2 - 1, barAreaY1 + barOffsetY + barHeight);
  }
  return box;
}


Rect uiScrollableControl::getHScrollBarRects(Rect * leftButton, Rect * rightButton, Rect * bar)
{
  Rect cArea = uiControl::clientRect(uiOrigin::Window);
  const int sbSize = m_scrollableControlStyle.scrollBarSize;
  Rect box = Rect(cArea.X1, cArea.Y2 + 1 - sbSize, cArea.X2 - (m_VScrollBarRange ? sbSize : 0), cArea.Y2);
  if (leftButton && rightButton && bar) {
    // buttons
    *leftButton  = Rect(box.X1 + 2, box.Y1 + 2, box.X1 + sbSize - 2, box.Y2 - 2);
    *rightButton = Rect(box.X2 - sbSize + 2, box.Y1 + 2, box.X2 - 2, box.Y2 - 2);
    // the bar
    int barAreaX1 = leftButton->X2 + 2;
    int barAreaX2 = rightButton->X1 - 2;
    m_HBarArea = barAreaX2 - barAreaX1 + 1;
    int barOffsetX = m_HScrollBarPosition * m_HBarArea / m_HScrollBarRange;
    int barWidth   = m_HScrollBarVisible * m_HBarArea / m_HScrollBarRange;
    *bar = Rect(barAreaX1 + barOffsetX, box.Y1 + 1, barAreaX1 + barOffsetX + barWidth, box.Y2 - 1);
  }
  return box;
}


void uiScrollableControl::paintScrollableControl()
{
  RGB FColor          = m_scrollableControlStyle.scrollBarForegroundColor;
  RGB mouseOverFColor = m_scrollableControlStyle.mouseOverScrollBarForegroundColor;
  if (m_HScrollBarRange) {
    //// paint horizontal scroll bar (at bottom side of the window)
    Rect lbtn, rbtn, bar;
    Rect box = getHScrollBarRects(&lbtn, &rbtn, &bar);
    // background
    Canvas.setBrushColor(m_scrollableControlStyle.scrollBarBackgroundColor);
    Canvas.setPenColor(m_scrollableControlStyle.scrollBarBackgroundColor);
    Canvas.fillRectangle(Rect(box.X1, box.Y1, bar.X1 - 1, box.Y2)); // left part
    Canvas.fillRectangle(Rect(bar.X2 + 1, box.Y1, box.X2, box.Y2)); // right part
    Canvas.drawLine(bar.X1, box.Y1, bar.X2, box.Y1);  // fill line above the bar
    Canvas.drawLine(bar.X1, box.Y2, bar.X2, box.Y2);  // fill line below the bar
    // buttons (arrows)
    Canvas.setPenColor(m_mouseOverItem == uiScrollBarItem::LeftButton ? mouseOverFColor : FColor);
    Canvas.drawLine(lbtn.X2, lbtn.Y1, lbtn.X1, lbtn.Y1 + lbtn.height() / 2);
    Canvas.drawLine(lbtn.X1, lbtn.Y1 + lbtn.height() / 2, lbtn.X2, lbtn.Y2);
    Canvas.setPenColor(m_mouseOverItem == uiScrollBarItem::RightButton ? mouseOverFColor : FColor);
    Canvas.drawLine(rbtn.X1, rbtn.Y1, rbtn.X2, rbtn.Y1 + lbtn.height() / 2);
    Canvas.drawLine(rbtn.X2, rbtn.Y1 + lbtn.height() / 2, rbtn.X1, rbtn.Y2);
    // the bar
    Canvas.setBrushColor(m_mouseOverItem == uiScrollBarItem::HBar ? mouseOverFColor : FColor);
    Canvas.fillRectangle(bar);
  }
  if (m_VScrollBarRange) {
    // paint vertical scroll bar (at right side of the window)
    Rect ubtn, bbtn, bar;
    Rect box = getVScrollBarRects(&ubtn, &bbtn, &bar);
    // background
    Canvas.setBrushColor(m_scrollableControlStyle.scrollBarBackgroundColor);
    Canvas.setPenColor(m_scrollableControlStyle.scrollBarBackgroundColor);
    Canvas.fillRectangle(Rect(box.X1, box.Y1, box.X2, bar.Y1 - 1)); // upper part
    Canvas.fillRectangle(Rect(box.X1, bar.Y2 + 1, box.X2, box.Y2)); // bottom part
    Canvas.drawLine(box.X1, bar.Y1, box.X1, bar.Y2);  // fill line at left of the bar
    Canvas.drawLine(box.X2, bar.Y1, box.X2, bar.Y2);  // fill line at right of the bar
    // fill box between scrollbars
    if (m_HScrollBarRange)
      Canvas.fillRectangle(Rect(box.X1, box.Y2 + 1, box.X2, box.Y2 + m_scrollableControlStyle.scrollBarSize));
    // buttons (arrows)
    Canvas.setPenColor(m_mouseOverItem == uiScrollBarItem::TopButton ? mouseOverFColor : FColor);
    Canvas.drawLine(ubtn.X1, ubtn.Y2, ubtn.X1 + ubtn.width() / 2, ubtn.Y1);
    Canvas.drawLine(ubtn.X1 + ubtn.width() / 2, ubtn.Y1, ubtn.X2, ubtn.Y2);
    Canvas.setPenColor(m_mouseOverItem == uiScrollBarItem::BottomButton ? mouseOverFColor : FColor);
    Canvas.drawLine(bbtn.X1, bbtn.Y1, bbtn.X1 + bbtn.width() / 2, bbtn.Y2);
    Canvas.drawLine(bbtn.X1 + bbtn.width() / 2, bbtn.Y2, bbtn.X2, bbtn.Y1);
    // the bar
    Canvas.setBrushColor(m_mouseOverItem == uiScrollBarItem::VBar ? mouseOverFColor : FColor);
    Canvas.fillRectangle(bar);
  }
}


Rect uiScrollableControl::clientRect(uiOrigin origin)
{
  Rect r = uiControl::clientRect(origin);
  r.X2 -= (m_VScrollBarRange ? m_scrollableControlStyle.scrollBarSize : 0);
  r.Y2 -= (m_HScrollBarRange ? m_scrollableControlStyle.scrollBarSize : 0);
  return r;
}


// uiScrollableControl
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
// uiCustomListBox


uiCustomListBox::uiCustomListBox(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiScrollableControl(parent, pos, size, visible),
    m_firstVisibleItem(0)
{
  objectType().uiCustomListBox = true;

  windowProps().focusable = true;

  windowStyle().borderSize  = 1;
  windowStyle().borderColor = RGB(1, 1, 1);
}


uiCustomListBox::~uiCustomListBox()
{
}


void uiCustomListBox::processEvent(uiEvent * event)
{
  uiScrollableControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiScrollableControl::clientRect(uiOrigin::Window));
      paintListBox();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1)
        handleMouseDown(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_KEYDOWN:
      handleKeyDown(event->params.key);
      break;

    case UIEVT_KEYUP:
      onKeyUp(event->params.key);
      break;

    case UIEVT_KILLFOCUS:
      onKillFocus();
      break;

    case UIEVT_SHOW:
      makeItemVisible(firstSelectedItem());
      break;

    default:
      break;
  }
}


void uiCustomListBox::handleKeyDown(uiKeyEventInfo key)
{
  bool shift = key.SHIFT;
  switch (key.VK) {
    case VK_UP:
    case VK_KP_UP:
      selectItem(firstSelectedItem() - 1, shift, false);
      break;

    case VK_DOWN:
    case VK_KP_DOWN:
      selectItem(lastSelectedItem() + 1, shift, false);
      break;

    case VK_PAGEUP:
    case VK_KP_PAGEUP:
      selectItem(firstSelectedItem() - VScrollBarVisible(), shift, false);
      break;

    case VK_PAGEDOWN:
    case VK_KP_PAGEDOWN:
      selectItem(lastSelectedItem() + VScrollBarVisible(), shift, false);
      break;

    case VK_HOME:
    case VK_KP_HOME:
      selectItem(0, shift, shift);
      break;

    case VK_END:
    case VK_KP_END:
      selectItem(items_getCount() - 1, shift, shift);
      break;

    default:
      break;
  }
}


void uiCustomListBox::selectItem(int index, bool add, bool range)
{
  if (items_getCount() > 0) {
    index = iclamp(index, 0, items_getCount() - 1);
    int first = firstSelectedItem();
    if (!add)
      items_deselectAll();
    if (range) {
      if (index <= first) {
        for (int i = index; i <= first; ++i)
          items_select(i, true);
      } else {
        for (int i = index; i >= first; --i)
          items_select(i, true);
      }
    } else {
      items_select(index, true);
    }

    // make sure the selected item is visible
    makeItemVisible(index);

    onChange();

    repaint();
  }
}


void uiCustomListBox::makeItemVisible(int index)
{
  if (VScrollBarVisible()) {
    if (index < m_firstVisibleItem)
      m_firstVisibleItem = index;
    else if (index >= m_firstVisibleItem + VScrollBarVisible())
      m_firstVisibleItem = index - VScrollBarVisible() + 1;
  }
}


void uiCustomListBox::deselectAll()
{
  items_deselectAll();
  onChange();
  repaint();
}


void uiCustomListBox::paintListBox()
{
  Rect cliRect = uiScrollableControl::clientRect(uiOrigin::Window);
  Rect itmRect = Rect(cliRect.X1, cliRect.Y1, cliRect.X2, cliRect.Y1 + m_listBoxStyle.itemHeight - 1);

  // do we need a vert scrollbar?
  if (itmRect.height() * items_getCount() > cliRect.height()) {
    int visible = cliRect.height() / itmRect.height();
    int range = items_getCount();
    if (!VScrollBarVisible() || visible != VScrollBarVisible() || range != VScrollBarRange() || m_firstVisibleItem != VScrollBarPos()) {
      // show vertical scrollbar
      setScrollBar(uiOrientation::Vertical, m_firstVisibleItem, visible, range, false);
      repaint();
      return;
    }
  } else if (VScrollBarVisible()) {
    // hide vertical scrollbar
    m_firstVisibleItem = 0;
    setScrollBar(uiOrientation::Vertical, 0, 0, 0, false);
    repaint();
    return;
  }

  int index = m_firstVisibleItem;
  while (true) {
    if (!itmRect.intersects(cliRect))
      break;

    // background
    RGB bkColor = hasFocus() ? m_listBoxStyle.focusedBackgroundColor : m_listBoxStyle.backgroundColor;
    if (index < items_getCount() && items_selected(index))
      bkColor = (hasFocus() ? m_listBoxStyle.focusedSelectedBackgroundColor : m_listBoxStyle.selectedBackgroundColor);
    Canvas.setBrushColor(bkColor);
    Canvas.fillRectangle(itmRect);

    if (index < items_getCount()) {
      // text
      Canvas.setPenColor(items_selected(index) ? m_listBoxStyle.selectedTextColor : m_listBoxStyle.textColor);
      items_draw(index, itmRect);
    }

    // move to next item
    itmRect = itmRect.translate(0, m_listBoxStyle.itemHeight);
    ++index;
  }

}


// get first selected item (-1 = no selected item)
int uiCustomListBox::firstSelectedItem()
{
  for (int i = 0; i < items_getCount(); ++i)
    if (items_selected(i))
      return i;
  return -1;
}


// get last selected item (-1 = no selected item)
int uiCustomListBox::lastSelectedItem()
{
  for (int i = items_getCount() - 1; i >= 0; --i)
    if (items_selected(i))
      return i;
  return -1;
}


void uiCustomListBox::setScrollBar(uiOrientation orientation, int position, int visible, int range, bool repaintScrollbar)
{
  uiScrollableControl::setScrollBar(orientation, position, visible, range, false);
  if (VScrollBarVisible() && m_firstVisibleItem != VScrollBarPos()) {
    m_firstVisibleItem = VScrollBarPos();
    repaint();
  }
}


int uiCustomListBox::getItemAtMousePos(int mouseX, int mouseY)
{
  Rect cliRect = uiScrollableControl::clientRect(uiOrigin::Window);
  if (cliRect.contains(mouseX, mouseY)) {
    int idx = m_firstVisibleItem + (mouseY - cliRect.Y1) / m_listBoxStyle.itemHeight;
    if (idx < items_getCount())
      return idx;
  }
  return -1;
}


void uiCustomListBox::handleMouseDown(int mouseX, int mouseY)
{
  int idx = getItemAtMousePos(mouseX, mouseY);
  if (idx > -1) {
    if (Keyboard.isVKDown(VK_LCTRL) || Keyboard.isVKDown(VK_RCTRL)) {
      // CTRL is down
      items_select(idx, !items_selected(idx));
    } else {
      // CTRL is up
      items_deselectAll();
      items_select(idx, true);
    }
  } else
    items_deselectAll();
  onChange();
  repaint();
}


// uiCustomListBox
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// uiListBox


uiListBox::uiListBox(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiCustomListBox(parent, pos, size, visible)
{
  objectType().uiListBox = true;
}


void uiListBox::items_draw(int index, const Rect & itemRect)
{
  int x = itemRect.X1 + 1;
  int y = itemRect.Y1 + (itemRect.height() - listBoxStyle().textFont->height) / 2;
  Canvas.drawText(listBoxStyle().textFont, x, y, m_items.get(index));
}


// uiListBox
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFileBrowser


uiFileBrowser::uiFileBrowser(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiCustomListBox(parent, pos, size, visible),
    m_selected(-1)
{
  objectType().uiFileBrowser = true;
}


void uiFileBrowser::items_draw(int index, const Rect & itemRect)
{
  int x = itemRect.X1 + 1;
  int y = itemRect.Y1 + (itemRect.height() - listBoxStyle().textFont->height) / 2;
  Canvas.drawText(listBoxStyle().textFont, x, y, m_dir.get(index)->name);
  if (m_dir.get(index)->isDir) {
    static const char * DIRTXT = "[dir]";
    int x = itemRect.X2 - Canvas.textExtent(listBoxStyle().textFont, DIRTXT);
    Canvas.drawText(listBoxStyle().textFont, x, y, DIRTXT);
  }
}


void uiFileBrowser::items_select(int index, bool select)
{
  if (select)
    m_selected = index;
  else if (index == m_selected || index == -1)
    m_selected = -1;
}


void uiFileBrowser::setDirectory(char const * path)
{
  m_dir.setDirectory(path);
  m_selected = m_dir.count() > 0 ? 0 : -1;
  repaint();
}


char const * uiFileBrowser::filename()
{
  return m_selected >= 0 ? m_dir.get(m_selected)->name : nullptr;
}


bool uiFileBrowser::isDirectory()
{
  return m_selected >= 0 ? m_dir.get(m_selected)->isDir : false;
}


void uiFileBrowser::enterSubDir()
{
  if (m_selected >= 0) {
    auto selItem = m_dir.get(m_selected);
    if (selItem->isDir) {
      m_dir.changeDirectory(selItem->name);
      m_selected = 0;
      onChange();
      repaint();
    }
  }
}


void uiFileBrowser::update()
{
  m_dir.reload();
  m_selected = imin(m_dir.count() - 1, m_selected);
  onChange();
  repaint();
}


void uiFileBrowser::processEvent(uiEvent * event)
{
  uiCustomListBox::processEvent(event);

  switch (event->id) {

    case UIEVT_KEYDOWN:
      if (event->params.key.VK == VK_RETURN)
        enterSubDir();
      else if (event->params.key.VK == VK_BACKSPACE) {
        // backspace moves to parent dir
        m_selected = 0; // select ".."
        enterSubDir();
      }
      break;

    case UIEVT_KEYUP:
      break;

    case UIEVT_DBLCLICK:
      enterSubDir();
      break;

    default:
      break;
  }
}


// uiFileBrowser
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// uiComboBox


uiComboBox::uiComboBox(uiWindow * parent, const Point & pos, const Size & size, int listHeight, bool visible)
  : uiTextEdit(parent, "", pos, size, visible),
    m_listBox(new uiListBox(parent, Point(0, 0), Size(0, 0), false)),
    m_listHeight(listHeight)
{
  objectType().uiComboBox = true;

  textEditProps().hasCaret  = false;
  textEditProps().allowEdit = false;

  m_listBox->onKillFocus = [&]() {
    closeListBox();
  };

  m_listBox->onChange = [&]() {
    updateTextEdit();
    onChange();
  };

  m_listBox->onKeyUp = [&](uiKeyEventInfo key) {
    if (key.VK == VK_RETURN) {
      closeListBox();
      app()->setFocusedWindow(this);
    }
  };
}


uiComboBox::~uiComboBox()
{
}


// index = -1 -> deselect all
void uiComboBox::selectItem(int index)
{
  if (index < 0)
    m_listBox->deselectAll();
  else
    m_listBox->selectItem(index);
  updateTextEdit();
}


// refresh text edit with the selected listbox item
void uiComboBox::updateTextEdit()
{
  int idx = selectedItem();
  setText(idx > -1 ? items().get(idx) : "");
  repaint();
}


void uiComboBox::processEvent(uiEvent * event)
{
  uiTextEdit::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiTextEdit::clientRect(uiOrigin::Window));
      paintComboBox();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1 && getButtonRect().contains(event->params.mouse.status.X, event->params.mouse.status.Y))
        switchListBox();
      break;

    case UIEVT_SETFOCUS:
      if (m_comboBoxProps.openOnFocus && event->params.oldFocused != m_listBox)
        openListBox();
      break;

    case UIEVT_KILLFOCUS:
      if (event->params.newFocused != m_listBox)
        closeListBox();
      break;

    case UIEVT_KEYDOWN:
      m_listBox->processEvent(event);
      break;

    case UIEVT_KEYUP:
      // ALT-DOWN or ALT-UP or ENTER opens listbox
      if (((event->params.key.RALT || event->params.key.LALT) && (event->params.key.VK == VK_DOWN || event->params.key.VK == VK_UP)) || (event->params.key.VK == VK_RETURN))
        openListBox();
      break;

    default:
      break;
  }
}


void uiComboBox::openListBox()
{
  Rect r = rect(uiOrigin::Parent);
  r.Y1 = r.Y2 + 1;
  r.Y2 = r.Y1 + m_listHeight;
  m_listBox->bringOnTop();
  app()->reshapeWindow(m_listBox, r);
  app()->showWindow(m_listBox, true);
  app()->setFocusedWindow(m_listBox);
}


void uiComboBox::closeListBox()
{
  app()->showWindow(m_listBox, false);
}


void uiComboBox::switchListBox()
{
  if (m_listBox->state().visible)
    closeListBox();
  else
    openListBox();
}


int uiComboBox::buttonWidth()
{
  return size().height / 2;
}


Rect uiComboBox::getEditRect()
{
  Rect r = uiTextEdit::getEditRect();
  r.X2 -= buttonWidth();
  return r;
}


Rect uiComboBox::getButtonRect()
{
  Rect btnRect = uiTextEdit::getEditRect();
  btnRect.X1 = btnRect.X2 - buttonWidth() + 1;
  return btnRect;
}


void uiComboBox::paintComboBox()
{
  Rect btnRect = getButtonRect();

  // button background
  Canvas.setBrushColor(m_comboBoxStyle.buttonBackgroundColor);
  Canvas.fillRectangle(btnRect);

  // button glyph
  Canvas.setPenColor(m_comboBoxStyle.buttonColor);
  Rect arrowRect = btnRect.hShrink(1).vShrink(2);
  int hHeight = arrowRect.height() / 2;
  int hWidth  = arrowRect.width() / 2;
  constexpr int vDist = 2;
  Canvas.drawLine(arrowRect.X1, arrowRect.Y1 + hHeight - vDist, arrowRect.X1 + hWidth, arrowRect.Y1);
  Canvas.drawLine(arrowRect.X1 + hWidth, arrowRect.Y1, arrowRect.X2, arrowRect.Y1 + hHeight - vDist);
  Canvas.drawLine(arrowRect.X1, arrowRect.Y1 + hHeight + vDist, arrowRect.X1 + hWidth, arrowRect.Y2);
  Canvas.drawLine(arrowRect.X1 + hWidth, arrowRect.Y2, arrowRect.X2, arrowRect.Y1 + hHeight + vDist);
}


// uiComboBox
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
// uiCheckBox


uiCheckBox::uiCheckBox(uiWindow * parent, const Point & pos, const Size & size, uiCheckBoxKind kind, bool visible)
  : uiControl(parent, pos, size, visible),
    m_checked(false),
    m_kind(kind),
    m_groupIndex(-1)
{
  objectType().uiCheckBox = true;

  windowProps().focusable = true;

  windowStyle().borderSize         = 1;
  windowStyle().focusedBorderSize  = 2;
  windowStyle().borderColor        = RGB(1, 1, 1);
  windowStyle().focusedBorderColor = RGB(0, 0, 3);
}


uiCheckBox::~uiCheckBox()
{
}


void uiCheckBox::paintCheckBox()
{
  Rect bkgRect = uiControl::clientRect(uiOrigin::Window);
  // background
  RGB bkColor = m_checked ? m_checkBoxStyle.checkedBackgroundColor : m_checkBoxStyle.backgroundColor;
  if (isMouseOver())
    bkColor = m_checkBoxStyle.mouseOverBackgroundColor;
  Canvas.setBrushColor(bkColor);
  Canvas.fillRectangle(bkgRect);
  // content
  if (m_checked) {
    Rect r = rect(uiOrigin::Window).shrink(5);
    switch (m_kind) {
      case uiCheckBoxKind::CheckBox:
        Canvas.setPenColor(m_checkBoxStyle.foregroundColor);
        Canvas.drawLine(r.X1, r.Y2 - r.height() / 3, r.X1 + r.width() / 3, r.Y2);
        Canvas.drawLine(r.X1 + r.width() / 3, r.Y2, r.X2, r.Y1);
        break;
      case uiCheckBoxKind::RadioButton:
        Canvas.setBrushColor(m_checkBoxStyle.foregroundColor);
        Canvas.fillEllipse(r.X1 + r.width() / 2 - 1, r.Y1 + r.height() / 2 - 1, r.width(), r.height());
        break;
    }
  }
}


void uiCheckBox::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintCheckBox();
      break;

    case UIEVT_CLICK:
      trigger();
      break;

    case UIEVT_MOUSEENTER:
      repaint();  // to update background color
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1)
        repaint();
      break;

    case UIEVT_MOUSELEAVE:
      repaint();  // to update background
      break;

    case UIEVT_KEYUP:
      if (event->params.key.VK == VK_RETURN || event->params.key.VK == VK_KP_ENTER || event->params.key.VK == VK_SPACE) {
        trigger();
        onClick();
      }
      break;

    default:
      break;
  }
}


// action to perfom on mouse up or keyboard space/enter
void uiCheckBox::trigger()
{
  switch (m_kind) {
    case uiCheckBoxKind::CheckBox:
      m_checked = !m_checked;
      break;
    case uiCheckBoxKind::RadioButton:
      m_checked = true;
      unCheckGroup();
      break;
  }
  onChange();
  repaint();
}


void uiCheckBox::setChecked(bool value)
{
  if (value != m_checked) {
    m_checked = value;
    if (m_kind == uiCheckBoxKind::RadioButton && value == true)
      unCheckGroup();
    repaint();
  }
}


// unchecks all other items of the same group
void uiCheckBox::unCheckGroup()
{
  if (m_groupIndex == -1)
    return;
  for (auto sibling = parent()->firstChild(); sibling; sibling = sibling->next()) {
    if (sibling != this && objectType().uiCheckBox) {
      uiCheckBox * chk = (uiCheckBox*) sibling;
      if (chk->m_groupIndex == m_groupIndex)
        chk->setChecked(false);
    }
  }
}



// uiCheckBox
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiSlider


uiSlider::uiSlider(uiWindow * parent, const Point & pos, const Size & size, uiOrientation orientation, bool visible)
  : uiControl(parent, pos, size, visible),
    m_orientation(orientation),
    m_position(0),
    m_min(0),
    m_max(99),
    m_ticksFrequency(25)
{
  objectType().uiSlider = true;

  windowStyle().borderSize         = 1;
  windowStyle().borderColor        = RGB(3, 3, 3);
  windowStyle().focusedBorderColor = RGB(0, 0, 3);

  windowProps().focusable = true;
}


uiSlider::~uiSlider()
{
}


void uiSlider::setPosition(int value)
{
  if (value != m_position) {
    m_position = iclamp(value, m_min, m_max);
    repaint();
    onChange();
  }
}


void uiSlider::setup(int min, int max, int ticksFrequency)
{
  m_min = min;
  m_max = max;
  m_ticksFrequency = ticksFrequency;
  m_position = iclamp(m_position, m_min, m_max);
}


void uiSlider::paintSlider()
{
  Rect cRect     = uiControl::clientRect(uiOrigin::Window);
  Rect slideRect = cRect.shrink(4);
  Rect gripRect  = getGripRect();
  // background
  Canvas.setBrushColor(m_sliderStyle.backgroundColor);
  Canvas.fillRectangle(cRect);
  // slide
  Canvas.setBrushColor(m_sliderStyle.slideColor);
  switch (m_orientation) {
    case uiOrientation::Horizontal:
      Canvas.fillRectangle(gripRect.X2, slideRect.Y1, slideRect.X2, slideRect.Y2);  // right slide
      Canvas.setBrushColor(m_sliderStyle.rangeColor);
      Canvas.fillRectangle(slideRect.X1, slideRect.Y1, gripRect.X1, slideRect.Y2);  // left slide
      break;
    case uiOrientation::Vertical:
      Canvas.fillRectangle(slideRect.X1, slideRect.Y1, slideRect.X2, gripRect.Y1);  // upper slide
      Canvas.setBrushColor(m_sliderStyle.rangeColor);
      Canvas.fillRectangle(slideRect.X1, gripRect.Y2, slideRect.X2, slideRect.Y2);  // bottom slide
      break;
  }
  // ticks
  if (m_ticksFrequency > 0) {
    Canvas.setPenColor(m_sliderStyle.ticksColor);
    int range = m_max - m_min + 0;
    for (int p = m_min; p <= m_max; p += m_ticksFrequency) {
      switch (m_orientation) {
        case uiOrientation::Horizontal:
        {
          int x = slideRect.X1 + slideRect.width() * (p - m_min) / range;
          Canvas.drawLine(x, slideRect.Y1, x, slideRect.Y2);
          break;
        }
        case uiOrientation::Vertical:
        {
          int y = slideRect.Y2 - slideRect.height() * (p - m_min) / range;
          Canvas.drawLine(slideRect.X1, y, slideRect.X2, y);
          break;
        }
      }
    }
  }
  // grip
  Canvas.setBrushColor(m_sliderStyle.gripColor);
  Canvas.fillRectangle(gripRect);
}


Rect uiSlider::getGripRect()
{
  Rect cRect = uiControl::clientRect(uiOrigin::Window);
  Rect slideRect = cRect.shrink(4);
  int range = m_max - m_min + 0;
  switch (m_orientation) {
    case uiOrientation::Horizontal:
    {
      int x = slideRect.X1 + slideRect.width() * (m_position - m_min) / range;
      return Rect(x - 4, cRect.Y1, x + 4, cRect.Y2);
    }
    case uiOrientation::Vertical:
    {
      int y = slideRect.Y2 - slideRect.height() * (m_position - m_min) / range;
      return Rect(cRect.X1, y - 4, cRect.X2, y + 4);
    }
    default:
      return Rect();
  }
}


void uiSlider::moveGripTo(int x, int y)
{
  Rect cRect = uiControl::clientRect(uiOrigin::Window);
  Rect slideRect = cRect.shrink(4);
  int range = m_max - m_min + 1;
  switch (m_orientation) {
    case uiOrientation::Horizontal:
      setPosition(m_min + (x - slideRect.X1) * range / slideRect.width());
      break;
    case uiOrientation::Vertical:
      setPosition(m_min + (slideRect.Y2 - y) * range / slideRect.height());
      break;
  }
}


void uiSlider::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event, uiControl::clientRect(uiOrigin::Window));
      paintSlider();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      moveGripTo(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this)
        moveGripTo(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_KEYDOWN:
      handleKeyDown(event->params.key);
      break;

    default:
      break;
  }
}


void uiSlider::handleKeyDown(uiKeyEventInfo key)
{
  switch (key.VK) {
    case VK_UP:
    case VK_KP_UP:
    case VK_LEFT:
    case VK_KP_LEFT:
      setPosition(m_position - 1);
      break;

    case VK_DOWN:
    case VK_KP_DOWN:
    case VK_RIGHT:
    case VK_KP_RIGHT:
      setPosition(m_position + 1);
      break;

    case VK_PAGEUP:
    case VK_KP_PAGEUP:
      setPosition(m_position + m_ticksFrequency);
      break;

    case VK_PAGEDOWN:
    case VK_KP_PAGEDOWN:
      setPosition(m_position - m_ticksFrequency);
      break;

    case VK_HOME:
    case VK_KP_HOME:
      setPosition(m_min);
      break;

    case VK_END:
    case VK_KP_END:
      setPosition(m_max);
      break;

    default:
      break;
  }
}




// uiSlider
////////////////////////////////////////////////////////////////////////////////////////////////////




} // end of namespace

