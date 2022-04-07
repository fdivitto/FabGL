/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
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


#include <memory>

#include "fabgl.h"
#include "fabui.h"
#include "fabutils.h"


#include "mconf.h"


using std::unique_ptr;
using fabgl::imin;




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
    else if (strcmp("chs0", tag) == 0)
      item->setCHS(2, value);
    else if (strcmp("chs1", tag) == 0)
      item->setCHS(3, value);
    else if (strcmp("boot", tag) == 0)
      item->setBootDrive(value);
  }
  addItem(item);
}


void MachineConf::saveToFile(FILE * file)
{
  for (auto item = m_itemsList; item; item = item->next) {
    fputs("desc \"", file);
    fputs(item->desc, file);
    fputs("\" ", file);
    for (int d = 0; d < DISKCOUNT; ++d)
      if (item->disk[d] && strlen(item->disk[d]) > 0) {
        fputs(MachineConfItem::driveIndexToStr(d), file);
        fputs(" \"", file);
        fputs(item->disk[d], file);
        fputs("\" ", file);
      }
    for (int hd = 2; hd < 4; ++hd)
      if (item->cylinders[hd] > 0 && item->heads[hd] > 0 && item->sectors[hd] > 0)
        fprintf(file, "chs%d %hu,%hu,%hu ", hd - 2, item->cylinders[hd], item->heads[hd], item->sectors[hd]);
    if (item->bootDrive > 0)
      fprintf(file, "boot %s ", MachineConfItem::driveIndexToStr(item->bootDrive));
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
  uiButton        * saveButton;
  uiButton        * cancelButton;
  uiButton        * createImageButton;
  uiTextEdit      * descEdit;
  uiTextEdit      * FD0PathEdit;
  uiTextEdit      * FD1PathEdit;
  uiTextEdit      * HD0PathEdit, * HD0CylEdit, * HD0HdsEdit, * HD0SecEdit;
  uiTextEdit      * HD1PathEdit, * HD1CylEdit, * HD1HdsEdit, * HD1SecEdit;
  uiButton        * browseFD0Button;
  uiButton        * browseFD1Button;
  uiButton        * browseHD0Button;
  uiButton        * browseHD1Button;
  uiComboBox      * bootDriveComboBox;


  void init() {
    rootWindow()->frameStyle().backgroundColor = backgroundColor;

    rootWindow()->onPaint = [&]() { drawInfo(canvas()); };

    mainFrame = new uiFrame(rootWindow(), "Machine Configuration", UIWINDOW_PARENTCENTER, Size(460, 250));
    mainFrame->frameProps().resizeable        = false;
    mainFrame->frameProps().hasMaximizeButton = false;
    mainFrame->frameProps().hasMinimizeButton = false;
    mainFrame->frameProps().hasCloseButton    = false;
    mainFrame->onKeyUp = [&](uiKeyEventInfo const & key) {
      if (key.VK == VirtualKey::VK_RETURN || key.VK == VirtualKey::VK_KP_ENTER) {
        saveAndQuit();
      } else if (key.VK == VirtualKey::VK_ESCAPE) {
        justQuit();
      }
    };

    int x = 6;
    int y = 24;
    constexpr int oy = 3;
    constexpr int hh = 20;
    constexpr int dy = hh + 9;

    // description
    new uiStaticLabel(mainFrame, "Description", Point(x, y + oy));
    descEdit = new uiTextEdit(mainFrame, item->desc, Point(70, y), Size(270, hh));

    y += dy;

    // floppy 0
    new uiStaticLabel(mainFrame, "Floppy 0", Point(x, y + oy));
    FD0PathEdit = new uiTextEdit(mainFrame, item->disk[0], Point(60, y), Size(260, hh));
    browseFD0Button = new uiButton(mainFrame, "...", Point(322, y), Size(20, hh));
    browseFD0Button->onClick = [&]() { browseFilename(FD0PathEdit); };


    y += dy;

    // floppy 1
    new uiStaticLabel(mainFrame, "Floppy 1", Point(x, y + oy));
    FD1PathEdit = new uiTextEdit(mainFrame, item->disk[1], Point(60, y), Size(260, hh));
    browseFD1Button = new uiButton(mainFrame, "...", Point(322, y), Size(20, hh));
    browseFD1Button->onClick = [&]() { browseFilename(FD1PathEdit); };

    y += dy;

    // HDD 0
    new uiStaticLabel(mainFrame, "HDD 0", Point(x, y + oy));
    HD0PathEdit = new uiTextEdit(mainFrame, item->disk[2], Point(60, y), Size(260, hh));
    browseHD0Button = new uiButton(mainFrame, "...", Point(322, y), Size(20, hh));
    browseHD0Button->onClick = [&]() { browseFilename(HD0PathEdit); };
    new uiStaticLabel(mainFrame, "Cyls", Point(350, y - 13));
    HD0CylEdit  = new uiTextEdit(mainFrame, "", Point(350, y), Size(34, hh));
    HD0CylEdit->setTextFmt("%hu", item->cylinders[2]);
    new uiStaticLabel(mainFrame, "Head", Point(385, y - 13));
    HD0HdsEdit  = new uiTextEdit(mainFrame, "", Point(385, y), Size(34, hh));
    HD0HdsEdit->setTextFmt("%hu", item->heads[2]);
    new uiStaticLabel(mainFrame, "Sect", Point(420, y - 13));
    HD0SecEdit  = new uiTextEdit(mainFrame, "", Point(420, y), Size(34, hh));
    HD0SecEdit->setTextFmt("%hu", item->sectors[2]);

    y += dy;

    // HDD 1
    new uiStaticLabel(mainFrame, "HDD 1", Point(x, y + oy));
    HD1PathEdit = new uiTextEdit(mainFrame, item->disk[3], Point(60, y), Size(260, hh));
    browseHD1Button = new uiButton(mainFrame, "...", Point(322, y), Size(20, hh));
    browseHD1Button->onClick = [&]() { browseFilename(HD1PathEdit); };
    HD1CylEdit  = new uiTextEdit(mainFrame, "", Point(350, y), Size(34, hh));
    HD1CylEdit->setTextFmt("%hu", item->cylinders[3]);
    HD1HdsEdit  = new uiTextEdit(mainFrame, "", Point(385, y), Size(34, hh));
    HD1HdsEdit->setTextFmt("%hu", item->heads[3]);
    HD1SecEdit  = new uiTextEdit(mainFrame, "", Point(420, y), Size(34, hh));
    HD1SecEdit->setTextFmt("%hu", item->sectors[3]);

    y += dy;

    // boot drive selection
    new uiStaticLabel(mainFrame, "Boot Drive", Point(x, y + oy));
    bootDriveComboBox = new uiComboBox(mainFrame, Point(60, y), Size(60, hh), 50);
    bootDriveComboBox->items().append("Floppy 0");
    bootDriveComboBox->items().append("Floppy 1");
    bootDriveComboBox->items().append("HDD 0");
    bootDriveComboBox->items().append("HDD 1");
    bootDriveComboBox->selectItem(item->bootDrive);

    // bottom buttons

    y = mainFrame->clientSize().height - 8;

    // Save Button
    saveButton = new uiButton(mainFrame, "Save", Point(mainFrame->clientSize().width - 75, y), Size(70, hh));
    saveButton->onClick = [&]() {
      saveAndQuit();
    };

    // Cancel Button
    cancelButton = new uiButton(mainFrame, "Cancel", Point(mainFrame->clientSize().width - 155, y), Size(70, hh));
    cancelButton->onClick = [&]() {
      justQuit();
    };

    // Create Disk Image Button
    createImageButton = new uiButton(mainFrame, "Create Disk", Point(10, y), Size(90, hh));
    createImageButton->onClick = [&]() {
      createDiskImage();
    };

    setActiveWindow(mainFrame);
  }


  void browseFilename(uiTextEdit * edit) {
    unique_ptr<char[]> dir(new char[MAXVALUELENGTH + 1] { '/', 'S', 'D', 0 } );
    unique_ptr<char[]> filename(new char[MAXVALUELENGTH + 1]);
    strcpy(filename.get(), edit->text());
    // is URL?
    if (strncmp("://", filename.get() + 4, 3)) {
      // no, does filename contain a path?
      auto p = strrchr(filename.get(), '/');
      if (p) {
        // yes, move it into "dir"
        strcat(dir.get(), "/");
        strncat(dir.get(), filename.get(), p - filename.get());
        memmove(filename.get(), p + 1, strlen(p + 1) + 1);
      }
    }
    if (fileDialog("Select drive image", dir.get(), MAXVALUELENGTH, filename.get(), MAXVALUELENGTH, "OK", "Cancel") == uiMessageBoxResult::ButtonOK) {
      if (strlen(dir.get()) > 4)
        edit->setTextFmt("%s/%s", dir.get() + 4, filename.get()); // bypass "/SD/"
      else
        edit->setText(filename.get());
      edit->repaint();
    }
  }


  void saveAndQuit() {
    // get fields
    item->setDesc(descEdit->text());
    item->setDisk(0, FD0PathEdit->text());
    item->setDisk(1, FD1PathEdit->text());
    item->setDisk(2, HD0PathEdit->text());
    item->setDisk(3, HD1PathEdit->text());
    item->cylinders[2] = atoi(HD0CylEdit->text());
    item->heads[2]     = atoi(HD0HdsEdit->text());
    item->sectors[2]   = atoi(HD0SecEdit->text());
    item->cylinders[3] = atoi(HD1CylEdit->text());
    item->heads[3]     = atoi(HD1HdsEdit->text());
    item->sectors[3]   = atoi(HD1SecEdit->text());
    item->bootDrive    = bootDriveComboBox->selectedItem();

    // save to file
    saveMachineConfiguration(mconf);

    justQuit();
  }


  void justQuit() {
    rootWindow()->frameProps().fillBackground = false;
    quit(0);
  }


  void createDiskImage() {
    InputBox ib(this);

    int s = ib.menu("Create Disk Image", "Select Disk Size", "Floppy 320K (FAT12);"         // 0
                                                             "Floppy 360K (FAT12);"         // 1
                                                             "Floppy 720K (FAT12);"         // 2
                                                             "Floppy 1.2M (FAT12);"         // 3
                                                             "Floppy 1.44M (FAT12);"        // 4
                                                             "Floppy 2.88M (FAT12);"        // 5
                                                             "Hard Disk (Unformatted)");    // 6
    int hdSize = 0;
    if (s == 6) {
      // set hard disk size
      char str[4] = "10";
      if (ib.textInput("Hard Disk Size", "Specify Hard Disk size in Megabytes", str, 3) == InputResult::Enter)
        hdSize = atoi(str);
      else
        return;
      if (hdSize <= 0 || hdSize > 512) {
        ib.message("Error", "Invalid Hard Disk Size!");
        return;
      }
    } else if (s < 0 || s > 6)
      return;

    // set filename
    unique_ptr<char[]> dir(new char[MAXVALUELENGTH + 1]      { '/', 'S', 'D', 0 } );
    unique_ptr<char[]> filename(new char[MAXVALUELENGTH + 1] { 'n', 'e', 'w', 'i', 'm', 'a', 'g', 'e', '.', 'i', 'm', 'g', 0 });
    if (fileDialog("Image Filename", dir.get(), MAXVALUELENGTH, filename.get(), MAXVALUELENGTH, "OK", "Cancel") != uiMessageBoxResult::ButtonOK)
      return;

    // create the image
    if (s == 6) {
      // Hard Disk
      ib.progressBox("", "Abort", true, 380, [&](fabgl::ProgressForm * form) {
        auto buf = (uint8_t*) SOC_EXTRAM_DATA_LOW; // use PSRAM as buffer
        constexpr int BUFSIZE = 4096;
        memset(buf, 0, BUFSIZE);
        auto file = FileBrowser(dir.get()).openFile(filename.get(), "wb");
        const int totSize = hdSize * 1048576;
        for (int sz = totSize; sz > 0; ) {
          sz -= fwrite(buf, 1, imin(BUFSIZE, sz), file);
          if (!form->update((int)((double)(totSize - sz) / totSize * 100), "Writing %s (%d / %d bytes)", filename.get(), (totSize - sz), totSize))
            break;
        }
        fclose(file);
      });
    } else {
      // FAT Formatted Floppy Disk
      createFATFloppyImage(&ib, s, dir.get(), filename.get());
    }

  }

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helpers


void loadMachineConfiguration(MachineConf * mconf)
{
  FileBrowser fb(SD_MOUNT_PATH);

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
  auto confFile = FileBrowser(SD_MOUNT_PATH).openFile(MACHINE_CONF_FILENAME, "wb");
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


void drawInfo(Canvas * canvas)
{
  canvas->setPenColor(RGB888(0, 255, 0));
  canvas->drawText(120, 5, "E S P 3 2   P C   E M U L A T O R");
  canvas->drawText(93, 25, "www.fabgl.com - by Fabrizio Di Vittorio");
}


// Create FAT formatted floppy image
// diskType:
//   0 = 320
//   1 = 360
//   2 = 720
//   3 = 1200
//   4 = 1440
//   5 = 2880
// directory: absolute path (includes mounting point, ie "/SD/..."
bool createFATFloppyImage(InputBox * ibox, int diskType, char const * directory, char const * filename)
{
  // boot sector for: 320K, 360K, 720K, 1440K, 2880K
  static const uint8_t BOOTSECTOR_WIN[512] = {
    0xeb, 0x3c, 0x90, 0x4d, 0x53, 0x57, 0x49, 0x4e, 0x34, 0x2e, 0x31, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x40, 0x0b, 0xf0, 0x09, 0x00,
    0x12, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x33, 0xc9, 0x8e, 0xd1, 0xbc, 0xfc, 0x7b, 0x16, 0x07, 0xbd,
    0x78, 0x00, 0xc5, 0x76, 0x00, 0x1e, 0x56, 0x16, 0x55, 0xbf, 0x22, 0x05, 0x89, 0x7e, 0x00, 0x89, 0x4e, 0x02, 0xb1, 0x0b, 0xfc, 0xf3, 0xa4, 0x06,
    0x1f, 0xbd, 0x00, 0x7c, 0xc6, 0x45, 0xfe, 0x0f, 0x38, 0x4e, 0x24, 0x7d, 0x20, 0x8b, 0xc1, 0x99, 0xe8, 0x7e, 0x01, 0x83, 0xeb, 0x3a, 0x66, 0xa1,
    0x1c, 0x7c, 0x66, 0x3b, 0x07, 0x8a, 0x57, 0xfc, 0x75, 0x06, 0x80, 0xca, 0x02, 0x88, 0x56, 0x02, 0x80, 0xc3, 0x10, 0x73, 0xed, 0x33, 0xc9, 0xfe,
    0x06, 0xd8, 0x7d, 0x8a, 0x46, 0x10, 0x98, 0xf7, 0x66, 0x16, 0x03, 0x46, 0x1c, 0x13, 0x56, 0x1e, 0x03, 0x46, 0x0e, 0x13, 0xd1, 0x8b, 0x76, 0x11,
    0x60, 0x89, 0x46, 0xfc, 0x89, 0x56, 0xfe, 0xb8, 0x20, 0x00, 0xf7, 0xe6, 0x8b, 0x5e, 0x0b, 0x03, 0xc3, 0x48, 0xf7, 0xf3, 0x01, 0x46, 0xfc, 0x11,
    0x4e, 0xfe, 0x61, 0xbf, 0x00, 0x07, 0xe8, 0x28, 0x01, 0x72, 0x3e, 0x38, 0x2d, 0x74, 0x17, 0x60, 0xb1, 0x0b, 0xbe, 0xd8, 0x7d, 0xf3, 0xa6, 0x61,
    0x74, 0x3d, 0x4e, 0x74, 0x09, 0x83, 0xc7, 0x20, 0x3b, 0xfb, 0x72, 0xe7, 0xeb, 0xdd, 0xfe, 0x0e, 0xd8, 0x7d, 0x7b, 0xa7, 0xbe, 0x7f, 0x7d, 0xac,
    0x98, 0x03, 0xf0, 0xac, 0x98, 0x40, 0x74, 0x0c, 0x48, 0x74, 0x13, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0xeb, 0xef, 0xbe, 0x82, 0x7d, 0xeb,
    0xe6, 0xbe, 0x80, 0x7d, 0xeb, 0xe1, 0xcd, 0x16, 0x5e, 0x1f, 0x66, 0x8f, 0x04, 0xcd, 0x19, 0xbe, 0x81, 0x7d, 0x8b, 0x7d, 0x1a, 0x8d, 0x45, 0xfe,
    0x8a, 0x4e, 0x0d, 0xf7, 0xe1, 0x03, 0x46, 0xfc, 0x13, 0x56, 0xfe, 0xb1, 0x04, 0xe8, 0xc2, 0x00, 0x72, 0xd7, 0xea, 0x00, 0x02, 0x70, 0x00, 0x52,
    0x50, 0x06, 0x53, 0x6a, 0x01, 0x6a, 0x10, 0x91, 0x8b, 0x46, 0x18, 0xa2, 0x26, 0x05, 0x96, 0x92, 0x33, 0xd2, 0xf7, 0xf6, 0x91, 0xf7, 0xf6, 0x42,
    0x87, 0xca, 0xf7, 0x76, 0x1a, 0x8a, 0xf2, 0x8a, 0xe8, 0xc0, 0xcc, 0x02, 0x0a, 0xcc, 0xb8, 0x01, 0x02, 0x80, 0x7e, 0x02, 0x0e, 0x75, 0x04, 0xb4,
    0x42, 0x8b, 0xf4, 0x8a, 0x56, 0x24, 0xcd, 0x13, 0x61, 0x61, 0x72, 0x0a, 0x40, 0x75, 0x01, 0x42, 0x03, 0x5e, 0x0b, 0x49, 0x75, 0x77, 0xc3, 0x03,
    0x18, 0x01, 0x27, 0x0d, 0x0a, 0x49, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x64, 0x69, 0x73, 0x6b,
    0xff, 0x0d, 0x0a, 0x44, 0x69, 0x73, 0x6b, 0x20, 0x49, 0x2f, 0x4f, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0xff, 0x0d, 0x0a, 0x52, 0x65, 0x70, 0x6c,
    0x61, 0x63, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2c, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x74, 0x68, 0x65, 0x6e, 0x20, 0x70,
    0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x0d, 0x0a, 0x00, 0x00, 0x49, 0x4f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x53, 0x59, 0x53, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x7f, 0x01, 0x00, 0x41, 0xbb, 0x00, 0x07, 0x60, 0x66, 0x6a,
    0x00, 0xe9, 0x3b, 0xff, 0x00, 0x00, 0x55, 0xaa };

  // boot sector for: 1200K
  static const uint8_t BOOTSECTOR_MSDOS5[512] = {
    0xeb, 0x3c, 0x90, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x35, 0x2e, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x60, 0x09, 0xf9, 0x08, 0x00,
    0x0f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0xd1, 0x40, 0x38, 0xda, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16, 0x07,
    0xbb, 0x78, 0x00, 0x36, 0xc5, 0x37, 0x1e, 0x56, 0x16, 0x53, 0xbf, 0x3e, 0x7c, 0xb9, 0x0b, 0x00, 0xfc, 0xf3, 0xa4, 0x06, 0x1f, 0xc6, 0x45, 0xfe,
    0x0f, 0x8b, 0x0e, 0x18, 0x7c, 0x88, 0x4d, 0xf9, 0x89, 0x47, 0x02, 0xc7, 0x07, 0x3e, 0x7c, 0xfb, 0xcd, 0x13, 0x72, 0x79, 0x33, 0xc0, 0x39, 0x06,
    0x13, 0x7c, 0x74, 0x08, 0x8b, 0x0e, 0x13, 0x7c, 0x89, 0x0e, 0x20, 0x7c, 0xa0, 0x10, 0x7c, 0xf7, 0x26, 0x16, 0x7c, 0x03, 0x06, 0x1c, 0x7c, 0x13,
    0x16, 0x1e, 0x7c, 0x03, 0x06, 0x0e, 0x7c, 0x83, 0xd2, 0x00, 0xa3, 0x50, 0x7c, 0x89, 0x16, 0x52, 0x7c, 0xa3, 0x49, 0x7c, 0x89, 0x16, 0x4b, 0x7c,
    0xb8, 0x20, 0x00, 0xf7, 0x26, 0x11, 0x7c, 0x8b, 0x1e, 0x0b, 0x7c, 0x03, 0xc3, 0x48, 0xf7, 0xf3, 0x01, 0x06, 0x49, 0x7c, 0x83, 0x16, 0x4b, 0x7c,
    0x00, 0xbb, 0x00, 0x05, 0x8b, 0x16, 0x52, 0x7c, 0xa1, 0x50, 0x7c, 0xe8, 0x92, 0x00, 0x72, 0x1d, 0xb0, 0x01, 0xe8, 0xac, 0x00, 0x72, 0x16, 0x8b,
    0xfb, 0xb9, 0x0b, 0x00, 0xbe, 0xe6, 0x7d, 0xf3, 0xa6, 0x75, 0x0a, 0x8d, 0x7f, 0x20, 0xb9, 0x0b, 0x00, 0xf3, 0xa6, 0x74, 0x18, 0xbe, 0x9e, 0x7d,
    0xe8, 0x5f, 0x00, 0x33, 0xc0, 0xcd, 0x16, 0x5e, 0x1f, 0x8f, 0x04, 0x8f, 0x44, 0x02, 0xcd, 0x19, 0x58, 0x58, 0x58, 0xeb, 0xe8, 0x8b, 0x47, 0x1a,
    0x48, 0x48, 0x8a, 0x1e, 0x0d, 0x7c, 0x32, 0xff, 0xf7, 0xe3, 0x03, 0x06, 0x49, 0x7c, 0x13, 0x16, 0x4b, 0x7c, 0xbb, 0x00, 0x07, 0xb9, 0x03, 0x00,
    0x50, 0x52, 0x51, 0xe8, 0x3a, 0x00, 0x72, 0xd8, 0xb0, 0x01, 0xe8, 0x54, 0x00, 0x59, 0x5a, 0x58, 0x72, 0xbb, 0x05, 0x01, 0x00, 0x83, 0xd2, 0x00,
    0x03, 0x1e, 0x0b, 0x7c, 0xe2, 0xe2, 0x8a, 0x2e, 0x15, 0x7c, 0x8a, 0x16, 0x24, 0x7c, 0x8b, 0x1e, 0x49, 0x7c, 0xa1, 0x4b, 0x7c, 0xea, 0x00, 0x00,
    0x70, 0x00, 0xac, 0x0a, 0xc0, 0x74, 0x29, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0xeb, 0xf2, 0x3b, 0x16, 0x18, 0x7c, 0x73, 0x19, 0xf7, 0x36,
    0x18, 0x7c, 0xfe, 0xc2, 0x88, 0x16, 0x4f, 0x7c, 0x33, 0xd2, 0xf7, 0x36, 0x1a, 0x7c, 0x88, 0x16, 0x25, 0x7c, 0xa3, 0x4d, 0x7c, 0xf8, 0xc3, 0xf9,
    0xc3, 0xb4, 0x02, 0x8b, 0x16, 0x4d, 0x7c, 0xb1, 0x06, 0xd2, 0xe6, 0x0a, 0x36, 0x4f, 0x7c, 0x8b, 0xca, 0x86, 0xe9, 0x8a, 0x16, 0x24, 0x7c, 0x8a,
    0x36, 0x25, 0x7c, 0xcd, 0x13, 0xc3, 0x0d, 0x0a, 0x4e, 0x6f, 0x6e, 0x2d, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x20,
    0x6f, 0x72, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x0d, 0x0a, 0x52, 0x65, 0x70, 0x6c, 0x61, 0x63, 0x65, 0x20, 0x61,
    0x6e, 0x64, 0x20, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x77, 0x68, 0x65, 0x6e, 0x20, 0x72, 0x65,
    0x61, 0x64, 0x79, 0x0d, 0x0a, 0x00, 0x49, 0x4f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x4d, 0x53, 0x44, 0x4f, 0x53, 0x20, 0x20,
    0x20, 0x53, 0x59, 0x53, 0x00, 0x00, 0x55, 0xaa };

  static const uint8_t * BOOTSECTOR[6] = {
    BOOTSECTOR_WIN,     // 320K
    BOOTSECTOR_WIN,     // 360K
    BOOTSECTOR_WIN,     // 720K
    BOOTSECTOR_MSDOS5,  // 1200K
    BOOTSECTOR_WIN,     // 1440K
    BOOTSECTOR_WIN,     // 2880K
  };

  // offset 0x0c (16 bytes):
  static const uint8_t GEOM[6][16]  = {
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0x80, 0x02, 0xFF, 0x01, 0x00, 0x08, 0x00, 0x02, 0x00 },  // 320K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xD0, 0x02, 0xFD, 0x02, 0x00, 0x09, 0x00, 0x02, 0x00 },  // 360K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xA0, 0x05, 0xF9, 0x03, 0x00, 0x09, 0x00, 0x02, 0x00 },  // 720K
    { 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x60, 0x09, 0xF9, 0x08, 0x00, 0x0F, 0x00, 0x02, 0x00 },  // 1200K
    { 0x02, 0x01, 0x01, 0x00, 0x02, 0xE0, 0x00, 0x40, 0x0B, 0xF0, 0x09, 0x00, 0x12, 0x00, 0x02, 0x00 },  // 1440K
    { 0x02, 0x02, 0x01, 0x00, 0x02, 0xF0, 0x00, 0x80, 0x16, 0xF0, 0x09, 0x00, 0x24, 0x00, 0x02, 0x00 },  // 2880K
  };

  // sizes in bytes
  static const int SIZES[6] = {
    512 *  8 * 40 * 2, // 327680 (320K)
    512 *  9 * 40 * 2, // 368640 (360K)
    512 *  9 * 80 * 2, // 737280 (720K)
    512 * 15 * 80 * 2, // 1228800 (1200K)
    512 * 18 * 80 * 2, // 1474560 (1440K)
    512 * 36 * 80 * 2, // 2949120 (2880K)
  };

  // FAT
  static const struct {
    uint8_t id;
    uint8_t fat1size;  // in 512 blocks
    uint8_t fat2size;  // in 512 blocks
  } FAT[6] = {
    { 0xff, 1,  8 },   // 320K
    { 0xfd, 2,  9 },   // 360K
    { 0xf9, 3, 10 },   // 720K
    { 0xf9, 8, 22 },   // 1200K
    { 0xf0, 9, 23 },   // 1440K
    { 0xf0, 9, 24 },   // 2880K
  };

  auto file = FileBrowser(directory).openFile(filename, "wb");

  if (file) {

    auto sectbuf = (uint8_t*) SOC_EXTRAM_DATA_LOW; // use PSRAM as buffer

    // initial fill with all 0xf6 up to entire size
    memset(sectbuf, 0xf6, 512);
    fseek(file, 0, SEEK_SET);

    ibox->progressBox("", "Abort", true, 380, [&](fabgl::ProgressForm * form) {
      for (int i = 0; i < SIZES[diskType]; i += 512) {
        fwrite(sectbuf, 1, 512, file);
        if (!form->update((int)((double)i / SIZES[diskType] * 100), "Formatting %s (%d / %d bytes)", filename, i, SIZES[diskType]))
          break;
      }
    });

    // offset 0x00 (512 bytes) : write boot sector
    fseek(file, 0, SEEK_SET);
    fwrite(BOOTSECTOR[diskType], 1, 512, file);

    // offset 0x0c (16 bytes) : disk geometry
    fseek(file, 0x0c, SEEK_SET);
    fwrite(GEOM[diskType], 1, 16, file);

    // offset 0x27 (4 bytes) : Volume Serial Number
    const uint32_t volSN = rand();
    fseek(file, 0x27, SEEK_SET);
    fwrite(&volSN, 1, 4, file);

    // FAT
    memset(sectbuf, 0x00, 512);
    fseek(file, 0x200, SEEK_SET);
    for (int i = 0; i < (FAT[diskType].fat1size + FAT[diskType].fat2size); ++i)
      fwrite(sectbuf, 1, 512, file);
    sectbuf[0] = FAT[diskType].id;
    sectbuf[1] = 0xff;
    sectbuf[2] = 0xff;
    fseek(file, 0x200, SEEK_SET);
    fwrite(sectbuf, 1, 3, file);
    fseek(file, 0x200 + FAT[diskType].fat1size * 512, SEEK_SET);
    fwrite(sectbuf, 1, 3, file);

    fclose(file);
  }

  return file != nullptr;
}
