/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

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


#include <string.h>

#include "inputbox.h"


#pragma GCC optimize ("O2")


namespace fabgl {



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputBox

InputBox::InputBox()
  : m_vga16Ctrl(nullptr),
    m_backgroundColor(RGB888(64, 64, 64))
{
}


InputBox::~InputBox()
{
  end();
}


void InputBox::begin(char const * modeline, int viewPortWidth, int viewPortHeight)
{
  // setup display controller
  m_vga16Ctrl = new VGA16Controller;
  m_dispCtrl = m_vga16Ctrl;
  m_vga16Ctrl->begin();
  m_vga16Ctrl->setResolution(modeline ? modeline : VESA_640x480_75Hz, viewPortWidth, viewPortHeight);

  // setup keyboard and mouse
  if (!PS2Controller::initialized())
    PS2Controller::begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);
}


void InputBox::begin(BitmappedDisplayController * displayController)
{
  m_dispCtrl = displayController;
}


void InputBox::end()
{
  if (m_vga16Ctrl) {
    m_vga16Ctrl->end();
    delete m_vga16Ctrl;
    m_vga16Ctrl = nullptr;
  }
}


InputResult InputBox::textInput(char const * titleText, char const * labelText, char * inOutString, int maxLength, char const * buttonCancelText, char const * buttonOKText, bool passwordMode)
{
  TextInputApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.labelText        = labelText;
  app.inOutString      = inOutString;
  app.maxLength        = maxLength;
  app.buttonCancelText = buttonCancelText;
  app.buttonOKText     = buttonOKText;
  app.passwordMode     = passwordMode;
  app.autoOK           = 0;

  app.run(m_dispCtrl);

  return app.retval;
}


InputResult InputBox::message(char const * titleText, char const * messageText, char const * buttonCancelText, char const * buttonOKText)
{
  MessageApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.messageText      = messageText;
  app.buttonCancelText = buttonCancelText;
  app.buttonOKText     = buttonOKText;
  app.autoOK           = 0;

  app.run(m_dispCtrl);

  return app.retval;
}


InputResult InputBox::messageFmt(char const * titleText, char const * buttonCancelText, char const * buttonOKText, const char * format, ...)
{
  auto r = InputResult::Cancel;
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    r = message(titleText, buf, buttonCancelText, buttonOKText);
  }
  va_end(ap);
  return r;
}


int InputBox::select(char const * titleText, char const * messageText, char const * itemsText, char separator, char const * buttonCancelText, char const * buttonOKText, int OKAfter)
{
  SelectApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.messageText      = messageText;
  app.items            = itemsText;
  app.separator        = separator;
  app.itemsList        = nullptr;
  app.buttonCancelText = buttonCancelText;
  app.buttonOKText     = buttonOKText;
  app.menuMode         = false;
  app.autoOK           = OKAfter;

  app.run(m_dispCtrl);

  return app.outSelected;
}


InputResult InputBox::select(char const * titleText, char const * messageText, StringList * items, char const * buttonCancelText, char const * buttonOKText, int OKAfter)
{
  SelectApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.messageText      = messageText;
  app.items            = nullptr;
  app.separator        = 0;
  app.itemsList        = items;
  app.buttonCancelText = buttonCancelText;
  app.buttonOKText     = buttonOKText;
  app.menuMode         = false;
  app.autoOK           = OKAfter;

  app.run(m_dispCtrl);

  return app.retval;
}


int InputBox::menu(char const * titleText, char const * messageText, char const * itemsText, char separator)
{
  SelectApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.messageText      = messageText;
  app.items            = itemsText;
  app.separator        = separator;
  app.itemsList        = nullptr;
  app.buttonCancelText = nullptr;
  app.buttonOKText     = nullptr;
  app.menuMode         = true;
  app.autoOK           = 0;

  app.run(m_dispCtrl);

  return app.outSelected;
}


int InputBox::menu(char const * titleText, char const * messageText, StringList * items)
{
  SelectApp app;
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.messageText      = messageText;
  app.items            = nullptr;
  app.separator        = 0;
  app.itemsList        = items;
  app.buttonCancelText = nullptr;
  app.buttonOKText     = nullptr;
  app.menuMode         = true;
  app.autoOK           = 0;

  app.run(m_dispCtrl);

  return items->getFirstSelected();
}


