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


/**
 * @file
 *
 * @brief This file contains all classes related to FabGL Graphical User Interface
 */



#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

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
          *uiLabel
          *uiImage
          *uiPanel
          *uiTextEdit
          *uiScrollableControl
            *uiPaintBox
              *uiCustomListBox
                *uiListBox
                *uiFileBrowser
            uiMemoEdit
          *uiCheckBox
          *uiComboBox
          uiMenu
          uiGauge
          *uiSlider
          uiSpinButton

*/


namespace fabgl {



// increase in case of garbage between windows!
#define FABGLIB_UI_EVENTS_QUEUE_SIZE 256



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
  UIEVT_RESTORE,    // Restore from UIEVT_MAXIMIZE or UIEVT_MINIMIZE
  UIEVT_SHOW,
  UIEVT_HIDE,
  UIEVT_SETFOCUS,
  UIEVT_KILLFOCUS,
  UIEVT_KEYDOWN,
  UIEVT_KEYUP,
  UIEVT_TIMER,
  UIEVT_CLICK,
  UIEVT_DBLCLICK,
  UIEVT_EXITMODAL,
  UIEVT_DESTROY,
  UIEVT_CLOSE,      // Request to close (frame Close button)
  UIEVT_QUIT,       // Quit the application
};


class uiEvtHandler;
class uiApp;
class uiWindow;


typedef void * uiTimerHandle;


/** @brief Contains details about the key event */
struct uiKeyEventInfo {
  VirtualKey VK;         /**< Virtual key */
  uint8_t    LALT  : 1;  /**< Status of left-ALT key */
  uint8_t    RALT  : 1;  /**< Status of right-ALT key */
  uint8_t    CTRL  : 1;  /**< Status of CTRL (left or right) key */
  uint8_t    SHIFT : 1;  /**< Status of SHIFT (left or right) key */
  uint8_t    GUI   : 1;  /**< Status of GUI (Windows logo) key */
};


/** @brief Contains details about the mouse event */
struct uiMouseEventInfo {
  MouseStatus status;         /**< Mouse position, wheel delta and buttons status */
  uint8_t     changedButton;  /**< Specifies which mouse button has changed 0 = none, 1 = left, 2 = middle, 3 = right */
};


struct uiEvent {
  uiEvtHandler * dest;
  uiEventID      id;

  union uiEventParams {
    // event: UIEVT_MOUSEMOVE, UIEVT_MOUSEWHEEL, UIEVT_MOUSEBUTTONDOWN, UIEVT_MOUSEBUTTONUP, UIEVT_CLICK, UIEVT_DBLCLICK
    uiMouseEventInfo mouse;
    // event: UIEVT_PAINT, UIEVT_GENPAINTEVENTS, UIEVT_RESHAPEWINDOW
    Rect rect;
    // event: UIEVT_SETPOS
    Point pos;
    // event: UIEVT_SETSIZE
    Size size;
    // event: UIEVT_DEBUGMSG
    char const * debugMsg;
    // event: UIEVT_KEYDOWN, UIEVT_KEYUP
    uiKeyEventInfo key;
    // event: UIEVT_TIMER
    uiTimerHandle timerHandle;
    // event: UIEVT_EXITMODAL
    int modalResult;
    // event: UIEVT_SETFOCUS
    uiWindow * oldFocused;
    // event: UIEVT_KILLFOCUS
    uiWindow * newFocused;
    // event: UIEVT_QUIT
    int exitCode;

    uiEventParams() { }
  } params;

  uiEvent() : dest(nullptr), id(UIEVT_NULL) { }
  uiEvent(uiEvent const & e) { dest = e.dest; id = e.id; params = e.params; }
  uiEvent(uiEvtHandler * dest_, uiEventID id_) : dest(dest_), id(id_) { }
};



/** \ingroup Enumerations
 * @brief Item direction/orientation
 */
enum class uiOrientation {
  Vertical,          /**< Vertical orientation */
  Horizontal,        /**< Horizontal orientation */
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiObject


/** @brief Specifies the object type */
struct uiObjectType {
  uint32_t uiApp               : 1;
  uint32_t uiEvtHandler        : 1;
  uint32_t uiWindow            : 1;
  uint32_t uiFrame             : 1;
  uint32_t uiControl           : 1;
  uint32_t uiScrollableControl : 1;
  uint32_t uiButton            : 1;
  uint32_t uiTextEdit          : 1;
  uint32_t uiLabel             : 1;
  uint32_t uiImage             : 1;
  uint32_t uiPanel             : 1;
  uint32_t uiPaintBox          : 1;
  uint32_t uiCustomListBox     : 1;
  uint32_t uiListBox           : 1;
  uint32_t uiFileBrowser       : 1;
  uint32_t uiComboBox          : 1;
  uint32_t uiCheckBox          : 1;
  uint32_t uiSlider            : 1;

  uiObjectType() : uiApp(0), uiEvtHandler(0), uiWindow(0), uiFrame(0), uiControl(0), uiScrollableControl(0), uiButton(0), uiTextEdit(0),
                   uiLabel(0), uiImage(0), uiPanel(0), uiPaintBox(0), uiCustomListBox(0), uiListBox(0), uiFileBrowser(0), uiComboBox(0), uiCheckBox(0), uiSlider(0)
    { }
};


/** @brief Base class of all UI elements like windows and controls */
class uiObject {

public:

  uiObject();

  virtual ~uiObject();

  /**
   * @brief Determines the object type.
   *
   * @return Object type.
   */
  uiObjectType & objectType() { return m_objectType; }

private:
  uiObjectType m_objectType;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiEvtHandler


/** @brief Base class of all UI elements that can receive events. */
class uiEvtHandler : public uiObject {

public:

  uiEvtHandler(uiApp * app);

  virtual ~uiEvtHandler();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Determines the app that owns this object.
   *
   * @return An uiApp object.
   */
  uiApp * app() { return m_app; }


protected:

  void setApp(uiApp * value) { m_app = value; }


private:

  uiApp * m_app;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiWindow

/** \ingroup Enumerations
 * @brief Specifies window rectangle origin
 */
enum class uiOrigin {
  Screen,   /**< Rectangle coordinates relative to the screen (that is the root window) */
  Parent,   /**< Rectangle coordiantes relative to parent window */
  Window,   /**< Rectangle coordinates relative to the window itself */
};


/** @brief Specifies current window state */
struct uiWindowState {
  uint8_t visible   : 1;  /**< 0 = hidden,   1 = visible   */
  uint8_t maximized : 1;  /**< 0 = normal,   1 = maximized */
  uint8_t minimized : 1;  /**< 0 = normal,   1 = minimized */
  uint8_t active    : 1;  /**< 0 = inactive, 1 = active    */
};


/** @brief Contains some window options */
struct uiWindowProps {
  uint8_t activable : 1;  /**< The window is activable (default for windows)  */
  uint8_t focusable : 1;  /**< The window is focusable (default for controls) */

  uiWindowProps() :
    activable(true),
    focusable(false)
  { }
};


/** @brief Contains the window style */
struct uiWindowStyle {
  CursorName       defaultCursor      = CursorName::CursorPointerSimpleReduced;  /**< Default window mouse cursor */
  RGB              borderColor        = RGB(2, 2, 2);                            /**< Border color */
  RGB              activeBorderColor  = RGB(2, 2, 3);                            /**< Border color when active */
  RGB              focusedBorderColor = RGB(0, 0, 3);                            /**< Border color when focused */
  uint8_t          borderSize         = 3;                                       /**< Border size in pixels. This determines also the resize grips area. */
  uint8_t          focusedBorderSize  = 1;                                       /**< Border size when focused */
};


/** @brief Contains anchors enable/disable switches */
struct uiAnchors {
  uint8_t left   : 1;     /**< Left anchor enable/disable switch */
  uint8_t top    : 1;     /**< Top anchor enable/disable switch */
  uint8_t right  : 1;     /**< Right anchor enable/disable switch */
  uint8_t bottom : 1;     /**< Bottom anchor enable/disable switch */

