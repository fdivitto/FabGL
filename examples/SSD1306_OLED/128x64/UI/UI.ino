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
  * OLED - SDA => GPIO 4
  * OLED - SCL => GPIO 15
  */



#include "fabgl.h"
#include "fabui.h"



#define OLED_SDA       GPIO_NUM_4
#define OLED_SCL       GPIO_NUM_15
#define OLED_ADDR      0x3C

// if your display hasn't RESET set to GPIO_UNUSED
#define OLED_RESET     GPIO_UNUSED  // ie Heltec has GPIO_NUM_16 for reset


fabgl::I2C               I2C;
fabgl::PS2Controller     PS2Controller;
fabgl::SSD1306Controller DisplayController;



void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR, OLED_RESET);
  DisplayController.setResolution(OLED_128x64);

  while (DisplayController.available() == false) {
    Serial.write("Error, SSD1306 not available!\n");
    delay(5000);
  }
}





class MyApp : public uiApp {

  void init() {
    appProps().realtimeReshaping = true;
    rootWindow()->frameStyle().backgroundColor = Color::Black;

    auto label1 = new uiLabel(rootWindow(), "Enter you name", Point(0, 0));
    label1->labelStyle().backgroundColor = Color::Black;
    label1->labelStyle().textColor = Color::White;

    auto textEdit1 = new uiTextEdit(rootWindow(), "", Point(0, 19), Size(128, 20));
    setFocusedWindow(textEdit1);

    auto button1 = new uiButton(rootWindow(), "OK", Point(48, 42), Size(30, 20));
    button1->onClick = [=]() { button1Click(textEdit1->text()); };
  }

  void button1Click(char const * txt) {
    auto frame = new uiFrame(rootWindow(), " ", Point(10, 10), Size(108, 44));
    auto label1 = new uiLabel(frame, "", Point(10, 20));
    label1->labelStyle().textFont = &fabgl::FONT_std_16;
    label1->setTextFmt("Hello %s!!", txt);
    setActiveWindow(frame);
  }

} app;



void loop()
{
  app.run(&DisplayController);
}






