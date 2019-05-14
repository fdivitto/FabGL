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
      Serial.printf("%s ", Keyboard.virtualKeyToString(event->params.key.VK));
      if (event->params.key.LALT) Serial.write(" +LALT");
      if (event->params.key.RALT) Serial.write(" +RALT");
      if (event->params.key.CTRL) Serial.write(" +CTRL");
      if (event->params.key.SHIFT) Serial.write(" +SHIFT");
      if (event->params.key.GUI) Serial.write(" +GUI");
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
  : uiEvtHandler(NULL),
    m_rootWindow(NULL),
    m_activeWindow(NULL),
    m_focusedWindow(NULL),
    m_capturedMouseWindow(NULL),
    m_freeMouseWindow(NULL),
    m_combineMouseMoveEvents(false)
{
  m_eventsQueue = xQueueCreate(FABGLIB_UI_EVENTS_QUEUE_SIZE, sizeof(uiEvent));
}


uiApp::~uiApp()
{
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
  m_rootWindow = new uiFrame(NULL, "", Point(0, 0), Size(Canvas.getWidth(), Canvas.getHeight()), false);
  m_rootWindow->setApp(this);

  m_rootWindow->style().borderSize      = 0;
  m_rootWindow->style().backgroundColor = RGB(3, 3, 3);

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
  if (event->dest == NULL) {
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
  }
}


