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
#include "fabui.h"




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

// select one Keyboard and Mouse configuration
#define KEYBOARD_ON_PORT0                0
#define MOUSE_ON_PORT0                   0
#define KEYBOARD_ON_PORT0_MOUSE_ON_PORT1 1

// indicate PS/2 GPIOs for each port
#define PS2_PORT0_CLK GPIO_NUM_33
#define PS2_PORT0_DAT GPIO_NUM_32
#define PS2_PORT1_CLK GPIO_NUM_26
#define PS2_PORT1_DAT GPIO_NUM_27

/* * * *  E N D   O F   C O N F I G U R A T I O N  * * * */




void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  #if KEYBOARD_ON_PORT0_MOUSE_ON_PORT1
  // both keyboard (port 0) and mouse configured (port 1)
  PS2Controller.begin(PS2_PORT0_CLK, PS2_PORT0_DAT, PS2_PORT1_CLK, PS2_PORT1_DAT);
  Keyboard.begin(true, false, 0);
  Mouse.begin(1);
  #elif KEYBOARD_ON_PORT0
  // only keyboard configured on port 0
  Keyboard.begin(PS2_PORT0_CLK, PS2_PORT0_DAT, true, false);
  #elif MOUSE_ON_PORT0
  // only mouse configured on port 0
  Mouse.begin(PS2_PORT0_CLK, PS2_PORT0_DAT);
  #endif

  #if USE_8_COLORS
  VGAController.begin(VGA_RED, VGA_GREEN, VGA_BLUE, VGA_HSYNC, VGA_VSYNC);
  #elif USE_64_COLORS
  VGAController.begin(VGA_RED1, VGA_RED0, VGA_GREEN1, VGA_GREEN0, VGA_BLUE1, VGA_BLUE0, VGA_HSYNC, VGA_VSYNC);
  #endif

  //VGAController.setResolution(VGA_320x200_75Hz);
  VGAController.setResolution(VGA_640x350_70HzAlt1);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(-6, 0);
}


struct TestTextEditFrame : public uiFrame {
  uiTextEdit * textEdit1, * textEdit2, * textEdit3, * textEdit4;
  uiButton * button1, * button2;
  uiPanel * panel;

  TestTextEditFrame(uiFrame * parent)
    : uiFrame(parent, "Test Text Edit", Point(120, 10), Size(300, 210)) {
    frameProps().resizeable        = false;
    frameProps().hasCloseButton    = false;
    frameProps().hasMaximizeButton = false;
    frameProps().hasMinimizeButton = false;

    new uiLabel(this, "This is a Modal Window: click on Close to continue", Point(5, 30));

    panel = new uiPanel(this, Point(5, 50), Size(290, 125));
    panel->panelStyle().backgroundColor = RGB(3, 3, 3);

    new uiLabel(panel, "First Name:",     Point(10,  5), Size( 80, 20));
    textEdit1 = new uiTextEdit(panel, "", Point(80,  5), Size(200, 20));
    new uiLabel(panel, "Last Name:",      Point(10, 35), Size( 80, 20));
    textEdit2 = new uiTextEdit(panel, "", Point(80, 35), Size(200, 20));
    new uiLabel(panel, "Address:",        Point(10, 65), Size( 80, 20));
    textEdit3 = new uiTextEdit(panel, "", Point(80, 65), Size(200, 20));
    new uiLabel(panel, "Phone:",          Point(10, 95), Size( 80, 20));
    textEdit4 = new uiTextEdit(panel, "", Point(80, 95), Size(200, 20));

    button1 = new uiButton(this, "Add Item", Point(5, 180), Size(80, 20));
    button1->onClick = [&]() {
      app()->messageBox("New Item", "Item added correctly", "OK", nullptr, nullptr, uiMessageBoxIcon::Info);
      textEdit1->setText("");
      textEdit2->setText("");
      textEdit3->setText("");
      textEdit4->setText("");
    };

    button2 = new uiButton(this, "Close", Point(90, 180), Size(80, 20));
    button2->onClick = [&]() {
      exitModal(0);
    };
  }
};


class MyApp : public uiApp {

  uiFrame * testsFrame;
  uiButton * createFrameButton, * destroyFrameButton, * textEditButton, * msgBoxButton;

  fabgl::Stack<uiFrame*> dynamicFrames;

  void OnInit() {

    // set root window background color to dark green
    rootWindow()->frameStyle().backgroundColor = RGB(0, 1, 0);

    // frame where to put test buttons
    testsFrame = new uiFrame(rootWindow(), "", Point(10, 10), Size(100, 330));
    testsFrame->frameStyle().backgroundColor = RGB(0, 0, 2);
    testsFrame->windowStyle().borderSize     = 0;

    // create a destroy frame buttons
    createFrameButton  = new uiButton(testsFrame, "Create Frame", Point(5, 20), Size(90, 20));
    createFrameButton->onClick = [&]() { onCreateFrameButtonClick(); };
    destroyFrameButton = new uiButton(testsFrame, "Destroy Frame", Point(5, 45), Size(90, 20));
    destroyFrameButton->onClick = [&]() { destroyWindow(dynamicFrames.pop()); };

    // test text edit button
    textEditButton = new uiButton(testsFrame, "Test uiTextEdit", Point(5, 70), Size(90, 20));
    textEditButton->onClick = [&]() { onTestTextEditButtonClick(); };

    // test message box
    msgBoxButton = new uiButton(testsFrame, "Test MessageBox", Point(5, 95), Size(90, 20));
    msgBoxButton->onClick = [&]() {
      app()->messageBox("This is the title", "This is the main text", "Button1", "Button2", "Button3", uiMessageBoxIcon::Info);
      app()->messageBox("This is the title", "This is the main text", "Yes", "No", nullptr, uiMessageBoxIcon::Question);
      app()->messageBox("This is the title", "This is the main text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Info);
      app()->messageBox("This is the title", "This is the main text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Error);
      app()->messageBox("This is the title", "Little text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Warning);
      app()->messageBox("This is the title", "No icon", "OK",  nullptr, nullptr, uiMessageBoxIcon::None);
      app()->messageBox("", "No title", "OK",  nullptr, nullptr);
    };

  }

  void onCreateFrameButtonClick() {
    char title[16];
    snprintf(title, sizeof(title), "Frame #%d", dynamicFrames.count());
    uiFrame * newFrame = new uiFrame(rootWindow(), title, Point(110 + random(400), random(300)), Size(150, 100));
    newFrame->frameStyle().backgroundColor = RGB(random(4), random(4), random(4));
    auto label = new uiLabel(newFrame, "Hello World!", Point(5, 30), Size(100, 30));
    label->labelStyle().textFont = Canvas.getPresetFontInfoFromHeight(24, false);
    label->labelStyle().textFontColor = RGB(random(4), random(4), random(4));
    label->labelStyle().backgroundColor = newFrame->frameStyle().backgroundColor;
    dynamicFrames.push(newFrame);
  }

  void onTestTextEditButtonClick() {
    // show TestTextEditFrame as modal window
    auto textEditTestFrame = new TestTextEditFrame(rootWindow());
    showModalWindow(textEditTestFrame);
    destroyWindow(textEditTestFrame);
  }

} app;



void loop()
{
  app.run();
}






