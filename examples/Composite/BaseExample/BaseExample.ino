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


#include "fabgl.h"
#include "devdrivers/cvbsgenerator.h"




fabgl::CVBS16Controller DisplayController;


using fabgl::CVBSGenerator;


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  DisplayController.begin();
  
  DisplayController.setHorizontalRate(2);
  DisplayController.setMonochrome(false);
  
  //DisplayController.setResolution("I-PAL-B");
  //DisplayController.setResolution("P-PAL-B");
  //DisplayController.setResolution("I-PAL-B-WIDE");
  //DisplayController.setResolution("I-NTSC-M");
  //DisplayController.setResolution("I-NTSC-M-WIDE");
  DisplayController.setResolution("P-NTSC-M");
  
  
  
  Canvas cv(&DisplayController);
  
  // linee rosse
  cv.setPenColor(255, 0, 0);
  cv.drawLine(0, 0, cv.getWidth() - 1, cv.getHeight() - 1);
  cv.drawLine(0, cv.getHeight() - 1, cv.getWidth() - 1, 0);
  cv.drawLine(cv.getWidth() / 2, 0, cv.getWidth() / 2, cv.getHeight() - 1);
  cv.drawLine(0, cv.getHeight() / 2, cv.getWidth() - 1, cv.getHeight() / 2);
  
  ///// bande colorate RGB, gialla e grigia
  cv.setBrushColor(Color::BrightRed);
  cv.fillRectangle(cv.getWidth()/2-80, cv.getHeight()/2-80, cv.getWidth()/2+80, cv.getHeight()/2+80);
  cv.setBrushColor(Color::BrightGreen);
  cv.fillRectangle(cv.getWidth()/2-60, cv.getHeight()/2-60, cv.getWidth()/2+60, cv.getHeight()/2+60);
  cv.setBrushColor(Color::BrightBlue);
  cv.fillRectangle(cv.getWidth()/2-40, cv.getHeight()/2-40, cv.getWidth()/2+40, cv.getHeight()/2+40);
  cv.setBrushColor(Color::BrightYellow);
  cv.fillRectangle(50, cv.getHeight()/2 - 20, 100, cv.getHeight()/2 + 20);
  cv.setBrushColor(Color::White);
  cv.fillRectangle(cv.getWidth() - 50, cv.getHeight()/2 - 20, cv.getWidth() - 100, cv.getHeight()/2 + 20);
    
  /*
  // tre bande orizzontali
  int w = cv.getWidth();
  int h = cv.getHeight() / 3;
  cv.setBrushColor(Color::BrightRed);
  cv.fillRectangle(0, 0, w, h);
  cv.setBrushColor(Color::BrightGreen);
  cv.fillRectangle(0, h, w, h + h);
  cv.setBrushColor(Color::BrightBlue);
  cv.fillRectangle(0, h + h, w, h + h + h);
  //*/
  
  /*
  // tutto rosso
  int w = cv.getWidth();
  int h = cv.getHeight();
  cv.setBrushColor(Color::BrightRed);
  cv.fillRectangle(0, 0, w, h);
  //*/
  
  
  //cv.setBrushColor(Color::BrightBlue);
  //cv.fillRectangle(0, 0, cv.getWidth(), cv.getHeight());

  /*
  while (1) {
    delay(10);
    //while (1) printf("frame=%d field=%d frameLine=%d interFrameLine=%d pictureLine=%d scPhaseSam=%d\n", CVBSGenerator::frame(), CVBSGenerator::field(), CVBSGenerator::frameLine(), CVBSGenerator::interFrameLine(), CVBSGenerator::pictureLine(), CVBSGenerator::subCarrierPhase());
  }
  */
  
}



void loop()
{
}
