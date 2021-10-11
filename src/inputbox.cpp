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
#include <memory>

#include "dispdrivers/vga2controller.h"
#include "dispdrivers/vga4controller.h"
#include "dispdrivers/vga8controller.h"
#include "dispdrivers/vga16controller.h"

#include "devdrivers/keyboard.h"
#include "inputbox.h"


#pragma GCC optimize ("O2")


using std::unique_ptr;


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
    m_autoOK(0),
    m_minButtonsWidth(40)
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
  else
    PS2Controller::keyboard()->enableVirtualKeys(true, true);
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


void InputBox::setupButton(int index, char const * text, char const * subItems, int subItemsHeight)
{
  m_buttonText[index]           = text;
  m_buttonSubItems[index]       = subItems;
  m_buttonSubItemsHeight[index] = subItemsHeight;
}


void InputBox::resetButtons()
{
  for (int i = 0; i < InputForm::BUTTONS; ++i) {
    m_buttonText[i]     = nullptr;
    m_buttonSubItems[i] = nullptr;
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
  resetButtons();
  m_buttonSubItem = form->buttonSubItem;
  m_lastResult    = form->retval;
}


InputResult InputBox::textInput(char const * titleText, char const * labelText, char * inOutString, int maxLength, char const * buttonCancelText, char const * buttonOKText, bool passwordMode)
{
  setupButton(B_CANCEL, buttonCancelText);
  setupButton(B_OK, buttonOKText);

  TextInputForm form(this);
  form.titleText            = titleText;
  form.labelText            = labelText;
  form.inOutString          = inOutString;
  form.maxLength            = maxLength;
  form.passwordMode         = passwordMode;
  form.autoOK               = m_autoOK;

  exec(&form);
  return form.retval;
}


InputResult InputBox::message(char const * titleText, char const * messageText, char const * buttonCancelText, char const * buttonOKText)
{
  setupButton(B_CANCEL, buttonCancelText);
  setupButton(B_OK, buttonOKText);

  MessageForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.autoOK               = m_autoOK;

  exec(&form);
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
  setupButton(B_CANCEL, buttonCancelText);
  setupButton(B_OK, buttonOKText);

  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = itemsText;
  form.separator            = separator;
  form.itemsList            = nullptr;
  form.menuMode             = false;
  form.autoOK               = m_autoOK;

  exec(&form);
  return form.outSelected;
}


InputResult InputBox::select(char const * titleText, char const * messageText, StringList * items, char const * buttonCancelText, char const * buttonOKText)
{
  setupButton(B_CANCEL, buttonCancelText);
  setupButton(B_OK, buttonOKText);

  SelectForm form(this);
  form.titleText            = titleText;
  form.messageText          = messageText;
  form.items                = nullptr;
  form.separator            = 0;
  form.itemsList            = items;
  form.menuMode             = false;
  form.autoOK               = m_autoOK;

  exec(&form);
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
  form.menuMode             = true;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);
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
  form.menuMode             = true;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);
  return items->getFirstSelected();
}


InputResult InputBox::progressBoxImpl(ProgressForm & form, char const * titleText, char const * buttonCancelText, bool hasProgressBar, int width)
{
  setupButton(B_CANCEL, buttonCancelText);

  form.titleText            = titleText;
  form.hasProgressBar       = hasProgressBar;
  form.width                = width;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);
  return form.retval;
}


InputResult InputBox::folderBrowser(char const * titleText, char const * directory, char const * buttonOKText)
{
  setupButton(B_OK, buttonOKText);

  FileBrowserForm form(this);
  form.titleText            = titleText;
  form.autoOK               = 0;  // no timeout supported here
  form.directory            = directory;

  exec(&form);
  return form.retval;
}


