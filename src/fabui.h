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

  *uiObject
    *uiEvtHandler
      *uiApp
      *uiWindow
        *uiFrame
        *uiControl
          *uiButton
          *uiTextEdit
          *uiLabel
          uiListBox
          uiCheckBox
          uiComboBox
          uiMenu
          uiGauge
          uiRadioButton
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
  UIEVT_TIMER,
  UIEVT_DBLCLICK,
  UIEVT_DESTROY,
};


class uiEvtHandler;
class uiApp;
class uiWindow;


typedef void * uiTimerHandle;


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
    // event: UIEVT_TIMER
    uiTimerHandle timerHandler;

    uiEventParams() { }
  } params;

  uiEvent() : dest(nullptr), id(UIEVT_NULL) { }
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


enum class uiWindowRectType {
  ScreenBased,
  ParentBased,
  WindowBased,
  ClientAreaScreenBased,
  ClientAreaParentBased,
  ClientAreaWindowBased,
};


struct uiWindowState {
  uint8_t visible   : 1;  // 0 = hidden,   1 = visible
  uint8_t maximized : 1;  // 0 = normal,   1 = maximized
  uint8_t minimized : 1;  // 0 = normal,   1 = minimized
  uint8_t active    : 1;  // 0 = inactive, 1 = active
};


struct uiWindowProps {
  uint8_t activable : 1;
  uint8_t focusable : 1;

  uiWindowProps() :
    activable(true),
    focusable(false)
  { }
};


struct uiWindowStyle {
  CursorName defaultCursor = CursorName::CursorPointerSimpleReduced;
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

  bool hasChildren() { return m_firstChild != nullptr; }

  void addChild(uiWindow * child);

  void removeChild(uiWindow * child, bool freeChild = true);

  void moveChildOnTop(uiWindow * child);

  bool isChild(uiWindow * window);

  Point pos() { return m_pos; }

  Size size() { return m_size; }

  virtual Rect rect(uiWindowRectType rectType);

  uiWindowState state() { return m_state; }

  uiWindowProps & windowProps() { return m_windowProps; }

  uiWindowStyle & windowStyle() { return m_windowStyle; }

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

  uiWindowStyle m_windowStyle;

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


enum class uiFrameSensiblePos {
  None,
  MoveArea,
  TopLeftResize,
  TopCenterResize,
  TopRightResize,
  CenterLeftResize,
  CenterRightResize,
  BottomLeftResize,
  BottomCenterResize,
  BottomRightResize,
  CloseButton,
  MaximizeButton,
  MinimizeButton,
};


class uiFrame : public uiWindow {

public:

  uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiFrame();

  virtual void processEvent(uiEvent * event);

  char const * title() { return m_title; }

  void setTitle(char const * value);

  uiFrameStyle & frameStyle() { return m_frameStyle; }

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


  uiFrameStyle       m_frameStyle;

  uiFrameProps       m_frameProps;

  char *             m_title;

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
  RGB              downBackgroundColor      = RGB(2, 2, 3); // when m_down = true
  RGB              mouseOverBackgroundColor = RGB(2, 2, 3);
  RGB              mouseDownBackgroundColor = RGB(3, 3, 3);
  RGB              borderColor              = RGB(1, 1, 1);
  RGB              focusedBorderColor       = RGB(0, 0, 3);
  RGB              textFontColor            = RGB(0, 0, 0);
  FontInfo const * textFont                 = Canvas.getPresetFontInfoFromHeight(14, false);
  uint8_t          borderSize               = 1;
  uint8_t          focusedBorderSize        = 2;
  uint8_t          bitmapTextSpace          = 4;
  Bitmap const *   bitmap                   = nullptr;
  Bitmap const *   downBitmap               = nullptr;
};


enum class uiButtonKind {
  Button,
  Switch,
};


class uiButton : public uiControl {

public:

  uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, uiButtonKind kind = uiButtonKind::Button, bool visible = true);

  virtual ~uiButton();

  virtual void processEvent(uiEvent * event);

  void setText(char const * value);

  char const * text() { return m_text; }

  uiButtonStyle & buttonStyle() { return m_buttonStyle; }

  bool down() { return m_down; }

  void setDown(bool value);


  // Delegates

  Delegate<> onClick;
  Delegate<> onChange;


private:

  void paintButton();
  void paintContent(Rect const & rect);

  void trigger();


  uiButtonStyle  m_buttonStyle;

  char *         m_text;
  int            m_textExtent;  // calculated by setText(). TODO: changing font doesn't update m_textExtent!

  bool           m_down;

