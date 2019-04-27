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



namespace fabgl {



// debug only!
void dumpEvent(uiEvent * event)
{
  static const char * TOSTR[] = { "UIEVT_NULL", "UIEVT_APPINIT", "UIEVT_ABSPAINT", "UIEVT_PAINT", "UIEVT_ACTIVATE",
                                  "UIEVT_DEACTIVATE", "UIEVT_MOUSEMOVE",
                                  "UIEVT_MOUSEWHEEL", "UIEVT_MOUSEBUTTONDOWN", "UIEVT_MOUSEBUTTONUP" };
  Serial.write(TOSTR[event->id]);
  if (event->dest && event->dest->evtHandlerProps().isFrame)
    Serial.printf(" dst=\"%s\"(%p) ", ((uiFrame*)(event->dest))->title(), event->dest);
  else
    Serial.printf(" dst=%p ", event->dest);
  switch (event->id) {
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
    case UIEVT_ABSPAINT:
      Serial.printf("rect=%d,%d,%d,%d", event->params.paintRect.X1, event->params.paintRect.Y1, event->params.paintRect.X2, event->params.paintRect.Y2);
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
    m_capturedMouseWindow(NULL)
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
  // root window always stays at 0, 0 and cannot be moved
  m_rootWindow = new uiFrame(NULL, "", Point(0, 0), Size(Canvas.getWidth(), Canvas.getHeight()), true);
  m_rootWindow->setApp(this);
  m_rootWindow->frameProps().hasBorder = false;

  m_activeWindow = m_rootWindow;

  // generate UIEVT_APPINIT event
  uiEvent evt = uiEvent(this, UIEVT_APPINIT);
  postEvent(&evt);

  repaintWindow(m_rootWindow);

  // dispatch events
  while (true) {
    uiEvent event;
    if (getEvent(&event, -1)) {

      preprocessEvent(&event);

      // debug
      //dumpEvent(&event);

      if (event.dest)
        event.dest->processEvent(&event);
    }
  }
}


void uiApp::preprocessEvent(uiEvent * event)
{
  if (event->dest == NULL) {
    // need to select a destination object
    switch (event->id) {
      case UIEVT_MOUSEMOVE:
      case UIEVT_MOUSEWHEEL:
      case UIEVT_MOUSEBUTTONDOWN:
      case UIEVT_MOUSEBUTTONUP:
        translateMouseEvent(event);
        break;
      case UIEVT_ABSPAINT:
        // absolute paint
        generatePaintEvents(m_rootWindow, event->params.paintRect);
        break;
      default:
        break;
    }
  }

  if (event->dest) {
    switch (event->id) {
      case UIEVT_MOUSEBUTTONDOWN:
        // activate window
        if (event->dest->evtHandlerProps().isWindow)
          setActiveWindow((uiWindow*) event->dest);
        break;
      default:
        break;
    }
  }
}


// look for destination window at event X, Y coordinates, then set "dest" field and modify mouse X, Y coordinates (convert to child coordinates)
void uiApp::translateMouseEvent(uiEvent * event)
{
  Point mousePos = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
  if (m_capturedMouseWindow) {
    // mouse captured, just go back up to m_rootWindow
    for (uiWindow * cur = m_capturedMouseWindow; cur != m_rootWindow; cur = cur->parent())
      mousePos = sub(mousePos, cur->pos());
    event->dest = m_capturedMouseWindow;
  } else {
    event->dest = screenToWindow(mousePos);
  }
  event->params.mouse.status.X = mousePos.X;
  event->params.mouse.status.Y = mousePos.Y;
}


// convert screen coordinates to window coordinates (the most visible window)
// return the window under specified absolute coordinates
uiWindow * uiApp::screenToWindow(Point & point)
{
  uiWindow * win = m_rootWindow;
  while (win->hasChildren()) {
    uiWindow * child = win->lastChild();
    for (; child; child = child->prev()) {
      if (child->isShown() && pointInRect(point, child->rect())) {
        win   = child;
        point = sub(point, child->pos());
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


bool uiApp::getEvent(uiEvent * event, int timeOutMS)
{
  return xQueueReceive(m_eventsQueue, event,  timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS)) == pdTRUE;
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
    m_activeWindow = value;

    if (m_activeWindow) {

      if (m_activeWindow->parent())
        m_activeWindow->parent()->moveChildOnTop(m_activeWindow);

      if (prev) {
        // deactivate previous window
        uiEvent evt = uiEvent(prev, UIEVT_DEACTIVATE);
        postEvent(&evt);
      }

      // activate window
      uiEvent evt = uiEvent(m_activeWindow, UIEVT_ACTIVATE);
      postEvent(&evt);

    }
  }

  return prev;
}


void uiApp::repaintWindow(uiWindow * window)
{
  repaintRect(window->screenRect());
}


void uiApp::repaintRect(Rect const & rect)
{
  uiEvent evt = uiEvent(NULL, UIEVT_ABSPAINT);
  evt.params.paintRect = rect;
  postEvent(&evt);
}


// move to position (x, y) relative to the parent window
void uiApp::moveWindow(uiWindow * window, int x, int y)
{
  Point prevPos = window->pos();
  window->setPos(x, y);

  //// repaint discovered areas

  int dx = x - prevPos.X;
  int dy = y - prevPos.Y;
  Rect winRect = window->screenRect();
  Rect BURect;  // bottom or upper rect
  Rect LRRect;  // left or right rect

  // moving to the right/left? repaint left/right side
  if (dx > 0)
    LRRect = Rect(winRect.X1 - dx, winRect.Y1, winRect.X1 - 1, winRect.Y2);
  else if (dx < 0)
    LRRect = Rect(winRect.X2 + 1, winRect.Y1, winRect.X2 - dx, winRect.Y2);

  // moving up/down? repaint bottom/up side
  if (dy < 0)
    BURect = Rect(winRect.X1, winRect.Y2 + 1, winRect.X2, winRect.Y2 - dy);
  else if (dy > 0)
    BURect = Rect(winRect.X1, winRect.Y1 - dy, winRect.X2, winRect.Y1 - 1);

  // moving diagonally, include corners in bottom or upper rectangle
  if (dy) {
    if (dx > 0)
      BURect.X1 -= dx;
    else if (dx < 0)
      BURect.X2 -= dx;
  }

  if (dx)
    repaintRect(LRRect);
  if (dy)
    repaintRect(BURect);

  //// repaint the window
  // TODO: what about just moving the screen buffer? - when is top and fully visible window
  repaintRect(winRect);
}


// given a relative paint rect generate a set of UIEVT_PAINT events
void uiApp::generatePaintEvents(uiWindow * baseWindow, Rect const & rect)
{
  Stack<Rect> rects;
  rects.push(rect);
  while (!rects.isEmpty()) {
    Rect thisRect = rects.pop();
    bool noIntesections = true;
    for (uiWindow * win = baseWindow->lastChild(); win; win = win->prev()) {
      if (win->isShown() && intersect(thisRect, win->rect())) {
        noIntesections = false;
        removeRectangle(rects, thisRect, win->rect());
        Rect newRect = translate(intersection(thisRect, win->rect()), -win->pos().X, -win->pos().Y);
        generatePaintEvents(win, newRect);
        break;
      }
    }
    if (noIntesections) {
      uiEvent evt = uiEvent(NULL, UIEVT_PAINT);
      evt.dest = baseWindow;
      evt.params.paintRect = thisRect;
      postEvent(&evt);
    }
  }
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
    m_isActive(false),
    m_isVisible(visible),
    m_mouseDownPos(Point(-1, -1)),
    m_next(NULL),
    m_prev(NULL),
    m_firstChild(NULL),
    m_lastChild(NULL)
{
  evtHandlerProps().isWindow = true;
  if (parent)
    parent->addChild(this);
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


// return window bounding rect relative to the root window
Rect uiWindow::screenRect()
{
  Rect r = rect();
  for (uiWindow * win = m_parent; win; win = win->m_parent)
    r = translate(r, win->m_pos);
  return r;
}


Rect uiWindow::clientRect()
{
  return Rect(0, 0, m_size.width - 1, m_size.height - 1);
}


void uiWindow::show(bool value)
{
  m_isVisible = value;
}


void uiWindow::beginPaint(uiEvent * event)
{
  Rect srect = screenRect();
  Canvas.setOrigin(srect.X1, srect.Y1);
  Rect clipRect = event->params.paintRect;
  if (m_parent)
    clipRect = intersection(clipRect, translate(m_parent->clientRect(), neg(m_pos)));
  Canvas.setClippingRect(clipRect);
}


void uiWindow::processEvent(uiEvent * event)
{
  uiEvtHandler::processEvent(event);

  switch (event->id) {

    case UIEVT_ACTIVATE:
      m_isActive = true;
      app()->repaintWindow(this);
      break;

    case UIEVT_DEACTIVATE:
      m_isActive = false;
      app()->repaintWindow(this);
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      m_mouseDownPos = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
      // capture mouse if left button is down
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(this);
      break;

    case UIEVT_MOUSEBUTTONUP:
      // end capture mouse if left button is up
      if (event->params.mouse.changedButton == 1)
        app()->captureMouse(NULL);
      break;

    default:
      break;
  }
}


// uiWindow
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame


uiFrame::uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible)
  : uiWindow(parent, pos, size, visible),
    m_title(title)
{
  evtHandlerProps().isFrame = true;
}


uiFrame::~uiFrame()
{
}


// todo: make a copy of title
void uiFrame::setTitle(char const * value)
{
  m_title = value;
}


Rect uiFrame::clientRect()
{
  Rect r = uiWindow::clientRect();
  // border
  if (m_props.hasBorder) {
    r.X1 += 1;
    r.Y1 += 1;
    r.X2 -= 1;
    r.Y2 -= 1;
  }
  // title bar
  if (strlen(m_title)) {
    r.Y1 += 1 + m_props.titleFont->height;
  }
  return r;
}


void uiFrame::paintFrame()
{
  // background
  Canvas.setBrushColor(m_props.backgroundColor);
  Canvas.fillRectangle(0, 0, size().width - 1, size().height - 1);
  if (m_props.hasBorder) {
    // border
    Canvas.setPenColor(isActive() ? m_props.activeBorderColor : m_props.normalBorderColor);
    Canvas.drawRectangle(0, 0, size().width - 1, size().height - 1);
  }
  // title bar
  if (strlen(m_title)) {
    // title bar background
    Canvas.setBrushColor(isActive() ? m_props.activeTitleBackgroundColor : m_props.normalTitleBackgroundColor);
    Canvas.fillRectangle(1, 1, size().width - 2, 1 + m_props.titleFont->height);
    // title
    Canvas.setPenColor(m_props.titleFontColor);
    Canvas.drawText(m_props.titleFont, 2, 2, m_title);
  }
}


bool uiFrame::isInsideTitleBar(Point const & point)
{
  return (point.X >= 1 && point.X <= size().width - 2 && point.Y >= 1 && point.Y <= 1 + m_props.titleFont->height);
}


void uiFrame::processEvent(uiEvent * event)
{
  uiWindow::processEvent(event);

  switch (event->id) {

    case UIEVT_PAINT:
      beginPaint(event);
      paintFrame();
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this && isInsideTitleBar(mouseDownPos()))
        app()->moveWindow(this, pos().X + event->params.mouse.status.X - mouseDownPos().X, pos().Y + event->params.mouse.status.Y - mouseDownPos().Y);
      break;

    default:
      break;
  }
}


// uiFrame
////////////////////////////////////////////////////////////////////////////////////////////////////




} // end of namespace