  uiAnchors() : left(true), top(true), right(false), bottom(false) { }
};


/** @brief Base class for all visible UI elements (Frames and Controls) */
class uiWindow : public uiEvtHandler {

friend class uiApp;

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A window must always have a parent window
   * @param pos Top-left coordinates of the window relative to the parent
   * @param size The window size
   * @param visible If true the window is immediately visible
   */
  uiWindow(uiWindow * parent, const Point & pos, const Size & size, bool visible);

  virtual ~uiWindow();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Gets next sibling
   *
   * Next sibling is more visible than calling object.
   *
   * @return Next uiWindow object
   */
  uiWindow * next()  { return m_next; }

  /**
   * @brief Gets previous sibling
   *
   * Previous sibling is less visible than calling object.
   *
   * @return Previous uiWindow object
   */
  uiWindow * prev()  { return m_prev; }

  /**
   * @brief Gets first child
   *
   * @return First uiWindow object
   */
  uiWindow * firstChild() { return m_firstChild; }

  /**
   * @brief Gets last child
   *
   * @return Last uiWindow object
   */
  uiWindow * lastChild() { return m_lastChild; }

  /**
   * @brief Determines whether this window has children
   *
   * @return True if window has one o more children
   */
  bool hasChildren() { return m_firstChild != nullptr; }

  /**
   * @brief Brings this window on top
   */
  void bringOnTop();

  /**
   * @brief Brings this window after another one
   *
   * @param insertionPoint This window will be placed after insertionPoint window. If insertionPoint is nullptr this window will be placed at the first position (location of less visibility.).
   */
  void bringAfter(uiWindow * insertionPoint);

  /**
   * @brief Determines the window position relative to parent window
   *
   * To change window position use uiApp.moveWindow().
   *
   * @return The top left point where this window is located
   */
  Point pos() { return m_pos; }

  /**
   * @brief Determines position of the client area
   *
   * @return The top left point where the client area starts
   */
  Point clientPos();

  /**
   * @brief Determines the window size
   *
   * To set window size use uiApp.resizeWindow().
   *
   * @return The window size
   */
  Size size() { return m_size; }

  /**
   * @brief Determines the client area size
   *
   * @return The client area size
   */
  Size clientSize();

  /**
   * @brief Determines the window bounding box
   *
   * To set window position use uiApp.moveWindow(), to set window size use uiApp.resizeWindow() or uiApp.reshapeWindow().
   *
   * @param origin Decides the origin of returned rectangle
   *
   * @return The window bounding box
   */
  Rect rect(uiOrigin origin);

  /**
   * @brief Determines the client area bounding box
   *
   * @param origin Decides the origin of returned rectangle
   *
   * @return The window client area bounding box
   */
  virtual Rect clientRect(uiOrigin origin);

  /**
   * @brief Determines the window state
   *
   * To set window state (hidden, visible, maximized, minimized) use uiApp.showWindow(), uiApp.maximizeWindow(), uiApp.minimizeWindow().
   *
   * @return Current window state
   */
  uiWindowState state() { return m_state; }

  /**
   * @brief Sets or gets window properties
   *
   * @return L-value representing some window properties
   */
  uiWindowProps & windowProps() { return m_windowProps; }

  /**
   * @brief Sets or gets window style
   *
   * @return L-value representing window style (colors, border size, etc...)
   */
  uiWindowStyle & windowStyle() { return m_windowStyle; }

  /**
   * @brief Determines the parent window
   *
   * @return Parent window
   */
  uiWindow * parent() { return m_parent; }

  /**
   * @brief Determines mouse position when left button was down
   *
   * @return Mouse position
   */
  Point mouseDownPos() { return m_mouseDownPos; }

  /**
   * @brief Transforms rectangle origins from current window to another one
   *
   * @param rect Rectangle to transform
   * @param baseWindow Window where the output rectangle will be relative to
   *
   * @return The translated rectangle
   */
  Rect transformRect(Rect const & rect, uiWindow * baseWindow);

  /**
   * @brief Repaints a rectangle of this window
   *
   * @param rect Rectangle to repaint
   */
  void repaint(Rect const & rect);

  /**
   * @brief Repaints this window
   */
  void repaint();

  /**
   * @brief Determines whether the mouse is over this window
   *
   * This methods returns True also when the mouse is captured.
   *
   * @return True if the mouse is over the window
   */
  bool isMouseOver() { return m_isMouseOver; }

  /**
   * @brief Exits from a modal window
   *
   * To open a modal window use uiApp.showModalWindow().
   *
   * @param modalResult An integer value to return to uiApp.showModalWindow().
   */
  void exitModal(int modalResult);

  /**
   * @brief Determines whether this window or control has focus
   *
   * To set currently focused window use uiApp.setFocusedWindow().
   *
   * @return True if this window has focus
   */
  bool hasFocus();

  /**
   * @brief Allows to switch on or off anchors
   *
   * @return An L-value used to switch on or off anchors
   */
  uiAnchors & anchors() { return m_anchors; }

  /**
   * @brief Sets the focus index (aka tab-index)
   *
   * @param value The focus index. -1 = control isn't included into the focusable controls. 0 = first focusable control.
   */
  void setFocusIndex(int value) { m_focusIndex = value; }

  /**
   * @brief Determines the focus index (aka tab-index)
   *
   * To set focus index use uiWindow.setFocusIndex().
   *
   * @return The focus index
   */
  int focusIndex() { return m_focusIndex; }


  // Delegates

  /**
   * @brief Mouse click event delegate
   *
   * This delegate is called when the mouse button is pressed and released on the same position.
   */
  Delegate<> onClick;

  /**
   * @brief Mouse double click event delegate
   *
   * This delegate is called when the mouse button is double pressed and released on the same position.
   * To change double click time use uiAppProps.doubleClickTime of uiApp.appProps().
   */
  Delegate<> onDblClick;


protected:

  void addChild(uiWindow * child);
  void insertAfter(uiWindow * child, uiWindow * underlyingChild);
  void freeChildren();
  void removeChild(uiWindow * child, bool freeChild = true);
  void moveChildOnTop(uiWindow * child);
  void moveAfter(uiWindow * child, uiWindow * underlyingChild);
  bool isChild(uiWindow * window);

  Size sizeAtMouseDown()              { return m_sizeAtMouseDown; }
  Point posAtMouseDown()              { return m_posAtMouseDown; }

  virtual Size minWindowSize()        { return Size(0, 0); }

  void beginPaint(uiEvent * paintEvent, Rect const & clippingRect);

  void generatePaintEvents(Rect const & paintRect);
  void reshape(Rect const & r);

  bool isFocusable();

private:

  void paintWindow();
  uiWindow * getChildWithFocusIndex(int focusIndex, int * maxIndex);


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

  uiAnchors     m_anchors;

  int16_t       m_focusIndex;      // -1 = doesn't partecipate to focus trip

  // double linked list, order is: bottom (first items) -> up (last items)
  uiWindow *    m_next;
  uiWindow *    m_prev;
  uiWindow *    m_firstChild;
  uiWindow *    m_lastChild;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFrame


/**
 * Specifies frame style (colors, title font, etc...)
 */
struct uiFrameStyle {
  RGB              backgroundColor                = RGB(3, 3, 3);  /**< Frame background color */
  RGB              titleBackgroundColor           = RGB(2, 2, 2);  /**< Title background color */
  RGB              activeTitleBackgroundColor     = RGB(2, 2, 3);  /**< Title background color when active */
  RGB              titleColor                     = RGB(0, 0, 0);  /**< Title color */
  RGB              activeTitleColor               = RGB(0, 0, 0);  /**< Title color when active */
  FontInfo const * titleFont                      = Canvas.getPresetFontInfoFromHeight(12, false);  /**< Title font */
  RGB              buttonColor                    = RGB(1, 1, 1);  /**< Color used to draw Close, Maximize and Minimize buttons */
  RGB              activeButtonColor              = RGB(0, 0, 0);  /**< Color used to draw Close, Maximize and Minimize buttons */
  RGB              mouseOverBackgroundButtonColor = RGB(0, 0, 3);  /**< Color used for background of Close, Maximize and Minimize buttons when mouse is over them */
  RGB              mouseOverButtonColor           = RGB(3, 3, 3);  /**< Color used for pen of Close, Maximize and Minimize buttons when mouse is over them */
};


/**
 * @brief Properties of the frame
 */
struct uiFrameProps {
  uint8_t resizeable        : 1; /**< Frame is resizeable. Make sure the window border has enough thickness setting also uiWindowStyle.borderSize */
  uint8_t moveable          : 1; /**< Frame is moveable. Make sure the window has a title bar setting window title */
  uint8_t hasCloseButton    : 1; /**< Frame has close button. Make sure the window has a title bar setting window title */
  uint8_t hasMaximizeButton : 1; /**< Frame has maximize button. Make sure the window has a title bar setting window title */
  uint8_t hasMinimizeButton : 1; /**< Frame has minimize button. Make sure the window has a title bar setting window title */

