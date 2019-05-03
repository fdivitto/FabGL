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


#pragma once



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



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvent

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



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiObject

class uiObject {

public:

  uiObject();

  virtual ~uiObject();
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvtHandler


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



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiWindow


enum uiWindowRectType {
  uiRect_ScreenBased,
  uiRect_ParentBased,
  uiRect_WindowBased,
  uiRect_ClientAreaParentBased,
  uiRect_ClientAreaWindowBased,
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

  virtual Rect rect(uiWindowRectType rectType);

  bool isActive() { return m_isActive; }

  void show(bool value);

  bool isShown() { return m_isVisible; }

  uiWindow * parent() { return m_parent; }

  Point mouseDownPos() { return m_mouseDownPos; }


protected:

  // these do not repaint the window, use app()->move() instead
  void setPos(int x, int y)           { m_pos = Point(x, y); }
  void setSize(int width, int height) { m_size = Size(width, height); }

  Size sizeAtMouseDown()              { return m_sizeAtMouseDown; }
  Point posAtMouseDown()              { return m_posAtMouseDown; }

  virtual Size minWindowSize()        { return Size(0, 0); }

  void beginPaint(uiEvent * event);

private:

  uiWindow * m_parent;

  Point      m_pos;
  Size       m_size;

  bool       m_isActive;
  bool       m_isVisible;

  Point      m_mouseDownPos;    // mouse position when mouse down event has been received

  Point      m_posAtMouseDown;  // used to resize
  Size       m_sizeAtMouseDown; // used to resize

  // double linked list, order is: bottom (first items) -> up (last items)
  uiWindow * m_next;
  uiWindow * m_prev;
  uiWindow * m_firstChild;
  uiWindow * m_lastChild;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame

struct uiFrameStyle {
  Color            backgroundColor;
  Color            normalBorderColor;
  Color            activeBorderColor;
  Color            normalTitleBackgroundColor;
  Color            activeTitleBackgroundColor;
  Color            titleFontColor;
  FontInfo const * titleFont;
  int              borderSize;

  uiFrameStyle() :
    backgroundColor(Color::White),
    normalBorderColor(Color::BrightBlack),
    activeBorderColor(Color::BrightBlue),
    normalTitleBackgroundColor(Color::White),
    activeTitleBackgroundColor(Color::BrightWhite),
    titleFontColor(Color::BrightBlack),
    titleFont(Canvas.getPresetFontInfoFromHeight(14, false)),
    borderSize(2)
  { }
};


struct uiFrameProps {
  bool resizeable;
  bool moveable;

  uiFrameProps() :
    resizeable(true),
    moveable(true)
  { }
};


enum uiFrameSensiblePos {
  uiSensPos_None,
  uiSensPos_MoveArea,
  uiSensPos_TopLeftResize,
  uiSensPos_TopCenterResize,
  uiSensPos_TopRightResize,
  uiSensPos_CenterLeftResize,
  uiSensPos_CenterRightResize,
  uiSensPos_BottomLeftResize,
  uiSensPos_BottomCenterResize,
  uiSensPos_BottomRightResize,
};


class uiFrame : public uiWindow {

public:

  uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible);

  virtual ~uiFrame();

  virtual void processEvent(uiEvent * event);

  char const * title() { return m_title; }

  void setTitle(char const * value);

  uiFrameStyle & style() { return m_style; }

  uiFrameProps & props() { return m_props; }

  Rect rect(uiWindowRectType rectType);

protected:

  Size minWindowSize();

private:

  void paintFrame();
  void movingCapturedMouse(int mouseX, int mouseY);
  void movingFreeMouse(int mouseX, int mouseY);
  uiFrameSensiblePos getSensiblePosAt(int x, int y);


  static const int CORNERSENSE = 10;


  uiFrameStyle m_style;

  uiFrameProps m_props;

  char const * m_title;

  uiFrameSensiblePos m_mouseDownSensiblePos;

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiApp


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

  void resizeWindow(uiWindow * window, int width, int height);

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



