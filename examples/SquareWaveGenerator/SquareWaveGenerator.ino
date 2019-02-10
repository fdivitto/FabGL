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
#include "swgenerator.h"



void printHelp()
{
  Serial.printf("Square Wave Generator\n");
  Serial.printf("Chip Revision: %d   Chip Frequency: %d MHz\n\n", ESP.getChipRevision(), ESP.getCpuFreqMHz());
  Serial.printf("Commands:\n");
  Serial.printf("  !number = frequency in Hz (82500...62500000)\n");
  Serial.printf("Various:\n");
  Serial.printf("  h = Print This help\n\n");
}


int freq;


void setFrequency(int f)
{
  SquareWaveGenerator.stop();
  freq = f;
  SquareWaveGenerator.play(freq);
  Serial.printf("Frequency = %dHz\n", freq);
}



void setup()
{
  Serial.begin(115200);

  // avoid garbage into the UART
  delay(500);

  SquareWaveGenerator.begin(true, GPIO_NUM_22, GPIO_NUM_19);

  printHelp();

/*
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.printf("silicon revision %d, ", chip_info.revision);
    */

  setFrequency(20000000);
}


void loop()
{
  if (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        printHelp();
        break;
      case '!':
        setFrequency(atoi(Serial.readStringUntil('\n').c_str()));
        break;
      case '^':
        for (int i = 82500; i < 62500000; ++i) {
          setFrequency(i);
        }
        break;
    }
  }
}
