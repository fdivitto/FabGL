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


#include <Preferences.h>


#include "fabgl.h"
#include "devdrivers/cvbsgenerator.h"


#define VIDEOOUT_GPIO GPIO_NUM_25


Preferences preferences;


fabgl::CVBS16Controller DisplayController;
fabgl::PS2Controller    PS2Controller;



// preferences keys
static const char * PREF_MODE    = "Mode";
static const char * PREF_MONO    = "Mono";
static const char * PREF_HRATE   = "HRate";



// defaults
#define MODES_DEFAULT 1   // P-PAL-B
#define MONO_DEFAULT  0   // color
#define HRATE_DEFAULT 2   // horizontal rate (1 = x1, 2 = x2, 3 = x3)



// modes

static const char * MODES_DESC[]  = { "Interlaced PAL-B",
                                      "Progressive PAL-B",
                                      "Interlaced NTSC-M",
                                      "Progressive NTSC-M",
                                      "Interlaced PAL-B Wide",
                                      "Progressive PAL-B Wide",
                                      "Interlaced NTSC-M Wide",
                                      "Progressive NTSC-M Wide",
                                      "Progressive NTSC-M Ext",
                                    };
                                    
static const char * MODES_STD[]   = { "I-PAL-B",
                                      "P-PAL-B",
                                      "I-NTSC-M",
                                      "P-NTSC-M",
                                      "I-PAL-B-WIDE",
                                      "P-PAL-B-WIDE",
                                      "I-NTSC-M-WIDE",
                                      "P-NTSC-M-WIDE",
                                      "P-NTSC-M-EXT",
                                    };
                                    


// UI Style

enum { STYLE_NONE, STYLE_LABEL, STYLE_STATICLABEL, STYLE_BUTTON, STYLE_COMBOBOX, STYLE_CHECKBOX };

#define BACKGROUND_COLOR RGB888(0, 0, 0)

#define COLORBARS_HEIGHT 30
#define CIRCLE_SIZE      80

struct DialogStyle : uiStyle {
  void setStyle(uiObject * object, uint32_t styleClassID) {
    switch (styleClassID) {
      case STYLE_LABEL:
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 255);
        break;
      case STYLE_STATICLABEL:
        ((uiStaticLabel*)object)->labelStyle().backgroundColor      = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor            = RGB888(255, 255, 255);
        break;
      case STYLE_BUTTON:
        ((uiButton*)object)->windowStyle().borderColor              = RGB888(128, 128, 128);
        ((uiButton*)object)->buttonStyle().backgroundColor          = RGB888(0, 255, 0);
        break;
      case STYLE_COMBOBOX:
        ((uiComboBox*)object)->windowStyle().borderColor            = RGB888(255, 255, 255);
        ((uiComboBox*)object)->windowStyle().borderSize             = 1;
        break;
      case STYLE_CHECKBOX:
        ((uiCheckBox*)object)->windowStyle().borderColor                = RGB888(255, 255, 255);
        ((uiCheckBox*)object)->checkBoxStyle().mouseOverForegroundColor = RGB888(0, 0, 0);
        ((uiCheckBox*)object)->checkBoxStyle().mouseOverBackgroundColor = RGB888(0, 255, 0);
        break;
    }
  }
} dialogStyle;



// main app

struct MyApp : public uiApp {

  uiComboBox *      modeComboBox;
  uiCheckBox *      monoCheckbox;
  uiComboBox *      hrateComboBox;
  
