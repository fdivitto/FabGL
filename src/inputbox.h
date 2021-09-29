/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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
 * @brief This file contains the InputBox class
 */



#include <stdint.h>
#include <stddef.h>

#include "fabglconf.h"
#include "fabui.h"
#include "fabutils.h"
#include "dispdrivers/vgapalettedcontroller.h"
#include "comdrivers/ps2controller.h"



namespace fabgl {



/** \ingroup Enumerations
 * @brief Result of InputBox dialogs helper class
 */
enum class InputResult {
  None        = 0,       /**< Still running */
  ButtonExt0  = 1,       /**< Button Ext 0 pressed */
  ButtonExt1  = 2,       /**< Button Ext 1 pressed */
  ButtonExt2  = 3,       /**< Button Ext 3 pressed */
  ButtonExt3  = 4,       /**< Button Ext 4 pressed */
  Cancel      = 5,       /**< Button CANCEL or ESC key pressed */
  ButtonLeft  = 5,       /**< Left button (cancel) or ESC key pressed */
  Enter       = 6,       /**< Button OK, ENTER or RETURN pressed */
  ButtonRight = 6,       /**< Right button (OK), ENTER or RETURN pressed */
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputForm


class InputBox;


struct InputForm {
  InputForm(InputBox * inputBox_)
    : inputBox(inputBox_)
  {
  }

  void init(uiApp * app_, bool modalDialog_);

  virtual void addControls()      = 0;
  virtual void calcRequiredSize() = 0;
  virtual void finalize()         { }
  virtual void show()             { }

  void doExit(int value);

  void setExtButtons(char const * values[]);


  static constexpr int BUTTONS  = 6;

  InputBox *       inputBox;

  uiApp *          app;

  char const *     titleText;
  char const *     buttonText[BUTTONS] = { };
  int              autoOK;

  FontInfo const * font;
  int              requiredWidth;
  int              requiredHeight;

  uiFrame *        mainFrame;
  uiPanel *        panel;
  uiLabel *        autoOKLabel;

  InputResult      retval;

  uiWindow *       controlToFocus;

  bool             modalDialog;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputApp


struct InputApp : public uiApp {
  InputApp(InputForm * form_)      { form = form_; }
  virtual void init()              { form->init(this, false); }

  InputForm * form;
};




////////////////////////////////////////////////////////////////////////////////////////////////////
// TextInputForm


struct TextInputForm : public InputForm {
  TextInputForm(InputBox * inputBox_)
    : InputForm(inputBox_)
  {
  }

  void addControls();
  void calcRequiredSize();
  void finalize();

  char const *     labelText;
  char *           inOutString;
  int              maxLength;
  bool             passwordMode;

  int              editExtent;
  int              labelExtent;

  uiTextEdit *     edit;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageForm


struct MessageForm : public InputForm {
  MessageForm(InputBox * inputBox_)
    : InputForm(inputBox_)
  {
  }

  void addControls();
  void calcRequiredSize();
  void finalize();

  char const *     messageText;

  int              messageExtent;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectForm


struct SelectForm : public InputForm {
  SelectForm(InputBox * inputBox_)
    : InputForm(inputBox_)
  {
  }

  void addControls();
  void calcRequiredSize();
  void finalize();

  int countItems(size_t * maxLength);

  char const *     messageText;
  char const *     items;       // "separator" separated items (zero ends the list)
  char             separator;
  StringList *     itemsList;
  bool             menuMode;

  int              listBoxHeight;
  int              outSelected;

  uiListBox *      listBox;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressForm


struct ProgressForm : public InputForm {
  ProgressForm(InputBox * inputBox_)
    : InputForm(inputBox_)
  {
  }

  void addControls();
  void calcRequiredSize();
  void show();

  bool update(int percentage, char const * format, ...);

  static const int        progressBarHeight = 16;

  bool                    hasProgressBar;
  Delegate<ProgressForm*> execFunc;
  int                     width;

  uiLabel *               label;
  uiProgressBar *         progressBar;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputBox


/** @brief InputBox is an helper class which allows to create simple UI interfaces, like wizards or simple input boxes */
class InputBox {

public:

  /**
   * @brief Creates a new InputBox instance
   *
   * @param app Optional existing uiApp object. If specified applications can use InputBox helpers inside an uiApp object.
   */
  InputBox(uiApp * app = nullptr);

  ~InputBox();

  /**
   * @brief Initializes InputBox from VGA modeline, using a VGA16Controller
   *
   * @param modeline Optional modeline (uses 640x480 resolution if not specified)
   * @param viewPortWidth Viewport width (-1 = automatic)
   * @param viewPortHeight Viewport height (-1 = automatic)
   * @param displayColors Number of colors for the display (2, 4, 8 or 16)
   */
  void begin(char const * modeline = nullptr, int viewPortWidth = -1, int viewPortHeight = -1, int displayColors = 16);

