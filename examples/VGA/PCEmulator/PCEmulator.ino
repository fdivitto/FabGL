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


 /* Instructions:

    - to run this application you need an ESP32 with PSRAM installed and an SD-CARD slot (ie TTGO VGA32 v1.4)
    - open this with Arduino and make sure PSRAM is DISABLED
    - partition scheme must be: Huge App
    - compile and upload the sketch

 */


#pragma message "This sketch requires Tools->Partition Scheme = Huge APP"


#include "esp32-hal-psram.h"
extern "C" {
#include "esp_spiram.h"
}
#include "esp_sntp.h"

#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "fabgl.h"

#include "machine.h"


using fabgl::StringList;
using fabgl::imin;
using fabgl::imax;



struct DiskConf {
  char const * desc;
  char const * diskA_URL;
  char const * diskC_URL;
};

static const DiskConf diskConfs[] = {
  { "FreeDOS (A:)",                               "http://www.fabglib.org/downloads/A_freedos.img",  nullptr },
  { "FreeDOS (A:) + DOS Programming Tools (C:)",  "http://www.fabglib.org/downloads/A_freedos.img",  "http://www.fabglib.org/downloads/C_dosdev.img" },
  { "FreeDOS (A:) + Windows 3.0 Hercules (C:)",   "http://www.fabglib.org/downloads/A_freedos.img",  "http://www.fabglib.org/downloads/C_winherc.img" },
  { "FreeDOS (A:) + DOS Programs and Games (C:)", "http://www.fabglib.org/downloads/A_freedos.img",  "http://www.fabglib.org/downloads/C_dosprog.img" },
  { "MS-DOS 3.31 (A:)",                           "http://www.fabglib.org/downloads/A_MSDOS331.img", nullptr },
  { "Linux ELKS 0.4.0",                           "http://www.fabglib.org/downloads/A_ELK040.img",   nullptr },
  { "CP/M 86 + Turbo Pascal 3",                   "http://www.fabglib.org/downloads/A_CPM86.img",    nullptr },
};


Preferences preferences;
InputBox    ibox;
Machine     * machine;


// try to connected using saved parameters
bool tryToConnect()
{
  bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected) {
    char SSID[32] = "";
    char psw[32]  = "";
    if (preferences.getString("SSID", SSID, sizeof(SSID)) && preferences.getString("WiFiPsw", psw, sizeof(psw))) {
      ibox.progressBox("", "Abort", true, 200, [&](fabgl::ProgressApp * app) {
        WiFi.begin(SSID, psw);
        for (int i = 0; i < 32 && WiFi.status() != WL_CONNECTED; ++i) {
          if (!app->update(i * 100 / 32, "Connecting to %s...", SSID))
            break;
          delay(500);
          if (i == 16)
            WiFi.reconnect();
        }
        connected = (WiFi.status() == WL_CONNECTED);
      });
      // show to user the connection state
      if (!connected) {
        WiFi.disconnect();
        ibox.message("", "WiFi Connection failed!");
      }
    }
  }
  return connected;
}


bool checkWiFi()
{
  bool connected = tryToConnect();
  if (!connected) {

    // configure WiFi?
    if (ibox.message("WiFi Configuration", "Configure WiFi?", "No", "Yes") == InputResult::Enter) {

      // repeat until connected or until user cancels
      do {

        // yes, scan for networks showing a progress dialog box
        int networksCount = 0;
        ibox.progressBox("", nullptr, false, 200, [&](fabgl::ProgressApp * app) {
          app->update(0, "Scanning WiFi networks...");
          networksCount = WiFi.scanNetworks();
        });

        // are there available WiFi?
        if (networksCount > 0) {

          // yes, show a selectable list
          StringList list;
          for (int i = 0; i < networksCount; ++i)
            list.appendFmt("%s (%d dBm)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
          int s = ibox.menu("WiFi Configuration", "Please select a WiFi network", &list);

          // user selected something?
          if (s > -1) {
            // yes, ask for WiFi password
            char psw[32] = "";
            if (ibox.textInput("WiFi Configuration", "Insert WiFi password", psw, 31, "Cancel", "OK", true) == InputResult::Enter) {
              // user pressed OK, connect to WiFi...
              preferences.putString("SSID", WiFi.SSID(s).c_str());
              preferences.putString("WiFiPsw", psw);
              connected = tryToConnect();
              // show to user the connection state
              if (connected)
                ibox.message("", "Connection succeeded!");
            } else
              break;
          } else
            break;
        } else {
          // there is no WiFi
          ibox.message("", "No WiFi network found!");
          break;
        }
        WiFi.scanDelete();

      } while (!connected);

    }

  }

  return connected;
}


void updateDateTime()
{
  // Set timezone
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  if (checkWiFi()) {

    // we need time right now
    ibox.progressBox("", nullptr, true, 200, [&](fabgl::ProgressApp * app) {
      sntp_setoperatingmode(SNTP_OPMODE_POLL);
      sntp_setservername(0, (char*)"pool.ntp.org");
      sntp_init();
      for (int i = 0; i < 12 && sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED; ++i) {
        app->update(i * 100 / 12, "Getting date-time from SNTP...");
        delay(500);
      }
      sntp_stop();
    });

  } else {

    // set default time
    //printf("Setting default date/time\n");
    auto tm = (struct tm){ .tm_sec  = 0, .tm_min  = 0, .tm_hour = 8, .tm_mday = 14, .tm_mon  = 7, .tm_year = 84 };
    auto now = (timeval){ .tv_sec = mktime(&tm) };
    settimeofday(&now, nullptr);

  }
}


// download specified filename from URL
bool downloadURL(char const * URL, FILE * file)
{
  bool success = false;

  char const * filename = strrchr(URL, '/') + 1;

  ibox.progressBox("", "Abort", true, 380, [&](fabgl::ProgressApp * app) {
    app->update(0, "Preparing to download %s", filename);
    HTTPClient http;
    http.begin(URL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      if (file) {
        int tlen = http.getSize();
        int tlenK = tlen / 1024;
        int len = tlen;
        constexpr int BUFSIZE = 6000;
        uint8_t * buf = (uint8_t*) malloc(BUFSIZE);
        WiFiClient * stream = http.getStreamPtr();
        int dsize = 0;
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buf, imin(BUFSIZE, size));
            fwrite(buf, c, 1, file);
            dsize += c;
            if (len > 0)
              len -= c;
            if (!app->update(dsize / 1024 * 100 / tlenK, "Downloading %s (%.2f / %d MB)", filename, (double)dsize / 1048576.0, tlenK / 1024))
              break;
          }
        }
        free(buf);
        success = (len == 0 || (len == -1 && dsize > 0));
      }
    }
  });

  return success;
}



