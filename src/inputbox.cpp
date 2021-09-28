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


#include <string.h>

#include "dispdrivers/vga2controller.h"
#include "dispdrivers/vga4controller.h"
#include "dispdrivers/vga8controller.h"
#include "dispdrivers/vga16controller.h"

#include "inputbox.h"


#pragma GCC optimize ("O2")


namespace fabgl {


// well known InputForm::buttonText[] indexes
#define B_CANCEL ((int)(InputResult::Cancel) - 1)
#define B_OK     ((int)(InputResult::Enter) - 1)



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputBox

InputBox::InputBox(uiApp * app)
  : m_vgaCtrl(nullptr),
    m_backgroundColor(RGB888(64, 64, 64)),
    m_existingApp(app),
    m_autoOK(0)
{
}


InputBox::~InputBox()
{
  end();
}


void InputBox::begin(char const * modeline, int viewPortWidth, int viewPortHeight, int displayColors)
{
  // setup display controller
  if (displayColors <= 2)
    m_vgaCtrl = new VGA2Controller;
  else if (displayColors <= 4)
    m_vgaCtrl = new VGA4Controller;
  else if (displayColors <= 8)
    m_vgaCtrl = new VGA8Controller;
  else
    m_vgaCtrl = new VGA16Controller;
  m_dispCtrl = m_vgaCtrl;
  m_vgaCtrl->begin();
  m_vgaCtrl->setResolution(modeline ? modeline : VESA_640x480_75Hz, viewPortWidth, viewPortHeight);

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
  if (m_vgaCtrl) {
    m_vgaCtrl->end();
    delete m_vgaCtrl;
    m_vgaCtrl = nullptr;
  }
}


void InputBox::exec(InputForm * form)
{
  if (m_existingApp) {
    form->init(m_existingApp, true);
    m_existingApp->showModalWindow(form->mainFrame);
    m_existingApp->destroyWindow(form->mainFrame);
  } else {
    // run in standalone mode
    InputApp app(form);
    app.run(m_dispCtrl);
  }
}


InputResult InputBox::textInput(char const * titleText, char const * labelText, char * inOutString, int maxLength, char const * buttonCancelText, char const * buttonOKText, bool passwordMode)
{
  TextInputForm form(this);
  form.titleText            = titleText;
  form.labelText            = labelText;
  form.inOutString          = inOutString;
  form.maxLength            = maxLength;
  form.buttonText[B_CANCEL] = buttonCancelText;
  form.buttonText[B_OK]     = buttonOKText;
  form.passwordMode         = passwordMode;
  form.autoOK               = m_autoOK;

  form.setExtButtons(m_extButtonText);

  exec(&form);

  m_lastResult = form.retval;
  return form.retval;
}


InputResult InputBox::message(char const * titleText, char const * messageText, char const * buttonCancelText, char const * buttonOKText)
{
  MessageForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.buttonText[B_CANCEL] = buttonCancelText;
  form.buttonText[B_OK]     = buttonOKText;
  form.autoOK               = m_autoOK;

  form.setExtButtons(m_extButtonText);

  exec(&form);

  m_lastResult = form.retval;
  return form.retval;
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


int InputBox::select(char const * titleText, char const * messageText, char const * itemsText, char separator, char const * buttonCancelText, char const * buttonOKText)
{
  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = itemsText;
  form.separator            = separator;
  form.itemsList            = nullptr;
  form.buttonText[B_CANCEL] = buttonCancelText;
  form.buttonText[B_OK]     = buttonOKText;
  form.menuMode             = false;
  form.autoOK               = m_autoOK;

  form.setExtButtons(m_extButtonText);

  exec(&form);

  m_lastResult = form.retval;
  return form.outSelected;
}


InputResult InputBox::select(char const * titleText, char const * messageText, StringList * items, char const * buttonCancelText, char const * buttonOKText)
{
  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = nullptr;
  form.separator            = 0;
  form.itemsList            = items;
  form.buttonText[B_CANCEL] = buttonCancelText;
  form.buttonText[B_OK]     = buttonOKText;
  form.menuMode             = false;
  form.autoOK               = m_autoOK;

  form.setExtButtons(m_extButtonText);

  exec(&form);

  m_lastResult = form.retval;
  return form.retval;
}


int InputBox::menu(char const * titleText, char const * messageText, char const * itemsText, char separator)
{
  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = itemsText;
  form.separator            = separator;
  form.itemsList            = nullptr;
  form.buttonText[B_CANCEL] = nullptr;
  form.buttonText[B_OK]     = nullptr;
  form.menuMode             = true;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);

  m_lastResult = form.retval;
  return form.outSelected;
}


int InputBox::menu(char const * titleText, char const * messageText, StringList * items)
{
  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = nullptr;
  form.separator            = 0;
  form.itemsList            = items;
  form.buttonText[B_CANCEL] = nullptr;
  form.buttonText[B_OK]     = nullptr;
  form.menuMode             = true;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);

