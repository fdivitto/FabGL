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


 /*

  MCP23S17 pins:
    MISO = 35   (2 on TTGO VGA32)
    MOSI = 12
    CLK  = 14
    CS   = none (13 on FabGL development board, shared and inverted with MicroSD)
    INT  = 36 (interrupt pin connected to INTA of MCP23S17)
 */


#include "fabgl.h"
#include "fabui.h"
#include "devdrivers/MCP23S17.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_event.h"



fabgl::VGA16Controller DisplayController;
fabgl::PS2Controller   PS2Controller;


#define MCP_INT 36


struct TestApp : public uiApp {

  uiFrame *        frame;
  uiButton *       soundButton;
  SoundGenerator * soundGenerator;
  uiTimerHandle    soundTimer;
  uiButton *       sdButton;
  uiStaticLabel *  sdResultLabel;
  uiComboBox *     gpioInComboBox;
  uiButton *       gpioInState;
  uiComboBox *     gpioOutComboBox;
  uiButton *       gpioOutState;
  uiTimerHandle    gpioTimer;
  uiButton *       wifiButton;
  uiLabel *        wifiResultLabel;
  uiLabel *        extgpioLabel[16];
  uiButton *       extgpioPlay;
  uiLabel *        extIntLabel;

  int              gpioIn, gpioOut;
  bool             gpioInPrevState;
  int              intOffDelay;

  fabgl::MCP23S17  mcp;


  ~TestApp() {
    delete soundGenerator;
  }