  /**
   * @brief Initializes InputBox from already initialized display controller
   *
   * @param displayController Display controller already active
   */
  void begin(BitmappedDisplayController * displayController);

  /**
   * @brief Gets created or assigned display controller
   *
   * @return Display controller object
   */
  BitmappedDisplayController * getDisplayController()                        { return m_dispCtrl; }

  /**
   * @brief Cleanup resources and eventually disable VGA output
   */
  void end();

  /**
   * @brief Sets the background color
   *
   * @param value Background color
   */
  void setBackgroundColor(RGB888 const & value)   { m_backgroundColor = value; }

  RGB888 backgroundColor()                        { return m_backgroundColor; }

  /**
   * @brief Specifies a timeout for the dialog
   *
   * The timeout countdown stops if user moves mouse are type keyboard.
   *
   * @param timeout If >0, OK is automatically trigged after specified number of seconds
   */
  void setAutoOK(int timeout)                     { m_autoOK = timeout; }

  /**
   * @brief Sets extended buttons text
   *
   * Extended button texts are reset to empty values after every dialog
   *
   * @param extButton A value from 0 to 3. 0 = leftmost button ... 3 = rightmost button
   * @param value Button text
   */
  void setExtButton(int extButton, char const * value) { m_extButtonText[extButton] = value; }

  /**
   * @brief Gets last dialog result
   *
   * @return Last result
   */
  InputResult getLastResult()                          { return m_lastResult; }

  /**
   * @brief Shows a dialog with a label and a text edit box
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param labelText Label text
   * @param inOutString This is the input and output edit box string
   * @param maxLength Maximum length of the edit box string (not including ending zero)
   * @param buttonCancelText Optional text for CANCEL button (nullptr = hasn't CANCEL button). Default is "Cancel".
   * @param buttonOKText Optional text for OK button (nullptr = hasn't OK button). Default is "OK".
   * @param passwordMode Optional password mode. If True all characters are shown using "*". Default is false.
   *
   * @return Dialog box result (Cancel or Enter)
   *
   * Example:
   *
   *     char wifiName[32];
   *     char wifiPsw[32];
   *     InputBox ib;
   *     ib.begin();
   *     if (ib.message("Network Configuration", "Configure WIFI network?", "No", "Yes") == InputResult::Enter) {
   *       ib.textInput("Network Configuration", "WiFi Name", wifiName, 31);
   *       ib.textInput("Network Configuration", "WiFi Password", wifiPsw, 31, "Cancel", "OK", true);
   *     }
   *     ib.end();
   */
  InputResult textInput(char const * titleText, char const * labelText, char * inOutString, int maxLength, char const * buttonCancelText = "Cancel", char const * buttonOKText = "OK", bool passwordMode = false);

  /**
   * @brief Shows a dialog with just a label
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param messageText Message to show
   * @param buttonCancelText Optional test for CANCEL button (nullptr = hasn't CANCEL button). Default is nullptr.
   * @param buttonOKText Optional text for OK button (nullptr = hasn't OK button). Default is "OK".
   *
   * @return Dialog box result (Cancel or Enter)
   *
   * Example:
   *
   *     char wifiName[32];
   *     char wifiPsw[32];
   *     InputBox ib;
   *     ib.begin();
   *     if (ib.message("Network Configuration", "Configure WIFI network?", "No", "Yes") == InputResult::Enter) {
   *       ib.textInput("Network Configuration", "WiFi Name", wifiName, 31);
   *       ib.textInput("Network Configuration", "WiFi Password", wifiPsw, 31, "Cancel", "OK", true);
   *     }
   *     ib.end();
   */
  InputResult message(char const * titleText, char const * messageText, char const * buttonCancelText = nullptr, char const * buttonOKText = "OK");

  /**
   * @brief Shows a dialog with a just a label. Allows printf like formatted text
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param buttonCancelText Optional test for CANCEL button (nullptr = hasn't CANCEL button)
   * @param buttonOKText Optional text for OK button (nullptr = hasn't OK button)
   * @param format printf like format string
   * @param ... printf like parameters
   *
   * @return Dialog box result (Cancel or Enter)
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     for (int i = 0; i < 3; ++i)
   *       if (ib.messageFmt(nullptr, "Abort", "Continue", "Iteration number %d of 3", i) == InputResult::Cancel)
   *        break;
   *     ib.end();
   */
  InputResult messageFmt(char const * titleText, char const * buttonCancelText, char const * buttonOKText, const char *format, ...);

  /**
   * @brief Shows a dialog with a label and a list box
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param messageText Message to show
   * @param itemsText String containing a separated list of items to show into the listbox
   * @param separator Optional items separator. Default is ';'
   * @param buttonCancelText Optional text for CANCEL button (nullptr = hasn't CANCEL button). Default is "Cancel".
   * @param buttonOKText Optional text for OK button (nullptr = hasn't OK button). Default is "OK".
   *
   * @return Index of the selected item or -1 if dialog has been canceled
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     int s = ib.select("Download Boot Disk", "Select boot disk to download", "FreeDOS;Minix 2.0;MS-DOS 3.3");
   *     ib.messageFmt("", nullptr, "OK", "You have selected %d", s);
   *     ib.end();
   */
  int select(char const * titleText, char const * messageText, char const * itemsText, char separator = ';', char const * buttonCancelText = "Cancel", char const * buttonOKText = "OK");