  uiFrameProps() :
    resizeable(true),
    moveable(true),
    hasCloseButton(true),
    hasMaximizeButton(true),
    hasMinimizeButton(true)
  { }
};


enum class uiFrameItem : uint8_t {
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


/**
 * @brief A frame is a window with a title bar, maximize/minimize/close buttons and that is resizeable or moveable
 */
class uiFrame : public uiWindow {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent Parent window
   * @param title Title of the frame. An empty string hides the title bar
   * @param pos Top-left coordinates of the frame relative to the parent
   * @param size The frame size
   * @param visible If true the frame is immediately visible
   */
  uiFrame(uiWindow * parent, char const * title, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiFrame();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Determines the window title
   *
   * @return The window title
   */
  char const * title() { return m_title; }

  /**
   * @brief Sets window title
   *
   * Window needs to be repainted in order to display changed title.
   *
   * @param value The window title. nullptr hides the title bar.
   */
  void setTitle(char const * value);

  /**
   * @brief Sets window title as formatted text
   *
   * Window needs to be repainted in order to display changed title.
   *
   * @param format Format specifier like printf.
   */
  void setTitleFmt(const char *format, ...);

  /**
   * @brief Sets or gets frame style
   *
   * @return L-value representing frame style (colors, border size, etc...)
   */
  uiFrameStyle & frameStyle() { return m_frameStyle; }

  /**
   * @brief Sets or gets frame properties
   *
   * @return L-value representing some frame properties
   */
  uiFrameProps & frameProps() { return m_frameProps; }

  Rect clientRect(uiOrigin origin);


  // Delegates

  /**
   * @brief Show window event delegate
   *
   * This delegate is called when the window is about to be shown.
   */
  Delegate<> onShow;

  /**
   * @brief Hide window event delegate
   *
   * This delegate is called when the window is about to be hidden.
   */
  Delegate<> onHide;

  /**
   * @brief Resize window event delegate
   *
   * This delegate is called after its size has changed.
   */
  Delegate<> onResize;

  /**
   * @brief Timer event delegate
   *
   * This delegate is called when the timer expires.
   * To create a timer use uiApp.setTimer().
   */
  Delegate<uiTimerHandle> onTimer;

  /**
   * @brief Key-down event delegate
   */
  Delegate<uiKeyEventInfo> onKeyDown;

  /**
   * @brief Key-up event delegate
   */
  Delegate<uiKeyEventInfo> onKeyUp;

  /**
   * @brief Paint event delegate
   */
  Delegate<> onPaint;


protected:

  Size minWindowSize();
  int titleBarHeight();
  Rect titleBarRect();

private:

  void paintFrame();
  int paintButtons(Rect const & bkgRect);
  void movingCapturedMouse(int mouseX, int mouseY, bool mouseIsDown);
  void movingFreeMouse(int mouseX, int mouseY);
  uiFrameItem getFrameItemAt(int x, int y);
  Rect getBtnRect(int buttonIndex);
  void handleButtonsClick(int x, int y, bool doubleClick);
  void drawTextWithEllipsis(FontInfo const * fontInfo, int X, int Y, char const * text, int maxX);
  void drawReshapingBox(Rect boxRect);


  static constexpr int CORNERSENSE = 10;


  uiFrameStyle       m_frameStyle;

  uiFrameProps       m_frameProps;

  char *             m_title;
  int                m_titleLength;

  uiFrameItem m_mouseDownFrameItem;  // frame item on mouse down
  uiFrameItem m_mouseMoveFrameItem;  // frame item on mouse move

  Rect        m_lastReshapingBox;    // last reshaping box painted by drawReshapingBox(), (0,0,0,0) if there isn't any

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiControl


/**
 * @brief This is the base class for all controls. A control can have focus and is not activable.
 */
class uiControl : public uiWindow {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent Parent window
   * @param pos Top-left coordinates of the control relative to the parent
   * @param size The control size
   * @param visible If true the control is immediately visible
   */
  uiControl(uiWindow * parent, const Point & pos, const Size & size, bool visible);

  virtual ~uiControl();

  virtual void processEvent(uiEvent * event);
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiScrollableControl


/** @brief Contains the scrollable control style */
struct uiScrollableControlStyle {
  RGB scrollBarBackgroundColor          = RGB(1, 1, 1);  /**< Background color of the scrollbar */
  RGB scrollBarForegroundColor          = RGB(2, 2, 2);  /**< Foreground color of the scrollbar */
  RGB mouseOverScrollBarForegroundColor = RGB(3, 3, 3);  /**< Foreground color of the scrollbar when mouse is over it */
  uint8_t scrollBarSize                 = 11;            /**< Width of vertical scrollbar, height of vertical scroll bar */
};


enum class uiScrollBarItem {
  None,
  LeftButton,
  RightButton,
  TopButton,
  BottomButton,
  HBar,
  VBar,
  PageUp,
  PageDown,
  PageLeft,
  PageRight,
};


/**
 * @brief A scrollable control is a control with optionally vertical and/or horizontal scrollbars
 */
class uiScrollableControl : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A control must always have a parent window
   * @param pos Top-left coordinates of the control relative to the parent
   * @param size The control size
   * @param visible If true the control is immediately visible
   */
  uiScrollableControl(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiScrollableControl();

  virtual void processEvent(uiEvent * event);

  Rect clientRect(uiOrigin origin);

  /**
   * @brief Sets or gets control style
   *
   * @return L-value representing control style (colors, scrollbars width, etc...)
   */
  uiScrollableControlStyle & scrollableControlStyle() { return m_scrollableControlStyle; }

  /**
   * @brief Determines position of the horizontal scrollbar thumb
   *
   * Minimum value is 0, maximum value is HScrollBarRange() - HScrollBarVisible().
   * Position is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar position in scroll units
   */
  int HScrollBarPos() { return m_HScrollBarPosition; }

  /**
   * @brief Determines horizontal scrollbar visible portion (aka thumb size) of the scrollable content
   *
   * Visible portion is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar visible portion in scroll units
   */
  int HScrollBarVisible() { return m_HScrollBarVisible; }

  /**
   * @brief Determines horizontal scrollbar range
   *
   * Range is the maximum position of the scrollbar. Maximum value of scrollbar position will HScrollBarRange() - HScrollBarVisible().
   * Range is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar range in scroll units.
   */
  int HScrollBarRange() { return m_HScrollBarRange; }

  /**
   * @brief Determines position of the vertical scrollbar thumb
   *
   * Minimum value is 0, maximum value is VScrollBarRange() - VScrollBarVisible().
   * Position is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar position in scroll units
   */
  int VScrollBarPos() { return m_VScrollBarPosition; }

  /**
   * @brief Determines vertical scrollbar visible portion (aka thumb size) of the scrollable content
   *
   * Visible portion is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar visible portion in scroll units
   */
  int VScrollBarVisible() { return m_VScrollBarVisible; }

  /**
   * @brief Determines vertical scrollbar range
   *
   * Range is the maximum position of the scrollbar. Maximum value of scrollbar position will VScrollBarRange() - VScrollBarVisible().
   * Range is measured in "scroll units". A scroll unit is determined by scrollbar size.
   *
   * @return Scrollbar range in scroll units.
   */
  int VScrollBarRange() { return m_VScrollBarRange; }


  // Delegates

  /**
   * @brief Horizontal scrollbar change event delegate
   */
  Delegate<> onChangeHScrollBar;

  /**
   * @brief Vertical scrollbar change event delegate
   */
  Delegate<> onChangeVScrollBar;


protected:

  /**
   * @brief Sets scrollbar position, visible portion and range
   *
   * @param orientation Specifies which scrollbar to update
   * @param position Sets position of the scrollbar thumb. Minimum value is 0, maximum value is range - visible. Position is measured in "scroll units". A scroll unit is determined by scrollbar size.
   * @param visible Sets scrollbar visible portion (aka thumb size) of the scrollable content. Visible portion is measured in "scroll units". A scroll unit is determined by scrollbar size.
   * @param range Sets vertical scrollbar range. Range is the maximum position of the scrollbar. Maximum value of scrollbar position will range - visible.
   * @param repaintScrollbar If True the scrollbar is repainted immediately.
   */
  virtual void setScrollBar(uiOrientation orientation, int position, int visible, int range, bool repaintScrollbar = true);


private:

  void paintScrollableControl();
  void paintScrollBars();
  Rect getVScrollBarRects(Rect * topButton = nullptr, Rect * bottonButton = nullptr, Rect * bar = nullptr);
  Rect getHScrollBarRects(Rect * leftButton = nullptr, Rect * rightButton = nullptr, Rect * bar = nullptr);
  uiScrollBarItem getItemAt(int x, int y);
  void repaintScrollBar(uiOrientation orientation);
  void handleFreeMouseMove(int mouseX, int mouseY);
  void handleCapturedMouseMove(int mouseX, int mouseY);
  void handleButtonsScroll();
  void handlePageScroll();

  uiScrollableControlStyle m_scrollableControlStyle;

  int16_t         m_HScrollBarPosition;
  int16_t         m_HScrollBarVisible;    // it means the "visible" area (how big is the bar)
  int16_t         m_HScrollBarRange;
  int16_t         m_VScrollBarPosition;
  int16_t         m_VScrollBarVisible;    // it means the "visible" area (how big is the bar)
  int16_t         m_VScrollBarRange;

  // values updated by getVScrollBarRects() and getHScrollBarRects()
  int16_t         m_HBarArea;
  int16_t         m_VBarArea;

  int16_t         m_mouseDownHScrollBarPosition;
  int16_t         m_mouseDownVScrollBarPosition;

  uiScrollBarItem m_mouseOverItem;

  // a timer is active while mouse is down and the mouse is over a button
  uiTimerHandle   m_scrollTimer;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiButton


/** @brief Contains the button style */
struct uiButtonStyle {
  RGB              backgroundColor          = RGB(2, 2, 2); /**< Background color */
  RGB              downBackgroundColor      = RGB(2, 2, 3); /**< Background color when button is down */
  RGB              mouseOverBackgroundColor = RGB(2, 2, 3); /**< Background color when mouse is over */
  RGB              mouseDownBackgroundColor = RGB(3, 3, 3); /**< Background color when mouse is down */
  RGB              textColor                = RGB(0, 0, 0); /**< Text color */
  FontInfo const * textFont                 = Canvas.getPresetFontInfoFromHeight(14, false); /**< Text font */
  uint8_t          bitmapTextSpace          = 4;            /**< Spaces between image and text */
  Bitmap const *   bitmap                   = nullptr;      /**< Bitmap to display */
  Bitmap const *   downBitmap               = nullptr;      /**< Bitmap to display when button is down */
};


/** \ingroup Enumerations
 * @brief Specifies the button kind
 */
enum class uiButtonKind {
  Button,   /**< Normal button - can have a single state. Generates only click events */
  Switch,   /**< Switch - can have two states. Generates onChange events */
};


/** @brief Represents a button control. A button can have text and optionally a bitmap */
class uiButton : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A button must always have a parent window
   * @param text The text of the button
   * @param pos Top-left coordinates of the button relative to the parent
   * @param size The button size
   * @param kind The button kind (button or switch)
   * @param visible If true the button is immediately visible
   */
  uiButton(uiWindow * parent, char const * text, const Point & pos, const Size & size, uiButtonKind kind = uiButtonKind::Button, bool visible = true);

  virtual ~uiButton();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets button text
   *
   * Button needs to be repainted in order to display changed text.
   *
   * @param value Button text
   */
  void setText(char const * value);

  /**
   * @brief Determines button text
   *
   * @return Button text
   */
  char const * text() { return m_text; }

  /**
   * @brief Sets or gets button style
   *
   * @return L-value representing button style (colors, bitmaps, etc...)
   */
  uiButtonStyle & buttonStyle() { return m_buttonStyle; }

  /**
   * @brief Determines whether the switch button is down or up
   *
   * Only a switch button can be down.
   *
   * @return True if the button is down
   */
  bool down() { return m_down; }

  /**
   * @brief Sets button state of a switch button
   *
   * Only a switch button can be down.
   *
   * @param value Button state (true = down, false = up)
   */
  void setDown(bool value);


  // Delegates

  /**
   * @brief Button changed event delegate
   *
   * This delegate is called whenever a switch button change its state
   */
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


/**
 * @brief Sets or gets text edit style
 *
 * @return L-value representing frame style (colors, font, etc...)
 */
struct uiTextEditStyle {
  RGB              backgroundColor            = RGB(2, 2, 2);                                   /**< Background color */
  RGB              mouseOverBackgroundColor   = RGB(2, 2, 3);                                   /**< Background color when mouse is over */
  RGB              focusedBackgroundColor     = RGB(3, 3, 3);                                   /**< Background color when focused */
  RGB              textColor                  = RGB(0, 0, 0);                                   /**< Text color */
  FontInfo const * textFont                   = Canvas.getPresetFontInfoFromHeight(14, false);  /**< Text font */
};


/**
 * @brief Properties of the text edit
 */
struct uiTextEditProps {
  uint8_t hasCaret  : 1;   /**< If True the edit box has a blinking caret */
  uint8_t allowEdit : 1;   /**< If True the edit box allows edit */

  uiTextEditProps()
    : hasCaret(true),
      allowEdit(true)
    {
    }
};


/**
 * @brief Represents a text edit control.
 *
 * A text edit is a single line editor, horizontally scrollable, text selection and replacement.
 */
class uiTextEdit : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A text edit must always have a parent window
   * @param text Optional initial text
   * @param pos Top-left coordinates of the text edit relative to the parent
   * @param size The text edit size
   * @param visible If true the button is immediately visible
   */
  uiTextEdit(uiWindow * parent, char const * text, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiTextEdit();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets text edit style
   *
   * @return L-value representing text edit style (colors, font, etc...)
   */
  uiTextEditStyle & textEditStyle() { return m_textEditStyle; }

  /**
   * @brief Sets or gets text edit properties
   *
   * @return L-value representing some text edit properties
   */
  uiTextEditProps & textEditProps() { return m_textEditProps; }

  /**
   * @brief Replaces current text
   *
   * Text edit needs to be repainted in order to display changed text.
   *
   * @param value Text to set
   */
  void setText(char const * value);

  /**
   * @brief Gets current content of the text edit
   *
   * @return Text edit content
   */
  char const * text() { return m_text; }


  // Delegates

  /**
   * @brief Text edit event delegate
   */
  Delegate<> onChange;


protected:

  virtual Rect getEditRect();

private:

  void paintTextEdit();
  void paintContent();

  uint8_t const * getCharInfo(char ch, int * width);
  int charColumnToWindowX(int col);
  void updateCaret();
  void moveCursor(int col, int selCol);
  int getColFromMouseX(int mouseX);
  void handleKeyDown(uiKeyEventInfo key);
  void checkAllocatedSpace(int requiredLength);
  void insert(char c);
  void removeSel();
  int getWordPosAtLeft();
  int getWordPosAtRight();
  void selectWordAt(int mouseX);


  uiTextEditStyle m_textEditStyle;
  uiTextEditProps m_textEditProps;

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


/** @brief Contains the label style */
struct uiLabelStyle {
  FontInfo const * textFont                 = Canvas.getPresetFontInfoFromHeight(14, false);  /**< Text font */
  RGB              backgroundColor          = RGB(3, 3, 3);                                   /**< Background color */
  RGB              textColor                = RGB(0, 0, 0);                                   /**< Text color */
};


/** @brief A label is a static text UI element */
class uiLabel : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A label must always have a parent window
   * @param text Text of the label
   * @param pos Top-left coordinates of the label relative to the parent
   * @param size The label size. If Size(0, 0) then size is automatically calculated
   * @param visible If true the label is immediately visible
   */
  uiLabel(uiWindow * parent, char const * text, const Point & pos, const Size & size = Size(0, 0), bool visible = true);

  virtual ~uiLabel();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets label text
   *
   * To update label content and size call uiLabel.update().
   *
   * @param value Label text
   */
  void setText(char const * value);

  /**
   * @brief Sets label formatted text
   *
   * To update label content and size call uiLabel.update().
   *
   * @param format Format specifier like printf.
   */
  void setTextFmt(const char *format, ...);

  /**
   * @brief Determines label text
   *
   * @return Label text
   */
  char const * text() { return m_text; }

  /**
   * @brief Sets or gets label style
   *
   * @return L-value representing label style (colors, font, etc...)
   */
  uiLabelStyle & labelStyle() { return m_labelStyle; }

  /**
   * @brief Updates the label content
   *
   * Call this method whenever text or font changes.
   */
  void update();


private:

  void paintLabel();


  char *         m_text;

  uiLabelStyle   m_labelStyle;

  uint16_t       m_textExtent;  // calculated by setText()

  uint8_t        m_autoSize;

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiImage


/** @brief Contains the image style */
struct uiImageStyle {
  RGB backgroundColor = RGB(3, 3, 3);   /**< Background color */
};


/** @brief Image control to display a static bitmap */
class uiImage : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. An image must always have a parent window
   * @param bitmap Bitmap to display
   * @param pos Top-left coordinates of the image relative to the parent
   * @param size The image size. If Size(0, 0) then size is automatically calculated
   * @param visible If true the image is immediately visible
   */
  uiImage(uiWindow * parent, Bitmap const * bitmap, const Point & pos, const Size & size = Size(0, 0), bool visible = true);

  virtual ~uiImage();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets image bitmap
   *
   * Image needs to be repainted in order to display changed bitmap.
   *
   * @param bitmap Image bitmap
   */
  void setBitmap(Bitmap const * bitmap);

  /**
   * @brief Gets image bitmap
   *
   * @return Image bitmap
   */
  Bitmap const * bitmap() { return m_bitmap; }

  /**
   * @brief Sets or gets image style
   *
   * @return L-value representing image style
   */
  uiImageStyle & imageStyle() { return m_imageStyle; }


private:

  void paintImage();


  Bitmap const * m_bitmap;

  uiImageStyle   m_imageStyle;

  bool           m_autoSize;

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiPanel


/** @brief Contains the panel style */
struct uiPanelStyle {
  RGB backgroundColor = RGB(2, 2, 2);    /**< Panel background color */
};


/** @brief A panel is used to contain and to group some controls */
class uiPanel : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A panel must always have a parent window
   * @param pos Top-left coordinates of the panel relative to the parent
   * @param size The panel size
   * @param visible If true the panel is immediately visible
   */
  uiPanel(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiPanel();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets panel style
   *
   * @return L-value representing panel style
   */
  uiPanelStyle & panelStyle() { return m_panelStyle; }


private:

  void paintPanel();


  uiPanelStyle   m_panelStyle;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiPaintBox


/** @brief Contains the paintbox style */
struct uiPaintBoxStyle {
  RGB backgroundColor = RGB(2, 2, 2);   /**< Paintbox background color */
};


/** @brief A paintbox control allows applications to perform custom drawings providing uiPaintBox.onPaint delegate. A paintbox can have horizontal and vertical scrollbars. */
class uiPaintBox : public uiScrollableControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A paintbox must always have a parent window
   * @param pos Top-left coordinates of the paintbox relative to the parent
   * @param size The paintbox size
   * @param visible If true the paintbox is immediately visible
   */
  uiPaintBox(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiPaintBox();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets paintbox style
   *
   * @return L-value representing paintbox style
   */
  uiPaintBoxStyle & paintBoxStyle() { return m_paintBoxStyle; }

  using uiScrollableControl::setScrollBar;

  // Delegates

  /**
   * @brief Paint event delegate
   *
   * Applications use this delegate to perform custom drawings.
   */
  Delegate<Rect> onPaint;


private:

  void paintPaintBox();


  uiPaintBoxStyle m_paintBoxStyle;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiCustomListBox


/** @brief Contains the listbox style */
struct uiListBoxStyle {
  RGB              backgroundColor                = RGB(2, 2, 2);                                   /**< Background color */
  RGB              focusedBackgroundColor         = RGB(3, 3, 3);                                   /**< Background color when focused */
  RGB              selectedBackgroundColor        = RGB(0, 0, 2);                                   /**< Background color when selected */
  RGB              focusedSelectedBackgroundColor = RGB(0, 0, 3);                                   /**< Background color when selected and focused */
  int              itemHeight                     = 16;                                             /**< Item height in pixels */
  FontInfo const * textFont                       = Canvas.getPresetFontInfoFromHeight(14, false);  /**< Text font */
  RGB              textColor                      = RGB(0, 0, 0);                                   /**< Text foreground color */
  RGB              selectedTextColor              = RGB(3, 3, 3);                                   /**< Text foreground color when selected */
};


/** @brief Shows generic a list of selectable items */
class uiCustomListBox : public uiScrollableControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A listbox must always have a parent window
   * @param pos Top-left coordinates of the listbox relative to the parent
   * @param size The listbox size
   * @param visible If true the listbox is immediately visible
   */
  uiCustomListBox(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  virtual ~uiCustomListBox();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets listbox style
   *
   * @return L-value representing listbox style
   */
  uiListBoxStyle & listBoxStyle() { return m_listBoxStyle; }

  /**
   * @brief Gets the first selected item
   *
   * @return Index of selected item or -1 if no item is selected
   */
  int firstSelectedItem();

  /**
   * @brief Gets the last selected item
   *
   * @return Index of selected item or -1 if no item is selected
   */
  int lastSelectedItem();

  /**
   * @brief Selects a listbox item
   *
   * @param index Index of item to select
   * @param add If true this item is added to the selected items, otherwise all other items are deselected
   * @param range If true selects a range of items
   */
  void selectItem(int index, bool add = false, bool range = false);

  /**
   * @brief Deselects all selected items
   */
  void deselectAll();


  // Delegates

  /**
   * @brief Change event delegate
   *
   * This delegate is called whenever an item is selected or deselected.
   */
  Delegate<> onChange;

  /**
   * @brief Kill focus event delegate
   */
  Delegate<> onKillFocus;

  /**
   * @brief Key-up event delegate
   */
  Delegate<uiKeyEventInfo> onKeyUp;


protected:

  void setScrollBar(uiOrientation orientation, int position, int visible, int range, bool repaintScrollbar);

  // must be implemented by inherited class
  virtual int items_getCount()                              = 0;
  virtual void items_deselectAll()                          = 0;
  virtual void items_select(int index, bool select)         = 0;
  virtual bool items_selected(int index)                    = 0;
  virtual void items_draw(int index, const Rect & itemRect) = 0;

private:

  void paintListBox();
  int getItemAtMousePos(int mouseX, int mouseY);
  void handleMouseDown(int mouseX, int mouseY);
  void handleKeyDown(uiKeyEventInfo key);
  void makeItemVisible(int index);


  uiListBoxStyle m_listBoxStyle;
  int            m_firstVisibleItem;     // the item on the top
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiListBox

/** @brief Shows a list of selectable string items */
class uiListBox : public uiCustomListBox {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A listbox must always have a parent window
   * @param pos Top-left coordinates of the listbox relative to the parent
   * @param size The listbox size
   * @param visible If true the listbox is immediately visible
   */
  uiListBox(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  /**
   * @brief A list of strings representing the listbox content
   *
   * Other than actual strings, StringList indicates which items are selected.
   * Repainting is required when the string list changes.
   *
   * @return L-value representing listbox items
   */
  StringList & items() { return m_items; }

protected:

  virtual int items_getCount()                      { return m_items.count(); }
  virtual void items_deselectAll()                  { m_items.deselectAll(); }
  virtual void items_select(int index, bool select) { m_items.select(index, select); }
  virtual bool items_selected(int index)            { return m_items.selected(index); }
  virtual void items_draw(int index, const Rect & itemRect);


private:

  StringList     m_items;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// uiFileBrowser

/** @brief Shows and navigates Virtual Filesystem content */
class uiFileBrowser : public uiCustomListBox {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A listbox must always have a parent window
   * @param pos Top-left coordinates of the listbox relative to the parent
   * @param size The listbox size
   * @param visible If true the listbox is immediately visible
   */
  uiFileBrowser(uiWindow * parent, const Point & pos, const Size & size, bool visible = true);

  /**
   * @brief Sets current directory
   *
   * Path can include subdirectories (even SPIFFS emulated directories).
   *
   * @param path Absolute path. It musts include filesystem path (ie "/spiffs")
   */
  void setDirectory(char const * path);

  /**
   * @brief Determines current directory
   *
   * @return Full path of current directory
   */
  char const * directory() { return m_dir.directory(); }

  /**
   * @brief Determines number of files in current directory
   *
   * @return Number of files, included parent link ("..").
   */
  int count()              { return m_dir.count(); }

  /**
   * @brief Currently selected filename
   *
   * @return Currently selected filename or nullptr.
   */
  char const * filename();

  /**
   * @brief Determines whether currently selected item is a directory
   *
   * @return True if currently selected item is a directory. If False it is an ordinary file.
   */
  bool isDirectory();

  void processEvent(uiEvent * event);

  /**
   * @brief Reloads current directory content and repaints
   */
  void update();

  /**
   * @brief Contains current directory representation
   *
   * @return A FileBrowser object which allows to know directory content and to manipulate it
   */
  FileBrowser & content() { return m_dir; }


protected:

  virtual int items_getCount()                      { return m_dir.count(); }
  virtual void items_deselectAll()                  { m_selected = -1; }
  virtual void items_select(int index, bool select);
  virtual bool items_selected(int index)            { return index == m_selected; }
  virtual void items_draw(int index, const Rect & itemRect);

private:

  void enterSubDir();

  FileBrowser m_dir;
  int         m_selected;  // -1 = no sel

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiComboBox


/** @brief Contains the listbox style */
struct uiComboBoxStyle {
  RGB buttonBackgroundColor = RGB(1, 1, 1);  /**< Background color of open/close button */
  RGB buttonColor           = RGB(2, 2, 2);  /**< Foreground color of open/close button */
};


/** @brief Properties of the combobox */
struct uiComboBoxProps {
  uint8_t openOnFocus  : 1;  /**< Open the combobox when it acquires focus */

  uiComboBoxProps()
    : openOnFocus(true)
  {
  }
};


/** @brief This is a combination of a listbox and a single-line editable textbox */
class uiComboBox : public uiTextEdit
{

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A combobox must always have a parent window
   * @param pos Top-left coordinates of the combobox relative to the parent
   * @param size The combobox size
   * @param listHeight Height in pixels of the open listbox
   * @param visible If true the combobox is immediately visible
   */
  uiComboBox(uiWindow * parent, const Point & pos, const Size & size, int listHeight, bool visible = true);

  ~uiComboBox();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief A list of strings representing items of the combobox
   *
   * Repainting is required when the string list changes.
   *
   * @return L-value representing combobox items
   */
  StringList & items() { return m_listBox->items(); }

  /**
   * @brief Sets or gets combobox style
   *
   * @return L-value representing combobox style
   */
  uiComboBoxStyle & comboBoxStyle() { return m_comboBoxStyle; }

  /**
   * @brief Sets or gets listbox style
   *
   * @return L-value representing listbox style
   */
  uiListBoxStyle & listBoxStyle() { return m_listBox->listBoxStyle(); }

  /**
   * @brief Sets or gets combobox properties
   *
   * @return L-value representing some combobox properties
   */
  uiComboBoxProps & comboBoxProps() { return m_comboBoxProps; }

  /**
   * @brief Represents currently selected item
   *
   * @return Index of the selected item or -1 if no item is selected
   */
  int selectedItem() { return m_listBox->firstSelectedItem(); }

  /**
   * @brief Selects an item
   *
   * @param index Index of the item to select
   */
  void selectItem(int index);


  // Delegates

  /**
   * @brief Change event delegate
   *
   * This delegate is called whenever an item is selected.
   */
  Delegate<> onChange;


protected:

  virtual Rect getEditRect();


private:

  void paintComboBox();
  Rect getButtonRect();
  int buttonWidth();
  void openListBox();
  void closeListBox();
  void switchListBox();
  void updateTextEdit();


  uiListBox *     m_listBox;
  int             m_listHeight;
  uiComboBoxStyle m_comboBoxStyle;
  uiComboBoxProps m_comboBoxProps;


};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiCheckBox


/** @brief Contains the checkbox style */
struct uiCheckBoxStyle {
  RGB              backgroundColor          = RGB(2, 2, 2);  /**< Background color */
  RGB              checkedBackgroundColor   = RGB(2, 2, 3);  /**< Background color when checked */
  RGB              mouseOverBackgroundColor = RGB(2, 2, 3);  /**< Background color when mouse is over */
  RGB              foregroundColor          = RGB(0, 0, 0);  /**< Foreground color */
};


/** \ingroup Enumerations
 * @brief Specifies the combobox behaviour
 */
enum class uiCheckBoxKind : int8_t {
  CheckBox,          /**< Normal checkbox */
  RadioButton,       /**< Radio-button */
};


/**
 * @brief Represents a checkbox or a radiobutton.
 *
 * A checkbox permits the user to make a binary choice.
 * A radio button allows the user to choose only one of a predefined set of mutually exclusive options.
 */
class uiCheckBox : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A checkbox must always have a parent window
   * @param pos Top-left coordinates of the checkbox relative to the parent
   * @param size The checkbox size
   * @param kind Defines the checkbox behaviour: checkbox or radiobutton
   * @param visible If true the checkbox is immediately visible
   */
  uiCheckBox(uiWindow * parent, const Point & pos, const Size & size, uiCheckBoxKind kind = uiCheckBoxKind::CheckBox, bool visible = true);

  virtual ~uiCheckBox();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets checkbox style
   *
   * @return L-value representing checkbox style
   */
  uiCheckBoxStyle & checkBoxStyle() { return m_checkBoxStyle; }

  /**
   * @brief Determines whether the checkbox or radiobutton is checked
   *
   * @return True when checked
   */
  bool checked()                    { return m_checked; }

  /**
   * @brief Sets current checkbox or radiobutton checked status
   *
   * When in radiobutton mode, setting True all other items with the same uiCheckBox.groupIndex() are deselected.
   *
   * @param value Checkbox or radiobutton checked status
   */
  void setChecked(bool value);

  /**
   * @brief Determines radiobutton group index
   *
   * @return The radiobutton group index (-1 = no group)
   */
  int groupIndex()                  { return m_groupIndex; }

  /**
   * @brief Sets radiobutton group index
   *
   * @param value Radiobutton group index (-1 = no group)
   */
  void setGroupIndex(int value)     { m_groupIndex = value; }


  // Delegates

  /**
   * @brief Change event delegate
   *
   * This delegate is called whenever checkbox or radiobutton is checked or unchecked.
   */
  Delegate<> onChange;


private:

  void paintCheckBox();
  void trigger();
  void unCheckGroup();


  uiCheckBoxStyle m_checkBoxStyle;
  bool            m_checked;
  uiCheckBoxKind  m_kind;
  int16_t         m_groupIndex; // -1 = no group

};



////////////////////////////////////////////////////////////////////////////////////////////////////
// uiSlider


/** @brief Contains the slider style */
struct uiSliderStyle {
  RGB backgroundColor = RGB(3, 3, 3);    /**< Slider background color */
  RGB slideColor      = RGB(0, 2, 2);    /**< Color of internal slide */
  RGB rangeColor      = RGB(0, 2, 3);    /**< Color of slide before the grip */
  RGB gripColor       = RGB(0, 0, 3);    /**< Color of slider grip */
  RGB ticksColor      = RGB(3, 3, 3);    /**> Ticks color */
};


/** @brief A slider or track bar is a graphical control element with which a user may set a value by moving an indicator  */
class uiSlider : public uiControl {

public:

  /**
   * @brief Creates an instance of the object
   *
   * @param parent The parent window. A slider must always have a parent window
   * @param pos Top-left coordinates of the slider relative to the parent
   * @param size The slider size
   * @param orientation The slider orientation
   * @param visible If true the slider is immediately visible
   */
  uiSlider(uiWindow * parent, const Point & pos, const Size & size, uiOrientation orientation, bool visible = true);

  virtual ~uiSlider();

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Sets or gets slider style
   *
   * @return L-value representing slider style
   */
  uiSliderStyle & sliderStyle() { return m_sliderStyle; }

  /**
   * @brief Determines slider position
   *
   * @return Slider position
   */
  int position() { return m_position; }

  /**
   * @brief Sets the slider position
   *
   * @param value New slider position
   */
  void setPosition(int value);

  /**
   * @brief Gets minimum position
   *
   * @return Minimum position
   */
  int min() { return m_min; }

  /**
   * @brief Gets maximum position
   *
   * @return Maximum position
   */
  int max() { return m_max; }

  /**
   * @brief Sets minimum, maximum position and ticks frequency
   *
   * @param min Minimum position (default is 0)
   * @param max Maximum position (default is 99)
   * @param ticksFrequency Frequency of ticks drawings (default 25, 0 = no ticks)
   */
  void setup(int min, int max, int ticksFrequency);


  /**
   * @brief Slider changed event delegate
   *
   * This delegate is called whenever slider changes its position
   */
  Delegate<> onChange;


private:

  void paintSlider();
  Rect getGripRect();
  void moveGripTo(int x, int y);
  void handleKeyDown(uiKeyEventInfo key);


  uiSliderStyle m_sliderStyle;
  uiOrientation m_orientation;

  int16_t       m_position;
  int16_t       m_min;
  int16_t       m_max;
  int16_t       m_ticksFrequency;
};





////////////////////////////////////////////////////////////////////////////////////////////////////
// uiApp


/** @brief Properties of the application */
struct uiAppProps {
  uint16_t caretBlinkingTime = 500;   /**< Caret blinking time (MS) */
  uint16_t doubleClickTime   = 250;   /**< Maximum delay required for two consecutive clicks to become double click (MS) */
  bool     realtimeReshaping = false; /**< If true moving/resizing a window always repaints it, otherwise the moved/resized window is represented by an inverted rectangle */
};


/** \ingroup Enumerations
 * @brief Return values from uiApp.messageBox() method
 */
enum class uiMessageBoxResult {
  Cancel,    /**< Messagebox has been canceled */
  Button1,   /**< Button1 has been pressed */
  Button2,   /**< Button2 has been pressed */
  Button3,   /**< Button3 has been pressed */
};


/** \ingroup Enumerations
 * @brief Icon displayed by the uiApp.messageBox() method
 */
enum class uiMessageBoxIcon {
  None,      /**< No icon is displayed */
  Question,  /**< Question icon */
  Info,      /**< Info icon */
  Warning,   /**< Warning icon */
  Error,     /**< Error icon */
};


struct ModalWindowState {
  uiWindow * window;
  uiWindow * prevFocusedWindow;
  uiWindow * prevActiveWindow;
  uiWindow * prevModal;
  int        modalResult;
};



/**
 * @brief Represents the whole application base class.
 *
 * FabGL UI apps must inherit from uiApp, create UI elements in uiApp.init() method, and finally call uiApp.run() to start the application.
 * uiApp is responsible for events queue handling, windows manager, windows and controls lifecycle and memory management.
 */
class uiApp : public uiEvtHandler {

public:

  uiApp();

  virtual ~uiApp();

  /**
   * @brief Initializes application and executes the main event loop
   *
   * This is the last method that should be called: it never returns.
   *
   * @return exitCode specified calling uiApp.quit().
   */
  int run();

  /**
   * @brief Terminates application and free resources
   *
   * @param exitCode Value returned by uiApp.run().
   */
  void quit(int exitCode);

  /**
   * @brief Places an event in the event queue and returns without waiting for the receiver to process the event
   *
   * @param event Event to send. A copy of the event is sent.
   *
   * @return True if the event is correctly placed. False if there is no available space in the event queue
   */
  bool postEvent(uiEvent const * event);

  /**
   * @brief Inserts (first position) an event in the event queue and returns without waiting for the receiver to process the event
   *
   * @param event Event to insert. A copy of the event is sent.
   *
   * @return True if the event is correctly placed. False if there is no available space in the event queue
   */
  bool insertEvent(uiEvent const * event);

  void postDebugMsg(char const * msg);

  virtual void processEvent(uiEvent * event);

  /**
   * @brief Processes all events in queue
   *
   * This method is useful to update UI while running long time tasks.
   */
  void processEvents();

  /**
   * @brief Gets a pointer to the root window
   *
   * The root window is the first window created and covers the whole screen.
   * All windows and controls must have the root window as parent, or as ancestor.
   *
   * @return The root window pointer
   */
  uiFrame * rootWindow() { return m_rootWindow; }

  /**
   * @brief Gets a pointer to the currently active window
   *
   * There is only one active window at the time. Active window is the foreground window and receives (along with focused control) all keyboard events.
   * To set the active window call uiApp.setActiveWindow().
   *
   * @return The active window, or nullptr if there isn't one
   */
  uiWindow * activeWindow() { return m_activeWindow; }

  /**
   * @brief Sets the active window
   *
   * There is only one active window at the time. Active window is the foreground window and receives (along with focused control) all keyboard events.
   *
   * @param value Pointer of the active window. nullptr to set no active window
   *
   * @return Previous active window
   */
  uiWindow * setActiveWindow(uiWindow * value);

  /**
   * @brief Gets the focused window (control)
   *
   * There is only one focused window or control at the time. Focused window is also the active window (or its parent is the active window).
   * Focused window receives (along with active window) all keyboard events.
   * Focused window can have a blinking caret.
   * Finally, a focused window partecipates to the TAB "trip", respecting the tab-order (see uiWindow.focusIndex()).
   *
   * To set the focoused window use uiApp.setFocusedWindow().
   *
   * @return Focused window or nullptr if there isn't one
   */
  uiWindow * focusedWindow() { return m_focusedWindow; }

  /**
   * @brief Sets the focused window (control)
   *
   * There is only one focused window or control at the time. Focused window is also the active window (or its parent is the active window).
   * Focused window receives (along with active window) all keyboard events.
   * Focused window can have a blinking caret.
   * Finally, a focused window partecipates to the TAB "trip", respecting the tab-order (see uiWindow.focusIndex()).
   *
   * @return Previous focused window
   */
  uiWindow * setFocusedWindow(uiWindow * value);

  /**
   * @brief Move focus to a control with current focus index plus a delta
   *
   * A focused window partecipates to the TAB "trip", respecting the tab-order (see uiWindow.focusIndex()).
   * Calling moveFocus(1) is the same of pressing TAB. Calling moveFocus(-1) is the same of pressing SHIFT-TAB.
   *
   * @param delta Specifies how many controls pass (1 = next control, -1 = previous control)
   *
   * @return Previous focused window
   */
  uiWindow * moveFocus(int delta);

  void captureMouse(uiWindow * window);

  /**
   * @brief Determines which window is capturing mouse
   *
   * Mouse is captured by a window when mouse is dragged (left button down while moving mouse) starting from window area.
   *
   * @return Window that captured mouse
   */
  uiWindow * capturedMouseWindow() { return m_capturedMouseWindow; }

  /**
   * @brief Repaints a window
   *
   * @param window The window to be repainted
   */
  void repaintWindow(uiWindow * window);

  /**
   * @brief Repaints a screen area
   *
   * @param rect Rectangle to be repainted (in screen coordiantes)
   */
  void repaintRect(Rect const & rect);

  /**
   * @brief Moves a window
   *
   * @param window Window to be moved
   * @param x New horizontal coordinate, relative to parent window
   * @param y New vertical coordinate, relative to parent window
   */
  void moveWindow(uiWindow * window, int x, int y);

  /**
   * @brief Resizes a window
   *
   * @param window Window to be resized
   * @param width New horizontal size
   * @param height New vertical size
   */
  void resizeWindow(uiWindow * window, int width, int height);

  /**
   * @brief Resizes a window
   *
   * @param window Window to be resized
   * @param size New window size
   */
  void resizeWindow(uiWindow * window, Size size);

  /**
   * @brief Reshapes a window
   *
   * @param window Window to be reshaped
   * @param rect New window bounding box, relative to the parent window
   */
  void reshapeWindow(uiWindow * window, Rect const & rect);

  /**
   * @brief Determines which window a point belongs to
   *
   * @param point A point relative to the screen
   *
   * @return A window
   */
  uiWindow * screenToWindow(Point & point);

  /**
   * @brief Makes a window visible or invisible
   *
   * @param window Window to be made visible
   * @param value If true the window will be shown, if false the window will be hidden
   */
  void showWindow(uiWindow * window, bool value);

  /**
   * @brief Makes a window visible and handles it has a modal window
   *
   * A modal window disables the main window but keeps it visible. Users must interact with the modal window before they can return to the parent window.
   * A modal window exits from modal state using uiWindow.exitModal().
   * showModalWindow() is the combination of initModalWindow(), processModalWindowEvents() and endModalWindow() called in sequence.
   *
   * @param window Window to be made visible and modal
   *
   * @return The same value specified calling uiWindow.exitModal()
   */
  int showModalWindow(uiWindow * window);

  /**
   * @brief Begins modal window processing
   *
   * initModalWindow(), processModalWindowEvents() and endModalWindow() are useful when a long processing operation is necessary inside a modal window.
   *
   * @param window The form to use as modal window
   *
   * @return Modal window status. This value must be passed to processModalWindowEvents() and endModalWindow() to maintain modal window state.
   */
  ModalWindowState * initModalWindow(uiWindow * window);

  /**
   * @brief Processes all messages from modal window
   *
   * This method must be called whenever UI needs to be updated.
   *
   * @param state This parameter comes from initModalWindow() and is used internally to maintain modal window status.
   * @param timeout Timeout in milliseconds to wait for messages. -1 = infinite timeout, 0 = no timeout.
   *
   * @return False: EXIT or CLOSE received, modal window should close (call endModalWindow). True  = other processModalWindowEvents required, continue outer loop.
   */
  bool processModalWindowEvents(ModalWindowState * state, int timeout);

  /**
   * @brief Ends modal window processing
   *
   * Ends the modal window status. After this other windows can receive input.
   * This method should be always called when processModalWindowEvents() returns False, or when modal window must be anyway closed.
   *
   * @param state This parameter comes from initModalWindow() and is used internally to maintain modal window status.
   *
   * @return The same value specified calling uiWindow.exitModal()
   */
  int endModalWindow(ModalWindowState * state);

  /**
   * @brief Maximizes or restores a window
   *
   * @param window Window to be maximized or restored
   * @param value True maximizes the window, False restores it from maximized state
   */
  void maximizeWindow(uiWindow * window, bool value);

  /**
   * @brief Minimizes or restores a window
   *
   * @param window Window to be minimized or restored
   * @param value True minimizes the window, False restores it from minimized state
   */
  void minimizeWindow(uiWindow * window, bool value);

  void combineMouseMoveEvents(bool value) { m_combineMouseMoveEvents = value; }

  void showCaret(uiWindow * window);

  void setCaret(bool value);

  void setCaret(Point const & pos);

  void setCaret(Rect const & rect);

  /**
   * @brief Setups a timer
   *
   * A timer fires uiApp.onTimer or uiFrame.onTimer delegate.
   * To destroy a timer use uiApp.killTimer().
   *
   * @param dest Destination window or app
   * @param periodMS Timer period in milliseconds
   *
   * @return Handle identifying the new timer
   */
  uiTimerHandle setTimer(uiEvtHandler * dest, int periodMS);

  /**
   * @brief Kills a timer
   *
   * To create a timer use uiApp.setTimer().
   *
   * @param handle Handle identifying the timer to destroy
   */
  void killTimer(uiTimerHandle handle);

  /**
   * @brief Sets or gets application properties
   *
   * @return L-value representing some application properties
   */
  uiAppProps & appProps() { return m_appProps; }

  /**
   * @brief Destroys a window
   *
   * @param window Window to destroy
   */
  void destroyWindow(uiWindow * window);

  void cleanWindowReferences(uiWindow * window);

  /**
   * @brief Enables or disables mouse and keyboard events
   *
   * For default mouse and keyboard events are enabled.
   *
   * @param value True to enable events, False to disable events
   */
  void enableKeyboardAndMouseEvents(bool value);

  /**
   * @brief Displays a modal dialog box with an icon, text and some buttons
   *
   * @param title The dialog box title. If nullptr the messaebox has no title bar
   * @param text The message to be displayed
   * @param button1Text Left button text
   * @param button2Text Middle button text (may be nullptr, if not present)
   * @param button3Text Right button text (may be nullptr, if not present)
   * @param icon Icon to be displayed
   *
   * @return Message box result
   */
  uiMessageBoxResult messageBox(char const * title, char const * text, char const * button1Text, char const * button2Text = nullptr, char const * button3Text = nullptr, uiMessageBoxIcon icon = uiMessageBoxIcon::Question);

  /**
   * @brief Displays a modal dialog box with a text, a text edit and up to two buttons
   *
   * Pressing ENTER (RETURN) equals to press first button.
   * Pressing ESC cancels the dialog box.
   *
   * @param title The dialog box title. If nullptr the messaebox has no title bar
   * @param text The message to be displayed
   * @param inOutString Initial string of the text edit
   * @param maxLength Maximum length of inOutString buffer (ending zero not included)
   * @param button1Text Left button text
   * @param button2Text Right button text (may be nullptr, if not present)
   *
   * @return Message box result
   */
  uiMessageBoxResult inputBox(char const * title, char const * text, char * inOutString, int maxLength, char const * button1Text, char const * button2Text = nullptr);

  /**
   * @brief Method to inherit to implement an application
   */
  virtual void init();

  // delegates

  /**
   * @brief Timer event delegate
   *
   * This delegate is called when the timer expires.
   * To create a timer use uiApp.setTimer().
   */
  Delegate<uiTimerHandle> onTimer;

protected:

  bool getEvent(uiEvent * event, int timeOutMS);
  bool peekEvent(uiEvent * event, int timeOutMS);


private:

  void preprocessEvent(uiEvent * event);
  void preprocessMouseEvent(uiEvent * event);
  void preprocessKeyboardEvent(uiEvent * event);
  void filterModalEvent(uiEvent * event);

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

  uiWindow *    m_modalWindow;         // current modal window

  bool          m_combineMouseMoveEvents;

  uiWindow *    m_caretWindow;         // nullptr = caret is not visible
  Rect          m_caretRect;           // caret rect relative to m_caretWindow
  uiTimerHandle m_caretTimer;
  int           m_caretInvertState;    // -1 = suspended, 1 = rect reversed (cat visible), 0 = rect not reversed (caret invisible)

  int           m_lastMouseUpTimeMS;   // time (MS) at mouse up. Used to measure double clicks
  Point         m_lastMouseUpPos;      // screen position of last mouse up
};




} // end of namespace



