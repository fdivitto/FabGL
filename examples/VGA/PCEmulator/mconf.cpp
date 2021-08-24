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
#define MAXVALUELENGTH 128


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


void MachineConf::loadFromFile(FILE * file)
{
  cleanUp();
  auto item = new MachineConfItem;
  char tag[MAXTAGLENGTH + 1];
  char * value = (char*) malloc(MAXVALUELENGTH + 1);
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
    else if (strcmp("dska", tag) == 0)
      item->setDska(value);
    else if (strcmp("dskb", tag) == 0)
      item->setDskb(value);
    else if (strcmp("dskc", tag) == 0)
      item->setDskc(value);
  }
  addItem(item);
  free(value);
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
    if (item->dska)
      saveTag(file, "dska", item->dska);
    if (item->dskb)
      saveTag(file, "dskb", item->dskb);
    if (item->dskc)
      saveTag(file, "dskc", item->dskc);
    fputs("\r\n", file);
  }
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// loadMachineConfiguration


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
  uiTextEdit      * editDiskA;
  uiTextEdit      * editDiskB;
  uiTextEdit      * editDiskC;
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

    // disk A
    new uiLabel(mainFrame, "Disk A", Point(x, y + oy));
    editDiskA = new uiTextEdit(mainFrame, item->dska, Point(50, y), Size(290, hh));
    browseAButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseAButton->onClick = [&]() { browseFilename(editDiskA); };


    y += dy;

    // disk B
    new uiLabel(mainFrame, "Disk B", Point(x, y + oy));
    editDiskB = new uiTextEdit(mainFrame, item->dskb, Point(50, y), Size(290, hh));
    browseBButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseBButton->onClick = [&]() { browseFilename(editDiskB); };

    y += dy;

    // disk C
    new uiLabel(mainFrame, "Disk C", Point(x, y + oy));
    editDiskC = new uiTextEdit(mainFrame, item->dskc, Point(50, y), Size(290, hh));
    browseCButton = new uiButton(mainFrame, "...", Point(345, y), Size(20, hh));
    browseCButton->onClick = [&]() { browseFilename(editDiskC); };

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
    item->setDska(editDiskA->text());
    item->setDskb(editDiskB->text());
    item->setDskc(editDiskC->text());

    // save to file
    FileBrowser fb("/SD");
    auto confFile = fb.openFile(MACHINE_CONF_FILENAME, "wb");
    mconf->saveToFile(confFile);
    fclose(confFile);

    // quit
    quit(0);
  }

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// editConfigDialog


void editConfigDialog(InputBox * ibox, MachineConf * mconf, int idx)
{
  ConfigDialog app;
  app.mconf           = mconf;
  app.item            = mconf->getItem(idx);
  app.backgroundColor = ibox->backgroundColor();
  app.run(ibox->getDisplayController());
}