  m_lastResult = form.retval;
  return items->getFirstSelected();
}


InputResult InputBox::progressBoxImpl(ProgressForm & form, char const * titleText, char const * buttonCancelText, bool hasProgressBar, int width)
{
  form.titleText            = titleText;
  form.buttonText[B_CANCEL] = buttonCancelText;
  form.buttonText[B_OK]     = nullptr;
  form.hasProgressBar       = hasProgressBar;
  form.width                = width;
  form.autoOK               = 0;  // no timeout supported here

  form.setExtButtons(m_extButtonText);

  exec(&form);

  m_lastResult = form.retval;
  return form.retval;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// InputForm


void InputForm::init(uiApp * app_, bool modalDialog_)
{
  retval = InputResult::None;

  app         = app_;
  modalDialog = modalDialog_;

  if (!modalDialog) {
    app->rootWindow()->frameStyle().backgroundColor = inputBox->backgroundColor();
    app->rootWindow()->onPaint = [&]() {
      inputBox->onPaint(app->canvas());
    };
  }

  font = &FONT_std_14;

  const int titleHeight = titleText && strlen(titleText) ? font->height : 0;

  constexpr int buttonsSpace    = 10;
  constexpr int minButtonsWidth = 40;

  int buttonsWidth = minButtonsWidth;
  int totButtons   = 0;

  for (auto btext : buttonText)
    if (btext) {
      int buttonExtent = app->canvas()->textExtent(font, btext) + 10;
      buttonsWidth     = imax(buttonsWidth, buttonExtent);
      ++totButtons;
    }

  const int buttonsHeight = totButtons ? font->height + 6 : 0;

  requiredWidth  = buttonsWidth * totButtons + (2 * buttonsSpace) * totButtons;
  requiredHeight = buttonsHeight + titleHeight + font->height * 2 + 5;

  calcRequiredSize();

  requiredWidth  = imin(requiredWidth, app->canvas()->getWidth());

  controlToFocus = nullptr;

  mainFrame = new uiFrame(app->rootWindow(), titleText, UIWINDOW_PARENTCENTER, Size(requiredWidth, requiredHeight), false);
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
  mainFrame->onShow = [&]() {
    if (controlToFocus)
      app->setFocusedWindow(controlToFocus);
    show();
  };

  autoOKLabel = nullptr;

  if (totButtons) {

    // setup panel (where buttons are positioned)

    int panelHeight = buttonsHeight + 10;
    panel = new uiPanel(mainFrame, Point(mainFrame->clientPos().X - 1, mainFrame->clientPos().Y + mainFrame->clientSize().height - panelHeight), Size(mainFrame->clientSize().width + 2, panelHeight));
    panel->windowStyle().borderColor    = RGB888(128, 128, 128);
    panel->panelStyle().backgroundColor = mainFrame->frameStyle().backgroundColor;

    // setup buttons

    int y = (panelHeight - buttonsHeight) / 2;
    int x = panel->clientSize().width - buttonsWidth * totButtons - buttonsSpace * totButtons;  // right aligned

    for (int i = 0; i < BUTTONS; ++i)
      if (buttonText[i]) {
        auto button = new uiButton(panel, buttonText[i], Point(x, y), Size(buttonsWidth, buttonsHeight));
        button->onClick = [&, i]() {
          retval = (InputResult)(i + 1);
          finalize();
        };
        x += buttonsWidth + buttonsSpace;
        controlToFocus = button;
      }

    if (autoOK > 0) {
      autoOKLabel = new uiLabel(panel, "", Point(4, y + 2));

      mainFrame->onTimer = [&](uiTimerHandle t) {
        int now = esp_timer_get_time() / 1000;
        if (app->lastUserActionTime() + 900 > now) {
          app->killTimer(t);
          app->destroyWindow(autoOKLabel);
          return;
        }
        if (autoOK <= 0) {
          app->killTimer(t);
          retval = InputResult::Enter;
          finalize();
        }
        --autoOK;
        autoOKLabel->setTextFmt("%d", autoOK);
      };
      app->setTimer(mainFrame, 1000);

    }

  } else {
    panel = nullptr;
  }

  addControls();

  if (!modalDialog) {
    app->showWindow(mainFrame, true);
    app->setActiveWindow(mainFrame);
  }
}


void InputForm::doExit(int value)
{
  if (modalDialog)
    mainFrame->exitModal(value);
  else {
    app->quit(value);
    // this avoids flickering of content painted in onPaint
    app->rootWindow()->frameProps().fillBackground = false;
  }
}


void InputForm::setExtButtons(char const * values[])
{
  for (int i = 0; i < BUTTONS - 2; ++i) {
    buttonText[i] = values[i];
    values[i] = nullptr;
  }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// TextInputForm


void TextInputForm::calcRequiredSize()
{
  labelExtent     = app->canvas()->textExtent(font, labelText);
  editExtent      = imin(maxLength * app->canvas()->textExtent(font, "M") + 15, app->rootWindow()->clientSize().width - labelExtent);
  requiredWidth   = imax(requiredWidth, editExtent + labelExtent + 10);
  requiredHeight += font->height;
}


void TextInputForm::addControls()
{
  const Point clientPos = mainFrame->clientPos();

  int x = clientPos.X + 4;
  int y = clientPos.Y + 8;

  new uiLabel(mainFrame, labelText, Point(x, y));

  edit = new uiTextEdit(mainFrame, inOutString, Point(x + labelExtent + 5, y - 4), Size(editExtent - 15, font->height + 6));
  edit->textEditProps().passwordMode = passwordMode;

  controlToFocus = edit;
}


void TextInputForm::finalize()
{
  if (retval == InputResult::Enter) {
    int len = imin(maxLength, strlen(edit->text()));
    memcpy(inOutString, edit->text(), len);
    inOutString[len] = 0;
  }
  doExit(0);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageForm


void MessageForm::calcRequiredSize()
{
  messageExtent   = app->canvas()->textExtent(font, messageText);
  requiredWidth   = imax(requiredWidth, messageExtent + 20);
  requiredHeight += font->height;
}


void MessageForm::addControls()
{
  int x = mainFrame->clientPos().X + (mainFrame->clientSize().width - messageExtent) / 2;
  int y = mainFrame->clientPos().Y + 6;

  new uiLabel(mainFrame, messageText, Point(x, y));
}


void MessageForm::finalize()
{
  doExit(0);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// SelectForm


void SelectForm::calcRequiredSize()
{
  auto messageExtent = app->canvas()->textExtent(font, messageText);
  requiredWidth      = imax(requiredWidth, messageExtent + 20);

  // calc space for message
  requiredHeight    += font->height;

  // calc space for list box
  size_t maxLength;
  auto itemsCount         = countItems(&maxLength);
  listBoxHeight           = (font->height + 4) * itemsCount;
  int requiredHeightUnCut = requiredHeight + listBoxHeight;
  requiredHeight          = imin(requiredHeightUnCut, app->canvas()->getHeight());
  requiredWidth           = imax(requiredWidth, maxLength * app->canvas()->textExtent(font, "M"));
  if (requiredHeightUnCut > requiredHeight)
    listBoxHeight -= requiredHeightUnCut - requiredHeight;
}


void SelectForm::addControls()
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
  } else {
    listBox->onDblClick = [&]() {
      retval = InputResult::Enter;
      finalize();
    };
  }

  controlToFocus = listBox;
}


void SelectForm::finalize()
{
  if (items) {
    outSelected = (retval == InputResult::Enter ? listBox->firstSelectedItem() : -1);
  } else {
    itemsList->copySelectionMapFrom(listBox->items());
  }
  doExit(0);
}


int SelectForm::countItems(size_t * maxLength)
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
// ProgressForm


void ProgressForm::calcRequiredSize()
{
  requiredWidth   = imax(requiredWidth, width);
  requiredHeight += font->height + (hasProgressBar ? progressBarHeight : 0);
}


void ProgressForm::addControls()
{
  int x = mainFrame->clientPos().X + 4;
  int y = mainFrame->clientPos().Y + 6;

  label = new uiLabel(mainFrame, "", Point(x, y));

  if (hasProgressBar) {
    y += font->height + 4;

    progressBar = new uiProgressBar(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - 8, font->height));
  }
}


void ProgressForm::show()
{
  execFunc(this);
  if (retval != InputResult::Cancel)
    retval = InputResult::Enter;
  doExit(0);
}


// return True if not Abort
bool ProgressForm::update(int percentage, char const * format, ...)
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

  app->processEvents();
  return retval == InputResult::None;
}


}   // namespace fabgl
