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


#include "fabgl.h"
#include "fabui.h"

#include <WiFi.h>



fabgl::VGA16Controller DisplayController;
fabgl::PS2Controller   PS2Controller;





struct TestApp : public uiApp {

  uiFrame *        frame;
  uiButton *       soundButton;
  SoundGenerator * soundGenerator;
  uiTimerHandle    soundTimer;
  uiButton *       sdButton;
  uiLabel *        sdResultLabel;
  uiComboBox *     gpioInComboBox;
  uiButton *       gpioInState;
  uiComboBox *     gpioOutComboBox;
  uiButton *       gpioOutState;
  uiTimerHandle    gpioTimer;
  uiButton *       wifiButton;
  uiLabel *        wifiResultLabel;

  int              gpioIn, gpioOut;


  ~TestApp() {
    delete soundGenerator;
  }


  void init() {

    rootWindow()->frameStyle().backgroundColor = Color::Black;
    frame = new uiFrame(rootWindow(), "Hardware test", UIWINDOW_PARENTCENTER, Size(600, 350));

    int y = 20;

    // colors
    new uiLabel(frame, "COLORS TEST:", Point(10, y));
    y += 20;
    static const char * COLORSL[8] = { "Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White" };
    static const char * COLORSH[8] = { "B. Black", "B. Red", "B. Green", "B. Yellow", "B. Blue", "B. Magenta", "B. Cyan", "B. White" };
    for (int i = 0; i < 8; ++i) {
      int left = 60;
      int hspc = 60;
      int vspc = 30;
      new uiLabel(frame, COLORSL[i], Point(left + i * hspc, y));
      new uiColorBox(frame, Point(left + i * hspc, y + 15), Size(50, 14), (Color)i);
      new uiLabel(frame, COLORSH[i], Point(left + i * hspc, y + vspc));
      new uiColorBox(frame, Point(left + i * hspc, y + 15 + vspc), Size(50, 14), (Color)(i+ 8));
    }

    y += 80;

    // keyboard test
    new uiLabel(frame, "KEYBOARD TEST:", Point(10, y));
    new uiTextEdit(frame, "Please type something", Point(120, y - 3), Size(450, 20));

    y += 40;

    // sound test
    soundGenerator = new SoundGenerator;
    new uiLabel(frame, "SOUND TEST:", Point(10, y));
    soundButton = new uiButton(frame, "Start", Point(120, y - 3), Size(80, 20), uiButtonKind::Switch);
    soundButton->onChange = [&]() {
      if (soundButton->down()) {
        soundButton->setText("Stop");
        soundTimer = setTimer(frame, 250);
      } else {
        soundButton->setText("Start");
        killTimer(soundTimer);
      }
    };

    y += 40;

    // SD Card test
    new uiLabel(frame, "SDCARD TEST:", Point(10, y));
    sdButton = new uiButton(frame, "Start", Point(120, y - 3), Size(80, 20));
    sdButton->onClick = [&]() {
      testSD();
    };
    sdResultLabel = new uiLabel(frame, "", Point(210, y));

    y += 44;

    // GPIO test
    static const int GPIOS_IN[] = { 0, 1, 2, 3, 12, 13, 14, 16, 17, 18, 34, 35, 36, 39 };
    static const int GPIOS_OUT[] = { 0, 1, 2, 3, 12, 13, 14, 16, 17, 18 };
    new uiLabel(frame, "GPIO TEST:", Point(10, y));

    new uiLabel(frame, "In", Point(120, y - 18));
    gpioInComboBox = new uiComboBox(frame, Point(120, y - 3), Size(34, 22), 60);
    for (int i = 0; i < sizeof(GPIOS_IN) / sizeof(int); ++i)
      gpioInComboBox->items().appendFmt("%d", GPIOS_IN[i]);
    gpioInComboBox->onChange = [&]() {
      gpioIn = gpioInComboBox->selectedItem() > -1 ? GPIOS_IN[gpioInComboBox->selectedItem()] : GPIOS_IN[10];
      pinMode(gpioIn, INPUT);
    };
    gpioIn = GPIOS_IN[10];
    pinMode(gpioIn, INPUT);
    gpioInComboBox->selectItem(10);
    new uiLabel(frame, "State", Point(162, y - 18));
    gpioInState = new uiButton(frame, "", Point(162, y - 3), Size(25, 22), uiButtonKind::Switch);

    new uiLabel(frame, "Out", Point(250, y - 18));
    gpioOutComboBox = new uiComboBox(frame, Point(250, y - 3), Size(34, 22), 60);
    for (int i = 0; i < sizeof(GPIOS_OUT) / sizeof(int); ++i)
      gpioOutComboBox->items().appendFmt("%d", GPIOS_OUT[i]);
    gpioOutComboBox->onChange = [&]() {
      gpioOut = gpioOutComboBox->selectedItem() > -1 ? GPIOS_OUT[gpioOutComboBox->selectedItem()] : GPIOS_OUT[2];
      pinMode(gpioOut, OUTPUT);
      updateGPIOOutState();
    };
    gpioOut = GPIOS_OUT[2];
    pinMode(gpioOut, OUTPUT);
    gpioOutComboBox->selectItem(2);
    new uiLabel(frame, "State", Point(292, y - 18));
    gpioOutState = new uiButton(frame, "", Point(292, y - 3), Size(25, 22), uiButtonKind::Switch);
    gpioOutState->setDown(false);
    gpioOutState->onChange = [&]() {
      updateGPIOOutState();
    };
    updateGPIOOutState();

    gpioTimer = setTimer(frame, 100);

    y += 40;

    // wifi
    new uiLabel(frame, "WIFI TEST:", Point(10, y));
    wifiButton = new uiButton(frame, "Start", Point(120, y - 3), Size(80, 20));
    wifiButton->onClick = [&]() {
      testWifi();
    };
    wifiResultLabel = new uiLabel(frame, "", Point(210, y));


    // timer
    frame->onTimer = [&](uiTimerHandle h) {
      if (h == soundTimer) {
        soundGenerator->playSound(SineWaveformGenerator(), random(100, 3000), 230, 100);
      }
      if (h == gpioTimer) {
        updateGPIOInState();
      }
    };


    new uiLabel(frame, "FabGL - Copyright 2019-2021 by Fabrizio Di Vittorio", Point(175, 313));
    new uiLabel(frame, "WWW.FABGL.COM", Point(260, 330));

  }


