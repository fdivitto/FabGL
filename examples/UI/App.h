#pragma once

#include "TestEditFrame.h"
#include "TestPaintBoxFrame.h"
#include "TestTimerFrame.h"


class MyApp : public uiApp {

  uiFrame * testsFrame;
  uiButton * createFrameButton, * destroyFrameButton, * textEditButton, * msgBoxButton;
  uiButton * testPaintBoxButton, * testTimerButton;
  TestPaintBoxFrame * paintBoxFrame;
  TestTimerFrame * testTimerFrame;

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

    // button to show TestPaintBoxFrame
    paintBoxFrame = new TestPaintBoxFrame(rootWindow());
    testPaintBoxButton = new uiButton(testsFrame, "Test PaintBox", Point(5, 120), Size(90, 20));
    testPaintBoxButton->onClick = [&]() { showWindow(paintBoxFrame, true); };

    // button to show TestTimerFrame
    testTimerFrame = new TestTimerFrame(rootWindow());
    testTimerButton = new uiButton(testsFrame, "Test Timer", Point(5, 145), Size(90, 20));
    testTimerButton->onClick = [&]() { showWindow(testTimerFrame, true); };
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

};

