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



// increase in case of garbage between windows!
#define FABGLIB_UI_EVENTS_QUEUE_SIZE 64



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvent

enum uiEventID {
  UIEVT_NULL,
  UIEVT_DEBUGMSG,
  UIEVT_APPINIT,
  UIEVT_GENPAINTEVENTS,
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
  UIEVT_SETFOCUS,
  UIEVT_KILLFOCUS,
  UIEVT_KEYDOWN,
  UIEVT_KEYUP,
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
    // event: UIEVT_PAINT, UIEVT_GENPAINTEVENTS, UIEVT_RESHAPEWINDOW
    Rect rect;
    // event: UIEVT_SETPOS
    Point pos;
    // event: UIEVT_SETSIZE
    Size size;
    // event: UIEVT_DEBUGMSG
    char const * debugMsg;
    // event: UIEVT_KEYDOWN, UIEVT_KEYUP
    struct {
      VirtualKey VK;
      uint8_t    LALT  : 1;  // status of left-ALT key
      uint8_t    RALT  : 1;  // status of right-ALT key
      uint8_t    CTRL  : 1;  // status of CTRL (left or right) key
      uint8_t    SHIFT : 1;  // status of SHIFT (left or right) key
      uint8_t    GUI   : 1;  // status of GUI (Windows logo) key
    } key;

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
  int isFrame          : 1; // is a uiFrame or inherited object
  int isControl        : 1; // is a uiControl or inherited object

  uiEvtHandlerProps() :
    isWindow(false),
    isFrame(false),
    isControl(false)
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

  uiEvtHandlerProps & evtHandlerProps() { return m_evtHandlerProps; }

private:

  uiApp *           m_app;
  uiEvtHandlerProps m_evtHandlerProps;
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


struct uiWindowProps {
  uint8_t activable : 1;
  uint8_t focusable : 1;

  uiWindowProps() :
    activable(true),
    focusable(false)
  { }
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

  uiWindowProps & windowProps() { return m_windowProps; }

  uiWindow * parent() { return m_parent; }

  Point mouseDownPos() { return m_mouseDownPos; }

  Rect transformRect(Rect const & rect, uiWindow * baseWindow);

  void repaint(Rect const & rect);

  void repaint();

  bool isMouseOver() { return m_isMouseOver; }

protected:

  Size sizeAtMouseDown()              { return m_sizeAtMouseDown; }
  Point posAtMouseDown()              { return m_posAtMouseDown; }

  virtual Size minWindowSize()        { return Size(0, 0); }

  void beginPaint(uiEvent * event);

  void generatePaintEvents(Rect const & paintRect);
  void generateReshapeEvents(Rect const & r);

private:

  uiWindow *    m_parent;

  Point         m_pos;
  Size          m_size;

  // saved screen rect before Maximize or Minimize
  Rect          m_savedScreenRect;

  uiWindowState m_state;

  uiWindowProps m_windowProps;

  Point         m_mouseDownPos;    // mouse position when mouse down event has been received

  Point         m_posAtMouseDown;  // used to resize
  Size          m_sizeAtMouseDown; // used to resize

  bool          m_isMouseOver;     // true after mouse entered, false after mouse left

  // double linked list, order is: bottom (first items) -> up (last items)
  uiWindow *    m_next;
  uiWindow *    m_prev;
  uiWindow *    m_firstChild;
  uiWindow *    m_lastChild;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame

struct uiFrameStyle {
  RGB              backgroundColor                = RGB(3, 3, 3);
  RGB              borderColor                    = RGB(2, 2, 2);
  RGB              activeBorderColor              = RGB(2, 2, 3);
  RGB              titleBackgroundColor           = RGB(2, 2, 2);
  RGB              activeTitleBackgroundColor     = RGB(2, 2, 3);
  RGB              titleFontColor                 = RGB(0, 0, 0);
  RGB              activeTitleFontColor           = RGB(0, 0, 0);
  FontInfo const * titleFont                      = Canvas.getPresetFontInfoFromHeight(14, false);
  int              borderSize                     = 2;
  RGB              buttonColor                    = RGB(1, 1, 1);  // color used to draw Close, Maximize and Minimize buttons
  RGB              activeButtonColor              = RGB(0, 0, 0);  // color used to draw Close, Maximize and Minimize buttons
  RGB              mouseOverBackgroundButtonColor = RGB(0, 0, 3);  // color used for background of Close, Maximize and Minimize buttons when mouse is over them
  RGB              mouseOverButtonColor           = RGB(3, 3, 3);  // color used for pen of Close, Maximize and Minimize buttons when mouse is over them
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

