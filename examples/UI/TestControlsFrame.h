#pragma once


struct TestControlsFrame : public uiFrame {
  uiTextEdit * textEdit;
  uiButton * normalButton, * switchButton;
  uiListBox * listBox;

  TestControlsFrame(uiFrame * parent)
    : uiFrame(parent, "Test Controls", Point(150, 20), Size(320, 210), false) {

    new uiLabel(this, "Text Label:", Point(10,  30), Size(80, 20));
    textEdit = new uiTextEdit(this, "Text Edit", Point(70,  30), Size(240, 20));
    textEdit->anchors().right = true;

    normalButton = new uiButton(this, "Normal Button", Point(10, 60), Size(80, 20));
    normalButton->onClick = [&]() { app()->messageBox("", "Button Pressed!", "OK", nullptr, nullptr, uiMessageBoxIcon::Info); };

    switchButton = new uiButton(this, "Switch Button OFF", Point(120, 60), Size(100, 20), uiButtonKind::Switch);
    switchButton->onChange = [&]() { switchButton->setText( switchButton->down() ? "Switch Button ON" : "Switch Button OFF" ); };

    listBox = new uiListBox(this, Point(10, 90), Size(150, 80));
    listBox->anchors().right = true;
    listBox->items().append("Listbox Row 0");
    listBox->items().append("Listbox Row 1");
    listBox->items().append("Listbox Row 2");
    listBox->items().append("Listbox Row 3");
    listBox->items().append("Listbox Row 4");
    listBox->items().append("Listbox Row 5");
    listBox->items().append("Listbox Row 6");
    listBox->items().append("Listbox Row 7");

  }
};