InputResult InputBox::fileSelector(char const * titleText, char const * messageText, char * inOutDirectory, int maxDirectoryLength, char * inOutFilename, int maxFilenameLength, char const * buttonCancelText, char const * buttonOKText)
{
  setupButton(B_CANCEL, buttonCancelText);
  setupButton(B_OK, buttonOKText);

  FileSelectorForm form(this);
  form.titleText            = titleText;
  form.labelText            = messageText;
  form.inOutDirectory       = inOutDirectory;
  form.maxDirectoryLength   = maxDirectoryLength;
  form.inOutFilename        = inOutFilename;
  form.maxFilenameLength    = maxFilenameLength;
  form.autoOK               = 0;  // no timeout supported here

  exec(&form);
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

  constexpr int buttonsSpace = 10;

  int buttonsWidth = inputBox->minButtonsWidth();
  int totButtons   = 0;

  for (int i = 0; i < BUTTONS; ++i) {
    auto btext = inputBox->buttonText(i);
    if (btext) {
      int buttonExtent = app->canvas()->textExtent(font, btext) + 10;
      buttonsWidth     = imax(buttonsWidth, buttonExtent);
      ++totButtons;
    }
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
    panel->anchors().top    = false;
    panel->anchors().bottom = true;
    panel->anchors().right  = true;

    // setup buttons

    int y = (panelHeight - buttonsHeight) / 2;
    int x = panel->clientSize().width - buttonsWidth * totButtons - buttonsSpace * (totButtons - 1) - buttonsSpace / 2;  // right aligned

    for (int i = 0; i < BUTTONS; ++i)
      if (inputBox->buttonText(i)) {
        uiWindow * ctrl;
        if (inputBox->buttonSubItems(i)) {
          auto splitButton = new uiSplitButton(panel, inputBox->buttonText(i), Point(x, y), Size(buttonsWidth, buttonsHeight), inputBox->buttonsSubItemsHeight(i), inputBox->buttonSubItems(i));
          splitButton->onSelect = [&, i](int idx) {
            buttonSubItem = idx;
            retval = (InputResult)(i + 1);
            finalize();
          };
          ctrl = splitButton;
        } else {
          auto button = new uiButton(panel, inputBox->buttonText(i), Point(x, y), Size(buttonsWidth, buttonsHeight));
          button->onClick = [&, i]() {
            retval = (InputResult)(i + 1);
            finalize();
          };
          ctrl = button;
        }
        ctrl->anchors().left  = false;
        ctrl->anchors().right = true;
        x += buttonsWidth + buttonsSpace;
        controlToFocus = ctrl;
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


void InputForm::defaultEnterHandler(uiKeyEventInfo const & key)
{
  if (key.VK == VK_RETURN || key.VK == VK_KP_ENTER) {
    retval = InputResult::Enter;
    finalize();
  }
}


void InputForm::defaultEscapeHandler(uiKeyEventInfo const & key)
{
  if (key.VK == VK_ESCAPE) {
    retval = InputResult::Cancel;
    finalize();
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
  mainFrame->frameProps().resizeable        = true;
  mainFrame->frameProps().hasMaximizeButton = true;

  const Point clientPos = mainFrame->clientPos();

  int x = clientPos.X + 4;
  int y = clientPos.Y + 8;

  new uiLabel(mainFrame, labelText, Point(x, y));

  edit = new uiTextEdit(mainFrame, inOutString, Point(x + labelExtent + 5, y - 4), Size(editExtent - 15, font->height + 6));
  edit->anchors().right = true;
  edit->textEditProps().passwordMode = passwordMode;
  edit->onKeyType = [&](uiKeyEventInfo const & key) { defaultEnterHandler(key); defaultEscapeHandler(key); };

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

  mainFrame->onKeyUp = [&](uiKeyEventInfo const & key) { defaultEnterHandler(key); defaultEscapeHandler(key); };
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
  listBoxHeight           = 16 * itemsCount + 2;
  int requiredHeightUnCut = requiredHeight + listBoxHeight;
  requiredHeight          = imin(requiredHeightUnCut, app->canvas()->getHeight());
  requiredWidth           = imax(requiredWidth, maxLength * app->canvas()->textExtent(font, "M"));
  if (requiredHeightUnCut > requiredHeight)
    listBoxHeight -= requiredHeightUnCut - requiredHeight;
}


void SelectForm::addControls()
{
  mainFrame->frameProps().resizeable        = true;
  mainFrame->frameProps().hasMaximizeButton = true;

  int x = mainFrame->clientPos().X + 4;
  int y = mainFrame->clientPos().Y + 6;

  new uiLabel(mainFrame, messageText, Point(x, y));

  y += font->height + 6;

  listBox = new uiListBox(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - 10, listBoxHeight));
  listBox->anchors().right = true;
  listBox->anchors().bottom = true;
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
  listBox->onKeyType = [&](uiKeyEventInfo const & key) { defaultEnterHandler(key); defaultEscapeHandler(key); };

  controlToFocus = listBox;
}


void SelectForm::finalize()
{
  if (items) {
    outSelected = (retval == InputResult::Enter ? listBox->firstSelectedItem() : -1);
  } else {
    if (retval == InputResult::Cancel)
      itemsList->deselectAll();
    else
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

  mainFrame->onKeyUp = [&](uiKeyEventInfo const & key) { defaultEscapeHandler(key); };
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



////////////////////////////////////////////////////////////////////////////////////////////////////
// FileBrowserForm


void FileBrowserForm::calcRequiredSize()
{
  requiredWidth  = imax(requiredWidth, BROWSER_WIDTH + CTRLS_DIST + SIDE_BUTTONS_WIDTH);
  requiredHeight = imax(requiredHeight, BROWSER_HEIGHT);
}


void FileBrowserForm::addControls()
{
  mainFrame->frameProps().resizeable        = true;
  mainFrame->frameProps().hasMaximizeButton = true;

  mainFrame->onKeyUp = [&](uiKeyEventInfo const & key) { defaultEscapeHandler(key); };

  int x = mainFrame->clientPos().X + CTRLS_DIST;
  int y = mainFrame->clientPos().Y + CTRLS_DIST;

  fileBrowser = new uiFileBrowser(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - x - CTRLS_DIST - SIDE_BUTTONS_WIDTH, mainFrame->clientSize().height - panel->size().height - CTRLS_DIST * 2));
  fileBrowser->anchors().right  = true;
  fileBrowser->anchors().bottom = true;
  fileBrowser->setDirectory(directory);

  x += fileBrowser->size().width + CTRLS_DIST;

  newFolderButton = new uiButton(mainFrame, "New Folder", Point(x, y), Size(SIDE_BUTTONS_WIDTH, SIDE_BUTTONS_HEIGHT));
  newFolderButton->anchors().left  = false;
  newFolderButton->anchors().right = true;
  newFolderButton->onClick = [&]() {
    unique_ptr<char[]> dirname(new char[MAXNAME + 1] { 0 } );
    if (app->inputBox("Create Folder", "Name", dirname.get(), MAXNAME, "Create", "Cancel") == uiMessageBoxResult::Button1) {
      fileBrowser->content().makeDirectory(dirname.get());
      fileBrowser->update();
    }
  };

  y += SIDE_BUTTONS_HEIGHT + CTRLS_DIST;

  renameButton = new uiButton(mainFrame, "Rename", Point(x, y), Size(SIDE_BUTTONS_WIDTH, SIDE_BUTTONS_HEIGHT));
  renameButton->anchors().left  = false;
  renameButton->anchors().right = true;
  renameButton->onClick = [&]() {
    if (strcmp(fileBrowser->filename(), "..") != 0) {
      int maxlen = fabgl::imax(MAXNAME, strlen(fileBrowser->filename()));
      unique_ptr<char[]> filename(new char[MAXNAME + 1] { 0 } );
      strcpy(filename.get(), fileBrowser->filename());
      if (app->inputBox("Rename File", "New name", filename.get(), maxlen, "Rename", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().rename(fileBrowser->filename(), filename.get());
        fileBrowser->update();
      }
    }
  };

  y += SIDE_BUTTONS_HEIGHT + CTRLS_DIST;

  deleteButton = new uiButton(mainFrame, "Delete", Point(x, y), Size(SIDE_BUTTONS_WIDTH, SIDE_BUTTONS_HEIGHT));
  deleteButton->anchors().left  = false;
  deleteButton->anchors().right = true;
  deleteButton->onClick = [&]() {
    if (strcmp(fileBrowser->filename(), "..") != 0) {
      if (app->messageBox("Delete file/directory", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().remove( fileBrowser->filename() );
        fileBrowser->update();
      }
    }
  };

  y += SIDE_BUTTONS_HEIGHT + CTRLS_DIST;

  copyButton = new uiButton(mainFrame, "Copy", Point(x, y), Size(SIDE_BUTTONS_WIDTH, SIDE_BUTTONS_HEIGHT));
  copyButton->anchors().left  = false;
  copyButton->anchors().right = true;
  copyButton->onClick = [&]() { doCopy(); };

  y += SIDE_BUTTONS_HEIGHT + CTRLS_DIST;

  pasteButton = new uiButton(mainFrame, "Paste", Point(x, y), Size(SIDE_BUTTONS_WIDTH, SIDE_BUTTONS_HEIGHT));
  pasteButton->anchors().left  = false;
  pasteButton->anchors().right = true;
  pasteButton->onClick = [&]() { doPaste(); };
  app->showWindow(pasteButton, false);

}


void FileBrowserForm::finalize()
{
  doExit(0);
}


void FileBrowserForm::doCopy()
{
  if (!fileBrowser->isDirectory()) {
    if (srcDirectory)
      free(srcDirectory);
    if (srcFilename)
      free(srcFilename);
    srcDirectory = strdup(fileBrowser->directory());
    srcFilename  = strdup(fileBrowser->filename());
    app->showWindow(pasteButton, true);
  }
}


void FileBrowserForm::doPaste()
{
  if (strcmp(srcDirectory, fileBrowser->content().directory()) == 0) {
    app->messageBox("", "Please select a different folder", "OK",  nullptr, nullptr, uiMessageBoxIcon::Error);
    return;
  }
  FileBrowser fb_src(srcDirectory);
  auto fileSize = fb_src.fileSize(srcFilename);
  auto src = fb_src.openFile(srcFilename, "rb");
  if (!src) {
    app->messageBox("", "Unable to find source file", "OK",  nullptr, nullptr, uiMessageBoxIcon::Error);
    return;
  }
  if (fileBrowser->content().exists(srcFilename, false)) {
    if (app->messageBox("", "Overwrite file?", "Yes", "No", nullptr, uiMessageBoxIcon::Question) != uiMessageBoxResult::ButtonOK)
      return;
  }
  auto dst = fileBrowser->content().openFile(srcFilename, "wb");

  auto bytesToCopy = fileSize;

  InputBox ib(app);
  ib.progressBox("Copying", "Abort", true, app->canvas()->getWidth() * 2 / 3, [&](fabgl::ProgressForm * form) {
    constexpr int BUFLEN = 4096;
    unique_ptr<uint8_t[]> buf(new uint8_t[BUFLEN]);
    while (bytesToCopy > 0) {
      auto r = fread(buf.get(), 1, imin(BUFLEN, bytesToCopy), src);
      fwrite(buf.get(), 1, r, dst);
      bytesToCopy -= r;
      if (r == 0)
        break;
      if (!form->update((int)((double)(fileSize - bytesToCopy) / fileSize * 100), "Writing %s (%d / %d bytes)", srcFilename, (fileSize - bytesToCopy), fileSize))
        break;
    }
  });

  fclose(dst);
  fclose(src);
  if (bytesToCopy > 0) {
    fileBrowser->content().remove(srcFilename);
    app->messageBox("", "File not copied", "OK",  nullptr, nullptr, uiMessageBoxIcon::Error);
  }
  fileBrowser->update();
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSelectorForm


void FileSelectorForm::calcRequiredSize()
{
  labelExtent     = app->canvas()->textExtent(font, labelText);
  editExtent      = imin(maxFilenameLength * app->canvas()->textExtent(font, "M") + 15, app->rootWindow()->clientSize().width - labelExtent);
  requiredWidth   = imax(requiredWidth, imax(BROWSER_WIDTH, labelExtent + CTRLS_DIST + MINIMUM_EDIT_WIDTH) + CTRLS_DIST);
  requiredHeight += font->height + CTRLS_DIST + BROWSER_HEIGHT;
}


void FileSelectorForm::addControls()
{
  mainFrame->frameProps().resizeable        = true;
  mainFrame->frameProps().hasMaximizeButton = true;

  mainFrame->onKeyUp = [&](uiKeyEventInfo const & key) { defaultEscapeHandler(key); };

  int x = mainFrame->clientPos().X + CTRLS_DIST;
  int y = mainFrame->clientPos().Y + CTRLS_DIST;

  new uiLabel(mainFrame, labelText, Point(x, y + 4));

  edit = new uiTextEdit(mainFrame, inOutFilename, Point(x + labelExtent + CTRLS_DIST, y), Size(mainFrame->clientSize().width - labelExtent - x - CTRLS_DIST - 1, font->height + 6));
  edit->anchors().right = true;

  y += edit->size().height + CTRLS_DIST;

  fileBrowser = new uiFileBrowser(mainFrame, Point(x, y), Size(mainFrame->clientSize().width - x - 1, mainFrame->clientSize().height - panel->size().height - y + CTRLS_DIST * 2 ));
  fileBrowser->anchors().right  = true;
  fileBrowser->anchors().bottom = true;
  fileBrowser->setDirectory(inOutDirectory);
  fileBrowser->onChange = [&]() {
    if (!fileBrowser->isDirectory()) {
      edit->setText(fileBrowser->filename());
      edit->repaint();
    }
  };
  fileBrowser->onDblClick = [&]() {
    if (!fileBrowser->isDirectory()) {
      retval = InputResult::Enter;
      finalize();
    }
  };
  fileBrowser->onKeyType = [&](uiKeyEventInfo const & key) { defaultEnterHandler(key); defaultEscapeHandler(key); };

  controlToFocus = edit;
}


void FileSelectorForm::finalize()
{
  if (retval == InputResult::Enter) {
    // filename
    int len = imin(maxFilenameLength, strlen(edit->text()));
    memcpy(inOutFilename, edit->text(), len);
    inOutFilename[len] = 0;
    // directory
    len = imin(maxDirectoryLength, strlen(fileBrowser->directory()));
    memcpy(inOutDirectory, fileBrowser->directory(), len);
    inOutDirectory[len] = 0;
  }
  doExit(0);
}



}   // namespace fabgl
