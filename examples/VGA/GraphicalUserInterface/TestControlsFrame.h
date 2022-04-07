#pragma once


struct TestControlsFrame : public uiFrame {
  uiTextEdit * textEdit;
  uiButton * normalButton, * switchButton;
  uiListBox * listBox;
  uiComboBox * comboBox1, * comboBox2;
  uiCheckBox * checkBox;
  uiCheckBox * radio1, * radio2, * radio3;
  uiSlider * slider1, * slider2;
  uiLabel * sliderLabel1, * sliderLabel2;
  uiSplitButton * splitButton1;

  TestControlsFrame(uiFrame * parent)
    : uiFrame(parent, "Test Controls", Point(150, 20), Size(420, 270), false) {

    new uiStaticLabel(this, "Text Label:", Point(10,  33));
    textEdit = new uiTextEdit(this, "Text Edit", Point(70,  30), Size(340, 20));
    textEdit->anchors().right = true;

    normalButton = new uiButton(this, "Normal Button", Point(10, 60), Size(80, 20));
    normalButton->onClick = [&]() { app()->messageBox("", "Button Pressed!", "OK", nullptr, nullptr, uiMessageBoxIcon::Info); };

    switchButton = new uiButton(this, "Switch Button OFF", Point(120, 60), Size(100, 20), uiButtonKind::Switch);
    switchButton->onChange = [&]() { switchButton->setText( switchButton->down() ? "Switch Button ON" : "Switch Button OFF" ); };

    splitButton1 = new uiSplitButton(this, "Split Button", Point(250, 60), Size(80, 20), 80, "Option 1;Option 2;Option 3;Option 4;Option 5;Option 6");
    splitButton1->onSelect = [&](int index) { InputBox(app()).messageFmt(nullptr, nullptr, "OK", "Selected item %d (%s)", index, splitButton1->items().get(index)); };

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

    auto label = new uiStaticLabel(this, "Normal ComboBox:", Point(170, 93));
    label->anchors().left  = false;
    label->anchors().right = true;
    comboBox1 = new uiComboBox(this, Point(270, 90), Size(140, 20), 80);
    comboBox1->anchors().left  = false;
    comboBox1->anchors().right = true;
    comboBox1->items().append("ComboBox Row 0");
    comboBox1->items().append("ComboBox Row 1");
    comboBox1->items().append("ComboBox Row 2");
    comboBox1->items().append("ComboBox Row 3");
    comboBox1->items().append("ComboBox Row 4");
    comboBox1->items().append("ComboBox Row 5");
    comboBox1->items().append("ComboBox Row 6");
    comboBox1->items().append("ComboBox Row 7");
    comboBox1->items().append("ComboBox Row 8");
    comboBox1->items().append("ComboBox Row 9");
    label = new uiStaticLabel(this, "Editable ComboBox:", Point(170, 123));
    label->anchors().left  = false;
    label->anchors().right = true;
    comboBox2 = new uiComboBox(this, Point(270, 120), Size(140, 20), 80);
    comboBox2->comboBoxProps().openOnFocus = false;
    comboBox2->textEditProps().hasCaret  = true;
    comboBox2->textEditProps().allowEdit = true;
    comboBox2->anchors().left  = false;
    comboBox2->anchors().right = true;
    comboBox2->items().copyFrom(comboBox1->items());

    label = new uiStaticLabel(this, "CheckBox: ", Point(200, 150));
    label->anchors().left  = false;
    label->anchors().right = true;
    checkBox = new uiCheckBox(this, Point(270, 150), Size(16, 16));
    checkBox->anchors().left  = false;
    checkBox->anchors().right = true;

    new uiStaticLabel(this, "Radio1", Point(10, 180));
    new uiStaticLabel(this, "Radio2", Point(80, 180));
    new uiStaticLabel(this, "Radio3", Point(150, 180));
    radio1 = new uiCheckBox(this, Point(45, 180), Size(16, 16), uiCheckBoxKind::RadioButton);
    radio2 = new uiCheckBox(this, Point(115, 180), Size(16, 16), uiCheckBoxKind::RadioButton);
    radio3 = new uiCheckBox(this, Point(185, 180), Size(16, 16), uiCheckBoxKind::RadioButton);
    radio1->setGroupIndex(1);
    radio2->setGroupIndex(1);
    radio3->setGroupIndex(1);

    sliderLabel1 = new uiLabel(this, "0", Point(10, 206));
    slider1 = new uiSlider(this, Point(30, 205), Size(300, 17), uiOrientation::Horizontal);
    slider1->anchors().right = true;
    slider1->onChange = [&]() {
      sliderLabel1->setTextFmt("%d", slider1->position());
    };

    sliderLabel2 = new uiLabel(this, "0", Point(354, 250));
    sliderLabel2->anchors().left = false;
    sliderLabel2->anchors().right = true;
    slider2 = new uiSlider(this, Point(350, 150), Size(17, 94), uiOrientation::Vertical);
    slider2->anchors().left = false;
    slider2->anchors().right = true;
    slider2->onChange = [&]() {
      sliderLabel2->setTextFmt("%d", slider2->position());
    };

  }
};

