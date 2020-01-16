#pragma once

#include "TestModalDialog.h"
#include "TestPaintBoxFrame.h"
#include "TestTimerFrame.h"
#include "TestControlsFrame.h"


class MyApp : public uiApp {

  uiFrame * testsFrame;
  uiButton * createFrameButton, * destroyFrameButton, * testModalDialogButton, * msgBoxButton;
  uiButton * testPaintBoxButton, * testTimerButton, * testControlsButton;
  TestPaintBoxFrame * paintBoxFrame;
  TestTimerFrame * testTimerFrame;
  uiLabel * freeMemLabel, * authorLabel;
  TestControlsFrame * testControlsFrame;

  fabgl::Stack<uiFrame*> dynamicFrames;

  void init() {

    // set root window background color to dark green
    rootWindow()->frameStyle().backgroundColor = RGB888(0, 64, 0);

    // setup a timer to show updated free memory every 2s
    setTimer(this, 2000);
    onTimer = [&](uiTimerHandle tHandle) { showFreeMemory(); };

    // author label
    authorLabel = new uiLabel(rootWindow(), "www.fabgl.com - by Fabrizio Di Vittorio", Point(376, 324));
    authorLabel->labelStyle().backgroundColor = RGB888(255, 255, 0);
    authorLabel->labelStyle().textFont = &fabgl::FONT_std_17;
    authorLabel->update();

    // frame where to put test buttons
    testsFrame = new uiFrame(rootWindow(), "", Point(10, 10), Size(115, 330));
    testsFrame->frameStyle().backgroundColor = RGB888(255, 255, 0);
    testsFrame->windowStyle().borderSize     = 0;

    // label where to show free memory
    freeMemLabel = new uiLabel(testsFrame, "", Point(2, 312));
    freeMemLabel->labelStyle().textFont = &fabgl::FONT_std_12;

    // button to show TestControlsFrame
    testControlsFrame = new TestControlsFrame(rootWindow());
    testControlsButton = new uiButton(testsFrame, "Test Controls", Point(5, 20), Size(105, 20));
    testControlsButton->onClick = [&]() { showWindow(testControlsFrame, true); setActiveWindow(testControlsFrame); };

    // create a destroy frame buttons
    createFrameButton  = new uiButton(testsFrame, "Create Frame", Point(5, 45), Size(105, 20));
    createFrameButton->onClick = [&]() { onCreateFrameButtonClick(); };
    destroyFrameButton = new uiButton(testsFrame, "Destroy Frame", Point(5, 70), Size(105, 20));
    destroyFrameButton->onClick = [&]() { destroyWindow(dynamicFrames.pop()); };

    // test modal dialog button
    testModalDialogButton = new uiButton(testsFrame, "Test Modal Dialog", Point(5, 95), Size(105, 20));
    testModalDialogButton->onClick = [&]() { onTestModalDialogButtonClick(); };

    // test message box
    msgBoxButton = new uiButton(testsFrame, "Test MessageBox", Point(5, 120), Size(105, 20));
    msgBoxButton->onClick = [&]() {
      app()->messageBox("This is the title", "This is the main text", "Button1", "Button2", "Button3", uiMessageBoxIcon::Info);
      app()->messageBox("This is the title", "This is the main text", "Yes", "No", nullptr, uiMessageBoxIcon::Question);
      app()->messageBox("This is the title", "This is the main text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Info);
      app()->messageBox("This is the title", "This is the main text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Error);
      app()->messageBox("This is the title", "Little text", "OK",  nullptr, nullptr, uiMessageBoxIcon::Warning);
      app()->messageBox("This is the title", "No icon", "OK",  nullptr, nullptr, uiMessageBoxIcon::None);
      app()->messageBox("", "No title", "OK",  nullptr, nullptr);
    };

    // button to show TestPaintBoxFrame
    paintBoxFrame = new TestPaintBoxFrame(rootWindow());
    testPaintBoxButton = new uiButton(testsFrame, "Test PaintBox", Point(5, 145), Size(105, 20));
    testPaintBoxButton->onClick = [&]() { showWindow(paintBoxFrame, true); };

    // button to show TestTimerFrame
    testTimerFrame = new TestTimerFrame(rootWindow());
    testTimerButton = new uiButton(testsFrame, "Test Timer", Point(5, 170), Size(105, 20));
    testTimerButton->onClick = [&]() { showWindow(testTimerFrame, true); };

    setActiveWindow(testsFrame);

  }

  void showFreeMemory() {
    freeMemLabel->setTextFmt("Std: %d * 32bit: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_32BIT));
    freeMemLabel->repaint();
  }

  void onCreateFrameButtonClick() {
    uiFrame * newFrame = new uiFrame(rootWindow(), "", Point(110 + random(400), random(300)), Size(175, 80));
    newFrame->setTitleFmt("Frame #%d", dynamicFrames.count());
    newFrame->frameStyle().backgroundColor = RGB888(random(256), random(256), random(256));
    auto label = new uiLabel(newFrame, "FabGL - www.fabgl.com", Point(5, 30), Size(160, 35));
    label->anchors().left = false;
    label->anchors().top = false;
    label->labelStyle().textFont  = &fabgl::FONT_std_17;
    label->labelStyle().textColor = RGB888(random(256), random(256), random(256));
    label->labelStyle().backgroundColor = newFrame->frameStyle().backgroundColor;
    dynamicFrames.push(newFrame);
  }

  void onTestModalDialogButtonClick() {
    // show TestModalDialog as modal window
    auto testModalDialog = new TestModalDialog(rootWindow());
    showModalWindow(testModalDialog);
    destroyWindow(testModalDialog);
  }

};

