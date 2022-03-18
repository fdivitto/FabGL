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


#include <WiFi.h>

#include "fabgl.h"
#include "inputbox.h"
#include "fonts/font_sanserif_8x16.h"


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY
}


void loop()
{
  // initializes input box
  InputBox ib;
  ib.begin(VGA_640x480_60Hz, 640, 350);
  ib.setBackgroundColor(RGB888(0, 0, 0));

  ib.onPaint = [&](Canvas * canvas) {
    canvas->selectFont(&fabgl::FONT_SANSERIF_8x16);
    canvas->setPenColor(RGB888(255, 255, 0));
    canvas->drawText(85, 5, "InputBox Demo - www.fabgl.com - by Fabrizio Di Vittorio");
  };

  // setup automatic OK after 10 seconds
  ib.setAutoOK(10);


  ib.message("Welcome!", "Welcome to FabGL InputBox demo!");


  ////////////////////////////////////////////////////
  // Example of progress bar
  InputResult r = ib.progressBox("Example of Progress Bar", "Abort", true, 200, [&](fabgl::ProgressForm * form) {
    for (int i = 0; i <= 100; ++i) {
      if (!form->update(i, "Index is %d/100", i))
        break;
      delay(40);
    }
    delay(800);
  });
  if (r == InputResult::Cancel)
    ib.message("", "Operation Aborted");


  ////////////////////////////////////////////////////
  // Example of simple menu (items from separated strings)
  int s = ib.menu("Simple Menu", "Click on one item", "Item number zero;Item number one;Item number two;Item number three");
  ib.messageFmt("", nullptr, "OK", "You have selected item %d", s);


  ////////////////////////////////////////////////////
  // Example of simple menu (items from StringList)
  fabgl::StringList list;
  list.append("Option Zero");
  list.append("Option One");
  list.append("Option Two");
  list.append("Option Three");
  list.select(1, true);
  s = ib.menu("Menu", "Click on an item", &list);
  ib.messageFmt("", nullptr, "OK", "You have selected item %d", s);


  ////////////////////////////////////////////////////
  // Example of options selection box with OK button (items from separated strings)
  list.clear();
  list.append("Item 0");
  list.append("Item 1");
  list.append("Item 2");
  for (bool loop = true; loop; ) {
    ib.setupButton(0, "Add");
    ib.setupButton(1, "Remove");
    ib.setupButton(2, "Options", "Edit;Restore;Advanced", 50);
    ib.setMinButtonsWidth(60);
    auto r = ib.select("Items selection", "Select an item", &list, "Cancel", "OK");
    switch (r) {
      // OK button
      case InputResult::Enter:
        ib.messageFmt(nullptr, nullptr, "OK", "You have selected item %d", list.getFirstSelected());
        loop = false;
        break;
      // add new item button
      case InputResult::ButtonExt0:
      {
        char value[32] = "";
        if (ib.textInput("New Item", "Please enter item value", value, 31) == InputResult::Enter) {
          list.takeStrings();
          list.append(value);
        }
        break;
      }
      // remove item button
      case InputResult::ButtonExt1:
        if (list.getFirstSelected() > -1 && ib.message("Please confirm", "Remove Item?", "No", "Yes") == InputResult::Enter)
          list.remove(list.getFirstSelected());
        break;
      // button with subitems (Edit / Restore / Advanced)
      case InputResult::ButtonExt2:
        switch (ib.selectedSubItem()) {
          // Edit sub-button
          case 0:
            ib.message(nullptr, "Edit - not implented!");
            break;
          // Restore
          case 1:
            ib.message(nullptr, "Restore - not implented!");
            break;
          // Advanced
          case 2:
            ib.message(nullptr, "Advanced - not implented!");
            break;
        }
        break;
      // cancel
      default:
        loop = false;
        break;
    }
  }

  ////////////////////////////////////////////////////
  // Example of options selection box with OK button (items from StringList)
  fabgl::StringList quiz;
  quiz.append("A) Connecting to the network");
  quiz.append("B) Creating new user accounts");
  quiz.append("C) Providing power to the computer");
  quiz.append("D) Connecting smartphones and other peripherals");
  if (ib.select("QUIZ", "What is an Ethernet port used for?", &quiz, nullptr, "Submit") == InputResult::Enter) {
    ib.message("Result", quiz.selected(0) ? "That's right, great job!" : "Sorry, that's not correct");
  }


  ////////////////////////////////////////////////////
  // Example of text input box
  char name[32] = "";
  if (ib.textInput("Asking name", "What's your name?", name, 31) == InputResult::Enter)
    ib.messageFmt("", nullptr, "OK", "Nice to meet you %s!", name);


  ////////////////////////////////////////////////////
  // File Browser
  char const * path = nullptr;
  if (FileBrowser::mountSDCard(false, "/SD"))
    path = "/SD";
  else if (FileBrowser::mountSPIFFS(true, "/Flash"))
    path = "/Flash";
  if (path) {
    ib.folderBrowser("Folder Browser", path);
  }


  ////////////////////////////////////////////////////
  // File Select
  if (path) {
    char filename[16] = "";
    char directory[32];
    strcpy(directory, path);
    if (ib.fileSelector("File Select", "Filename: ", directory, sizeof(directory) - 1, filename, sizeof(filename) - 1) == InputResult::Enter) {
      ib.messageFmt("", nullptr, "OK", "Folder = %s, File = %s", directory, filename);
    }
    FileBrowser::unmountSDCard();
    FileBrowser::unmountSPIFFS();
  }


  ////////////////////////////////////////////////////
  // Example WiFi connection wizard
  if (ib.message("WiFi Configuration", "Configure WiFi?", "No", "Yes") == InputResult::Enter) {

    // yes, scan for networks showing a progress dialog box
    int networksCount = 0;
    ib.progressBox("", nullptr, false, 200, [&](fabgl::ProgressForm * form) {
      form->update(0, "Scanning WiFi networks...");
      networksCount = WiFi.scanNetworks();
    });

    // are there available WiFi?
    if (networksCount > 0) {

      // yes, show a selectable list
      fabgl::StringList list;
      for (int i = 0; i < networksCount; ++i)
        list.appendFmt("%s (%d dBm)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      int s = ib.menu("WiFi Configuration", "Please select a WiFi network", &list);

      // user selected something?
      if (s > -1) {

        // yes, ask for WiFi password
        char psw[32] = "";
        if (ib.textInput("WiFi Configuration", "Insert WiFi password", psw, 31, "Cancel", "OK", true) == InputResult::Enter) {

          // user pressed OK, connect to WiFi...
          char SSID[50];
          strcpy(SSID, WiFi.SSID(s).c_str());
          bool connected = false;
          ib.progressBox("", nullptr, true, 200, [&](fabgl::ProgressForm * form) {
            WiFi.begin(SSID, psw);
            for (int i = 0; i < 32 && WiFi.status() != WL_CONNECTED; ++i) {
              if (!form->update(i * 100 / 32, "Connecting to %s...", SSID))
                break;
              delay(500);
              if (i == 16)
                WiFi.reconnect();
            }
            connected = (WiFi.status() == WL_CONNECTED);
          });

          // show to user the connection state
          ib.message("", connected ? "Connection succeeded" : "Connection failed");

        }
      }
    } else {
      // there is no WiFi
      ib.message("", "No WiFi network found!");
    }
    WiFi.scanDelete();
  }

  ib.message("Restart", "Ok, press OK to restart!");


  // finalizes input box
  ib.end();

}






