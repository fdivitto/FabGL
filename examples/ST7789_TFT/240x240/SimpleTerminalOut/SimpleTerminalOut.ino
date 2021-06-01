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
  */



#include "fabgl.h"


#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_DC     22
#define TFT_RESET  21
#define TFT_SPIBUS VSPI_HOST





fabgl::ST7789Controller DisplayController;
fabgl::Terminal         Terminal;


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  DisplayController.begin(TFT_SCK, TFT_MOSI, TFT_DC, TFT_RESET, -1, TFT_SPIBUS);
  DisplayController.setResolution(TFT_240x240);

  Terminal.begin(&DisplayController);
  //Terminal.setLogStream(Serial);  // DEBUG ONLY

  Terminal.loadFont(&fabgl::FONT_9x18);
  Terminal.enableCursor(true);
}


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
      delay(25);
    }
  }
  va_end(ap);
}


void demo1()
{
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  Terminal.write("\e[2J");     // clear screen
  Terminal.write("\e[1;1H");   // move cursor to 1,1
  slowPrintf("**** WELCOME TO FabGL ****\r\n");
  slowPrintf("by Fabrizio Di Vittorio\r\nwww.fabgl.com\r\n");
  slowPrintf("==========================\r\n\n");
  slowPrintf("This is a Display Controller, PS2 Mouse and Keyboard Controller, Graphics Library,  Game Engine and ANSI/VT Terminal for the ESP32\r\n\n");
  slowPrintf("Current settings\r\n");
  slowPrintf("Screen Size   : %d x %d\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
  slowPrintf("Terminal Size : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  slowPrintf("Free Memory   : %d bytes\r\n\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

void demo2()
{
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  slowPrintf("8 or 64 colors supported (depends by GPIOs used)\r\n");
  slowPrintf("ANSI colors:\r\n");
  // foregrounds
  Terminal.write("\e[31mRED\t"); delay(500);
  Terminal.write("\e[32mGREEN\r\n"); delay(500);
  Terminal.write("\e[33mYELLOW\t"); delay(500);
  Terminal.write("\e[34mBLUE\r\n"); delay(500);
  Terminal.write("\e[35mMAGENTA\t"); delay(500);
  Terminal.write("\e[36mCYAN\r\n"); delay(500);
  Terminal.write("\e[37mWHITE\t"); delay(500);
  Terminal.write("\e[90mHBLACK\r\n"); delay(500);
  Terminal.write("\e[91mHRED\t"); delay(500);
  Terminal.write("\e[92mHGREEN\r\n"); delay(500);
  Terminal.write("\e[93mHYELLOW\t"); delay(500);
  Terminal.write("\e[94mHBLUE\r\n"); delay(500);
  Terminal.write("\e[95mHMAGENTA\t"); delay(500);
  Terminal.write("\e[96mHCYAN\r\n"); delay(500);
  Terminal.write("\e[97mHWHITE\t"); delay(500);
  // backgrounds
  Terminal.write("\e[40mBLACK\r\n"); delay(500);
  Terminal.write("\e[41mRED\e[40m\t"); delay(500);
  Terminal.write("\e[42mGREEN\e[40m\r\n"); delay(500);
  Terminal.write("\e[43mYELLOW\e[40m\t"); delay(500);
  Terminal.write("\e[44mBLUE\e[40m\r\n"); delay(500);
  Terminal.write("\e[45mMAGENTA\e[40m\t"); delay(500);
  Terminal.write("\e[46mCYAN\e[40m\r\n"); delay(500);
  Terminal.write("\e[47mWHITE\e[40m\t"); delay(500);
  Terminal.write("\e[100mHBLACK\e[40m\r\n"); delay(500);
  Terminal.write("\e[101mHRED\e[40m\t"); delay(500);
  Terminal.write("\e[102mHGREEN\e[40m\r\n"); delay(500);
  Terminal.write("\e[103mHYELLOW\e[40m\t"); delay(500);
  Terminal.write("\e[104mHBLUE\e[40m\r\n"); delay(500);
  Terminal.write("\e[105mHMAGENTA\e[40m\t"); delay(500);
  Terminal.write("\e[106mHCYAN\e[40m\r\n"); delay(500);
}

void demo3()
{
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  slowPrintf("\nSupported styles:\r\n");
  slowPrintf("\e[0mNormal\r\n");
  slowPrintf("\e[1mBold\e[0m\r\n");
  slowPrintf("\e[3mItalic\e[0m\r\n");
  slowPrintf("\e[4mUnderlined\e[0m\r\n");
  slowPrintf("\e[5mBlink\e[0m\r\n");
  slowPrintf("\e[7mInverse\e[0m\r\n");
  slowPrintf("\e[1;3mBoldItalic\e[0m\r\n");
  slowPrintf("\e[1;3;4mBoldItalicUnderlined\e[0m\r\n");
  slowPrintf("\e[1;3;4;5mBoldItalicUnderlinedBlinking\e[0m\r\n");
  slowPrintf("\e[1;3;4;5;7mBoldItalicUnderlinedBlinkingInverse\e[0m\r\n");
  slowPrintf("\e#6Double Width Line\r\n");
  slowPrintf("\e#6\e#3Double Height Line\r\n"); // top half
  slowPrintf("\e#6\e#4Double Height Line\r\n"); // bottom half
}

void demo4()
{
  Canvas cv(&DisplayController);
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  slowPrintf("\nMixed text and graphics:\r\n");
  slowPrintf("Points...\r\n");
  for (int i = 0; i < 500; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.setPixel(random(cv.getWidth()), random(cv.getHeight()));
    delay(15);
  }
  delay(500);
  slowPrintf("\e[40;92mLines...\r\n");
  for (int i = 0; i < 50; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawLine(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  delay(500);
  slowPrintf("\e[40;92mRectangles...\r\n");
  for (int i = 0; i < 50; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawRectangle(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  delay(500);
  slowPrintf("\e[40;92mEllipses...\r\n");
  for (int i = 0; i < 50; ++i) {
    cv.setPenColor(random(256), random(256), random(256));
    cv.drawEllipse(random(cv.getWidth()), random(cv.getHeight()), random(cv.getWidth()), random(cv.getHeight()));
    delay(50);
  }
  for (int i = 0; i < 15; ++i) {
    Terminal.write("\e[40;92mScrolling...\r\n");
    delay(250);
  }
  cv.clear();
}

void loop()
{
  delay(1000);
  demo1();
  delay(4000);
  demo2();
  delay(4000);
  demo3();
  delay(4000);
  demo4();
  delay(4000);
}