  uiFrameProps & frameProps() { return m_frameProps; }

  Rect rect(uiWindowRectType rectType);

protected:

  Size minWindowSize();
  int titleBarHeight();

private:

  void paintFrame();
  int paintButtons();
  void paintTitle(int maxX);
  void movingCapturedMouse(int mouseX, int mouseY);
  void movingFreeMouse(int mouseX, int mouseY);
  uiFrameSensiblePos getSensiblePosAt(int x, int y);
  Rect getBtnRect(int buttonIndex);
  void handleButtonsClick(int x, int y);
  void drawTextWithEllipsis(FontInfo const * fontInfo, int X, int Y, char const * text, int maxX);


  static const int CORNERSENSE = 10;


  uiFrameStyle m_style;

  uiFrameProps m_frameProps;

  char * m_title;

  uiFrameSensiblePos m_mouseDownSensiblePos;  // sensible position on mouse down
  uiFrameSensiblePos m_mouseMoveSensiblePos;  // sensible position on mouse move

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiControl


class uiControl : public uiWindow {

public:

  uiControl(uiWindow * parent, const Point & pos, const Size & size, bool visible);

  virtual ~uiControl();

  virtual void processEvent(uiEvent * event);
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiButton



struct uiButtonStyle {
  RGB              backgroundColor          = RGB(2, 2, 2);
  RGB              mouseOverBackgroundColor = RGB(2, 2, 3);
  RGB              downBackgroundColor      = RGB(3, 3, 3);
  RGB              borderColor              = RGB(1, 1, 1);
  RGB              focusedBorderColor       = RGB(0, 0, 3);
  RGB              textFontColor            = RGB(0, 0, 0);
  FontInfo const * textFont                 = Canvas.getPresetFontInfoFromHeight(14, false);
  int              borderSize               = 1;
  int              focusedBorderSize        = 2;
};


class uiButton : public uiControl {

public:

  uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible);

  virtual ~uiButton();

  virtual void processEvent(uiEvent * event);

  void setText(char const * value);

  char const * text() { return m_text; }


  // Delegates

  Delegate<> onClick;


private:

  void paintButton();
  void paintText(Rect const & rect);


  uiButtonStyle m_style;

  char * m_text;
  int    m_textExtent;  // calculated by setText(). TODO: changing font doesn't update m_textExtent!
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

  uiWindow * focusedWindow() { return m_focusedWindow; }

  uiWindow * setFocusedWindow(uiWindow * value);

  void captureMouse(uiWindow * window);

  uiWindow * capturedMouseWindow() { return m_capturedMouseWindow; }

  void repaintWindow(uiWindow * window);

  void repaintRect(Rect const & rect);

  void moveWindow(uiWindow * window, int x, int y);

  void resizeWindow(uiWindow * window, int width, int height);

  void resizeWindow(uiWindow * window, Size size);

  void reshapeWindow(uiWindow * window, Rect const & rect);

  uiWindow * screenToWindow(Point & point);

  void showWindow(uiWindow * window, bool value);

  void maximizeWindow(uiWindow * window, bool value);

  void minimizeWindow(uiWindow * window, bool value);

  void combineMouseMoveEvents(bool value) { m_combineMouseMoveEvents = value; }

  // events

  virtual void OnInit();

protected:

  bool getEvent(uiEvent * event, int timeOutMS);
  bool peekEvent(uiEvent * event, int timeOutMS);


private:

  void preprocessEvent(uiEvent * event);
  void preprocessMouseEvent(uiEvent * event);
  void preprocessKeyboardEvent(uiEvent * event);


  QueueHandle_t m_eventsQueue;

  uiFrame *     m_rootWindow;

  uiWindow *    m_activeWindow;        // foreground window. Also gets keyboard events (other than focused window)

  uiWindow *    m_focusedWindow;       // window that captures keyboard events (other than active window)

  uiWindow *    m_capturedMouseWindow; // window that has captured mouse

  uiWindow *    m_freeMouseWindow;     // window where mouse is over

  bool          m_combineMouseMoveEvents;
};




} // end of namespace



