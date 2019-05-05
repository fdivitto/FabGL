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




#define FABGLIB_UI_EVENTS_QUEUE_SIZE 64 // increase in case of garbage between windows!



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvent

enum uiEventID {
  UIEVT_NULL,
  UIEVT_DEBUGMSG,
  UIEVT_APPINIT,
  UIEVT_ABSPAINT,
  UIEVT_PAINT,
  UIEVT_ACTIVATE,
  UIEVT_DEACTIVATE,
  UIEVT_MOUSEMOVE,
  UIEVT_MOUSEWHEEL,
  UIEVT_MOUSEBUTTONDOWN,
  UIEVT_MOUSEBUTTONUP,
  UIEVT_SETPOS,
  UIEVT_SETSIZE,
  UIEVT_RESHAPEWINDOW,
  UIEVT_MOUSEENTER,
  UIEVT_MOUSELEAVE,
  UIEVT_MAXIMIZE,   // Request for maximize
  UIEVT_MINIMIZE,   // Request for minimize
  UIEVT_RESTORE,    // restore from UIEVT_MAXIMIZE or UIEVT_MINIMIZE
  UIEVT_SHOW,
  UIEVT_HIDE,
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
    // event: UIEVT_PAINT, UIEVT_ABSPAINT, UIEVT_RESHAPEWINDOW
    Rect rect;
    // event: UIEVT_SETPOS
    Point pos;
    // event: UIEVT_SETSIZE
    Size size;
    // event: UIEVT_DEBUGMSG
    char const * debugMsg;

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


struct uiWindowState {
  uint8_t visible   : 1;  // 0 = hidden, 1 = visible
  uint8_t maximized : 1;  // 0 = normal, 1 = maximized
  uint8_t minimized : 1;  // 0 = normal, 1 = minimized
  uint8_t active    : 1;  // 0 = inactive, 1= active
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

  uiWindowState state() { return m_state; }

  uiWindow * parent() { return m_parent; }

  Point mouseDownPos() { return m_mouseDownPos; }

  Rect transformRect(Rect const & rect, uiWindow * baseWindow);

  void repaint(Rect const & rect);

  void repaint();


protected:

  Size sizeAtMouseDown()              { return m_sizeAtMouseDown; }
  Point posAtMouseDown()              { return m_posAtMouseDown; }

  virtual Size minWindowSize()        { return Size(0, 0); }

  void beginPaint(uiEvent * event);

  void setPos(Point const & p)        { m_pos = p; }
  void setSize(Size const & s)        { m_size = s; }

private:

  uiWindow *    m_parent;

  Point         m_pos;
  Size          m_size;

  // saved screen rect before Maximize or Minimize
  Rect          m_savedScreenRect;

  uiWindowState m_state;

  Point         m_mouseDownPos;    // mouse position when mouse down event has been received

  Point         m_posAtMouseDown;  // used to resize
  Size          m_sizeAtMouseDown; // used to resize

  // double linked list, order is: bottom (first items) -> up (last items)
  uiWindow *    m_next;
  uiWindow *    m_prev;
  uiWindow *    m_firstChild;
  uiWindow *    m_lastChild;
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
  Color            activeButtonColor; // color used to draw Close, Maximize and Minimize buttons
  Color            normalButtonColor; // color used to draw Close, Maximize and Minimize buttons
  Color            mouseOverBackgroundButtonColor;  // color used for background of Close, Maximize and Minimize buttons when mouse is over them
  Color            mouseOverBruttonColor;           // color used for pen of Close, Maximize and Minimize buttons when mouse is over them

  uiFrameStyle() :
    backgroundColor(Color::White),
    normalBorderColor(Color::BrightBlack),
    activeBorderColor(Color::BrightBlue),
    normalTitleBackgroundColor(Color::White),
    activeTitleBackgroundColor(Color::BrightWhite),
    titleFontColor(Color::BrightBlack),
    titleFont(Canvas.getPresetFontInfoFromHeight(14, false)),
    borderSize(2),
    activeButtonColor(Color::BrightBlue),
    normalButtonColor(Color::BrightBlack),
    mouseOverBackgroundButtonColor(Color::BrightBlue),
    mouseOverBruttonColor(Color::BrightWhite)
  { }
};


struct uiFrameProps {
  uint8_t resizeable        : 1;
  uint8_t moveable          : 1;
  uint8_t hasCloseButton    : 1;
  uint8_t hasMaximizeButton : 1;
  uint8_t hasMinimizeButton : 1;

  uiFrameProps() :
    resizeable(true),
    moveable(true),
    hasCloseButton(true),
    hasMaximizeButton(true),
    hasMinimizeButton(true)
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
  uiSensPos_CloseButton,
  uiSensPos_MaximizeButton,
  uiSensPos_MinimizeButton,
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
  Rect getBtnRect(int buttonIndex);
  void handleButtonsClick(int x, int y);


  static const int CORNERSENSE = 10;


  uiFrameStyle m_style;

  uiFrameProps m_props;

  char const * m_title;

  uiFrameSensiblePos m_mouseDownSensiblePos;  // sensible position on mouse down
  uiFrameSensiblePos m_mouseMoveSensiblePos;  // sensible position on mouse move

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiApp


class uiApp : public uiEvtHandler {

public:

  uiApp();

  virtual ~uiApp();

  void run();

  bool postEvent(uiEvent const * event);

  bool insertEvent(uiEvent const * event);

  void postDebugMsg(char const * msg);

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

  void reshapeWindow(uiWindow * window, Rect const & rect);

  uiWindow * screenToWindow(Point & point);

  void showWindow(uiWindow * window, bool value);

  void maximizeWindow(uiWindow * window, bool value);

  void minimizeWindow(uiWindow * window, bool value);


  // events

  virtual void OnInit();

protected:

  bool getEvent(uiEvent * event, int timeOutMS);


private:

  void preprocessEvent(uiEvent * event);
  void preprocessMouseEvent(uiEvent * event);
  void generatePaintEvents(uiWindow * baseWindow, Rect const & rect);
  void generateReshapeEvents(uiWindow * window, Rect const & rect);

  QueueHandle_t m_eventsQueue;

  uiFrame * m_rootWindow;
  uiWindow * m_activeWindow;

  uiWindow * m_capturedMouseWindow; // window that has captured mouse

  uiWindow * m_freeMouseWindow;     // window where mouse is over
};




} // end of namespace



