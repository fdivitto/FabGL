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




#define OLED_SDA       GPIO_NUM_4
#define OLED_SCL       GPIO_NUM_15
#define OLED_ADDR      0x3C


fabgl::I2C               I2C;
fabgl::SSD1306Controller DisplayController;
fabgl::Terminal          Terminal;




void slowPrintf(const char * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    for (int i = 0; i < size; ++i) {
      Terminal.write(buf[i]);
      delay(45);
    }
  }
  va_end(ap);
}


void demo1()
{
  Terminal.write("\e[40;32m"); // background: black, foreground: green
  Terminal.write("\e[2J");     // clear screen
  Terminal.write("\e[1;1H");   // move cursor to 1,1
  slowPrintf("\e[1mWELCOME TO FabGL\e[0m\r\n");
  slowPrintf("www.fabgl.com\r\n");
  slowPrintf("================\r\n\n");
  slowPrintf("This is a Display Controller, PS2 Mouse and Keyboard Controller, Graphics Library, Game Engine and ANSI/VT Terminal for the ESP32\r\n\n");
  slowPrintf("Current settings\r\n");
  slowPrintf("Screen Size:\r\n  \e[1m%d x %d\e[0m\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
  slowPrintf("Terminal Size:\r\n  \e[1m%d x %d\e[0m\r\n", Terminal.getColumns(), Terminal.getRows());
  slowPrintf("Free Memory:\r\n  \e[1m%d bytes\e[0m\r\n\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

void demo3()
{
  Terminal.write("\e[40;32m"); // background: black, foreground: green
  slowPrintf("\nSupported styles:\r\n");
  slowPrintf("\e[0mNormal\r\n");
  slowPrintf("\e[1mBold\e[0m\r\n");
  slowPrintf("\e[3mItalic\e[0m\r\n");
  slowPrintf("\e[4mUnderlined\e[0m\r\n");
  slowPrintf("\e[5mBlink\e[0m\r\n");
  slowPrintf("\e[7mInverse\e[0m\r\n");
  slowPrintf("\e[1;3mBoldItalic\e[0m\r\n");
  slowPrintf("\e#6Double W.\r\n");
  slowPrintf("\e#6\e#3Double H.\r\n"); // top half
  slowPrintf("\e#6\e#4Double H.\r\n"); // bottom half
}

void demo4()
{
  Canvas cv(&DisplayController);
  Terminal.write("\e[40;32m"); // background: black, foreground: green
  slowPrintf("\nMixed text and graphics:\r\n");
  slowPrintf("Points...\r\n");
  for (int i = 0; i < 100; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.setPixel(random(cv.getWidth()), random(cv.getHeight()));
    delay(15);
  }
  delay(500);
  slowPrintf("\e[40;32mLines...\r\n");
  for (int i = 0; i < 10; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawLine(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  delay(500);
  slowPrintf("\e[40;32mRectangles...\r\n");
  for (int i = 0; i < 10; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawRectangle(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  delay(500);
  slowPrintf("\e[40;32mEllipses...\r\n");
  for (int i = 0; i < 10; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawEllipse(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  for (int i = 0; i < 30; ++i) {
    Terminal.write("\e[40;32mScrolling...\r\n");
    delay(250);
  }
  cv.clear();
}


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR);
  DisplayController.setResolution(OLED_128x32);

  while (DisplayController.available() == false) {
    Serial.write("Error, SSD1306 not available!\n");
    delay(5000);
  }

  Terminal.begin(&DisplayController);
  Terminal.setLogStream(Serial);  // DEBUG ONLY
  Terminal.loadFont(&fabgl::FONT_6x8);
  Terminal.enableCursor(true);
}


void loop()
{
  delay(1000);
  demo1();
  delay(4000);
  demo3();
  delay(4000);
  demo4();
}
