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

// if your display hasn't RESET set to GPIO_UNUSED
#define OLED_RESET     GPIO_UNUSED  // ie Heltec has GPIO_NUM_16 for reset



fabgl::I2C               I2C;
fabgl::SSD1306Controller DisplayController;



void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY  

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR, OLED_RESET);
  DisplayController.setResolution(OLED_128x64, 128, 64, true);

  while (!DisplayController.available()) {
    Serial.write("Error, SSD1306 not available!\n");
    delay(5000);
  }

}


void loop()
{
  Canvas cv(&DisplayController);

  for (int j = 1; j < 32; ++j) {
    cv.setPenWidth(j);
    for (int i = 0; i < 360; i += 10) {
      cv.clear();
      double a = (double) i * M_PI / 180.0;
      double w = cv.getWidth() / 2.0;
      double h = cv.getHeight() / 2.0;
      int x1 = w + cos(a) * w;
      int y1 = h + sin(a) * h;
      int x2 = w + cos(a + M_PI) * w;
      int y2 = h + sin(a + M_PI) * h;
      cv.drawLine(x1, y1, x2, y2);
      cv.swapBuffers();
      delay(60);
    }
  }


}
