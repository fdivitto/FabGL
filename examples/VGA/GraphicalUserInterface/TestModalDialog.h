#pragma once


struct TestModalDialog : public uiFrame {
  uiTextEdit * textEdit1, * textEdit2, * textEdit3, * textEdit4;
  uiButton * button1, * button2;
  uiPanel * panel;

  TestModalDialog(uiFrame * parent)
    : uiFrame(parent, "Test Modal Dialog", Point(150, 10), Size(300, 210)) {
    frameProps().resizeable        = false;
    frameProps().hasCloseButton    = false;
    frameProps().hasMaximizeButton = false;
    frameProps().hasMinimizeButton = false;

    new uiStaticLabel(this, "This is a Modal Window: click on Close to continue", Point(5, 30));

    panel = new uiPanel(this, Point(5, 50), Size(290, 125));
    panel->panelStyle().backgroundColor = RGB888(255, 255, 255);

    new uiStaticLabel(panel, "First Name:",  Point(10,  5));
    textEdit1 = new uiTextEdit(panel, "",    Point(80,  5), Size(200, 20));
    new uiStaticLabel(panel, "Last Name:",   Point(10, 35));
    textEdit2 = new uiTextEdit(panel, "",    Point(80, 35), Size(200, 20));
    new uiStaticLabel(panel, "Address:",     Point(10, 65));
    textEdit3 = new uiTextEdit(panel, "",    Point(80, 65), Size(200, 20));
    new uiStaticLabel(panel, "Phone:",       Point(10, 95));
    textEdit4 = new uiTextEdit(panel, "",    Point(80, 95), Size(200, 20));

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


