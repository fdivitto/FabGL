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
 * OLED and DS3231 - SDA => GPIO 4
 * OLED and DS3231 - SCL => GPIO 15
 */


#include "fabgl.h"



#define OLED_SDA       GPIO_NUM_4
#define OLED_SCL       GPIO_NUM_15
#define OLED_ADDR      0x3C

// if your display hasn't RESET set to GPIO_UNUSED
#define OLED_RESET     GPIO_UNUSED  // ie Heltec has GPIO_NUM_16 for reset

#define DS3231_ADDR    0x68


fabgl::I2C               I2C;
fabgl::SSD1306Controller DisplayController;
fabgl::DS3231            DS3231;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;



bool dateInvalid;


char const * encodeDate(fabgl::DateTime const & dt)
{
  static char str[11];
  sprintf(str, "%02d/%02d/%d", dt.dayOfMonth, dt.month, dt.year);
  return str;
}


bool decodeDate(fabgl::DateTime * dt, char const * str)
{
  int d, m, y;
  if (sscanf(str, "%02d/%02d/%d", &d, &m, &y) == 3) {
    dt->dayOfMonth = d;
    dt->month = m;
    dt->year = y;
    return true;
  }
  return false;
}


char const * encodeTime(fabgl::DateTime const & dt)
{
  static char str[9];
  sprintf(str, "%02d:%02d:%02d", dt.hours, dt.minutes, dt.seconds);
  return str;
}


bool decodeTime(fabgl::DateTime * dt, char const * str)
{
  int h, m, s;
  if (sscanf(str, "%02d:%02d:%02d", &h, &m, &s) == 3) {
    dt->hours = h;
    dt->minutes = m;
    dt->seconds = s;
    return true;
  }
  return false;
}


void inputDateTime()
{
  auto dt = DS3231.datetime();

  Terminal.clear();
  Terminal.enableCursor(true);

  fabgl::LineEditor LineEditor(&Terminal);
  LineEditor.setInsertMode(false);

  char const * dtstr;

  do {
    Terminal.write("Enter Date:\r\n");
    LineEditor.setText(encodeDate(dt), false);
    dtstr = LineEditor.edit();
  } while (!decodeDate(&dt, dtstr));

  do {
    Terminal.write("\nEnter Time:\r\n");
    LineEditor.setText(encodeTime(dt), false);
    dtstr = LineEditor.edit();
  } while (!decodeTime(&dt, dtstr));

  DS3231.setDateTime(dt);

  Terminal.enableCursor(false);
}


void paintClock(Canvas & cv, fabgl::DateTime & dt)
{
  double cx     = 96;
  double cy     = 32;
  double radius = 42;

  double pointsRadius  = radius * 0.76;
  double secondsRadius = radius * 0.72;
  double minutesRadius = radius * 0.60;
  double hoursRadius   = radius * 0.48;

  double s = dt.seconds / 60.0 * TWO_PI - HALF_PI;
  double m = (dt.minutes + dt.seconds / 60.0) / 60.0 * TWO_PI - HALF_PI;
  double h = (dt.hours + dt.minutes / 60.0) / 24.0 * TWO_PI * 2.0 - HALF_PI;

  cv.setPenColor(Color::BrightWhite);
  cv.setPenWidth(3);
  cv.drawLine(cx, cy, cx + cos(h) * hoursRadius,   cy + sin(h) * hoursRadius);
  cv.setPenWidth(1);
  cv.drawLine(cx, cy, cx + cos(s) * secondsRadius, cy + sin(s) * secondsRadius);
  cv.drawLine(cx, cy, cx + cos(m) * minutesRadius, cy + sin(m) * minutesRadius);


  for (int a = 0; a < 360; a += 6) {
    double arad = radians(a);
    double x = cx + cos(arad) * pointsRadius;
    double y = cy + sin(arad) * pointsRadius;
    cv.setPixel(x, y);
    if ((a % 15) == 0) {
      double x2 = cx + cos(arad) * (pointsRadius - 3);
      double y2 = cy + sin(arad) * (pointsRadius - 3);
      cv.drawLine(x, y, x2, y2);
    }
  }
}


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY  

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR, OLED_RESET);
  DisplayController.setResolution(OLED_128x64);

  while (!DisplayController.available()) {
    Serial.write("Error, SSD1306 not available!\n");
    delay(5000);
  }

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();
  Terminal.loadFont(&fabgl::FONT_8x8);

  Canvas cv(&DisplayController);
  cv.clear();

  DS3231.begin(&I2C);
  while (!DS3231.available()) {
    Serial.write("DS3231 not available!\n");
  }

  // uncomment to forget date/time on powerloss
  //DS3231.clockEnabled(false);

  dateInvalid = !DS3231.dateTimeValid();
  if (dateInvalid && PS2Controller.keyboard()->isKeyboardAvailable()) {
    // date/time invalid, request user to insert new datetime
    inputDateTime();
  }

  cv.clear();

}


void loop()
{
  static const char * D2S[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static const char * M2S[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  auto dt = DS3231.datetime();

  Canvas cv(&DisplayController);
  cv.beginUpdate();
  cv.clear();
  cv.selectFont(&fabgl::FONT_std_15);
  cv.drawTextFmt(0, 0, "%s, %s %d", D2S[dt.dayOfWeek-1], M2S[dt.month - 1], dt.dayOfMonth);
  cv.drawTextFmt(13, 18, "%d", dt.year);

  cv.selectFont(&fabgl::FONT_std_12);
  cv.drawTextFmt(78, 44, "%s", encodeTime(dt));

  cv.selectFont(&fabgl::FONT_std_17);
  cv.setGlyphOptions(GlyphOptions().Bold(true));
  cv.drawTextFmt(4, 44, "%0.1f \xb0 C", DS3231.temperature());
  cv.resetGlyphOptions();


  paintClock(cv, dt);

  cv.endUpdate();

  if (Terminal.available()) {
    char c = Terminal.read();
    if (c == 0x0d) {
      // ENTER, set date/time
      inputDateTime();
    }
  }

  delay(1000);

}