// look for destination window at event X, Y coordinates, then set "dest" field and modify mouse X, Y coordinates (convert to child coordinates)
// generate UIEVT_MOUSEENTER and UIEVT_MOUSELEAVE events
void uiApp::preprocessMouseEvent(uiEvent * event)
{
  if (m_combineMouseMoveEvents && event->id == UIEVT_MOUSEMOVE) {
    uiEvent nextEvent;
    while (peekEvent(&nextEvent, 0) && nextEvent.id == UIEVT_MOUSEMOVE)
      getEvent(event, -1);
  }

  uiWindow * oldFreeMouseWindow = m_freeMouseWindow;
  Point mousePos = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
  if (m_capturedMouseWindow) {
    // mouse captured, just go back up to m_rootWindow
    for (uiWindow * cur = m_capturedMouseWindow; cur != m_rootWindow; cur = cur->parent())
      mousePos = mousePos.sub(cur->pos());
    event->dest = m_capturedMouseWindow;
    if (event->id == UIEVT_MOUSEBUTTONUP && event->params.mouse.changedButton == 1) {
      // mouse up will end mouse capture, check that mouse is still inside
      if (!m_capturedMouseWindow->rect(uiRect_WindowBased).contains(mousePos)) {
        // mouse is not inside, post mouse leave and enter events
        uiEvent evt = uiEvent(m_capturedMouseWindow, UIEVT_MOUSELEAVE);
        postEvent(&evt);
        m_freeMouseWindow = oldFreeMouseWindow = NULL;
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
}


void uiApp::preprocessKeyboardEvent(uiEvent * event)
{
  if (m_focusedWindow) {
    event->dest = m_focusedWindow;
  }
}


// allow a window to capture mouse. window = NULL to end mouse capture
void uiApp::captureMouse(uiWindow * window)
{
  m_capturedMouseWindow = window;
}


// convert screen coordinates to window coordinates (the most visible window)
// return the window under specified absolute coordinates
uiWindow * uiApp::screenToWindow(Point & point)
{
  uiWindow * win = m_rootWindow;
  while (win->hasChildren()) {
    uiWindow * child = win->lastChild();
    for (; child; child = child->prev()) {
      if (child->state().visible && win->rect(uiRect_ClientAreaWindowBased).contains(point) && child->rect(uiRect_ParentBased).contains(point)) {
        win   = child;
        point = point.sub(child->pos());
        break;
      }
    }
    if (child == NULL)
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
  uiEvent evt = uiEvent(NULL, UIEVT_DEBUGMSG);
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


uiWindow * uiApp::setFocusedWindow(uiWindow * value)
{
  uiWindow * prev = m_focusedWindow;
  m_focusedWindow = value;

  if (prev) {
    uiEvent evt = uiEvent(prev, UIEVT_KILLFOCUS);
    postEvent(&evt);
  }

  if (m_focusedWindow) {
    uiEvent evt = uiEvent(m_focusedWindow, UIEVT_SETFOCUS);
    postEvent(&evt);
  }

  return prev;
}


void uiApp::repaintWindow(uiWindow * window)
{
  repaintRect(window->rect(uiRect_ScreenBased));
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
  reshapeWindow(window, window->rect(uiRect_ParentBased).resize(width, height));
}


void uiApp::resizeWindow(uiWindow * window, Size size)
{
  reshapeWindow(window, window->rect(uiRect_ParentBased).resize(size));
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


// uiApp
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiWindow


uiWindow::uiWindow(uiWindow * parent, const Point & pos, const Size & size, bool visible)
  : uiEvtHandler(parent ? parent->app() : NULL),
    m_parent(parent),
    m_pos(pos),
    m_size(size),
    m_mouseDownPos(Point(-1, -1)),
    m_isMouseOver(false),
    m_next(NULL),
    m_prev(NULL),
    m_firstChild(NULL),
    m_lastChild(NULL)
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
  m_firstChild = m_lastChild = NULL;
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
    child->m_prev = child->m_next = NULL;
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
  app()->repaintRect(rect(uiRect_ScreenBased));
}


Rect uiWindow::rect(uiWindowRectType rectType)
{
  switch (rectType) {
    case uiRect_ScreenBased:
      return transformRect(Rect(0, 0, m_size.width - 1, m_size.height - 1), app()->rootWindow());
    case uiRect_ParentBased:
      return Rect(m_pos.X, m_pos.Y, m_pos.X + m_size.width - 1, m_pos.Y + m_size.height - 1);
    case uiRect_WindowBased:
      return Rect(0, 0, m_size.width - 1, m_size.height - 1);
    case uiRect_ClientAreaParentBased:
      return Rect(m_pos.X, m_pos.Y, m_pos.X + m_size.width - 1, m_pos.Y + m_size.height - 1);
    case uiRect_ClientAreaWindowBased:
      return Rect(0, 0, m_size.width - 1, m_size.height - 1);
  }
  return Rect();
}


void uiWindow::beginPaint(uiEvent * event)
{
  Rect srect = rect(uiRect_ScreenBased);
  Canvas.setOrigin(srect.X1, srect.Y1);
  Rect clipRect = event->params.rect;
  if (m_parent)
    clipRect = clipRect.intersection(m_parent->rect(uiRect_ClientAreaWindowBased).translate(m_pos.neg()));
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
        for (uiWindow * child = this; child->parent() != NULL; child = child->parent()) {
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
      if (windowProps().focusable)
        app()->setFocusedWindow(this);
      // capture mouse if left button is down
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(this);
      break;

    case UIEVT_MOUSEBUTTONUP:
      // end capture mouse if left button is up
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(NULL);
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
        m_savedScreenRect = rect(uiRect_ParentBased);
      m_state.maximized = true;
      m_state.minimized = false;
      app()->reshapeWindow(this, m_parent->rect(uiRect_ClientAreaWindowBased));
      break;

    case UIEVT_MINIMIZE:
      if (!m_state.maximized)
        m_savedScreenRect = rect(uiRect_ParentBased);
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
      Rect winRect = rect(uiRect_ClientAreaWindowBased).intersection(win->rect(uiRect_ParentBased));
      if (win->state().visible && thisRect.intersects(winRect)) {
        noIntesections = false;
        removeRectangle(rects, thisRect, winRect);
        Rect newRect = thisRect.intersection(winRect).translate(-win->pos().X, -win->pos().Y);
        win->generatePaintEvents(newRect);
        break;
      }
    }
    if (noIntesections) {
      uiEvent evt = uiEvent(NULL, UIEVT_PAINT);
      evt.dest = this;
      evt.params.rect = thisRect;
      app()->insertEvent(&evt);
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
  Rect oldRect = rect(uiRect_ScreenBased);

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
    m_title(NULL),
    m_mouseDownSensiblePos(uiSensPos_None),
    m_mouseMoveSensiblePos(uiSensPos_None)
{
  evtHandlerProps().isFrame = true;
  setTitle(title);
}


uiFrame::~uiFrame()
{
}


void uiFrame::setTitle(char const * value)
{
  int len = strlen(value);
  m_title = (char*) realloc(m_title, len + 1);
  strcpy(m_title, value);
}


int uiFrame::titleBarHeight()
{
  return m_style.titleFont->height;
}


Rect uiFrame::rect(uiWindowRectType rectType)
{
  Rect r = uiWindow::rect(rectType);
  switch (rectType) {
    case uiRect_ClientAreaParentBased:
    case uiRect_ClientAreaWindowBased:
      // border
      r.X1 += m_style.borderSize;
      r.Y1 += m_style.borderSize;
      r.X2 -= m_style.borderSize;
      r.Y2 -= m_style.borderSize;
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
  r.width  += m_style.borderSize * 2;
  r.height += m_style.borderSize * 2;
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
  Rect btnRect = Rect(size().width - 1 - m_style.borderSize - btnSize - CORNERSENSE,
                      m_style.borderSize,
                      size().width - 1 - m_style.borderSize - CORNERSENSE,
                      m_style.borderSize + btnSize);
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
    RGB titleBarBrushColor = state().active ? m_style.activeTitleBackgroundColor : m_style.titleBackgroundColor;
    Canvas.setBrushColor(titleBarBrushColor);
    Canvas.fillRectangle(m_style.borderSize, m_style.borderSize, size().width - 1 - m_style.borderSize, 1 + barHeight + m_style.borderSize);
    // close, maximize and minimze buttons
    int btnX = paintButtons();
    // title
    paintTitle(btnX);
    // adjust background rect
    bkgRect.Y1 = 2 + barHeight;
  }
  // border
  if (m_style.borderSize > 0) {
    Canvas.setPenColor(state().active ? m_style.activeBorderColor : m_style.borderColor);
    for (int i = 0; i < m_style.borderSize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    bkgRect.X1 += m_style.borderSize;
    bkgRect.Y1 += m_style.borderSize;
    bkgRect.X2 -= m_style.borderSize;
    bkgRect.Y2 -= m_style.borderSize;
  }
  // background
  if (!state().minimized && bkgRect.width() > 0 && bkgRect.height() > 0) {
    Canvas.setBrushColor(m_style.backgroundColor);
    Canvas.fillRectangle(bkgRect);
  }
}


// maxX maximum X coordinate allowed
void uiFrame::paintTitle(int maxX)
{
  Canvas.setPenColor(state().active ? m_style.activeTitleFontColor : m_style.titleFontColor);
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(false).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
  Canvas.drawTextWithEllipsis(m_style.titleFont, 1 + m_style.borderSize, 1 + m_style.borderSize, m_title, maxX);
}


// return the X coordinate where button start
int uiFrame::paintButtons()
{
  int buttonsX = m_style.borderSize;
  if (m_frameProps.hasCloseButton) {
    // close button
    Rect r = getBtnRect(0);
    buttonsX = r.X1;
    if (m_mouseMoveSensiblePos == uiSensPos_CloseButton) {
      Canvas.setBrushColor(m_style.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_style.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_style.activeButtonColor : m_style.buttonColor);
    r = r.shrink(4);
    Canvas.drawLine(r.X1, r.Y1, r.X2, r.Y2);
    Canvas.drawLine(r.X2, r.Y1, r.X1, r.Y2);
  }
  if (m_frameProps.hasMaximizeButton) {
    // maximize/restore button
    Rect r = getBtnRect(1);
    buttonsX = r.X1;
    if (m_mouseMoveSensiblePos == uiSensPos_MaximizeButton) {
      Canvas.setBrushColor(m_style.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_style.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_style.activeButtonColor : m_style.buttonColor);
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
    if (m_mouseMoveSensiblePos == uiSensPos_MinimizeButton) {
      Canvas.setBrushColor(m_style.mouseOverBackgroundButtonColor);
      Canvas.fillRectangle(r);
      Canvas.setPenColor(m_style.mouseOverButtonColor);
    } else
      Canvas.setPenColor(state().active ? m_style.activeButtonColor : m_style.buttonColor);
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
      if (event->params.mouse.changedButton == 1)
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
      if (m_mouseMoveSensiblePos == uiSensPos_CloseButton)
        repaint(getBtnRect(0));
      if (m_mouseMoveSensiblePos == uiSensPos_MaximizeButton)
        repaint(getBtnRect(1));
      if (m_mouseMoveSensiblePos == uiSensPos_MinimizeButton)
        repaint(getBtnRect(2));
      m_mouseMoveSensiblePos = uiSensPos_None;
      break;

    default:
      break;
  }
}


uiFrameSensiblePos uiFrame::getSensiblePosAt(int x, int y)
{
  Point p = Point(x, y);

  if (m_frameProps.hasCloseButton && getBtnRect(0).contains(p))
    return uiSensPos_CloseButton;    // on Close Button area

  if (m_frameProps.hasMaximizeButton && getBtnRect(1).contains(p))
    return uiSensPos_MaximizeButton; // on maximize button area

  if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(p))
    return uiSensPos_MinimizeButton; // on minimize button area

  int w = size().width;
  int h = size().height;

  if (m_frameProps.resizeable && !state().maximized && !state().minimized) {

    // on top center, resize
    if (Rect(CORNERSENSE, 0, w - CORNERSENSE, m_style.borderSize).contains(p))
      return uiSensPos_TopCenterResize;

    // on left center side, resize
    if (Rect(0, CORNERSENSE, m_style.borderSize, h - CORNERSENSE).contains(p))
      return uiSensPos_CenterLeftResize;

    // on right center side, resize
    if (Rect(w - m_style.borderSize, CORNERSENSE, w - 1, h - CORNERSENSE).contains(p))
      return uiSensPos_CenterRightResize;

    // on bottom center, resize
    if (Rect(CORNERSENSE, h - m_style.borderSize, w - CORNERSENSE, h - 1).contains(p))
      return uiSensPos_BottomCenterResize;

    // on top left side, resize
    if (Rect(0, 0, CORNERSENSE, CORNERSENSE).contains(p))
      return uiSensPos_TopLeftResize;

    // on top right side, resize
    if (Rect(w - CORNERSENSE, 0, w - 1, CORNERSENSE).contains(p))
      return uiSensPos_TopRightResize;

    // on bottom left side, resize
    if (Rect(0, h - CORNERSENSE, CORNERSENSE, h - 1).contains(p))
      return uiSensPos_BottomLeftResize;

    // on bottom right side, resize
    if (Rect(w - CORNERSENSE, h - CORNERSENSE, w - 1, h - 1).contains(p))
      return uiSensPos_BottomRightResize;

  }

  if (m_frameProps.moveable && !state().maximized && Rect(1, 1, w - 2, 1 + titleBarHeight()).contains(p))
    return uiSensPos_MoveArea;       // on title bar, moving area

  return uiSensPos_None;
}


void uiFrame::movingCapturedMouse(int mouseX, int mouseY)
{
  int dx = mouseX - mouseDownPos().X;
  int dy = mouseY - mouseDownPos().Y;

  Size minSize = minWindowSize();

  switch (m_mouseDownSensiblePos) {

    case uiSensPos_MoveArea:
      app()->moveWindow(this, pos().X + dx, pos().Y + dy);
      break;

    case uiSensPos_CenterRightResize:
      {
        int newWidth = sizeAtMouseDown().width + dx;
        if (newWidth >= minSize.width)
          app()->resizeWindow(this, newWidth, sizeAtMouseDown().height);
        break;
      }

    case uiSensPos_CenterLeftResize:
      {
        Rect r = rect(uiRect_ParentBased);
        r.X1 = pos().X + dx;
        if (r.size().width >= minSize.width)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiSensPos_TopLeftResize:
      {
        Rect r = rect(uiRect_ParentBased);
        r.X1 = pos().X + dx;
        r.Y1 = pos().Y + dy;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiSensPos_TopCenterResize:
      {
        Rect r = rect(uiRect_ParentBased);
        r.Y1 = pos().Y + dy;
        if (r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiSensPos_TopRightResize:
      {
        Rect r = rect(uiRect_ParentBased);
        r.Y1 = pos().Y + dy;
        r.X2 = pos().X + sizeAtMouseDown().width + dx;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiSensPos_BottomLeftResize:
      {
        Rect r = rect(uiRect_ParentBased);
        r.X1 = pos().X + dx;
        r.Y2 = pos().Y + sizeAtMouseDown().height + dy;
        if (r.size().width >= minSize.width && r.size().height >= minSize.height)
          app()->reshapeWindow(this, r);
        break;
      }

    case uiSensPos_BottomCenterResize:
      {
        int newHeight = sizeAtMouseDown().height + dy;
        if (newHeight >= minSize.height)
          app()->resizeWindow(this, sizeAtMouseDown().width, newHeight);
        break;
      }

    case uiSensPos_BottomRightResize:
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

  if ((m_mouseMoveSensiblePos == uiSensPos_CloseButton || prevSensPos == uiSensPos_CloseButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(0));

  if ((m_mouseMoveSensiblePos == uiSensPos_MaximizeButton || prevSensPos == uiSensPos_MaximizeButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(1));

  if ((m_mouseMoveSensiblePos == uiSensPos_MinimizeButton || prevSensPos == uiSensPos_MinimizeButton) && m_mouseMoveSensiblePos != prevSensPos)
    repaint(getBtnRect(2));

  CursorName cur = CursorName::CursorPointerSimpleReduced;  // this is the default

  switch (m_mouseMoveSensiblePos) {

    case uiSensPos_TopLeftResize:
      cur = CursorName::CursorResize2;
      break;

    case uiSensPos_TopCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiSensPos_TopRightResize:
      cur = CursorName::CursorResize1;
      break;

    case uiSensPos_CenterLeftResize:
      cur = CursorName::CursorResize4;
      break;

    case uiSensPos_CenterRightResize:
      cur = CursorName::CursorResize4;
      break;

    case uiSensPos_BottomLeftResize:
      cur = CursorName::CursorResize1;
      break;

    case uiSensPos_BottomCenterResize:
      cur = CursorName::CursorResize3;
      break;

    case uiSensPos_BottomRightResize:
      cur = CursorName::CursorResize2;
      break;

    default:
      break;
  }

  VGAController.setMouseCursor(cur);
}


void uiFrame::handleButtonsClick(int x, int y)
{
  if (m_frameProps.hasCloseButton && getBtnRect(0).contains(x, y))
    app()->showWindow(this, false);
  else if (m_frameProps.hasMaximizeButton && getBtnRect(1).contains(x, y))
    app()->maximizeWindow(this, !state().maximized && !state().minimized);  // used also for "restore" from minimized
  else if (m_frameProps.hasMinimizeButton && !state().minimized && getBtnRect(2).contains(x, y))
    app()->minimizeWindow(this, !state().minimized);
  else
    return;
  // this avoids the button remains selected (background colored) when window change size
  m_mouseMoveSensiblePos = uiSensPos_None;
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


uiButton::uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible)
  : uiControl(parent, pos, size, visible),
    m_text(NULL),
    m_textExtent(0)
{
  windowProps().focusable = true;
  setText(text);
}


uiButton::~uiButton()
{
}


void uiButton::setText(char const * value)
{
  int len = strlen(value);
  m_text = (char*) realloc(m_text, len + 1);
  strcpy(m_text, value);

  m_textExtent = Canvas.textExtent(m_style.textFont, value);
}


void uiButton::paintButton()
{
  bool hasFocus = (app()->focusedWindow() == this);
  Rect bkgRect = Rect(0, 0, size().width - 1, size().height - 1);
  // border
  if (m_style.borderSize > 0) {
    Canvas.setPenColor(hasFocus ? m_style.focusedBorderColor : m_style.borderColor);
    int bsize = hasFocus? m_style.focusedBorderSize : m_style.borderSize;
    for (int i = 0; i < bsize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    bkgRect = bkgRect.shrink(bsize);
  }
  // background
  RGB bkColor = m_style.backgroundColor;
  if (app()->capturedMouseWindow() == this)
    bkColor = m_style.downBackgroundColor;
  else if (isMouseOver())
    bkColor = m_style.mouseOverBackgroundColor;
  Canvas.setBrushColor(bkColor);
  Canvas.fillRectangle(bkgRect);
  // text
  paintText(bkgRect);
}


void uiButton::paintText(Rect const & rect)
{
  int x = rect.X1 + (rect.size().width - m_textExtent) / 2;
  int y = rect.Y1 + (rect.size().height - m_style.textFont->height) / 2;
  Canvas.setPenColor(m_style.textFontColor);
  Canvas.drawText(m_style.textFont, x, y, m_text);
}


void uiButton::processEvent(uiEvent * event)
{
  uiControl::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event);
      paintButton();
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      repaint();
      break;

    case UIEVT_MOUSEBUTTONUP:
      // this check is required to avoid onclick event when mouse is captured and moved out of button area
      if (rect(uiRect_WindowBased).contains(event->params.mouse.status.X, event->params.mouse.status.Y))
        onClick();
      repaint();
      break;

    case UIEVT_MOUSEMOVE:
      break;

    case UIEVT_MOUSEENTER:
      VGAController.setMouseCursor(CursorName::CursorPointerSimpleReduced);
      repaint();  // to update background color
      break;

    case UIEVT_MOUSELEAVE:
      repaint();  // to update background color
      break;

    default:
      break;
  }
}



// uiButton
////////////////////////////////////////////////////////////////////////////////////////////////////



} // end of namespace

