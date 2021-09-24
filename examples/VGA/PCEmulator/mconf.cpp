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


#include "fabgl.h"
#include "fabui.h"
#include "fabutils.h"


#include "mconf.h"



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MachineConf


#define MAXTAGLENGTH     6
#define MAXVALUELENGTH 256


MachineConf::MachineConf()
  : m_itemsList(nullptr)
{
}


MachineConf::~MachineConf()
{
  cleanUp();
}


void MachineConf::cleanUp()
{
  while (deleteItem(0))
    ;
}


bool MachineConf::deleteItem(int index)
{
  MachineConfItem * item = m_itemsList;
  MachineConfItem * prev = nullptr;
  while (item && --index > -1) {
    prev = item;
    item = item->next;
  }
  if (item) {
    if (prev)
      prev->next = item->next;
    else
      m_itemsList = item->next;
    delete item;
  }
  return item != nullptr;
}


MachineConfItem * MachineConf::getItem(int index)
{
  auto item = m_itemsList;
  while (item && --index > -1)
    item = item->next;
  return item;
}


void MachineConf::addItem(MachineConfItem * item)
{
  if (item->isValid()) {
    if (m_itemsList) {
      auto prev = m_itemsList;
      while (prev->next)
        prev = prev->next;
      prev->next = item;
    } else
      m_itemsList = item;
  } else
    delete item;
}


// insertingPoint = -1, append
// insertingPoint = 0, insert before first
// insertingPoint > 0, insert before insertingPoint
void MachineConf::insertItem(int insertingPoint, MachineConfItem * item)
{
  if (insertingPoint == 0) {
    item->next = m_itemsList;
    m_itemsList = item;
  } else if (insertingPoint < 0) {
    addItem(item);
  } else {
    auto prev = getItem(insertingPoint - 1);
    item->next = prev->next;
    prev->next = item;
  }
}


void MachineConf::loadFromFile(FILE * file)
{
  cleanUp();
  auto item = new MachineConfItem;
  char * value = (char*) SOC_EXTRAM_DATA_LOW; // use PSRAM as buffer
  char * tag   = (char*) SOC_EXTRAM_DATA_LOW + MAXVALUELENGTH + 1;
  int ipos;
  char c = fgetc(file);
  while (!feof(file)) {
    while (!feof(file) && (isspace(c) || iscntrl(c))) {
      if (c == 0x0a) {
        // new line
        addItem(item);
        item = new MachineConfItem;
      }
      c = fgetc(file);
    }
    // get tag
    ipos = 0;
    while (!feof(file) && !isspace(c)) {
      if (ipos < MAXTAGLENGTH)
        tag[ipos++] = c;
      c = fgetc(file);
    }
    tag[ipos] = 0;
    // bypass spaces
    while (!feof(file) && isspace(c))
      c = fgetc(file);
    // get tag value
    ipos = 0;
    bool quote = false;
    while (!feof(file) && (quote || !isspace(c))) {
      if (c == '"')
        quote = !quote;
      else if (ipos < MAXVALUELENGTH)
        value[ipos++] = c;
      c = fgetc(file);
    }
    value[ipos] = 0;
    // assign tag
    if (strcmp("desc", tag) == 0)
      item->setDesc(value);
    else if (strcmp("dska", tag) == 0 || strcmp("fd0", tag) == 0)
      item->setDisk(0, value);
    else if (strcmp("dskb", tag) == 0 || strcmp("fd1", tag) == 0)
      item->setDisk(1, value);
    else if (strcmp("dskc", tag) == 0 || strcmp("hd0", tag) == 0)
      item->setDisk(2, value);
    else if (strcmp("dskd", tag) == 0 || strcmp("hd1", tag) == 0)
      item->setDisk(3, value);
  }
  addItem(item);
}


void MachineConf::saveTag(FILE * file, char const * tag, char const * value)
{
  fputs(tag, file);
  fputs(" \"", file);
  fputs(value, file);
  fputs("\" ", file);
}


