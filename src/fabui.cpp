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
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), false, true, this);
  VGAController.setMouseCursor(CursorName::CursorPointerSimpleReduced);

  // root window always stays at 0, 0 and cannot be moved
  m_rootWindow = new uiFrame(NULL, "", Point(0, 0), Size(Canvas.getWidth(), Canvas.getHeight()), true);
  m_rootWindow->setApp(this);
  m_rootWindow->style().hasBorder = false;

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
      if (child->isShown() && pointInRect(point, win->rect(uiRect_ClientAreaWindowBased)) && pointInRect(point, child->rect(uiRect_ParentBased))) {
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

      if (prev) {
        // deactivate previous window
        uiEvent evt = uiEvent(prev, UIEVT_DEACTIVATE);
        postEvent(&evt);
        repaintWindow(prev);
      }

      // activate window
      uiEvent evt = uiEvent(m_activeWindow, UIEVT_ACTIVATE);
      postEvent(&evt);

      for (uiWindow * child = m_activeWindow; child->parent() != NULL; child = child->parent()) {
        child->parent()->moveChildOnTop(child);
        repaintWindow(child);
      }

    }
  }

  return prev;
}


void uiApp::repaintWindow(uiWindow * window)
{
  repaintRect(window->rect(uiRect_ScreenBased));
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
  Rect winRect = window->rect(uiRect_ScreenBased);
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


void uiApp::resizeWindow(uiWindow * window, int width, int height)
{
  Size prevSize = window->size();
  window->setSize(width, height);

  //// repaint discovered areas

  int dx = width - prevSize.width;
  int dy = height - prevSize.height;
  Rect winRect = window->rect(uiRect_ScreenBased);
  Rect RRect;  // right rect
  Rect BRect;  // bottom rect

  if (dx < 0)
    RRect = Rect(winRect.X2, winRect.Y1, winRect.X2 - dx, winRect.Y2);
  if (dy < 0)
    BRect = Rect(winRect.X1, winRect.Y2, winRect.X2, winRect.Y2 - dy);

  // resizing diagonally, include corners in bottom or upper rectangle
  if (dy && dx < 0)
      BRect.X2 -= dx;

  if (dx < 0)
    repaintRect(RRect);
  if (dy < 0)
    repaintRect(BRect);

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
      Rect winRect = intersection(baseWindow->rect(uiRect_ClientAreaWindowBased), win->rect(uiRect_ParentBased));
      if (win->isShown() && intersect(thisRect, winRect)) {
        noIntesections = false;
        removeRectangle(rects, thisRect, winRect);
        Rect newRect = translate(intersection(thisRect, winRect), -win->pos().X, -win->pos().Y);
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


Rect uiWindow::rect(uiWindowRectType rectType)
{
  switch (rectType) {
    case uiRect_ScreenBased:
      {
        // return window bounding rect relative to the root window
        Rect r = rect(uiRect_ParentBased);
        for (uiWindow * win = m_parent; win; win = win->m_parent)
          r = translate(r, win->m_pos);
        return r;
      }
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


void uiWindow::show(bool value)
{
  m_isVisible = value;
}


void uiWindow::beginPaint(uiEvent * event)
{
  Rect srect = rect(uiRect_ScreenBased);
  Canvas.setOrigin(srect.X1, srect.Y1);
  Rect clipRect = event->params.paintRect;
  if (m_parent)
    clipRect = intersection(clipRect, translate(m_parent->rect(uiRect_ClientAreaWindowBased), neg(m_pos)));
  Canvas.setClippingRect(clipRect);
}


void uiWindow::processEvent(uiEvent * event)
{
  uiEvtHandler::processEvent(event);

  switch (event->id) {

    case UIEVT_ACTIVATE:
      m_isActive = true;
      break;

    case UIEVT_DEACTIVATE:
      m_isActive = false;
      break;

    case UIEVT_MOUSEBUTTONDOWN:
      m_mouseDownPos    = Point(event->params.mouse.status.X, event->params.mouse.status.Y);
      m_posAtMouseDown  = m_pos;
      m_sizeAtMouseDown = m_size;
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
    m_isResizeable(true),
    m_title(title),
    m_mouseDownSensiblePos(uiSensPos_None)
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


Rect uiFrame::rect(uiWindowRectType rectType)
{
  Rect r = uiWindow::rect(rectType);
  switch (rectType) {
    case uiRect_ClientAreaParentBased:
    case uiRect_ClientAreaWindowBased:
      // border
      if (m_style.hasBorder) {
        r.X1 += m_style.borderSize;
        r.Y1 += m_style.borderSize;
        r.X2 -= m_style.borderSize;
        r.Y2 -= m_style.borderSize;
      }
      // title bar
      if (strlen(m_title))
        r.Y1 += 1 + m_style.titleFont->height;
      return r;
    default:
      return r;
  }
}


void uiFrame::paintFrame()
{
  Rect bkgRect = Rect(0, 0, size().width - 1, size().height - 1);
  // title bar
  if (strlen(m_title)) {
    // title bar background
    Canvas.setBrushColor(isActive() ? m_style.activeTitleBackgroundColor : m_style.normalTitleBackgroundColor);
    Canvas.fillRectangle(m_style.borderSize, m_style.borderSize, size().width - 1 - m_style.borderSize, 1 + m_style.titleFont->height + m_style.borderSize);
    // title
    Canvas.setPenColor(m_style.titleFontColor);
    Canvas.setGlyphOptions(GlyphOptions().FillBackground(true).DoubleWidth(0).Bold(false).Italic(false).Underline(false).Invert(0));
    Canvas.drawText(m_style.titleFont, 1 + m_style.borderSize, 1 + m_style.borderSize, m_title);
    // adjust background rect
    bkgRect.Y1 += 1 + m_style.titleFont->height;
  }
  // border
  if (m_style.hasBorder) {
    Canvas.setPenColor(isActive() ? m_style.activeBorderColor : m_style.normalBorderColor);
    for (int i = 0; i < m_style.borderSize; ++i)
      Canvas.drawRectangle(0 + i, 0 + i, size().width - 1 - i, size().height - 1 - i);
    // adjust background rect
    bkgRect.X1 += m_style.borderSize;
    bkgRect.Y1 += m_style.borderSize;
    bkgRect.X2 -= m_style.borderSize;
    bkgRect.Y2 -= m_style.borderSize;
  }
  // background
  Canvas.setBrushColor(m_style.backgroundColor);
  Canvas.fillRectangle(bkgRect);
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
      break;

    case UIEVT_MOUSEBUTTONUP:
      // this sets the right mouse cursor in case of end of capturing
      movingFreeMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    case UIEVT_MOUSEMOVE:
      if (app()->capturedMouseWindow() == this)
        movingCapturedMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      else
        movingFreeMouse(event->params.mouse.status.X, event->params.mouse.status.Y);
      break;

    default:
      break;
  }
}


Size uiFrame::minWindowSize()
{
  Size r = Size(0, 0);
  if (m_isResizeable) {
    r.width  += CORNERSENSE * 2 + m_style.borderSize * 2;
    r.height += CORNERSENSE * 2 + m_style.borderSize * 2;
  }
  if (strlen(m_title))
    r.height += 1 + m_style.titleFont->height;
  return r;
}


uiFrameSensiblePos uiFrame::getSensiblePosAt(int x, int y)
{
  Point p = Point(x, y);

  int w = size().width;
  int h = size().height;

  if (m_isResizeable) {

    // on top center, resize
    if (pointInRect(p, Rect(CORNERSENSE, 0, w - CORNERSENSE, m_style.borderSize)))
      return uiSensPos_TopCenterResize;

    // on left center side, resize
    if (pointInRect(p, Rect(0, CORNERSENSE, m_style.borderSize, h - CORNERSENSE)))
      return uiSensPos_CenterLeftResize;

    // on right center side, resize
    if (pointInRect(p, Rect(w - m_style.borderSize, CORNERSENSE, w - 1, h - CORNERSENSE)))
      return uiSensPos_CenterRightResize;

    // on bottom center, resize
    if (pointInRect(p, Rect(CORNERSENSE, h - m_style.borderSize, w - CORNERSENSE, h - 1)))
      return uiSensPos_BottomCenterResize;

    // on top left side, resize
    if (pointInRect(p, Rect(0, 0, CORNERSENSE, CORNERSENSE)))
      return uiSensPos_TopLeftResize;

    // on top right side, resize
    if (pointInRect(p, Rect(w - CORNERSENSE, 0, w - 1, CORNERSENSE)))
      return uiSensPos_TopRightResize;

    // on bottom left side, resize
    if (pointInRect(p, Rect(0, h - CORNERSENSE, CORNERSENSE, h - 1)))
      return uiSensPos_BottomLeftResize;

    // on bottom right side, resize
    if (pointInRect(p, Rect(w - CORNERSENSE, h - CORNERSENSE, w - 1, h - 1)))
      return uiSensPos_BottomRightResize;

  }

  // on title bar, moving area
  if (pointInRect(p, Rect(1, 1, w - 2, 1 + m_style.titleFont->height)))
    return uiSensPos_MoveArea;

  return uiSensPos_None;
}


void uiFrame::movingCapturedMouse(int mouseX, int mouseY)
{
  int dx = mouseX - mouseDownPos().X;
  int dy = mouseY - mouseDownPos().Y;

  Size minSize = minWindowSize();

  switch (m_mouseDownSensiblePos) {

    case uiSensPos_MoveArea:
      // this works because mouseX and mouseY are always relative to current window position, so dx and dy are actually relative movements
      app()->moveWindow(this, pos().X + dx, pos().Y + dy);
      break;

    case uiSensPos_CenterRightResize:
      {
        int newWidth = sizeAtMouseDown().width + dx;
        if (newWidth >= minSize.width) {
          app()->resizeWindow(this, newWidth, sizeAtMouseDown().height);
        }
        break;
      }

    case uiSensPos_CenterLeftResize:
      {
        int newPosX  = pos().X + dx;
        int newWidth = sizeAtMouseDown().width + posAtMouseDown().X - newPosX;
        if (newWidth >= minSize.width) {
          app()->moveWindow(this, newPosX, pos().Y);
          app()->resizeWindow(this, newWidth, size().height);
        }
        break;
      }

    case uiSensPos_TopLeftResize:
      {
        int newPosX  = pos().X + dx;
        int newPosY  = pos().Y + dy;
        int newWidth = sizeAtMouseDown().width + posAtMouseDown().X - newPosX;
        int newHeight = sizeAtMouseDown().height + posAtMouseDown().Y - newPosY;
        if (newWidth >= minSize.width && newHeight >= minSize.height) {
          app()->moveWindow(this, newPosX, newPosY);
          app()->resizeWindow(this, newWidth, newHeight);
        }
        break;
      }

    case uiSensPos_TopCenterResize:
      {
        int newPosY  = pos().Y + dy;
        int newHeight = sizeAtMouseDown().height + posAtMouseDown().Y - newPosY;
        if (newHeight >= minSize.height) {
          app()->moveWindow(this, pos().X, newPosY);
          app()->resizeWindow(this, size().width, newHeight);
        }
        break;
      }

    case uiSensPos_TopRightResize:
      {
        int newPosY  = pos().Y + dy;
        int newWidth = sizeAtMouseDown().width + dx;
        int newHeight = sizeAtMouseDown().height + posAtMouseDown().Y - newPosY;
        if (newWidth >= minSize.width && newHeight >= minSize.height) {
          app()->moveWindow(this, pos().X, newPosY);
          app()->resizeWindow(this, newWidth, newHeight);
        }
        break;
      }

    case uiSensPos_BottomLeftResize:
      {
        int newPosX  = pos().X + dx;
        int newWidth = sizeAtMouseDown().width + posAtMouseDown().X - newPosX;
        int newHeight = sizeAtMouseDown().height + dy;
        if (newWidth >= minSize.width && newHeight >= minSize.height) {
          app()->moveWindow(this, newPosX, pos().Y);
          app()->resizeWindow(this, newWidth, newHeight);
        }
        break;
      }

    case uiSensPos_BottomCenterResize:
      {
        int newHeight = sizeAtMouseDown().height + dy;
        if (newHeight >= minSize.height) {
          app()->resizeWindow(this, sizeAtMouseDown().width, newHeight);
        }
        break;
      }

    case uiSensPos_BottomRightResize:
      {
        int newWidth = sizeAtMouseDown().width + dx;
        int newHeight = sizeAtMouseDown().height + dy;
        if (newWidth >= minSize.width && newHeight >= minSize.height) {
          app()->resizeWindow(this, newWidth, newHeight);
        }
        break;
      }

    default:
      break;
  }
}


void uiFrame::movingFreeMouse(int mouseX, int mouseY)
{
  CursorName cur = CursorName::CursorPointerSimpleReduced;  // this is the default

  uiFrameSensiblePos sensPos = getSensiblePosAt(mouseX, mouseY);

  switch (sensPos) {
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


// uiFrame
////////////////////////////////////////////////////////////////////////////////////////////////////




} // end of namespace

