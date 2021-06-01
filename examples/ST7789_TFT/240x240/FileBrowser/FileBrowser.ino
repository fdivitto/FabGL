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


/*
 * TFT Display signals:
 *   SCK  => GPIO 18
 *   MOSI => GPIO 23
 *   D/C  => GPIO 22
 *   RESX => GPIO 21
 *
 *
 * Optional SD Card connection:
 *   MISO => GPIO 16
 *   MOSI => GPIO 17
 *   CLK  => GPIO 14
 *   CS   => GPIO 13
 *
 * To change above assignment fill other paramaters of FileBrowser::mountSDCard().
 */



#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <stdio.h>

#include "fabgl.h"
#include "fabui.h"



#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_DC     22
#define TFT_RESET  21
#define TFT_SPIBUS VSPI_HOST // HSPI used by SD Card



#define FORMAT_ON_FAIL     true

#define SPIFFS_MOUNT_PATH  "/flash"
#define SDCARD_MOUNT_PATH  "/SD"


Preferences preferences;

fabgl::ST7789Controller DisplayController;
fabgl::PS2Controller    PS2Controller;




class MyApp : public uiApp {

  uiLabel *       WiFiStatusLbl;
  uiFileBrowser * fileBrowser;
  uiLabel *       freeSpaceLbl;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB888(255, 255, 255);

    // file browser
    fileBrowser = new uiFileBrowser(rootWindow(), Point(10, 5), Size(130, 170));
    fileBrowser->setDirectory("/");
    fileBrowser->onChange = [&]() {
      updateFreeSpaceLabel();
    };

    // create directory button
    auto createDirBtn = new uiButton(rootWindow(), "Create Dir", Point(150, 5), Size(80, 20));
    createDirBtn->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char dirname[MAXSTRLEN + 1] = "";
      if (inputBox("Create Directory", "Name", dirname, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().makeDirectory(dirname);
        updateBrowser();
      }
    };

    // create empty file button
    auto createEmptyFile = new uiButton(rootWindow(), "Create File", Point(150, 30), Size(80, 20));
    createEmptyFile->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char filename[MAXSTRLEN + 1] = "";
      if (inputBox("Create Empty File", "Name", filename, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        int len = fileBrowser->content().getFullPath(filename);
        char fullpath[len];
        fileBrowser->content().getFullPath(filename, fullpath, len);
        FILE * f = fopen(fullpath, "wb");
        fclose(f);
        updateBrowser();
      }
    };

    // rename button
    auto renameBtn = new uiButton(rootWindow(), "Rename", Point(150, 55), Size(80, 20));
    renameBtn->onClick = [&]() {
      int maxlen = fabgl::imax(16, strlen(fileBrowser->filename()));
      char filename[maxlen + 1];
      strcpy(filename, fileBrowser->filename());
      if (inputBox("Rename File", "New name", filename, maxlen, "Rename", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().rename(fileBrowser->filename(), filename);
        updateBrowser();
      }
    };

    // delete button
    auto deleteBtn = new uiButton(rootWindow(), "Delete", Point(150, 80), Size(80, 20));
    deleteBtn->onClick = [&]() {
      if (messageBox("Delete file/directory", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().remove( fileBrowser->filename() );
        updateBrowser();
      }
    };

    // format button
    auto formatBtn = new uiButton(rootWindow(), "Format", Point(150, 105), Size(80, 20));
    formatBtn->onClick = [&]() {
      if (messageBox("Format sdcard", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        FileBrowser::format(fileBrowser->content().getCurrentDriveType(), 0);
        updateBrowser();
      }
    };

    // setup wifi button
    auto setupWifiBtn = new uiButton(rootWindow(), "Setup WiFi", Point(150, 130), Size(80, 20));
    setupWifiBtn->onClick = [=]() {
      char SSID[32] = "";
      char psw[32]  = "";
      if (inputBox("WiFi Connect", "Network Name", SSID, sizeof(SSID), "OK", "Cancel") == uiMessageBoxResult::Button1 &&
          inputBox("WiFi Connect", "Password", psw, sizeof(psw), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        preferences.putString("SSID", SSID);
        preferences.putString("WiFiPsw", psw);
        connectWiFi();
      }
    };

    // download file button
    auto downloadBtn = new uiButton(rootWindow(), "Download", Point(150, 155), Size(80, 20));
    downloadBtn->onClick = [=]() {
      char URL[128] = "http://";
      if (inputBox("Download File", "URL", URL, sizeof(URL), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        download(URL);
        updateFreeSpaceLabel();
      }
    };

    // free space label
    freeSpaceLbl = new uiLabel(rootWindow(), "", Point(10, 200));
    updateFreeSpaceLabel();

    // wifi status label
    WiFiStatusLbl = new uiLabel(rootWindow(), "", Point(10, 215));
    connectWiFi();

    setFocusedWindow(fileBrowser);
  }

  void updateBrowser()
  {
    fileBrowser->update();
    updateFreeSpaceLabel();
  }

  // connect to wifi using SSID and PSW from Preferences
  void connectWiFi()
  {
    WiFiStatusLbl->setText("WiFi Not Connected");
    WiFiStatusLbl->labelStyle().textColor = RGB888(255, 0, 0);
    char SSID[32], psw[32];
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
            fwrite(buf, c, 1, f);
            if (len > 0)
              len -= c;
          }
        }

        fclose(f);

        updateBrowser();
      }
    }
  }

  // show used and free SD space
  void updateFreeSpaceLabel() {
    int64_t total, used;
    FileBrowser::getFSInfo(fileBrowser->content().getCurrentDriveType(), 0, &total, &used);
    freeSpaceLbl->setTextFmt("%lld KiB used, %lld KiB free", used / 1024, (total - used) / 1024);
    freeSpaceLbl->update();
  }

} app;


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  preferences.begin("FileBrowser", false);
  preferences.clear();

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  DisplayController.begin(TFT_SCK, TFT_MOSI, TFT_DC, TFT_RESET, -1, TFT_SPIBUS);
  DisplayController.setResolution(TFT_240x240);

  Canvas cv(&DisplayController);
  cv.clear();
  cv.drawText(50, 170, "Initializing...");
  cv.waitCompletion();

  FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH);
  FileBrowser::mountSPIFFS(FORMAT_ON_FAIL, SPIFFS_MOUNT_PATH);
}


void loop()
{
  app.run(&DisplayController);
}






