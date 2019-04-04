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


int indicatorX = 300;
int indicatorY = 170;
int cursor     = 0;


void showCursorPos(MouseStatus const & status)
{
  Canvas.setPenColor(Color::Blue);
  Canvas.setBrushColor(Color::Yellow);
  Canvas.drawTextFmt(indicatorX, indicatorY, " %d %d ", status.X, status.Y);
}


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  // 8 colors
  //VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);

  // 64 colors
  VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);

  VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);
  //VGAController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  // Setup pins GPIO26 for CLK and GPIO27 for DATA
  Mouse.begin(GPIO_NUM_26, GPIO_NUM_27);
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), true, true);
  VGAController.setMouseCursorBitmap((CursorName)cursor);

  Canvas.setBrushColor(Color::Blue);
  Canvas.clear();
  Canvas.selectFont(Canvas.getPresetFontInfo(80, 25));
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(true));

  showCursorPos(Mouse.status());
}


void loop()
{
  MouseStatus status = Mouse.getNextStatus(-1); // -1 = blocking operation

  // left button writes pixels
  if (status.buttons.left) {
    Canvas.setPenColor(Color::BrightRed);
    Canvas.setPixel(status.X, status.Y);
    Canvas.moveTo(status.X, status.Y);
  }

  // right button change mouse shape
  if (status.buttons.right) {
    cursor = ((CursorName)cursor == CursorName::CursorTextInput ? 0 : cursor + 1);
    VGAController.setMouseCursorBitmap((CursorName)cursor);
  }

  // middle button clear screen
  if (status.buttons.middle) {
    Canvas.setBrushColor(Color::Blue);
    Canvas.clear();
  }

  // mouse wheel moves cursor position indicator
  if (status.wheelDelta != 0) {
    indicatorY = fabgl::tclamp(indicatorY + status.wheelDelta, 0, Canvas.getHeight() - 16);
    Mouse.status().wheelDelta = 0;
    Canvas.setBrushColor(Color::Blue);
    Canvas.clear();
  }

  showCursorPos(status);
}