  uiButtonKind   m_kind;

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiTextEdit
// single line text edit


struct uiTextEditStyle {
  RGB              backgroundColor            = RGB(2, 2, 2);
  RGB              mouseOverBackgroundColor   = RGB(2, 2, 3);
  RGB              focusedBackgroundColor     = RGB(3, 3, 3);
  RGB              borderColor                = RGB(1, 1, 1);
  RGB              focusedBorderColor         = RGB(0, 0, 3);
  RGB              textFontColor              = RGB(0, 0, 0);
  FontInfo const * textFont                   = Canvas.getPresetFontInfoFromHeight(14, false);
  uint8_t          borderSize                 = 1;
};


class uiTextEdit : public uiControl {

public:

  uiTextEdit(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiTextEdit();

  virtual void processEvent(uiEvent * event);

  uiTextEditStyle & textEditStyle() { return m_textEditStyle; }

  void setText(char const * value);

  char const * text() { return m_text; }


  // Delegates

  Delegate<> onClick;
  Delegate<> onChange;


private:

  void paintTextEdit();
  void paintContent();

  uint8_t const * getCharInfo(char ch, int * width);
  int charColumnToWindowX(int col);
  void updateCaret();
  void moveCursor(int col, int selCol);
  int getColFromMouseX(int mouseX);
  void handleKeyDown(uiEvent * event);
  void checkAllocatedSpace(int requiredLength);
  void insert(char c);
  void removeSel();
  int getWordPosAtLeft();
  int getWordPosAtRight();
  void selectWordAt(int mouseX);


  uiTextEditStyle m_textEditStyle;

  char *          m_text;
  int             m_textLength; // text length NOT including ending zero
  int             m_textSpace;  // actual space allocated including ending zero

  // rectangle where text will be painted (this is also the text clipping rect)
  Rect            m_contentRect;  // updated on painting

  // where text starts to be painted. Values less than m_contentRect.X1 are used to show characters which do not fit in m_contentRect
  int             m_viewX;

  // character index of cursor position (0 = at first char)
  int             m_cursorCol;

  // character index at start of selection (not included if < m_cursorCol, included if > m_cursorCol)
  int             m_selCursorCol;

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiLabel


struct uiLabelStyle {
  RGB              backgroundColor          = RGB(3, 3, 3);
  RGB              textFontColor            = RGB(0, 0, 0);
  FontInfo const * textFont                 = Canvas.getPresetFontInfoFromHeight(14, false);
};


class uiLabel : public uiControl {

public:

  uiLabel(uiWindow * parent, char const * text, const Point & pos, const Size & size = Size(0, 0), bool visible = true);

  virtual ~uiLabel();

  virtual void processEvent(uiEvent * event);

  void setText(char const * value);

  char const * text() { return m_text; }


  // Delegates

  Delegate<> onClick;
  Delegate<> onDblClick;


private:

  void paintLabel();


  char *         m_text;
  int            m_textExtent;  // calculated by setText()

  uiLabelStyle   m_labelStyle;

  bool           m_autoSize;

};





////////////////////////////////////////////////////////////////////////////////////////////////////
// uiApp



struct uiAppProps {
  uint16_t caretBlinkingTime = 500;   // caret blinking time (MS)
  uint16_t doubleClickTime   = 250;   // maximum delay required for two consecutive clicks to become double click (MS)
};



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

  uiWindow * setFocusedWindowNext();

  uiWindow * setFocusedWindowPrev();

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

  void showCaret(uiWindow * window);

  void setCaret();

  void setCaret(Point const & pos);

  void setCaret(Rect const & rect);

  uiTimerHandle setTimer(uiWindow * window, int periodMS);

  void killTimer(uiTimerHandle handle);

  uiAppProps & appProps() { return m_appProps; }

  void destroyWindow(uiWindow * window);


  // events

  virtual void OnInit();

protected:

  bool getEvent(uiEvent * event, int timeOutMS);
  bool peekEvent(uiEvent * event, int timeOutMS);


private:

  void preprocessEvent(uiEvent * event);
  void preprocessMouseEvent(uiEvent * event);
  void preprocessKeyboardEvent(uiEvent * event);

  static void timerFunc(TimerHandle_t xTimer);

  void blinkCaret(bool forceOFF = false);
  void suspendCaret(bool value);


  uiAppProps    m_appProps;

  QueueHandle_t m_eventsQueue;

  uiFrame *     m_rootWindow;

  uiWindow *    m_activeWindow;        // foreground window. Also gets keyboard events (other than focused window)

  uiWindow *    m_focusedWindow;       // window that captures keyboard events (other than active window)

  uiWindow *    m_capturedMouseWindow; // window that has captured mouse

  uiWindow *    m_freeMouseWindow;     // window where mouse is over

  bool          m_combineMouseMoveEvents;

  uiWindow *    m_caretWindow;         // nullptr = caret is not visible
  Rect          m_caretRect;           // caret rect relative to m_caretWindow
  uiTimerHandle m_caretTimer;
  int           m_caretInvertState;    // -1 = suspended, 1 = rect reversed (cat visible), 0 = rect not reversed (caret invisible)

  int           m_lastMouseDownTimeMS; // time (MS) at mouse Down. Used to measure double clicks
};




} // end of namespace



