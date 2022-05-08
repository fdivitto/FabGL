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


#pragma once


enum { STYLE_NONE, STYLE_FRAME, STYLE_LABEL, STYLE_STATICLABEL, STYLE_LABELHELP, STYLE_LABELHELP2, STYLE_BUTTON, STYLE_COMBOBOX, STYLE_CHECKBOX, STYLE_LABELBUTTON };


#define BACKGROUND_COLOR RGB888(64, 64, 64)


struct DialogStyle : uiStyle {
  void setStyle(uiObject * object, uint32_t styleClassID) {
    switch (styleClassID) {
      case STYLE_FRAME:
        ((uiFrame*)object)->windowStyle().activeBorderColor         = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().activeTitleBackgroundColor = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().backgroundColor            = BACKGROUND_COLOR;
        break;
      case STYLE_LABEL:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_12;
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 255);
        break;
      case STYLE_STATICLABEL:
        ((uiStaticLabel*)object)->labelStyle().textFont             = &fabgl::FONT_std_12;
        ((uiStaticLabel*)object)->labelStyle().backgroundColor      = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor            = RGB888(255, 255, 255);
        break;
      case STYLE_LABELHELP:
        ((uiStaticLabel*)object)->labelStyle().textFont             = &fabgl::FONT_std_14;
        ((uiStaticLabel*)object)->labelStyle().backgroundColor      = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor            = RGB888(255, 255, 0);
        break;
      case STYLE_LABELHELP2:
        ((uiStaticLabel*)object)->labelStyle().textFont             = &fabgl::FONT_std_14;
        ((uiStaticLabel*)object)->labelStyle().backgroundColor      = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor            = RGB888(255, 255, 255);
        break;
      case STYLE_LABELBUTTON:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_14;
        ((uiLabel*)object)->labelStyle().backgroundColor            = RGB888(128, 128, 128);
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(0, 0, 0);
        ((uiLabel*)object)->labelStyle().textAlign                  = uiHAlign::Center;
        break;
      case STYLE_BUTTON:
        ((uiButton*)object)->windowStyle().borderColor              = RGB888(0, 0, 0);
        ((uiButton*)object)->buttonStyle().backgroundColor          = RGB888(128, 128, 64);
        ((uiButton*)object)->buttonStyle().textColor                = RGB888(0, 0, 0);
        break;
      case STYLE_COMBOBOX:
        ((uiComboBox*)object)->windowStyle().borderColor            = RGB888(255, 255, 255);
        ((uiComboBox*)object)->windowStyle().borderSize             = 1;
        break;
      case STYLE_CHECKBOX:
        ((uiCheckBox*)object)->windowStyle().borderColor            = RGB888(255, 255, 255);
        break;
    }
  }
} dialogStyle;

