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


#ifndef _FABUI_H_INCLUDED
#define _FABUI_H_INCLUDED


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "vgacontroller.h"
#include "canvas.h"



/*

  uiObject
    uiEvtHandler
      uiApp
      uiWindow
        uiFrame
        uiControl
          uiButton
          uiCheckBox
          uiListBox
          uiComboBox
          uiMenu
          uiGauge
          uiRadioButton
          uiTextCtrl
          uiRichTextCtrl
          uiScrollBar
          uiSlider
          uiSpinButton
*/


namespace fabgl {




#define FABGLIB_UI_EVENTS_QUEUE_SIZE 32



enum uiEventID {
  UIEVT_NULL,
  UIEVT_APPINIT,
  UIEVT_ABSPAINT,
  UIEVT_PAINT,
  UIEVT_ACTIVATE,
  UIEVT_DEACTIVATE,
  UIEVT_MOUSEMOVE,
  UIEVT_MOUSEWHEEL,
  UIEVT_MOUSEBUTTONDOWN,
  UIEVT_MOUSEBUTTONUP,
};


class uiEvtHandler;
class uiApp;
class uiWindow;


struct uiEvent {
  uiEvtHandler * dest;
  uiEventID      id;

  union uiEventParams {
    // event: UIEVT_MOUSEMOVE, UIEVT_MOUSEWHEEL, UIEVT_MOUSEBUTTONDOWN, UIEVT_MOUSEBUTTONUP
    struct {
      MouseStatus status;
      uint8_t     changedButton;  // 0 = none, 1 = left, 2 = middle, 3 = right
    } mouse;
    // event: UIEVT_PAINT, UIEVT_ABSPAINT
    Rect paintRect;

    uiEventParams() { }
  } params;

  uiEvent() : dest(NULL), id(UIEVT_NULL) { }
  uiEvent(uiEvent const & e) { dest = e.dest; id = e.id; params = e.params; }
  uiEvent(uiEvtHandler * dest_, uiEventID id_) : dest(dest_), id(id_) { }
};


class uiObject {

public:

  uiObject();

  virtual ~uiObject();
};


struct uiEvtHandlerProps {
  int isWindow         : 1; // is a uiWindow or inherited object
  int isFrame          : 1; // is a uiFrame of inherited object

  uiEvtHandlerProps() :
    isWindow(false),
    isFrame(false)
  {
  }
};


class uiEvtHandler : public uiObject {

public:

  uiEvtHandler(uiApp * app);

  virtual ~uiEvtHandler();

  virtual void processEvent(uiEvent * event);

  uiApp * app() { return m_app; }

  void setApp(uiApp * value) { m_app = value; }

  uiEvtHandlerProps & evtHandlerProps() { return m_props; }

private:

  uiApp *           m_app;
  uiEvtHandlerProps m_props;
};



class uiWindow : public uiEvtHandler {

friend class uiApp;

public:

  uiWindow(uiWindow * parent, const Point & pos, const Size & size, bool visible);

  virtual ~uiWindow();

  virtual void processEvent(uiEvent * event);

  void freeChildren();

  uiWindow * next()  { return m_next; }

  uiWindow * prev()  { return m_prev; }

  uiWindow * firstChild() { return m_firstChild; }

  uiWindow * lastChild() { return m_lastChild; }

  bool hasChildren() { return m_firstChild != NULL; }

  void addChild(uiWindow * child);

  void removeChild(uiWindow * child, bool freeChild);

  void moveChildOnTop(uiWindow * child);

  Point pos() { return m_pos; }

  Size size() { return m_size; }

  Rect rect() { return Rect(m_pos.X, m_pos.Y, m_pos.X + m_size.width - 1, m_pos.Y + m_size.height - 1); }

  Rect screenRect();

  virtual Rect clientRect();

  bool isActive() { return m_isActive; }

  void show(bool value);

  bool isShown() { return m_isVisible; }

  uiWindow * parent() { return m_parent; }

  Point mouseDownPos() { return m_mouseDownPos; }


protected:

  // this doesn't repaint the window, use app()->move() instead
  void setPos(int x, int y) { m_pos = Point(x, y); }

  void beginPaint(uiEvent * event);

private:

  uiWindow * m_parent;

  Point      m_pos;
  Size       m_size;

  bool       m_isActive;
  bool       m_isVisible;

  Point      m_mouseDownPos;

  // double linked list, order is: bottom (first items) -> up (last items)
  uiWindow * m_next;
  uiWindow * m_prev;
  uiWindow * m_firstChild;
  uiWindow * m_lastChild;
};


struct uiFrameProps {
  Color backgroundColor;
  Color normalBorderColor;
  Color activeBorderColor;
  Color normalTitleBackgroundColor;
  Color activeTitleBackgroundColor;
  Color titleFontColor;
  FontInfo const * titleFont;
  bool hasBorder;

  uiFrameProps() :
    backgroundColor(Color::White),
    normalBorderColor(Color::BrightBlack),
    activeBorderColor(Color::BrightBlue),
    normalTitleBackgroundColor(Color::White),
    activeTitleBackgroundColor(Color::BrightWhite),
    titleFontColor(Color::BrightBlack),
    titleFont(Canvas.getPresetFontInfo(80, 25)),
    hasBorder(true)
  { }
};


class uiFrame : public uiWindow {

public:

  uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible);

  virtual ~uiFrame();

  virtual void processEvent(uiEvent * event);

  char const * title() { return m_title; }

  void setTitle(char const * value);

  uiFrameProps & frameProps() { return m_props; }

  Rect clientRect();

private:

  void paintFrame();
  bool isInsideTitleBar(Point const & point);

  uiFrameProps m_props;

  char const * m_title;

};



class uiApp : public uiEvtHandler {

public:

  uiApp();

  virtual ~uiApp();

  void run();

  bool postEvent(uiEvent const * event);

  virtual void processEvent(uiEvent * event);

  uiFrame * rootWindow() { return m_rootWindow; }

  uiWindow * activeWindow() { return m_activeWindow; }

  uiWindow * setActiveWindow(uiWindow * value);

  void captureMouse(uiWindow * window) { m_capturedMouseWindow = window; }

  uiWindow * capturedMouseWindow() { return m_capturedMouseWindow; }

  void repaintWindow(uiWindow * window);

  void repaintRect(Rect const & rect);

  void moveWindow(uiWindow * window, int x, int y);

  uiWindow * screenToWindow(Point & point);


  // events

  virtual void OnInit();

protected:

  bool getEvent(uiEvent * event, int timeOutMS);


private:

  void preprocessEvent(uiEvent * event);
  void translateMouseEvent(uiEvent * event);
  void generatePaintEvents(uiWindow * baseWindow, Rect const & rect);

  QueueHandle_t m_eventsQueue;

  uiFrame * m_rootWindow;
  uiWindow * m_activeWindow;

  uiWindow * m_capturedMouseWindow; // window that has captured mouse
};




} // end of namespace





#endif