void MachineConf::saveToFile(FILE * file)
{
  for (auto item = m_itemsList; item; item = item->next) {
    saveTag(file, "desc", item->desc);
    if (item->disk[0])
      saveTag(file, "fd0", item->disk[0]);
    if (item->disk[1])
      saveTag(file, "fd1", item->disk[1]);
    if (item->disk[2])
      saveTag(file, "hd0", item->disk[2]);
    if (item->disk[3])
      saveTag(file, "hd1", item->disk[3]);
    fputs("\r\n", file);
  }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConfigDialog


struct ConfigDialog : public uiApp {

  MachineConf     * mconf;
  MachineConfItem * item;

  RGB888            backgroundColor;

  uiFrame         * mainFrame;
  uiButton        * buttonSave;
  uiButton        * buttonCancel;
  uiTextEdit      * editDesc;
  uiTextEdit      * editFD0;
  uiTextEdit      * editFD1;
  uiTextEdit      * editHD0;
  uiButton        * browseAButton;
  uiButton        * browseBButton;
  uiButton        * browseCButton;

  void init() {
    rootWindow()->frameStyle().backgroundColor = backgroundColor;

    mainFrame = new uiFrame(rootWindow(), "Machine Configuration", UIWINDOW_PARENTCENTER, Size(372, 200));
    mainFrame->frameProps().resizeable        = false;
    mainFrame->frameProps().hasMaximizeButton = false;
    mainFrame->frameProps().hasMinimizeButton = false;
    mainFrame->frameProps().hasCloseButton    = false;
    mainFrame->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_RETURN || key.VK == VirtualKey::VK_KP_ENTER) {
        saveAndQuit();
      } else if (key.VK == VirtualKey::VK_ESCAPE) {
        quit(0);
      }
    };

    int x = 6;
    int y = 24;
    constexpr int oy = 3;
    constexpr int hh = 20;
    constexpr int dy = hh + 9;

    // description
    new uiLabel(mainFrame, "Description", Point(x, y + oy));
    editDesc = new uiTextEdit(mainFrame, item->desc, Point(70, y), Size(270, hh));

    y += dy;

    // floppy 0
    new uiLabel(mainFrame, "Floppy 0", Point(x, y + oy));
    editFD0 = new uiTextEdit(mainFrame, item->disk[0], Point(60, y), Size(280, hh));
    browseAButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseAButton->onClick = [&]() { browseFilename(editFD0); };


    y += dy;

    // floppy 1
    new uiLabel(mainFrame, "Floppy 1", Point(x, y + oy));
    editFD1 = new uiTextEdit(mainFrame, item->disk[1], Point(60, y), Size(280, hh));
    browseBButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseBButton->onClick = [&]() { browseFilename(editFD1); };

    y += dy;

    // HDD 0
    new uiLabel(mainFrame, "Hard Disk", Point(x, y + oy));
    editHD0 = new uiTextEdit(mainFrame, item->disk[2], Point(60, y), Size(280, hh));
    browseCButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseCButton->onClick = [&]() { browseFilename(editHD0); };

    y += dy * 2;

    // Save Button
    buttonSave = new uiButton(mainFrame, "Save", Point(mainFrame->clientSize().width - 75, mainFrame->clientSize().height - 8), Size(70, hh));
    buttonSave->onClick = [&]() {
      saveAndQuit();
    };

    // Cancel Button
    buttonCancel = new uiButton(mainFrame, "Cancel", Point(mainFrame->clientSize().width - 155, mainFrame->clientSize().height - 8), Size(70, hh));
    buttonCancel->onClick = [&]() {
      quit(0);
    };


    setActiveWindow(mainFrame);
  }

  // @TODO: support files stored in subfolders!!
  void browseFilename(uiTextEdit * edit) {
    char * dir = (char*) malloc(MAXVALUELENGTH + 1);
    char * filename = (char*) malloc(MAXVALUELENGTH + 1);
    strcpy(dir, "/SD");
    strcpy(filename, edit->text());
    if (fileDialog("Select drive image", dir, MAXVALUELENGTH, filename, MAXVALUELENGTH, "OK", "Cancel") == uiMessageBoxResult::ButtonOK) {
      edit->setText(filename);
      edit->repaint();
    }
    free(filename);
    free(dir);
  }

  void saveAndQuit() {
    // get fields
    item->setDesc(editDesc->text());
    item->setDisk(0, editFD0->text());
    item->setDisk(1, editFD1->text());
    item->setDisk(2, editHD0->text());

    // save to file
    saveMachineConfiguration(mconf);

    // quit
    quit(0);
  }

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helpers


void loadMachineConfiguration(MachineConf * mconf)
{
  FileBrowser fb("/SD");

  // saves a default configuration file if necessary
  if (!fb.exists(MACHINE_CONF_FILENAME, false)) {
    auto confFile = fb.openFile(MACHINE_CONF_FILENAME, "wb");
    fwrite(DefaultConfFile, 1, sizeof(DefaultConfFile), confFile);
    fclose(confFile);
  }

  // load
  auto confFile = fb.openFile(MACHINE_CONF_FILENAME, "rb");
  mconf->loadFromFile(confFile);
  fclose(confFile);
}


void saveMachineConfiguration(MachineConf * mconf)
{
  FileBrowser fb("/SD");

  auto confFile = fb.openFile(MACHINE_CONF_FILENAME, "wb");
  mconf->saveToFile(confFile);
  fclose(confFile);
}


void editConfigDialog(InputBox * ibox, MachineConf * mconf, int idx)
{
  ConfigDialog app;
  app.mconf           = mconf;
  app.item            = mconf->getItem(idx);
  app.backgroundColor = ibox->backgroundColor();
  app.run(ibox->getDisplayController());
}


void newConfigDialog(InputBox * ibox, MachineConf * mconf, int idx)
{
  auto newItem = new MachineConfItem;
  newItem->setDesc("New Configuration");
  mconf->insertItem(idx, newItem);

  ConfigDialog app;
  app.mconf           = mconf;
  app.item            = newItem;
  app.backgroundColor = ibox->backgroundColor();
  app.run(ibox->getDisplayController());
}


void delConfigDialog(InputBox * ibox, MachineConf * mconf, int idx)
{
  if (idx > -1 && ibox->message("Please confirm", "Remove Configuration?", "No", "Yes") == InputResult::Enter) {
    mconf->deleteItem(idx);
    saveMachineConfiguration(mconf);
  }
}
