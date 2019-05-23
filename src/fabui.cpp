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


#include "Arduino.h"

#include "freertos/FreeRTOS.h"

#include "fabutils.h"
#include "fabui.h"
#include "canvas.h"
#include "mouse.h"
#include "keyboard.h"



namespace fabgl {



// debug only!
void dumpEvent(uiEvent * event)
{
  static int idx = 0;
  static const char * TOSTR[] = { "UIEVT_NULL", "UIEVT_DEBUGMSG", "UIEVT_APPINIT", "UIEVT_GENPAINTEVENTS", "UIEVT_PAINT", "UIEVT_ACTIVATE",
                                  "UIEVT_DEACTIVATE", "UIEVT_MOUSEMOVE", "UIEVT_MOUSEWHEEL", "UIEVT_MOUSEBUTTONDOWN",
                                  "UIEVT_MOUSEBUTTONUP", "UIEVT_SETPOS", "UIEVT_SETSIZE", "UIEVT_RESHAPEWINDOW",
                                  "UIEVT_MOUSEENTER", "UIEVT_MOUSELEAVE", "UIEVT_MAXIMIZE", "UIEVT_MINIMIZE", "UIEVT_RESTORE",
                                  "UIEVT_SHOW", "UIEVT_HIDE", "UIEVT_SETFOCUS", "UIEVT_KILLFOCUS", "UIEVT_KEYDOWN", "UIEVT_KEYUP",
                                  "UIEVT_TIMER", "UIEVT_DBLCLICK",
                                };
  Serial.printf("#%d ", idx++);
  Serial.write(TOSTR[event->id]);
  if (event->dest && event->dest->evtHandlerProps().isFrame)
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
      Serial.printf("handle=%p", event->params.timerHandler);
      break;
    default:
      break;
  }
  Serial.write("\n");
}



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
}


uiEvtHandler::~uiEvtHandler()
{
}


void uiEvtHandler::processEvent(uiEvent * event)
{
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
    m_combineMouseMoveEvents(false),
    m_caretWindow(nullptr),
    m_caretTimer(nullptr),
    m_caretInvertState(-1),
    m_lastMouseDownTimeMS(0)
{
  m_eventsQueue = xQueueCreate(FABGLIB_UI_EVENTS_QUEUE_SIZE, sizeof(uiEvent));
}


uiApp::~uiApp()
{
  showCaret(nullptr);
  vQueueDelete(m_eventsQueue);
  delete m_rootWindow;
}


void uiApp::run()
{
  // setup absolute events from mouse
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), false, true, this);

  // setup mouse cursor
  VGAController.setMouseCursor(CursorName::CursorPointerSimpleReduced);

  // setup keyboard
  Keyboard.setUIApp(this);

  // root window always stays at 0, 0 and cannot be moved
  m_rootWindow = new uiFrame(nullptr, "", Point(0, 0), Size(Canvas.getWidth(), Canvas.getHeight()), false);
  m_rootWindow->setApp(this);

  m_rootWindow->frameStyle().borderSize      = 0;
  m_rootWindow->frameStyle().backgroundColor = RGB(3, 3, 3);

  m_rootWindow->frameProps().resizeable = false;
  m_rootWindow->frameProps().moveable   = false;

  showWindow(m_rootWindow, true);

  m_activeWindow = m_rootWindow;

  // generate UIEVT_APPINIT event
  uiEvent evt = uiEvent(this, UIEVT_APPINIT);
  postEvent(&evt);

  // dispatch events
  while (true) {
    uiEvent event;
    if (getEvent(&event, -1)) {

      // debug
      //dumpEvent(&event);

      preprocessEvent(&event);

      if (event.dest)
        event.dest->processEvent(&event);
    }
  }
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
        if (event->params.timerHandler == m_caretTimer) {
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
}


