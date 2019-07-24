/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

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


void printHelp()
{
  Serial.printf("\n\nPS/2 Mouse Studio\n");
  Serial.printf("Chip Revision: %d   Chip Frequency: %d MHz\n", ESP.getChipRevision(), ESP.getCpuFreqMHz());

  printInfo();

  Serial.printf("Commands:\n");
  Serial.printf("  r = Reset  s = Change Sample Rate  e = Change Resolution  c = Change Scaling\n");
  Serial.printf("Various:\n");
  Serial.printf("  h = Print This help\n\n");
}


void printInfo()
{
  if (Mouse.isMouseAvailable()) {
    Serial.write("Device Id = ");
    switch (Mouse.identify()) {
      case fabgl::OldATKeyboard:
        Serial.write("\"Old AT Keyboard\"");
        break;
      case fabgl::MouseStandard:
        Serial.write("\"Standard Mouse\"");
        break;
      case fabgl::MouseWithScrollWheel:
        Serial.write("\"Mouse with scroll wheel\"");
        break;
      case fabgl::Mouse5Buttons:
        Serial.write("\"5 Buttons Mouse\"");
        break;
      case fabgl::MF2KeyboardWithTranslation:
        Serial.write("\"MF2 Keyboard with translation\"");
        break;
      case fabgl::M2Keyboard:
        Serial.write("\"MF2 keyboard\"");
        break;
      default:
        Serial.write("\"Unknown\"");
        break;
    }
  } else
    Serial.write("Mouse Error!");
  Serial.write("\n");
}



void setup()
{
  Serial.begin(115200);
  delay(500);  // avoid garbage into the UART
  Serial.write("\n\nReset\n");

  PS2Controller.begin();

  printHelp();
}


const uint8_t SampleRates[] = { 10, 20, 40, 60, 80, 100, 200 };
int currentSampleRateIndex = 5; // reset is 100 samples/sec


int currentResolution = 2;      // reset is 2 (4 counts/mm)


int currentScaling = 1;         // reset is 1 (1:1)


void loop()
{

  if (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        printHelp();
        break;
      case 'r':
        Mouse.reset();
        printInfo();
        break;
      case 's':
        ++currentSampleRateIndex;
        if (currentSampleRateIndex == sizeof(SampleRates))
          currentSampleRateIndex = 0;
        if (Mouse.setSampleRate(SampleRates[currentSampleRateIndex]))
          Serial.printf("Sample rate = %d\n", SampleRates[currentSampleRateIndex]);
        break;
      case 'e':
        ++currentResolution;
        if (currentResolution == 4)
          currentResolution = 0;
        if (Mouse.setResolution(currentResolution))
          Serial.printf("Resolution = %d\n", 1 << currentResolution);
        break;
      case 'c':
        ++currentScaling;
        if (currentScaling == 3)
          currentScaling = 1;
        if (Mouse.setScaling(currentScaling))
          Serial.printf("Scaling = 1:%d\n", currentScaling);
        break;
    }
  }

  if (Mouse.deltaAvailable()) {
    MouseDelta mouseDelta;
    Mouse.getNextDelta(&mouseDelta);

    Serial.printf("deltaX = %d  deltaY = %d  deltaZ = %d  leftBtn = %d  midBtn = %d  rightBtn = %d\n",
                  mouseDelta.deltaX, mouseDelta.deltaY, mouseDelta.deltaZ,
                  mouseDelta.buttons.left, mouseDelta.buttons.middle, mouseDelta.buttons.right);
  }

}