  void updateGPIOOutState() {
    if (gpioOutState->down()) {
      gpioOutState->setText("1");
      digitalWrite(gpioOut, HIGH);
    } else {
      gpioOutState->setText("0");
      digitalWrite(gpioOut, LOW);
    }
  }

  void updateGPIOInState() {
    if (digitalRead(gpioIn) == HIGH) {
      gpioInState->setText("1");
    } else {
      gpioInState->setText("0");
    }
    gpioInState->repaint();
  }


  void testSD() {
    // mount test
    FileBrowser fb;
    fb.unmountSDCard();
    bool r = fb.mountSDCard(false, "/SD");
    if (!r) {
      sdResultLabel->labelStyle().textColor = Color::BrightRed;
      sdResultLabel->setText("Mount Failed!");
      return;
    }
    // write test
    fb.setDirectory("/SD");
    constexpr int size = 16384;
    bool ok = true;
    auto fname = fb.createTempFilename();
    auto f = fopen(fname, "wb");
    if (f) {
      for (int i = 0; i < size; ++i)
        if (fputc(i & 0xff, f) != (i & 0xff)) {
          ok = false;
          break;
        }
      fclose(f);
    }
    if (!f || !ok) {
      free(fname);
      sdResultLabel->labelStyle().textColor = Color::BrightRed;
      sdResultLabel->setText("Write Failed!");
      return;
    }
    // read test
    f = fopen(fname, "rb");
    if (f) {
      for (int i = 0; i < size; ++i)
        if (fgetc(f) != (i & 0xff)) {
          ok = false;
          break;
        }
      fclose(f);
    }
    free(fname);
    if (!f || !ok) {
      sdResultLabel->labelStyle().textColor = Color::BrightRed;
      sdResultLabel->setText("Read Failed!");
      return;
    }

    sdResultLabel->labelStyle().textColor = Color::Green;
    sdResultLabel->setText("Success!");
  }

  void testWifi() {
    auto r = WiFi.scanNetworks(false, true);
    if (r == WIFI_SCAN_FAILED) {
      wifiResultLabel->labelStyle().textColor = Color::BrightRed;
      wifiResultLabel->setText("Wifi Scan Failed!");
    } else if (r == 0) {
      wifiResultLabel->labelStyle().textColor = Color::Yellow;
      wifiResultLabel->setText("Ok, No Network Found!");
    } else {
      wifiResultLabel->labelStyle().textColor = Color::Green;
      wifiResultLabel->setTextFmt("Ok, Found %d Networks", r);
    }
    WiFi.scanDelete();
  }

} app;



void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  DisplayController.queueSize = 350;  // trade UI speed using less RAM and allow both WiFi and SD Card FS
  DisplayController.begin();
  DisplayController.setResolution(VESA_640x480_75Hz, 640, 360);
}




void loop()
{
  app.runAsync(&DisplayController);
  vTaskDelete(NULL);
}