// look for destination window at event X, Y coordinates, then set "dest" field and modify mouse X, Y coordinates (convert to child coordinates)
// generate UIEVT_MOUSEENTER and UIEVT_MOUSELEAVE events
void uiApp::preprocessMouseEvent(uiEvent * event)
{
  // combine a sequence of UIEVT_MOUSEMOVE events?
  if (m_combineMouseMoveEvents && event->id == UIEVT_MOUSEMOVE) {
    uiEvent nextEvent;
    while (peekEvent(&nextEvent, 0) && nextEvent.id == UIEVT_MOUSEMOVE)
      getEvent(event, -1);
  }

  // search for window under the mouse or mouse capturing window
  // insert UIEVT_MOUSELEAVE when mouse capture is finished over a non-capturing window
  uiWindow * oldFreeMouseWindow = m_freeMouseWindow;
  Point mousePos = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
  if (m_capturedMouseWindow) {
    // mouse captured, just go back up to m_rootWindow
    for (uiWindow * cur = m_capturedMouseWindow; cur != m_rootWindow; cur = cur->parent())
      mousePos = mousePos.sub(cur->pos());
    event->dest = m_capturedMouseWindow;
    // left mouse button UP?
    if (event->id == UIEVT_MOUSEBUTTONUP && event->params.mouse.changedButton == 1) {
      // mouse up will end mouse capture, check that mouse is still inside
      if (!m_capturedMouseWindow->rect(uiWindowRectType::WindowBased).contains(mousePos)) {
        // mouse is not inside, post mouse leave and enter events
        uiEvent evt = uiEvent(m_capturedMouseWindow, UIEVT_MOUSELEAVE);
        postEvent(&evt);
        m_freeMouseWindow = oldFreeMouseWindow = nullptr;
      }
    }
  } else {
    m_freeMouseWindow = screenToWindow(mousePos);
    event->dest = m_freeMouseWindow;
  }
  event->params.mouse.status.X = mousePos.X;
  event->params.mouse.status.Y = mousePos.Y;

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
  if (event->id == UIEVT_MOUSEBUTTONDOWN) {
    int curTime = esp_timer_get_time() / 1000;  // uS -> MS
    if (curTime - m_lastMouseDownTimeMS <= m_appProps.doubleClickTime) {
      // post double click message
      uiEvent evt = *event;
      evt.id = UIEVT_DBLCLICK;
      postEvent(&evt);
    }
    m_lastMouseDownTimeMS = curTime;
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
      if (child->state().visible && win->rect(uiWindowRectType::ClientAreaWindowBased).contains(point) && child->rect(uiWindowRectType::ParentBased).contains(point)) {
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


void uiApp::OnInit()
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
  switch (event->id) {

    case UIEVT_APPINIT:
      OnInit();
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
// value = non-focusable window -> no change (focusable window remains focused)
uiWindow * uiApp::setFocusedWindow(uiWindow * value)
{
  uiWindow * prev = m_focusedWindow;

  if (m_focusedWindow != value && (value == nullptr || value->windowProps().focusable)) {

    if (prev) {
      uiEvent evt = uiEvent(prev, UIEVT_KILLFOCUS);
      postEvent(&evt);
    }

    m_focusedWindow = value;

    // changed focus, disable caret
    showCaret(nullptr);

    if (m_focusedWindow) {
      uiEvent evt = uiEvent(m_focusedWindow, UIEVT_SETFOCUS);
      postEvent(&evt);
    }

  }

  return prev;
}


uiWindow * uiApp::setFocusedWindowNext()
{
  uiWindow * old = m_focusedWindow;
  uiWindow * parent = old ? old->parent() : m_activeWindow;
  if (parent && parent->firstChild()) {
    uiWindow * proposed = old;
    do {
      if (!old && proposed)
        old = proposed; // just a way to exit loop when old=nullptr and no child is focusable
      proposed = proposed && proposed->next() ? proposed->next() : parent->firstChild();
    } while (!proposed->windowProps().focusable && proposed != old);
    setFocusedWindow(proposed);
  }
  return old;
}


uiWindow * uiApp::setFocusedWindowPrev()
{
  uiWindow * old = m_focusedWindow;
  uiWindow * parent = old ? old->parent() : m_activeWindow;
  if (parent && parent->lastChild()) {
    uiWindow * proposed = old;
    do {
      if (!old && proposed)
        old = proposed; // just a way to exit loop when old=nullptr and no child is focusable
      proposed = proposed && proposed->prev() ? proposed->prev() : parent->lastChild();
    } while (!proposed->windowProps().focusable && proposed != old);
    setFocusedWindow(proposed);
  }
  return old;
}


void uiApp::repaintWindow(uiWindow * window)
{
  repaintRect(window->rect(uiWindowRectType::ScreenBased));
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
  reshapeWindow(window, window->rect(uiWindowRectType::ParentBased).resize(width, height));
}


void uiApp::resizeWindow(uiWindow * window, Size size)
{
  reshapeWindow(window, window->rect(uiWindowRectType::ParentBased).resize(size));
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
  uiEvent evt = uiEvent(window, value ? UIEVT_SHOW : UIEVT_HIDE);
  postEvent(&evt);
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
  uiWindow * window = (uiWindow*) pvTimerGetTimerID(xTimer);
  uiEvent evt = uiEvent(window, UIEVT_TIMER);
  evt.params.timerHandler = xTimer;
  window->app()->postEvent(&evt);
}


// return handler to pass to deleteTimer()
uiTimerHandle uiApp::setTimer(uiWindow * window, int periodMS)
{
  TimerHandle_t h = xTimerCreate("", pdMS_TO_TICKS(periodMS), pdTRUE, window, &uiApp::timerFunc);
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
void uiApp::setCaret()
{
  blinkCaret();
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
    Canvas.setOrigin(m_rootWindow->pos());
    Canvas.setClippingRect(m_caretWindow->rect(uiWindowRectType::ClientAreaScreenBased));
    Rect aRect = m_caretWindow->transformRect(m_caretRect, m_rootWindow);
    Canvas.invertRectangle(aRect);
    m_caretInvertState = m_caretInvertState ? 0 : 1;
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
  m_state.visible   = false;
  m_state.maximized = false;
  m_state.minimized = false;
  m_state.active    = false;

  evtHandlerProps().isWindow = true;
  if (parent)
    parent->addChild(this);

  if (visible)
    app()->showWindow(this, true);
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


void uiWindow::removeChild(uiWindow * child, bool freeChild)
{
  if (child) {
    Rect childRect = child->rect(uiWindowRectType::ParentBased);

    if (child == m_firstChild)
      m_firstChild = child->m_next;
    else
      child->m_prev->m_next = child->m_next;

    if (child == m_lastChild)
      m_lastChild = child->m_prev;
    else
      child->m_next->m_prev = child->m_prev;

    if (freeChild)
      delete child;
    else
      child->m_prev = child->m_next = nullptr;

    repaint(childRect);
  }
}


// move to the last position (top window)
void uiWindow::moveChildOnTop(uiWindow * child)
{
  removeChild(child, false);
  addChild(child);
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
  app()->repaintRect(rect(uiWindowRectType::ScreenBased));
}


Rect uiWindow::rect(uiWindowRectType rectType)
{
  switch (rectType) {
    case uiWindowRectType::ScreenBased:
    case uiWindowRectType::ClientAreaScreenBased:
      return transformRect(Rect(0, 0, m_size.width - 1, m_size.height - 1), app()->rootWindow());
    case uiWindowRectType::ParentBased:
    case uiWindowRectType::ClientAreaParentBased:
      return Rect(m_pos.X, m_pos.Y, m_pos.X + m_size.width - 1, m_pos.Y + m_size.height - 1);
    case uiWindowRectType::WindowBased:
    case uiWindowRectType::ClientAreaWindowBased:
      return Rect(0, 0, m_size.width - 1, m_size.height - 1);
  }
  return Rect();
}


void uiWindow::beginPaint(uiEvent * event)
{
  Rect srect = rect(uiWindowRectType::ScreenBased);
  Canvas.setOrigin(srect.X1, srect.Y1);
  Rect clipRect = event->params.rect;
  if (m_parent)
    clipRect = clipRect.intersection(m_parent->rect(uiWindowRectType::ClientAreaWindowBased).translate(m_pos.neg()));
  Canvas.setClippingRect(clipRect);
}


void uiWindow::processEvent(uiEvent * event)
{
  uiEvtHandler::processEvent(event);

  switch (event->id) {

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
      m_mouseDownPos    = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
      m_posAtMouseDown  = m_pos;
      m_sizeAtMouseDown = m_size;
      // activate window? setActiveWindow() will activate the right window (maybe a parent)
      if (!m_state.active)
        app()->setActiveWindow(this);
      // focus window?
      app()->setFocusedWindow(this);
      // capture mouse if left button is down
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(this);
      break;

    case UIEVT_MOUSEBUTTONUP:
      // end capture mouse if left button is up
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(nullptr);
      break;

    case UIEVT_SHOW:
      m_state.visible = true;
      repaint();
      break;

    case UIEVT_HIDE:
      m_state.visible = false;
      repaint();
      break;

    case UIEVT_MAXIMIZE:
      if (!m_state.minimized)
        m_savedScreenRect = rect(uiWindowRectType::ParentBased);
      m_state.maximized = true;
      m_state.minimized = false;
      app()->reshapeWindow(this, m_parent->rect(uiWindowRectType::ClientAreaWindowBased));
      break;

    case UIEVT_MINIMIZE:
      if (!m_state.maximized)
        m_savedScreenRect = rect(uiWindowRectType::ParentBased);
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
      generateReshapeEvents(event->params.rect);
      break;

    case UIEVT_GENPAINTEVENTS:
      generatePaintEvents(event->params.rect);
      break;

    case UIEVT_MOUSEENTER:
      m_isMouseOver = true;
      break;

    case UIEVT_MOUSELEAVE:
      m_isMouseOver = false;
      break;

    case UIEVT_KEYUP:
      // only non-focusable windows can make focusable its children
      if (!m_windowProps.focusable) {
        // move focused child
        if (event->params.key.VK == VK_TAB) {
          if (event->params.key.SHIFT)
            app()->setFocusedWindowPrev();
          else
            app()->setFocusedWindowNext();
        }
      }
      break;

    default:
      break;
  }
}


// given a relative paint rect generate a set of UIEVT_PAINT events
void uiWindow::generatePaintEvents(Rect const & paintRect)
{
  Stack<Rect> rects;
  rects.push(paintRect);
  while (!rects.isEmpty()) {
    Rect thisRect = rects.pop();
    bool noIntesections = true;
    for (uiWindow * win = lastChild(); win; win = win->prev()) {
      Rect winRect = rect(uiWindowRectType::ClientAreaWindowBased).intersection(win->rect(uiWindowRectType::ParentBased));
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
      this->processEvent(&evt);
    }
  }
}


// insert UIEVT_PAINT, UIEVT_SETPOS and UIEVT_SETSIZE events in order to modify window bounding rect
// rect: new window rectangle based on parent coordinates
void uiWindow::generateReshapeEvents(Rect const & r)
{
  // new rect based on root window coordiantes
  Rect newRect = parent()->transformRect(r, app()->rootWindow());

  // old rect based on root window coordinates
  Rect oldRect = rect(uiWindowRectType::ScreenBased);

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

  app()->rootWindow()->generatePaintEvents(newRect);

  // generate set position event
  uiEvent evt = uiEvent(this, UIEVT_SETPOS);
  evt.params.pos = pos();
  app()->insertEvent(&evt);

  // generate set size event
  evt = uiEvent(this, UIEVT_SETSIZE);
  evt.params.size = size();
  app()->insertEvent(&evt);
}




// uiWindow
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame


uiFrame::uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible)
  : uiWindow(parent, pos, size, visible),
    m_title(nullptr),
    m_mouseDownSensiblePos(uiFrameSensiblePos::None),
    m_mouseMoveSensiblePos(uiFrameSensiblePos::None)
{
  evtHandlerProps().isFrame = true;
  setTitle(title);
}


uiFrame::~uiFrame()
{
  free(m_title);
}


void uiFrame::setTitle(char const * value)
{
  int len = strlen(value);
  m_title = (char*) realloc(m_title, len + 1);
  strcpy(m_title, value);
}


int uiFrame::titleBarHeight()
{
  return m_frameStyle.titleFont->height;
}


Rect uiFrame::rect(uiWindowRectType rectType)
{
  Rect r = uiWindow::rect(rectType);
  switch (rectType) {
    case uiWindowRectType::ClientAreaScreenBased:
    case uiWindowRectType::ClientAreaParentBased:
    case uiWindowRectType::ClientAreaWindowBased:
      // border
      r.X1 += m_frameStyle.borderSize;
      r.Y1 += m_frameStyle.borderSize;
      r.X2 -= m_frameStyle.borderSize;
      r.Y2 -= m_frameStyle.borderSize;
      // title bar
      if (strlen(m_title))
        r.Y1 += 1 + titleBarHeight();
      return r;
    default:
      return r;
  }
}


Size uiFrame::minWindowSize()
{
  bool hasTitle = strlen(m_title);
  Size r = Size(0, 0);
  if (m_frameProps.resizeable && !state().minimized && !hasTitle) {
    r.width  += CORNERSENSE * 2;
    r.height += CORNERSENSE * 2;
  }
  r.width  += m_frameStyle.borderSize * 2;
  r.height += m_frameStyle.borderSize * 2;
  if (hasTitle) {
    int barHeight = titleBarHeight();  // titleBarHeight is also the button width
    r.height += 1 + barHeight;
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
  Rect btnRect = Rect(size().width - 1 - m_frameStyle.borderSize - btnSize - CORNERSENSE,
                      m_frameStyle.borderSize,
                      size().width - 1 - m_frameStyle.borderSize - CORNERSENSE,
                      m_frameStyle.borderSize + btnSize);
  while (buttonIndex--)
    btnRect = btnRect.translate(-btnSize, 0);
  return btnRect;
}


void uiFrame::paintFrame()
{
  Rect bkgRect = Rect(0, 0, size().width - 1, size().height - 1);
  // title bar
  if (strlen(m_title)) {
    int barHeight = titleBarHeight();
    // title bar background
    RGB titleBarBrushColor = state().active ? m_frameStyle.activeTitleBackgroundColor : m_frameStyle.titleBackgroundColor;
    Canvas.setBrushColor(titleBarBrushColor);
    Canvas.fillRectangle(m_frameStyle.borderSize, m_frameStyle.borderSize, size().width - 1 - m_frameStyle.borderSize, 1 + barHeight + m_frameStyle.borderSize);
    // close, maximize and minimze buttons
    int btnX = paintButtons();
    // title
    paintTitle(btnX);
    // adjust background rect
    bkgRect.Y1 = 2 + barHeight;
  }
  // border
  if (m_frameStyle.borderSize > 0) {
    Canvas.setPenColor(state().active ? m_frameStyle.activeBorderColor : m_frameStyle.borderColor);
    for (int i = 0; i < m_frameStyle.borderSize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    bkgRect.X1 += m_frameStyle.borderSize;
    bkgRect.Y1 += m_frameStyle.borderSize;
    bkgRect.X2 -= m_frameStyle.borderSize;
    bkgRect.Y2 -= m_frameStyle.borderSize;
  }
  // background
  if (!state().minimized && bkgRect.width() > 0 && bkgRect.height() > 0) {
    Canvas.setBrushColor(m_frameStyle.backgroundColor);
    Canvas.fillRectangle(bkgRect);
  }
}


// maxX maximum X coordinate allowed
void uiFrame::paintTitle(int maxX)
{
  Canvas.setPenColor(state().active ? m_frameStyle.activeTitleFontColor : m_frameStyle.titleFontColor);
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
  Canvas.drawTextWithEllipsis(m_frameStyle.titleFont, 1 + m_frameStyle.borderSize, 1 + m_frameStyle.borderSize, m_title, maxX);
}


// return the X coordinate where button start
int uiFrame::paintButtons()
{
  int buttonsX = m_frameStyle.borderSize;
  if (m_frameProps.hasCloseButton) {
    // close button
    Rect r = getBtnRect(0);
    buttonsX = r.X1;
    if (m_mouseMoveSensiblePos == uiFrameSensiblePos::CloseButton) {
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
    if (m_mouseMoveSensiblePos == uiFrameSensiblePos::MaximizeButton) {
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
    if (m_mouseMoveSensiblePos == uiFrameSensiblePos::MinimizeButton) {
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
      beginPaint(event);
      paintFrame();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      m_mouseDownSensiblePos = getSensiblePosAt(event->params.mouse.status.X, event->params.mouse.status.Y);
      app()->combineMouseMoveEvents(true);
      break;

    case UIEVT_MOUSEBUTTONUP:
      // this sets the right mouse cursor in case of end of capturing
      movingFreeMouse(event->params.mouse.status.X, event->params.mouse.status.Y);

      // handle buttons clicks
      if (event->params.mouse.changedButton == 1) // 1 = left button
        handleButtonsClick(event->params.mouse.status.X, event->params.mouse.status.Y);

      app()->combineMouseMoveEvents(false);
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this)
        movingCapturedMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      else
        movingFreeMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_MOUSELEAVE:
      if (m_mouseMoveSensiblePos == uiFrameSensiblePos::CloseButton)
        repaint(getBtnRect(0));
      if (m_mouseMoveSensiblePos == uiFrameSensiblePos::MaximizeButton)
        repaint(getBtnRect(1));
      if (m_mouseMoveSensiblePos == uiFrameSensiblePos::MinimizeButton)
        repaint(getBtnRect(2));
      m_mouseMoveSensiblePos = uiFrameSensiblePos::None;
      break;

    default:
      break;
  }
}


uiFrameSensiblePos uiFrame::getSensiblePosAt(int x, int y)
{
  Point p = Point(x, y);

  if (m_frameProps.hasCloseButton && getBtnRect(0).contains(p))
    return uiFrameSensiblePos::CloseButton;    // on Close Button area

  if (m_frameProps.hasMaximizeButton && getBtnRect(1).contains(p))
    return uiFrameSensiblePos::MaximizeButton; // on maximize button area

  if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(p))
    return uiFrameSensiblePos::MinimizeButton; // on minimize button area

  int w = size().width;
  int h = size().height;

  if (m_frameProps.resizeable && !state().maximized && !state().minimized) {

    // on top center, resize
    if (Rect(CORNERSENSE, 0, w - CORNERSENSE, m_frameStyle.borderSize).contains(p))
      return uiFrameSensiblePos::TopCenterResize;

    // on left center side, resize
    if (Rect(0, CORNERSENSE, m_frameStyle.borderSize, h - CORNERSENSE).contains(p))
      return uiFrameSensiblePos::CenterLeftResize;

    // on right center side, resize
    if (Rect(w - m_frameStyle.borderSize, CORNERSENSE, w - 1, h - CORNERSENSE).contains(p))
      return uiFrameSensiblePos::CenterRightResize;

    // on bottom center, resize
    if (Rect(CORNERSENSE, h - m_frameStyle.borderSize, w - CORNERSENSE, h - 1).contains(p))
      return uiFrameSensiblePos::BottomCenterResize;

    // on top left side, resize
    if (Rect(0, 0, CORNERSENSE, CORNERSENSE).contains(p))
      return uiFrameSensiblePos::TopLeftResize;

    // on top right side, resize
    if (Rect(w - CORNERSENSE, 0, w - 1, CORNERSENSE).contains(p))
      return uiFrameSensiblePos::TopRightResize;

    // on bottom left side, resize
    if (Rect(0, h - CORNERSENSE, CORNERSENSE, h - 1).contains(p))
      return uiFrameSensiblePos::BottomLeftResize;

    // on bottom right side, resize
    if (Rect(w - CORNERSENSE, h - CORNERSENSE, w - 1, h - 1).contains(p))
      return uiFrameSensiblePos::BottomRightResize;

  }

  if (m_frameProps.moveable && !state().maximized && Rect(1, 1, w - 2, 1 + titleBarHeight()).contains(p))
    return uiFrameSensiblePos::MoveArea;       // on title bar, moving area

  return uiFrameSensiblePos::None;
}


void uiFrame::movingCapturedMouse(int mouseX, int mouseY)
{
  int dx = mouseX - mouseDownPos().X;
  int dy = mouseY - mouseDownPos().Y;

  Size minSize = minWindowSize();

  switch (m_mouseDownSensiblePos) {

    case uiFrameSensiblePos::MoveArea:
      app()->moveWindow(this, pos().X + dx, pos().Y + dy);
      break;

    case uiFrameSensiblePos::CenterRightResize:
      {
        int newWidth = sizeAtMouseDown().width + dx;
        if (newWidth >= minSize.width)
          app()->resizeWindow(this, newWidth, sizeAtMouseDown().height);
        break;
      }

    case uiFrameSensiblePos::CenterLeftResize:
      {
        Rect r = rect(uiWindowRectType::ParentBased);
        r.X1 = pos().X + dx;
        if (r.size().width >= minSize.width)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiFrameSensiblePos::TopLeftResize:
      {
        Rect r = rect(uiWindowRectType::ParentBased);
        r.X1 = pos().X + dx;
        r.Y1 = pos().Y + dy;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiFrameSensiblePos::TopCenterResize:
      {
        Rect r = rect(uiWindowRectType::ParentBased);
        r.Y1 = pos().Y + dy;
        if (r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiFrameSensiblePos::TopRightResize:
      {
        Rect r = rect(uiWindowRectType::ParentBased);
        r.Y1 = pos().Y + dy;
        r.X2 = pos().X + sizeAtMouseDown().width + dx;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiFrameSensiblePos::BottomLeftResize:
      {
        Rect r = rect(uiWindowRectType::ParentBased);
        r.X1 = pos().X + dx;
        r.Y2 = pos().Y + sizeAtMouseDown().height + dy;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiFrameSensiblePos::BottomCenterResize:
      {
        int newHeight = sizeAtMouseDown().height + dy;
        if (newHeight >= minSize.height)
          app()->resizeWindow(this, sizeAtMouseDown().width, newHeight);
        break;
      }

    case uiFrameSensiblePos::BottomRightResize:
      {
        int newWidth = sizeAtMouseDown().width + dx;
        int newHeight = sizeAtMouseDown().height + dy;
        if (newWidth >= minSize.width && newHeight >= minSize.height)
          app()->resizeWindow(this, newWidth, newHeight);
        break;
      }

    default:
      break;
  }
}


void uiFrame::movingFreeMouse(int mouseX, int mouseY)
{
  uiFrameSensiblePos prevSensPos = m_mouseMoveSensiblePos;

  m_mouseMoveSensiblePos = getSensiblePosAt(mouseX, mouseY);

  if ((m_mouseMoveSensiblePos == uiFrameSensiblePos::CloseButton || prevSensPos == uiFrameSensiblePos::CloseButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(0));

  if ((m_mouseMoveSensiblePos == uiFrameSensiblePos::MaximizeButton || prevSensPos == uiFrameSensiblePos::MaximizeButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(1));

  if ((m_mouseMoveSensiblePos == uiFrameSensiblePos::MinimizeButton || prevSensPos == uiFrameSensiblePos::MinimizeButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(2));

  CursorName cur = CursorName::CursorPointerSimpleReduced;  // this is the default

  switch (m_mouseMoveSensiblePos) {

    case uiFrameSensiblePos::TopLeftResize:
      cur = CursorName::CursorResize2;
      break;

    case uiFrameSensiblePos::TopCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiFrameSensiblePos::TopRightResize:
      cur = CursorName::CursorResize1;
      break;

    case uiFrameSensiblePos::CenterLeftResize:
      cur = CursorName::CursorResize4;
      break;

    case uiFrameSensiblePos::CenterRightResize:
      cur = CursorName::CursorResize4;
      break;

    case uiFrameSensiblePos::BottomLeftResize:
      cur = CursorName::CursorResize1;
      break;

    case uiFrameSensiblePos::BottomCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiFrameSensiblePos::BottomRightResize:
      cur = CursorName::CursorResize2;
      break;

    default:
      break;
  }

  VGAController.setMouseCursor(cur);
}


void uiFrame::handleButtonsClick(int x, int y)
{
  if (m_frameProps.hasCloseButton && getBtnRect(0).contains(x, y) && getBtnRect(0).contains(mouseDownPos()))
    app()->showWindow(this, false);
  else if (m_frameProps.hasMaximizeButton && getBtnRect(1).contains(x, y) && getBtnRect(1).contains(mouseDownPos()))
    app()->maximizeWindow(this, !state().maximized && !state().minimized);  // used also for "restore" from minimized
  else if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(x, y) && getBtnRect(2).contains(mouseDownPos()))
    app()->minimizeWindow(this, !state().minimized);
  else
    return;
  // this avoids the button remains selected (background colored) when window change size
  m_mouseMoveSensiblePos = uiFrameSensiblePos::None;
}


// uiFrame
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiControl


uiControl::uiControl(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiWindow(parent, pos, size, visible)
{
  evtHandlerProps().isControl = true;
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


uiButton::uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible, uiButtonKind kind)
  : uiControl(parent, pos, size, visible),
    m_text(nullptr),
    m_textExtent(0),
    m_down(false),
    m_kind(kind)
{
  windowProps().focusable = true;
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
  bool hasFocus = (app()->focusedWindow() == this);
  Rect bkgRect = Rect(0, 0, size().width - 1, size().height - 1);
  // border
  if (m_buttonStyle.borderSize > 0) {
    Canvas.setPenColor(hasFocus ? m_buttonStyle.focusedBorderColor : m_buttonStyle.borderColor);
    int bsize = hasFocus? m_buttonStyle.focusedBorderSize : m_buttonStyle.borderSize;
    for (int i = 0; i < bsize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    bkgRect = bkgRect.shrink(bsize);
  }
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
  Canvas.setPenColor(m_buttonStyle.textFontColor);
  Canvas.drawText(m_buttonStyle.textFont, x, y, m_text);
}


void uiButton::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event);
      paintButton();
      break;

    case UIEVT_MOUSEBUTTONUP:
      // this check is required to avoid onclick event when mouse is captured and moved out of button area
      if (rect(uiWindowRectType::WindowBased).contains(event->params.mouse.status.X, event->params.mouse.status.Y))
        trigger();
      break;

    case UIEVT_MOUSEENTER:
      VGAController.setMouseCursor(CursorName::CursorPointerSimpleReduced);
      repaint();  // to update background color
      break;

    case UIEVT_MOUSEBUTTONDOWN:
    case UIEVT_MOUSELEAVE:
    case UIEVT_SETFOCUS:
    case UIEVT_KILLFOCUS:
      repaint();  // to update background and border
      break;

    case UIEVT_KEYUP:
      if (event->params.key.VK == VK_RETURN || event->params.key.VK == VK_KP_ENTER || event->params.key.VK == VK_SPACE)
        trigger();
      break;

    default:
      break;
  }
}


// action to perfom on mouse up or keyboard space/enter
void uiButton::trigger()
{
  onClick();
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
  windowProps().focusable = true;
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
      beginPaint(event);
      paintTextEdit();
      app()->setCaret(); // force blinking (previous painting may cover caret)
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      if (event->params.mouse.changedButton == 1) {
        int col = getColFromMouseX(event->params.mouse.status.X);
        moveCursor(col, col);
      }
      repaint();
      break;

    case UIEVT_MOUSEBUTTONUP:
      break;

    case UIEVT_MOUSEENTER:
      VGAController.setMouseCursor(CursorName::CursorPointerSimpleReduced);
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
      updateCaret();
      app()->showCaret(this);
      repaint();
      break;

    case UIEVT_KILLFOCUS:
      app()->showCaret(NULL);
      moveCursor(0, 0);
      repaint();
      break;

    case UIEVT_KEYDOWN:
      handleKeyDown(event);
      break;

    case UIEVT_DBLCLICK:
      selectWordAt(event->params.mouse.status.X);
      break;

    default:
      break;
  }
}


void uiTextEdit::handleKeyDown(uiEvent * event)
{
  switch (event->params.key.VK) {

    case VK_LEFT:
    case VK_KP_LEFT:
    {
      // LEFT                : cancel selection and move cursor by one character
      // SHIFT + LEFT        : move cursor and select
      // CTRL + LEFT         : cancel selection and move cursor by one word
      // SHIFT + CTRL + LEFT : move cursor by one word and select
      int newCurCol = event->params.key.CTRL ? getWordPosAtLeft() : m_cursorCol - 1;
      moveCursor(newCurCol, (event->params.key.SHIFT ? m_selCursorCol : newCurCol));
      break;
    }

    case VK_RIGHT:
    case VK_KP_RIGHT:
    {
      // RIGHT                : cancel selection and move cursor by one character
      // SHIFT + RIGHT        : move cursor and select
      // CTRL + RIGHT         : cancel selection and move cursor by one word
      // SHIFT + CTRL + RIGHT : move cursor by one word and select
      int newCurCol = event->params.key.CTRL ? getWordPosAtRight() : m_cursorCol + 1;
      moveCursor(newCurCol, (event->params.key.SHIFT ? m_selCursorCol : newCurCol));
      break;
    }

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

    case VK_HOME:
    case VK_KP_HOME:
      // SHIFT + HOME, select up to Home
      // HOME, move cursor to home
      moveCursor(0, (event->params.key.SHIFT ? m_selCursorCol : 0));
      break;

    case VK_END:
    case VK_KP_END:
      // SHIFT + END, select up to End
      // END, move cursor to End
      moveCursor(m_textLength, (event->params.key.SHIFT ? m_selCursorCol : m_textLength));
      break;

    default:
    {
      if (event->params.key.CTRL) {
        // keys with CTRL
        switch (event->params.key.VK) {
          case VK_a:
            // CTRL+A, select all
            moveCursor(m_textLength, 0);
            break;
          default:
            break;
        }
      } else {
        // normal keys
        int c = Keyboard.virtualKeyToASCII(event->params.key.VK);
        if (c >= 0x20 && c != 0x7F) {
          if (m_cursorCol != m_selCursorCol)
            removeSel();  // there is a selection, same behavior of VK_DELETE
          insert(c);
        }
      }
      break;
    }
  }
}


void uiTextEdit::paintTextEdit()
{
  bool hasFocus = (app()->focusedWindow() == this);
  m_contentRect = Rect(0, 0, size().width - 1, size().height - 1);
  // border
  if (m_textEditStyle.borderSize > 0) {
    Canvas.setPenColor(hasFocus ? m_textEditStyle.focusedBorderColor : m_textEditStyle.borderColor);
    int bsize = m_textEditStyle.borderSize;
    for (int i = 0; i < bsize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    m_contentRect = m_contentRect.shrink(bsize);
  }
  // background
  RGB bkColor = hasFocus ? m_textEditStyle.focusedBackgroundColor : (isMouseOver() ? m_textEditStyle.mouseOverBackgroundColor : m_textEditStyle.backgroundColor);
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
  Canvas.setPenColor(m_textEditStyle.textFontColor);

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
  int x = charColumnToWindowX(m_cursorCol);
  app()->setCaret(Rect(x, m_contentRect.Y1, x, m_contentRect.Y1 + m_textEditStyle.textFont->height));
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



} // end of namespace

