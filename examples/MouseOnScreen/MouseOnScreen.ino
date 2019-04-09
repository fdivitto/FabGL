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


/* * * *  C O N F I G U R A T I O N  * * * */

// select one color configuration
#define USE_8_COLORS  0
#define USE_64_COLORS 1

// indicate VGA GPIOs to use for selected color configuration
#if USE_8_COLORS
  #define VGA_RED    GPIO_NUM_22
  #define VGA_GREEN  GPIO_NUM_21
  #define VGA_BLUE   GPIO_NUM_19
  #define VGA_HSYNC  GPIO_NUM_18
  #define VGA_VSYNC  GPIO_NUM_5
#elif USE_64_COLORS
  #define VGA_RED1   GPIO_NUM_22
  #define VGA_RED0   GPIO_NUM_21
  #define VGA_GREEN1 GPIO_NUM_19
  #define VGA_GREEN0 GPIO_NUM_18
  #define VGA_BLUE1  GPIO_NUM_5
  #define VGA_BLUE0  GPIO_NUM_4
  #define VGA_HSYNC  GPIO_NUM_23
  #define VGA_VSYNC  GPIO_NUM_15
#endif

// indicate PS/2 Mouse GPIOs
#define PS2_PORT0_CLK GPIO_NUM_26
#define PS2_PORT0_DAT GPIO_NUM_27

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */




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

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  VGAController.setResolution(VGA_640x350_70HzAlt1);
  //VGAController.setResolution(VGA_640x240_60Hz);    // select to have more free memory

  // Setup mouse
  Mouse.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);
  Mouse.setupAbsolutePositioner(Canvas.getWidth(), Canvas.getHeight(), true, true);
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