  void init() {

    rootWindow()->frameStyle().backgroundColor = Color::Cyan;
    frame = new uiFrame(rootWindow(), "Hardware test", UIWINDOW_PARENTCENTER, Size(555, 380));

    int y = 20;

    // colors
    static const char * COLORSL[8] = { "Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White" };
    static const char * COLORSH[8] = { "B. Black", "B. Red", "B. Green", "B. Yellow", "B. Blue", "B. Magenta", "B. Cyan", "B. White" };
    for (int i = 0; i < 8; ++i) {
      int left = 60;
      int hspc = 60;
      int vspc = 30;
      new uiStaticLabel(frame, COLORSL[i], Point(left + i * hspc, y));
      new uiColorBox(frame, Point(left + i * hspc, y + 15), Size(50, 14), (Color)i);
      new uiStaticLabel(frame, COLORSH[i], Point(left + i * hspc, y + vspc));
      new uiColorBox(frame, Point(left + i * hspc, y + 15 + vspc), Size(50, 14), (Color)(i+ 8));
    }

    y += 80;

    // keyboard test
    new uiStaticLabel(frame, "KEYBOARD TEST:", Point(10, y));
    new uiTextEdit(frame, "Please type something", Point(110, y - 3), Size(435, 20));

    y += 40;

    // sound test
    soundGenerator = new SoundGenerator;
    new uiStaticLabel(frame, "SOUND TEST:", Point(10, y));
    soundButton = new uiButton(frame, "Start", Point(110, y - 3), Size(80, 20), uiButtonKind::Switch);
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
    new uiStaticLabel(frame, "SDCARD TEST:", Point(10, y));
    sdButton = new uiButton(frame, "Start", Point(110, y - 3), Size(80, 20));
    sdButton->onClick = [&]() {
      testSD();
    };
    sdResultLabel = new uiStaticLabel(frame, "", Point(210, y));

    y += 44;

    // internal GPIOs test
    static const int GPIOS_IN[] = { 0, 1, 2, 3, 12, 13, 14, 16, 17, 18, 34, 35, 36, 39 };
    static const int GPIOS_OUT[] = { 0, 1, 2, 3, 12, 13, 14, 16, 17, 18 };
    new uiStaticLabel(frame, "GPIO TEST:", Point(10, y));

    new uiStaticLabel(frame, "In", Point(110, y - 18));
    gpioInComboBox = new uiComboBox(frame, Point(110, y - 3), Size(34, 22), 60);
    for (int i = 0; i < sizeof(GPIOS_IN) / sizeof(int); ++i)
      gpioInComboBox->items().appendFmt("%d", GPIOS_IN[i]);
    gpioInComboBox->onChange = [&]() {
      gpioIn = gpioInComboBox->selectedItem() > -1 ? GPIOS_IN[gpioInComboBox->selectedItem()] : GPIOS_IN[10];
      pinMode(gpioIn, INPUT);
    };
    gpioIn = GPIOS_IN[10];
    pinMode(gpioIn, INPUT);
    gpioInComboBox->selectItem(10);
    new uiStaticLabel(frame, "State", Point(152, y - 18));
    gpioInState = new uiButton(frame, "", Point(152, y - 3), Size(25, 22), uiButtonKind::Switch);
    gpioInPrevState = 0;

    new uiStaticLabel(frame, "Out", Point(220, y - 18));
    gpioOutComboBox = new uiComboBox(frame, Point(220, y - 3), Size(34, 22), 60);
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
    new uiStaticLabel(frame, "State", Point(262, y - 18));
    gpioOutState = new uiButton(frame, "", Point(262, y - 3), Size(25, 22), uiButtonKind::Switch);
    gpioOutState->setDown(false);
    gpioOutState->onChange = [&]() {
      updateGPIOOutState();
    };
    updateGPIOOutState();

    gpioTimer = setTimer(frame, 100);

    y += 44;


    // external GPIOs test

    if (initMCP()) {
      new uiStaticLabel(frame, "EXT GPIO TEST:", Point(10, y));
      new uiStaticLabel(frame, "Outputs", Point(110, y - 13));
      new uiStaticLabel(frame, "Inputs", Point(355, y - 13));
      constexpr int w = 20;
      constexpr int h = 22;
      for (int i = 0; i < 16; ++i) {
        extgpioLabel[i] = new uiLabel(frame, "", Point(110 + i * (w + 2), y + 3), Size(w, h));
        extgpioLabel[i]->labelStyle().textAlign = uiHAlign::Center;
        extgpioLabel[i]->setTextFmt("%c%d", i < 8 ? 'A' : 'B', i & 7);
        if (i >= MCP_B3) {
          // configure as IN
          mcp.configureGPIO(i, fabgl::MCPDir::Input);
          extgpioLabel[i]->labelStyle().backgroundColor = RGB888(64, 0, 0);
          // generate a flag/interrupt whenever input changes
          mcp.enableInterrupt(i, fabgl::MCPIntTrigger::PreviousChange);
        } else {
          // configure as OUT
          mcp.configureGPIO(i, fabgl::MCPDir::Output);
          extgpioLabel[i]->labelStyle().backgroundColor = RGB888(0, 64, 0);
          auto pmcp = &mcp;
          extgpioLabel[i]->onClick = [=]() {
            extGPIOSet(i, !pmcp->readGPIO(i));
          };
        }
      }

      // MCP_INT pin
      extIntLabel = new uiLabel(frame, "INT", Point(462, y + 3), Size(25, h));
      extIntLabel->labelStyle().textAlign = uiHAlign::Center;
      extIntLabel->labelStyle().backgroundColor = RGB888(64, 64, 0);
      mcp.enableINTMirroring(true); // INTA = INTB
      pinMode(MCP_INT, INPUT);

      // play button
      extgpioPlay = new uiButton(frame, "Play", Point(500, y + 3), Size(42, 22));
      extgpioPlay->onClick = [&]() { playExtGPIOS(); };

      y += 50;
    }


    // wifi
    new uiStaticLabel(frame, "WIFI TEST:", Point(10, y));
    wifiButton = new uiButton(frame, "Start", Point(110, y - 3), Size(80, 20));
    wifiButton->onClick = [&]() {
      testWifi();
    };
    wifiResultLabel = new uiLabel(frame, "", Point(200, y));


    // timer
    frame->onTimer = [&](uiTimerHandle h) {
      // sound test
      if (h == soundTimer) {
        soundGenerator->playSound(SineWaveformGenerator(), random(100, 3000), 230, 100);
      }
      // internal and external GPIOs
      if (h == gpioTimer) {
        updateGPIOInState();
      }
    };


    y = frame->size().height - 40;
    new uiStaticLabel(frame, "FabGL - Copyright 2019-2022 by Fabrizio Di Vittorio", Point(175, y));
    new uiStaticLabel(frame, "WWW.FABGL.COM", Point(260, y + 17));

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
    // read internal GPIOs
    if (gpioInPrevState != digitalRead(gpioIn)) {
      gpioInPrevState = !gpioInPrevState;
      gpioInState->setText(gpioInPrevState ? "1" : "0");
      gpioInState->repaint();
    }
    // read external GPIOs
    if (mcp.available()) {
      // check MCP_INT has been triggered correctly
      if (digitalRead(MCP_INT) == LOW) {
        extIntLabel->labelStyle().backgroundColor = RGB888(255, 255, 0);
        extIntLabel->repaint();
        intOffDelay = 2;
      } else if (--intOffDelay == 0) {
        extIntLabel->labelStyle().backgroundColor = RGB888(64, 64, 0);
        extIntLabel->repaint();
      }
      if (mcp.getPortIntFlags(MCP_PORTB)) {
        // read B3...B7 GPIOs
        for (int i = MCP_B3; i <= MCP_B7; ++i) {
          extgpioLabel[i]->labelStyle().backgroundColor = mcp.readGPIO(i) ? RGB888(255, 0, 0) : RGB888(64, 0, 0);
          extgpioLabel[i]->repaint();
        }
      }
    }
  }

