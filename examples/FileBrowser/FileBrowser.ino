/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
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


#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include <stdio.h>



#include "fabgl.h"
#include "fabui.h"



Preferences preferences;

fabgl::VGAController VGAController;
fabgl::PS2Controller PS2Controller;



void initSPIFFS()
{
  static bool initDone = false;
  if (!initDone) {
    // setup SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 4,
        .format_if_mount_failed = true
    };
    fabgl::suspendInterrupts();
    esp_vfs_spiffs_register(&conf);
    fabgl::resumeInterrupts();
  }
}


class MyApp : public uiApp {

  uiLabel *       WiFiStatusLbl;
  uiFileBrowser * fileBrowser;
  uiLabel *       freeSpaceLbl;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB888(0, 0, 64);

    auto frame = new uiFrame(rootWindow(), "FileBrowser Example", Point(15, 10), Size(375, 275));
    frame->frameProps().hasCloseButton = false;

    // file browser
    fileBrowser = new uiFileBrowser(frame, Point(10, 25), Size(140, 200));
    fileBrowser->setDirectory("/spiffs");

    // create directory button
    auto createDirBtn = new uiButton(frame, "Create Dir", Point(160, 25), Size(90, 20));
    createDirBtn->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char dirname[MAXSTRLEN + 1] = "";
      if (inputBox("Create Directory", "Name", dirname, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().makeDirectory(dirname);
        fileBrowser->update();
        updateFreeSpaceLabel();
      }
    };

    // create empty file button
    auto createEmptyFile = new uiButton(frame, "Create Empty File", Point(160, 50), Size(90, 20));
    createEmptyFile->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char filename[MAXSTRLEN + 1] = "";
      if (inputBox("Create Empty File", "Name", filename, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        int len = fileBrowser->content().getFullPath(filename);
        char fullpath[len];
        fileBrowser->content().getFullPath(filename, fullpath, len);
        fabgl::suspendInterrupts();
        FILE * f = fopen(fullpath, "wb");
        fclose(f);
        fabgl::resumeInterrupts();
        fileBrowser->update();
        updateFreeSpaceLabel();
      }
    };

    // rename button
    auto renameBtn = new uiButton(frame, "Rename", Point(160, 75), Size(90, 20));
    renameBtn->onClick = [&]() {
      int maxlen = fabgl::imax(16, strlen(fileBrowser->filename()));
      char filename[maxlen + 1];
      strcpy(filename, fileBrowser->filename());
      if (inputBox("Rename File", "New name", filename, maxlen, "Rename", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().rename(fileBrowser->filename(), filename);
        fileBrowser->update();
      }
    };

    // delete button
    auto deleteBtn = new uiButton(frame, "Delete", Point(160, 100), Size(90, 20));
    deleteBtn->onClick = [&]() {
      if (messageBox("Delete file/directory", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().remove( fileBrowser->filename() );
        fileBrowser->update();
        updateFreeSpaceLabel();
      }
    };

    // format button
    auto formatBtn = new uiButton(frame, "Format", Point(160, 125), Size(90, 20));
    formatBtn->onClick = [&]() {
      if (messageBox("Format SPIFFS", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        fabgl::suspendInterrupts();
        esp_task_wdt_init(45, false);
        esp_spiffs_format(nullptr);
        fabgl::resumeInterrupts();
        fileBrowser->update();
        updateFreeSpaceLabel();
      }
    };

    // setup wifi button
    auto setupWifiBtn = new uiButton(frame, "Setup WiFi", Point(260, 25), Size(90, 20));
    setupWifiBtn->onClick = [=]() {
      char SSID[32] = "";
      char psw[32]  = "";
      if (inputBox("WiFi Connect", "Network Name", SSID, sizeof(SSID), "OK", "Cancel") == uiMessageBoxResult::Button1 &&
          inputBox("WiFi Connect", "Password", psw, sizeof(psw), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        fabgl::suspendInterrupts();
        preferences.putString("SSID", SSID);
        preferences.putString("WiFiPsw", psw);
        fabgl::resumeInterrupts();
        connectWiFi();
      }
    };

    // download file button
    auto downloadBtn = new uiButton(frame, "Download", Point(260, 50), Size(90, 20));
    downloadBtn->onClick = [=]() {
      char URL[128] = "http://";
      if (inputBox("Download File", "URL", URL, sizeof(URL), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        download(URL);
        updateFreeSpaceLabel();
      }
    };

    // free space label
    freeSpaceLbl = new uiLabel(frame, "", Point(10, 235));
    updateFreeSpaceLabel();

    // wifi status label
    WiFiStatusLbl = new uiLabel(frame, "", Point(10, 255));
    connectWiFi();

    setFocusedWindow(fileBrowser);
  }

  // connect to wifi using SSID and PSW from Preferences
  void connectWiFi()
  {
    WiFiStatusLbl->setText("WiFi Not Connected");
    WiFiStatusLbl->labelStyle().textColor = RGB888(255, 0, 0);
    char SSID[32], psw[32];
    fabgl::suspendInterrupts();
    if (preferences.getString("SSID", SSID, sizeof(SSID)) && preferences.getString("WiFiPsw", psw, sizeof(psw))) {
      WiFi.begin(SSID, psw);
      for (int i = 0; i < 16 && WiFi.status() != WL_CONNECTED; ++i) {
        WiFi.reconnect();
        delay(1000);
      }
      if (WiFi.status() == WL_CONNECTED) {
        WiFiStatusLbl->setTextFmt("Connected to %s, IP is %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        WiFiStatusLbl->labelStyle().textColor = RGB888(0, 128, 0);
      }
    }
    fabgl::resumeInterrupts();
    WiFiStatusLbl->update();
  }

  // download from URL. Filename is the last part of the URL
  void download(char const * URL) {
    char const * slashPos = strrchr(URL, '/');
    if (slashPos) {
      char filename[slashPos - URL + 1];
      strcpy(filename, slashPos + 1);

      HTTPClient http;
      http.begin(URL);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {

        int fullpathLen = fileBrowser->content().getFullPath(filename);
        char fullpath[fullpathLen];
        fileBrowser->content().getFullPath(filename, fullpath, fullpathLen);
        FILE * f = fopen(fullpath, "wb");

        int len = http.getSize();
        uint8_t buf[128] = { 0 };
        WiFiClient * stream = http.getStreamPtr();
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buf, fabgl::imin(sizeof(buf), size));
            fabgl::suspendInterrupts();
            fwrite(buf, c, 1, f);
            fabgl::resumeInterrupts();
            if (len > 0)
              len -= c;
          }
        }

        fabgl::suspendInterrupts();
        fclose(f);
        fabgl::resumeInterrupts();

        fileBrowser->update();
      }
    }
  }

  // show used and free SPIFFS space
  void updateFreeSpaceLabel() {
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    freeSpaceLbl->setTextFmt("%d bytes used, %d bytes free", used, total - used);
    freeSpaceLbl->update();
  }

} app;


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  preferences.begin("FileBrowser", false);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  VGAController.begin();

  // maintain LOW!!! otherwise there isn't enough memory for WiFi!!!
  VGAController.setResolution(VGA_400x300_60Hz);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(-6, 0);

  Canvas cv(&VGAController);
  cv.clear();
  cv.drawText(50, 170, "Initializing SPIFFS...");
  cv.waitCompletion();
  initSPIFFS();
  cv.clear();
  cv.drawText(50, 170, "Connecting WiFi...");
  cv.waitCompletion();
}


void loop()
{
  app.run(&VGAController);
}