void setup()
{
  Serial.begin(115200); delay(500); printf("\n\n\nReset\n\n");// DEBUG ONLY

  disableCore0WDT();
  delay(100); // experienced crashes without this delay!
  disableCore1WDT();

  preferences.begin("PCEmulator", false);

  // uncomment to clear preferences
  //preferences.clear();

  ibox.begin(VGA_640x400_70Hz, 400, 300); // VGA_640x400_60Hz as alternative?
  ibox.setBackgroundColor(RGB888(0, 0, 0));

  // we need PSRAM for this app, but we will handle it manually, so please DO NOT enable PSRAM on your development env
  #ifdef BOARD_HAS_PSRAM
  ibox.message("Warning!", "Please disable PSRAM to improve performance!");
  #endif

  // note: we use just 2MB of PSRAM so the infamous PSRAM bug should not happen. But to avoid gcc compiler hack (-mfix-esp32-psram-cache-issue)
  // we enable PSRAM at runtime, otherwise the hack slows down CPU too much (PSRAM_HACK is no more required).
  if (esp_spiram_init() != ESP_OK)
    ibox.message("Error!", "This app requires a board with PSRAM!", nullptr, nullptr);

  #ifndef BOARD_HAS_PSRAM
  esp_spiram_init_cache();
  #endif

  if (!FileBrowser::mountSDCard(false, "/SD", 8))
    ibox.message("Error!", "This app requires a SD-CARD!", nullptr, nullptr);

  // uncomment to format SD!
  //FileBrowser::format(fabgl::DriveType::SDCard, 0);

  updateDateTime();

  // show a list of disks configurations
  StringList dconfs;
  for (int i = 0; i < sizeof(diskConfs) / sizeof(DiskConf); ++i)
    dconfs.append(diskConfs[i].desc);
  dconfs.select(preferences.getInt("dconf", 0), true);
  ibox.select("Disks Configurations", "Please select a disk configuration", &dconfs, nullptr, "OK", 8);
  int idx = imax(dconfs.getFirstSelected(), 0);
  preferences.putInt("dconf", idx);

  // load selected configuration
  FileBrowser fb;
  fb.setDirectory("/SD");
  bool disksExist = true;
  char const * filenameDiskA = nullptr;
  char const * filenameDiskC = nullptr;
  if (diskConfs[idx].diskA_URL) {
    filenameDiskA = strrchr(diskConfs[idx].diskA_URL, '/') + 1;
    disksExist = fb.exists(filenameDiskA, false);
    if (!disksExist) {
      // disk A doesn't exist, download
      auto file = fb.openFile(filenameDiskA, "wb");
      disksExist = downloadURL(diskConfs[idx].diskA_URL, file);
      fclose(file);
      if (!disksExist)
        fb.remove(filenameDiskA);
    }
  }
  if (diskConfs[idx].diskC_URL) {
    filenameDiskC = strrchr(diskConfs[idx].diskC_URL, '/') + 1;
    disksExist = disksExist && fb.exists(filenameDiskC, false);
    if (!disksExist) {
      // disk C doesn't exist, download
      auto file = fb.openFile(filenameDiskC, "wb");
      disksExist = downloadURL(diskConfs[idx].diskC_URL, file);
      fclose(file);
      if (!disksExist)
        fb.remove(filenameDiskC);
    }
  }

  if (!disksExist) {
    // unable to get disks
    ibox.message("Error!", "Unable to get system disks!");
    esp_restart();
  }

  ibox.end();


  machine = new Machine;
  machine->setDriveA(filenameDiskA);
  machine->setDriveC(filenameDiskC);
  machine->run();
}



#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
namespace fabgl {
  extern volatile uint64_t s_vgapalctrlcycles;
}
using fabgl::s_vgapalctrlcycles;
#endif



void loop()
{

#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  static uint32_t tcpu = 0, s1 = 0, count = 0;
  tcpu = machine->ticksCounter();
  s_vgapalctrlcycles = 0;
  s1 = fabgl::getCycleCount();
  delay(1000);
  printf("%d\tCPU: %d", count, machine->ticksCounter() - tcpu);
  printf("   Graph: %lld / %d   (%d%%)\n", s_vgapalctrlcycles, fabgl::getCycleCount() - s1, (int)((double)s_vgapalctrlcycles/240000000*100));
  ++count;
#else

  vTaskDelete(NULL);

#endif
}


