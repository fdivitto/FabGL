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
  VGAController.begin();
  VGAController.setResolution(VGA_640x350_70HzAlt1);
  //VGAController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  // Setup mouse
  PS2Controller.begin();
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), true, true, nullptr);
  VGAController.setMouseCursor((CursorName)cursor);

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
    VGAController.setMouseCursor((CursorName)cursor);
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
