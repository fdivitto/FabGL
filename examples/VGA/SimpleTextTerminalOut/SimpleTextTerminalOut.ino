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

#include "vtanimations.h"



fabgl::VGATextController DisplayController;
fabgl::Terminal          Terminal;


void setup()
{
  //Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  DisplayController.begin();
  DisplayController.setResolution();

  Terminal.begin(&DisplayController);
  //Terminal.setLogStream(Serial);  // DEBUG ONLY

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
  slowPrintf("* * * *  W E L C O M E   T O   F a b G L  * * * *\r\n");
  slowPrintf("2019-2021 by Fabrizio Di Vittorio - www.fabgl.com\r\n");
  slowPrintf("===============================================\r\n\n");
  slowPrintf("Text only VGA display controller demo\r\n\n");
  slowPrintf("Current settings\r\n");
  slowPrintf("Screen Size   : %d x %d\r\n", DisplayController.getScreenWidth(), DisplayController.getScreenHeight());
  slowPrintf("Terminal Size : %d x %d\r\n", Terminal.getColumns(), Terminal.getRows());
  slowPrintf("Free Memory   : %d bytes\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
}

void demo2()
{
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  slowPrintf("16 colors supported\r\n");
  slowPrintf("ANSI colors:\r\n");
  // foregrounds
  Terminal.write("\e[31mRED\t"); delay(500);
  Terminal.write("\e[32mGREEN\t"); delay(500);
  Terminal.write("\e[33mYELLOW\t"); delay(500);
  Terminal.write("\e[34mBLUE\t"); delay(500);
  Terminal.write("\e[35mMAGENTA\t"); delay(500);
  Terminal.write("\e[36mCYAN\t"); delay(500);
  Terminal.write("\e[37mWHITE\r\n"); delay(500);
  Terminal.write("\e[90mHBLACK\t"); delay(500);
  Terminal.write("\e[91mHRED\t"); delay(500);
  Terminal.write("\e[92mHGREEN\t"); delay(500);
  Terminal.write("\e[93mHYELLOW\t"); delay(500);
  Terminal.write("\e[94mHBLUE\t"); delay(500);
  Terminal.write("\e[95mHMAGENTA\t"); delay(500);
  Terminal.write("\e[96mHCYAN\t"); delay(500);
  Terminal.write("\e[97mHWHITE\r\n"); delay(500);
  // backgrounds
  Terminal.write("\e[40mBLACK\t"); delay(500);
  Terminal.write("\e[41mRED\e[40m\t"); delay(500);
  Terminal.write("\e[42mGREEN\e[40m\t"); delay(500);
  Terminal.write("\e[43mYELLOW\e[40m\t"); delay(500);
  Terminal.write("\e[44mBLUE\e[40m\t"); delay(500);
  Terminal.write("\e[45mMAGENTA\e[40m\t"); delay(500);
  Terminal.write("\e[46mCYAN\e[40m\t"); delay(500);
  Terminal.write("\e[47mWHITE\e[40m\r\n"); delay(500);
  Terminal.write("\e[100mHBLACK\e[40m\t"); delay(500);
  Terminal.write("\e[101mHRED\e[40m\t"); delay(500);
  Terminal.write("\e[102mHGREEN\e[40m\t"); delay(500);
  Terminal.write("\e[103mHYELLOW\e[40m\t"); delay(500);
  Terminal.write("\e[104mHBLUE\e[40m\t"); delay(500);
  Terminal.write("\e[105mHMAGENTA\e[40m\t"); delay(500);
  Terminal.write("\e[106mHCYAN\e[40m\r\n"); delay(500);
}

void demo3()
{
  Terminal.write("\e[40;92m"); // background: black, foreground: green
  slowPrintf("\nSupported styles:\r\n");
  slowPrintf("\e[0mNormal\r\n");
  slowPrintf("\e[1mBold\e[0m\r\n");
  slowPrintf("\e[4mUnderlined\e[0m\r\n");
  slowPrintf("\e[5mBlink\e[0m\r\n");
  slowPrintf("\e[7mInverse\e[0m\r\n");
  slowPrintf("\e[1;4mBoldUnderlined\e[0m\r\n");
  slowPrintf("\e[1;4;5mBoldUnderlinedBlinking\e[0m\r\n");
  slowPrintf("\e[1;4;5;7mBoldUnderlinedBlinkingInverse\e[0m\r\n");
}

void demo5()
{
  Terminal.write("\e[40;93m"); // background: black, foreground: yellow
  Terminal.write("\e[2J");     // clear screen
  slowPrintf("\e[10;56HFast Rendering");
  slowPrintf("\e[12;50HThis is a VT/ANSI animation");
  Terminal.write("\e[20h"); // automatic new line on
  Terminal.write("\e[92m"); // light-green
  Terminal.enableCursor(false);
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < sizeof(vt_animation); ++i) {
      Terminal.write(vt_animation[i]);
      if (vt_animation[i] == 0x1B && vt_animation[i + 1] == 0x5B && vt_animation[i + 2] == 0x48)
        delay(120); // pause 100ms every frame
    }
  }
  Terminal.enableCursor(true);
  Terminal.write("\e[20l"); // automatic new line off
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
  demo5();
  delay(4000);
}