  bool initMCP() {
    return mcp.begin();
  }

  void extGPIOSet(int gpio, bool value) {
    mcp.writeGPIO(gpio, value);
    extgpioLabel[gpio]->labelStyle().backgroundColor = value ? RGB888(0, 255, 0) : RGB888(0, 64, 0);
    extgpioLabel[gpio]->repaint();
  }

  void playExtGPIOS() {
    showWindow(extgpioPlay, false);
    // single left<->right
    for (int j = 0; j < 4; ++j) {
      for (int i = MCP_A0; i <= MCP_B2; ++i) {
        extGPIOSet(i, true);
        if (i > MCP_A0)
          extGPIOSet(i - 1, false);
        processEvents();
        delay(80);
      }
      for (int i = MCP_B2; i >= MCP_A0; --i) {
        extGPIOSet(i, true);
        if (i < MCP_B2)
          extGPIOSet(i + 1, false);
        processEvents();
        delay(80);
      }
    }
    // all together
    for (int j = 0; j < 4; ++j) {
      for (int i = MCP_A0; i <= MCP_B2; ++i) {
        extGPIOSet(i, true);
        processEvents();
        delay(80);
      }
      for (int i = MCP_A0; i <= MCP_B2; ++i) {
        extGPIOSet(i, false);
        processEvents();
        delay(80);
      }
    }
    // flashing
    for (int j = 0; j < 10; ++j) {
      for (int i = MCP_A0; i <= MCP_B2; ++i)
        extGPIOSet(i, true);
      processEvents();
      delay(120);
      for (int i = MCP_A0; i <= MCP_B2; ++i)
        extGPIOSet(i, false);
      processEvents();
      delay(120);
    }
    showWindow(extgpioPlay, true);
  }

  void testSD() {
    // disable MCP (just because SD should be initialized before MCP)
    bool mcpAvailable = mcp.available();
    mcp.end();
    // mount test
    FileBrowser fb;
    fb.unmountSDCard();
    bool r = fb.mountSDCard(false, "/SD", 1);
    if (!r) {
      sdResultLabel->labelStyle().textColor = Color::BrightRed;
      sdResultLabel->setText("Mount Failed!");
      if (mcpAvailable)
        initMCP();
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
    fb.unmountSDCard();
    if (mcpAvailable)
      initMCP();
    if (!f || !ok) {
      sdResultLabel->labelStyle().textColor = Color::BrightRed;
      sdResultLabel->setText("Read Failed!");
      return;
    }

    sdResultLabel->labelStyle().textColor = Color::Green;
    sdResultLabel->setText("Success!");
  }

  void testWifi() {
    // use API directly to be able to call esp_wifi_deinit() and free necessary memory to mount sd card
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    uint16_t number = 8;
    wifi_ap_record_t ap_info[number];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    auto r = esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&number, ap_info);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();

    if (r != ESP_OK) {
      wifiResultLabel->labelStyle().textColor = Color::BrightRed;
      wifiResultLabel->setText("Wifi Scan Failed!");
    } else if (ap_count == 0) {
      wifiResultLabel->labelStyle().textColor = Color::Yellow;
      wifiResultLabel->setText("Ok, No Network Found!");
    } else {
      wifiResultLabel->labelStyle().textColor = Color::Green;
      wifiResultLabel->setTextFmt("Ok, Found %d Networks", ap_count);
    }
  }

} app;


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  DisplayController.queueSize = 350;  // trade UI speed using less RAM and allow both WiFi and SD Card FS
  DisplayController.begin();
  DisplayController.setResolution(VESA_640x480_75Hz, 570, 390);
}




void loop()
{
  app.runAsync(&DisplayController);
  vTaskDelete(NULL);
}
