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


fabgl::VGAController DisplayController;
fabgl::Canvas        canvas(&DisplayController);
fabgl::PS2Controller PS2Controller;


int indicatorX = 300;
int indicatorY = 170;
int cursor     = 0;


void showCursorPos(MouseStatus const & status)
{
  canvas.setPenColor(Color::Blue);
  canvas.setBrushColor(Color::Yellow);
  canvas.drawTextFmt(indicatorX, indicatorY, " %d %d ", status.X, status.Y);
}


void setup()
{
  DisplayController.begin();
  DisplayController.setResolution(VGA_640x350_70HzAlt1);
  //DisplayController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  // Setup mouse
  PS2Controller.begin();
  PS2Controller.mouse()->setupAbsolutePositioner(canvas.getWidth(), canvas.getHeight(), true, &DisplayController);
  DisplayController.setMouseCursor((CursorName)cursor);

  canvas.setBrushColor(Color::Blue);
  canvas.clear();
  canvas.selectFont(&fabgl::FONT_8x8);
  canvas.setGlyphOptions(GlyphOptions().FillBackground(true));

  showCursorPos(PS2Controller.mouse()->status());
}


void loop()
{
  MouseStatus status = PS2Controller.mouse()->getNextStatus(-1); // -1 = blocking operation

  // left button writes pixels
  if (status.buttons.left) {
    canvas.setPenColor(Color::BrightRed);
    canvas.setPixel(status.X, status.Y);
    canvas.moveTo(status.X, status.Y);
  }

  // right button change mouse shape
  if (status.buttons.right) {
    cursor = ((CursorName)cursor == CursorName::CursorTextInput ? 0 : cursor + 1);
    DisplayController.setMouseCursor((CursorName)cursor);
  }

  // middle button clear screen
  if (status.buttons.middle) {
    canvas.setBrushColor(Color::Blue);
    canvas.clear();
  }

  // mouse wheel moves cursor position indicator
  if (status.wheelDelta != 0) {
    indicatorY = fabgl::tclamp(indicatorY + status.wheelDelta, 0, canvas.getHeight() - 16);
    PS2Controller.mouse()->status().wheelDelta = 0;
    canvas.setBrushColor(Color::Blue);
    canvas.clear();
  }

  showCursorPos(status);
}