  void init() {
  
    setStyle(&dialogStyle);

    // set root window background color to dark green
    rootWindow()->frameStyle().backgroundColor = BACKGROUND_COLOR;
    //rootWindow()->windowStyle().borderColor = RGB888(255, 255, 0);
    //rootWindow()->windowStyle().borderSize = 1;

    // some static text
    rootWindow()->onPaint = [&]() {
      auto cv = canvas();
      cv->selectFont(&fabgl::FONT_SLANT_8x14);
      cv->setPenColor(RGB888(0, 128, 255));
      cv->drawText(2, 5, "Composite Video Test - fabgl.com");

      cv->selectFont(&fabgl::FONT_std_12);
      cv->setPenColor(RGB888(255, 128, 0));
      cv->drawText(2, 19, "2019/22 by Fabrizio Di Vittorio");
      
      // color test
      auto w = cv->getWidth();
      auto h = cv->getHeight();
      cv->drawText(2, h - COLORBARS_HEIGHT - 15, "RGB Colors Test");
      cv->setBrushColor(Color::BrightRed);
      cv->fillRectangle(0, h - COLORBARS_HEIGHT, w / 3, h - 1);
      cv->setBrushColor(Color::BrightGreen);
      cv->fillRectangle(w / 3, h - COLORBARS_HEIGHT, w * 2 / 3, h - 1);
      cv->setBrushColor(Color::BrightBlue);
      cv->fillRectangle(w * 2 / 3, h - COLORBARS_HEIGHT, w - 1, h - 1);
      
      // aspect ratio test
      int cx = w - CIRCLE_SIZE / 2 - 1;
      int cy = h - COLORBARS_HEIGHT - CIRCLE_SIZE / 2 - 1;
      cv->setPenColor(Color::BrightYellow);
      cv->drawText(cx - CIRCLE_SIZE / 2 + 2, cy - 3, "Aspect Ratio Test");
      cv->setBrushColor(Color::BrightWhite);
      cv->drawEllipse(cx, cy, CIRCLE_SIZE, CIRCLE_SIZE);
    };
    
    // mode selection
    new uiStaticLabel(rootWindow(), "Mode", Point(2,  60), true, STYLE_STATICLABEL);
    modeComboBox = new uiComboBox(rootWindow(), Point(40, 58), Size(140, 20), 80, true, STYLE_COMBOBOX);
    modeComboBox->items().append(MODES_DESC, sizeof(MODES_DESC) / sizeof(char*));
    modeComboBox->selectItem(getMode());

    // monochromatic checkbox
    new uiStaticLabel(rootWindow(), "B/W", Point(2, 88), true, STYLE_STATICLABEL);
    monoCheckbox = new uiCheckBox(rootWindow(), Point(40, 85), Size(16, 20), uiCheckBoxKind::CheckBox, true, STYLE_CHECKBOX);
    monoCheckbox->setChecked(getMonochromatic());
    monoCheckbox->onChange = [&]() {
      DisplayController.setMonochrome(monoCheckbox->checked());
    };

    // horizontal rate
    new uiStaticLabel(rootWindow(), "H. Rate", Point(103,  88), true, STYLE_STATICLABEL);
    hrateComboBox = new uiComboBox(rootWindow(), Point(145, 84), Size(35, 20), 55, true, STYLE_COMBOBOX);
    hrateComboBox->items().appendSepList("x1,x2,x3", ',');
    hrateComboBox->selectItem(getHRate() - 1);

    // apply button
    auto applyButton = new uiButton(rootWindow(), "Apply", Point(40, 117), Size(80, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    applyButton->onClick = [&]() {
      apply();
    };

    // info label
    auto infoLabel = new uiLabel(rootWindow(), "", Point(2, rootWindow()->clientSize().height - COLORBARS_HEIGHT * 2), Size(0, 0), true, STYLE_LABEL);
    infoLabel->setTextFmt("Free Mem: %d KiB   Resolution: %d x %d", heap_caps_get_free_size(MALLOC_CAP_32BIT) / 1024, DisplayController.getViewPortWidth(), DisplayController.getViewPortHeight());
    
  }

  static int getMode() {
    return preferences.getInt(PREF_MODE, MODES_DEFAULT);
  }
  
  static bool getMonochromatic() {
     return preferences.getInt(PREF_MONO, MONO_DEFAULT);
  }
  
  static int getHRate() {
    return preferences.getInt(PREF_HRATE, HRATE_DEFAULT);
  }
  
  void apply() {
    bool reboot = modeComboBox->selectedItem()  != getMode() ||
                  hrateComboBox->selectedItem() != getHRate();
  
    preferences.putInt(PREF_MODE, modeComboBox->selectedItem());
    preferences.putInt(PREF_MONO, monoCheckbox->checked());
    preferences.putInt(PREF_HRATE, hrateComboBox->selectedItem() + 1);
    
    if (reboot)
      ESP.restart();
  }
  

} app;



void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  preferences.begin("CVBS", false);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  DisplayController.begin(VIDEOOUT_GPIO);
  
  auto mode = MyApp::getMode();
  
  DisplayController.setHorizontalRate(MyApp::getHRate());
  DisplayController.setMonochrome(MyApp::getMonochromatic());
  
  DisplayController.setResolution(MODES_STD[mode]);
  
}


#if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
namespace fabgl {
  extern volatile uint64_t s_cvbsctrlcycles;
}
using fabgl::s_cvbsctrlcycles;
#endif


void loop()
{
  app.runAsync(&DisplayController, 2000);
  
  #if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
  uint32_t s1 = 0;
  while (true) {
    printf("%lld / %d   (%d%%)\n", s_cvbsctrlcycles, fabgl::getCycleCount() - s1, (int)((double)s_cvbsctrlcycles/240000000*100));
    s_cvbsctrlcycles = 0;
    s1 = fabgl::getCycleCount();
    delay(1000);
  }
  #else
  vTaskDelete(NULL);
  #endif
  
  
}