InputResult InputBox::progressBoxImpl(ProgressApp & app, char const * titleText, char const * buttonCancelText, bool hasProgressBar, int width)
{
  app.backgroundColor  = m_backgroundColor;
  app.titleText        = titleText;
  app.buttonCancelText = buttonCancelText;
  app.buttonOKText     = nullptr;
  app.hasProgressBar   = hasProgressBar;
  app.width            = width;
  app.autoOK           = 0;

  app.run(m_dispCtrl);

  return app.retval;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputApp


void InputApp::init()
{
  retval = InputResult::None;

  rootWindow()->frameStyle().backgroundColor = backgroundColor;
  font = &FONT_std_14;

  const int titleHeight        = titleText && strlen(titleText) ? font->height : 0;
  const bool buttonsExist      = buttonCancelText || buttonOKText;
  const int buttonCancelExtent = buttonCancelText ? canvas()->textExtent(font, buttonCancelText) + 10 : 0;
  const int buttonOKExtent     = buttonOKText ? canvas()->textExtent(font, buttonOKText) + 10 : 0;
  const int buttonsWidth       = imax(imax(buttonCancelExtent, buttonOKExtent), 40);
  const int totButtons         = (buttonCancelExtent ? 1 : 0) + (buttonOKExtent ? 1 : 0);
  const int buttonsHeight      = buttonsExist ? font->height + 6 : 0;
  constexpr int buttonsSpace   = 10;

  requiredWidth  = buttonsWidth * totButtons + (2 * buttonsSpace) * totButtons;
  requiredHeight = buttonsHeight + titleHeight + font->height * 2 + 5;

  calcRequiredSize();

  requiredWidth  = imin(requiredWidth, canvas()->getWidth());

  mainFrame = new uiFrame(rootWindow(), titleText, UIWINDOW_PARENTCENTER, Size(requiredWidth, requiredHeight), false);
  mainFrame->frameProps().resizeable        = false;
  mainFrame->frameProps().hasMaximizeButton = false;
  mainFrame->frameProps().hasMinimizeButton = false;
  mainFrame->frameProps().hasCloseButton    = false;
  mainFrame->onKeyUp = [&](uiKeyEventInfo key) {
    if (key.VK == VK_RETURN || key.VK == VK_KP_ENTER) {
      retval = InputResult::Enter;
      finalize();
    } else if (key.VK == VK_ESCAPE) {
      retval = InputResult::Cancel;
      finalize();
    }
  };

  autoOKLabel = nullptr;

  uiWindow * controlToFocus = nullptr;

  if (buttonsExist) {

    // setup panel (where buttons are positioned)

    int panelHeight = buttonsHeight + 10;
    panel = new uiPanel(mainFrame, Point(mainFrame->clientPos().X - 1, mainFrame->clientPos().Y + mainFrame->clientSize().height - panelHeight), Size(mainFrame->clientSize().width + 2, panelHeight));
    panel->windowStyle().borderColor    = RGB888(128, 128, 128);
    panel->panelStyle().backgroundColor = mainFrame->frameStyle().backgroundColor;

    // setup buttons

    int y = (panelHeight - buttonsHeight) / 2;
    int x = panel->clientSize().width - buttonsWidth * totButtons - buttonsSpace * totButtons;  // right aligned

    if (buttonCancelText) {
      auto buttonCancel = new uiButton(panel, buttonCancelText, Point(x, y), Size(buttonsWidth, buttonsHeight));
      buttonCancel->onClick = [&]() {
        retval = InputResult::Cancel;
        finalize();
      };
      x += buttonsWidth + buttonsSpace;
      controlToFocus = buttonCancel;
    }

    if (buttonOKText) {
      auto buttonOK = new uiButton(panel, buttonOKText, Point(x, y), Size(buttonsWidth, buttonsHeight));
      buttonOK->onClick = [&]() {
        retval = InputResult::Enter;
        finalize();
      };
      controlToFocus = buttonOK;
    }

    if (autoOK > 0) {
      autoOKLabel = new uiLabel(panel, "", Point(4, y + 2));

      onTimer = [&](uiTimerHandle t) {
        int now = esp_timer_get_time() / 1000;
        if (lastUserActionTime() + 900 > now) {
          killTimer(t);
          destroyWindow(autoOKLabel);
          return;
        }
        if (autoOK <= 0) {
          killTimer(t);
          retval = InputResult::Enter;
          finalize();
        }
        --autoOK;
        autoOKLabel->setTextFmt("%d", autoOK);
      };
      setTimer(this, 1000);

    }

  } else {
    panel = nullptr;
  }

  addControls();

  showWindow(mainFrame, true);
  setActiveWindow(mainFrame);
  if (controlToFocus)
    setFocusedWindow(controlToFocus);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// TextInputApp


void TextInputApp::calcRequiredSize()
{
  labelExtent     = canvas()->textExtent(font, labelText);
  editExtent      = imin(maxLength * canvas()->textExtent(font, "M"), rootWindow()->clientSize().width / 2 - labelExtent);
  requiredWidth   = imax(requiredWidth, editExtent + labelExtent + 10);
  requiredHeight += font->height;
}


void TextInputApp::addControls()
{
  const Point clientPos = mainFrame->clientPos();

  int x = clientPos.X + 4;
  int y = clientPos.Y + 8;

  new uiLabel(mainFrame, labelText, Point(x, y));

  edit = new uiTextEdit(mainFrame, inOutString, Point(x + labelExtent + 5, y - 4), Size(editExtent - 15, font->height + 6));
  edit->textEditProps().passwordMode = passwordMode;

  mainFrame->onShow = [&]() { setFocusedWindow(edit); };
}


void TextInputApp::finalize()
{
  if (retval == InputResult::Enter) {
    int len = imin(maxLength, strlen(edit->text()));
    memcpy(inOutString, edit->text(), len);
    inOutString[len] = 0;
  }
  quit(0);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageApp


void MessageApp::calcRequiredSize()
{
  messageExtent   = canvas()->textExtent(font, messageText);
  requiredWidth   = imax(requiredWidth, messageExtent + 20);
  requiredHeight += font->height;
}


void MessageApp::addControls()
{
  int x = mainFrame->clientPos().X + (mainFrame->clientSize().width - messageExtent) / 2;
  int y = mainFrame->clientPos().Y + 6;

  new uiLabel(mainFrame, messageText, Point(x, y));
}


void MessageApp::finalize()
{
  quit(0);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectApp


void SelectApp::calcRequiredSize()
{
  auto messageExtent = canvas()->textExtent(font, messageText);
  requiredWidth      = imax(requiredWidth, messageExtent + 20);

  // calc space for message
  requiredHeight    += font->height;

  // calc space for list box
  size_t maxLength;
  auto itemsCount         = countItems(&maxLength);
  listBoxHeight           = (font->height + 4) * itemsCount;
  int requiredHeightUnCut = requiredHeight + listBoxHeight;
  requiredHeight          = imin(requiredHeightUnCut, canvas()->getHeight());
  requiredWidth           = imax(requiredWidth, maxLength * canvas()->textExtent(font, "M"));
  if (requiredHeightUnCut > requiredHeight)
    listBoxHeight -= requiredHeightUnCut - requiredHeight;
}


void SelectApp::addControls()
{
  int x = mainFrame->clientPos().X + 4;
  int y = mainFrame->clientPos().Y + 6;

  new uiLabel(mainFrame, messageText, Point(x, y));

  y += font->height + 6;

  listBox = new uiListBox(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - 10, listBoxHeight));
  if (items) {
    listBox->items().appendSepList(items, separator);
  } else {
    listBox->items().copyFrom(*itemsList);
    listBox->items().copySelectionMapFrom(*itemsList);
  }
  if (menuMode) {
    listBox->listBoxProps().allowMultiSelect  = false;
    listBox->listBoxProps().selectOnMouseOver = true;
    listBox->onClick = [&]() {
      retval = InputResult::Enter;
      finalize();
    };
  }

  mainFrame->onShow = [&]() { setFocusedWindow(listBox); };
}


void SelectApp::finalize()
{
  if (items) {
    outSelected = (retval == InputResult::Enter ? listBox->firstSelectedItem() : -1);
  } else {
    itemsList->copySelectionMapFrom(listBox->items());
  }
  quit(0);
}


int SelectApp::countItems(size_t * maxLength)
{
  *maxLength = 0;
  int count = 0;
  if (items) {
    char const * start = items;
    while (*start) {
      auto end = strchr(start, separator);
      if (!end)
        end = strchr(start, 0);
      int len = end - start;
      *maxLength = imax(*maxLength, len);
      start += len + (*end == 0 ? 0 : 1);
      ++count;
    }
  } else if (itemsList) {
    for (int i = 0; i < itemsList->count(); ++i)
      *maxLength = imax(*maxLength, strlen(itemsList->get(i)));
    count += itemsList->count();
  }
  return count;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressApp


void ProgressApp::calcRequiredSize()
{
  requiredWidth   = imax(requiredWidth, width);
  requiredHeight += font->height + (hasProgressBar ? progressBarHeight : 0);
}


void ProgressApp::addControls()
{
  int x = mainFrame->clientPos().X + 4;
  int y = mainFrame->clientPos().Y + 6;

  label = new uiLabel(mainFrame, "", Point(x, y));

  if (hasProgressBar) {
    y += font->height + 4;

    progressBar = new uiProgressBar(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - 8, font->height));
  }

  mainFrame->onShow = [&]() {
    execFunc(this);
    if (retval != InputResult::Cancel)
      retval = InputResult::Enter;
    quit(0);
  };
}


void ProgressApp::finalize()
{
}


// return True if not Abort
bool ProgressApp::update(int percentage, char const * format, ...)
{
  if (hasProgressBar)
    progressBar->setPercentage(percentage);

  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    label->setText(buf);
  }
  va_end(ap);

  processEvents();
  return retval == InputResult::None;
}


}   // namespace fabgl
