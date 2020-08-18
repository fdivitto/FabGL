 /*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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


#pragma once


#define STYLE_FRAME_ID     1
#define STYLE_LABEL_ID     2
#define STYLE_LABELHELP_ID 3
#define STYLE_BUTTON_ID    4
#define STYLE_COMBOBOX     5
#define STYLE_CHECKBOX     6

#define BACKGROUND_COLOR RGB888(64, 64, 64)
#define BUTTONS_BACKGROUND_COLOR RGB888(128, 128, 64);


struct DialogStyle : uiStyle {
  void setStyle(uiObject * object, uint32_t styleClassID) {
    switch (styleClassID) {
      case STYLE_FRAME_ID:
        ((uiFrame*)object)->windowStyle().activeBorderColor         = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().activeTitleBackgroundColor = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().backgroundColor            = BACKGROUND_COLOR;
        break;
      case STYLE_LABEL_ID:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_12;
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 255);
        break;
      case STYLE_LABELHELP_ID:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_14;
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 0);
        break;
      case STYLE_BUTTON_ID:
        ((uiButton*)object)->windowStyle().borderColor              = RGB888(0, 0, 0);
        ((uiButton*)object)->buttonStyle().backgroundColor          = BUTTONS_BACKGROUND_COLOR;
        break;
      case STYLE_COMBOBOX:
        ((uiFrame*)object)->windowStyle().borderColor               = RGB888(255, 255, 255);
        ((uiFrame*)object)->windowStyle().borderSize                = 1;
        break;
      case STYLE_CHECKBOX:
        ((uiFrame*)object)->windowStyle().borderColor               = RGB888(255, 255, 255);
        break;
    }
  }
} dialogStyle;