  /**
   * @brief Shows a dialog with a label and a list box
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param messageText Message to show
   * @param items StringList object containing the items to show into the listbox. This parameter contains items selected before and after the dialog
   * @param buttonCancelText Optional text for CANCEL button (nullptr = hasn't CANCEL button). Default is "Cancel".
   * @param buttonOKText Optional text for OK button (nullptr = hasn't OK button). Default is "OK".
   *
   * @return Dialog box result (Cancel or Enter)
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     StringList list;
   *     list.append("Option 0");
   *     list.append("Option 1");
   *     list.append("Option 2");
   *     list.append("Option 3");
   *     list.select(1, true);    // initially item 1 (Option 1) is selected
   *     if (ib.select("Select values", "Please select one or more items", &list) == InputResult::Enter) {
   *       for (int i = 0; i < list.count(); ++i)
   *         if (list.selected(i))
   *           ib.messageFmt("", nullptr, "OK", "You have selected %d", i);
   *     }
   *     ib.end();
   */
  InputResult select(char const * titleText, char const * messageText, StringList * items, char const * buttonCancelText = "Cancel", char const * buttonOKText = "OK");

  /**
   * @brief Shows a dialog with a label and a list box. The dialog exits when an item is selected, just like a menu
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param messageText Message to show
   * @param itemsText String containing a separated list of items to show into the listbox
   * @param separator Optional items separator. Default is ';'
   *
   * @return Index of the selected item or -1 if dialog has been canceled
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     int s = ib.menu("Menu", "Click on an item", "Item number one;Item number two;Item number three;Item number four");
   *     ib.messageFmt("", nullptr, "OK", "You have selected %d", s);
   *     ib.end();
   */
  int menu(char const * titleText, char const * messageText, char const * itemsText, char separator = ';');

  /**
   * @brief Shows a dialog with a label and a list box. The dialog exits when an item is selected, just like a menu
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param messageText Message to show
   * @param items StringList object containing the items to show into the listbox
   *
   * @return Index of the selected item or -1 if dialog has been canceled
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     StringList list;
   *     list.append("Menu item 0");
   *     list.append("Menu item 1");
   *     list.append("Menu item 2");
   *     list.append("Menu item 3");
   *     int s = ib.menu("Menu", "Click on a menu item", &list);
   *     ib.messageFmt("", nullptr, "OK", "You have selected %d", s);
   *     ib.end();
   */
  int menu(char const * titleText, char const * messageText, StringList * items);

  /**
   * @brief Shows a dialog with a label and a progress bar, updated dynamically by a user function
   *
   * @param titleText Optional title of the dialog (nullptr = the dialogs hasn't a title)
   * @param buttonCancelText Optional text for CANCEL button (nullptr = hasn't CANCEL button)
   * @param hasProgressBar If true a progress bar is shown
   * @param width Width in pixels of the dialog
   * @param execFunc A lambda function which updates the text and the progress bar calling the parameter object's update() function
   *
   * @return Dialog box result (Cancel or Enter)
   *
   * Example:
   *
   *     InputBox ib;
   *     ib.begin();
   *     auto r = ib.progressBox("This is the title", "Abort", true, 200, [&](fabgl::ProgressForm * form) {
   *       for (int i = 0; i <= 100; ++i) {
   *         if (!form->update(i, "Index is %d/100", i))
   *           break;
   *         delay(100);
   *       }
   *     });
   *     if (r == InputResult::Cancel)
   *       ib.message("", "Operation Aborted");
   *     ib.end();
   */
  template <typename Func>
  InputResult progressBox(char const * titleText, char const * buttonCancelText, bool hasProgressBar, int width, Func execFunc) {
    ProgressForm form(this);
    form.execFunc = execFunc;
    return progressBoxImpl(form, titleText, buttonCancelText, hasProgressBar, width);
  }


  // delegates

  /**
   * @brief Paint event delegate
   */
  Delegate<Canvas *> onPaint;
  


private:

  InputResult progressBoxImpl(ProgressForm & form, char const * titleText, char const * buttonCancelText, bool hasProgressBar, int width);

  void exec(InputForm * form);

  BitmappedDisplayController * m_dispCtrl;
  VGAPalettedController      * m_vgaCtrl;
  RGB888                       m_backgroundColor;
  uiApp *                      m_existingApp; // uiApp in case of running on existing app
  uint16_t                     m_autoOK;    // auto ok in seconds
  char const *                 m_extButtonText[InputForm::BUTTONS - 2] = { };
  InputResult                  m_lastResult = InputResult::None;
};



}   // namespace fabgl
