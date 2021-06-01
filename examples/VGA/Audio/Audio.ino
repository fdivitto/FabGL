/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

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

#include "speech.h"
#include "loop.h"
#include "mario.h"


fabgl::VGAController DisplayController;
fabgl::PS2Controller PS2Controller;
SoundGenerator       soundGenerator;


void setup()
{
  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  DisplayController.begin();
  DisplayController.setResolution(VGA_640x350_70HzAlt1);

  // adjust this to center screen in your monitor
  //DisplayController.moveScreen(20, -2);
  //DisplayController.moveScreen(-6, 0);
}


class ChannelFrame : public uiFrame {

  SineWaveformGenerator sine;
  SquareWaveformGenerator square;
  NoiseWaveformGenerator noise;
  TriangleWaveformGenerator triangle;
  SawtoothWaveformGenerator sawtooth;
  SamplesGenerator loop = SamplesGenerator(loopSamples, sizeof(loopSamples));
  SamplesGenerator speech = SamplesGenerator(speechSamples, sizeof(speechSamples));
  SamplesGenerator mario = SamplesGenerator(marioSamples, sizeof(marioSamples));
  WaveformGenerator * curGen = nullptr;

  uiLabel * volumeLabel, * frequencyLabel;
  uiSlider * volumeSlider, * frequencySlider;
  uiComboBox * genComboBox;
  uiCheckBox * enableCheck;

public:

  ChannelFrame(uiWindow * parent)
   : uiFrame(parent, "", Point(0, 0), Size(408, 80))
  {
    static int channel_ = 0;
    static int y = 4, x = 4;
    static int startingFreq = 150;

    app()->moveWindow(this, x, y);
    y += size().height + 4;
    if (y + size().height > parent->size().height) {
      x += 25;
      y = x;
    }

    frameProps().resizeable = false;
    frameProps().hasCloseButton = false;

    setTitleFmt("Channel %d", ++channel_);

    // volume slider
    volumeLabel = new uiLabel(this, "", Point(5, 20));
    volumeSlider = new uiSlider(this, Point(72, 19), Size(150, 17), uiOrientation::Horizontal);
    volumeSlider->onChange = [&]() {
      volumeLabel->setTextFmt("Volume %d", volumeSlider->position());
      if (curGen)
        curGen->setVolume(volumeSlider->position());
    };
    volumeSlider->setup(0, 127, 16);
    volumeSlider->setPosition(100);

    // generator combobox
    new uiLabel(this, "Type", Point(230, 20));
    genComboBox = new uiComboBox(this, Point(260, 20), Size(140, 16), 35);
    genComboBox->listBoxStyle().textFont = &fabgl::FONT_std_12;
    genComboBox->textEditStyle().textFont = &fabgl::FONT_std_12;
    genComboBox->items().append("Sine Wave");
    genComboBox->items().append("Square Wave");
    genComboBox->items().append("Triangle Wave");
    genComboBox->items().append("Sawtooth Wave");
    genComboBox->items().append("Noise");
    genComboBox->items().append("Loop");
    genComboBox->items().append("Speech");
    genComboBox->items().append("Mario");
    genComboBox->selectItem(0);
    genComboBox->onChange = [&]() {
      setGenerator(genComboBox->selectedItem());
    };

    // frequency slider
    frequencyLabel = new uiLabel(this, "", Point(5, 39));
    frequencySlider = new uiSlider(this, Point(72, 38), Size(330, 17), uiOrientation::Horizontal);
    frequencySlider->onChange = [&]() {
      frequencyLabel->setTextFmt("Freq %d Hz", frequencySlider->position());
      if (curGen)
        curGen->setFrequency(frequencySlider->position());
    };
    frequencySlider->setup(0, 8000, 500);
    frequencySlider->setPosition(startingFreq);
    startingFreq += 50;

    // enable checkbox
    new uiLabel(this, "Enable", Point(5, 76-17));
    enableCheck = new uiCheckBox(this, Point(77, 75-17), Size(17, 17));
    enableCheck->onChange = [&]() {
      if (curGen)
        curGen->enable(enableCheck->checked());
    };

    // set default generator
    setGenerator(0);
  }

  void setGenerator(int index) {
    soundGenerator.detach(curGen);
    switch (index) {
      case 0:
        curGen = &sine;
        break;
      case 1:
        curGen = &square;
        break;
      case 2:
        curGen = &triangle;
        break;
      case 3:
        curGen = &sawtooth;
        break;
      case 4:
        curGen = &noise;
        break;
      case 5:
        curGen = &loop;
        break;
      case 6:
        curGen = &speech;
        break;
      case 7:
        curGen = &mario;
        break;
    }
    soundGenerator.attach(curGen);
    curGen->enable(enableCheck->checked());
    curGen->setFrequency(frequencySlider->position());
  }
};


class CommandsFrame : public uiFrame {

public:
  CommandsFrame(uiWindow * parent)
   : uiFrame(parent, "Main Controls", Point(500, 4), Size(133, 200))
  {
    frameStyle().titleFont = &fabgl::FONT_std_12;
    frameProps().resizeable = false;
    frameProps().hasMaximizeButton = false;
    frameProps().hasMinimizeButton = false;
    frameProps().hasCloseButton = false;

    uiButton * addChannelBtn = new uiButton(this, "Add Channel", Point(7, 22), Size(80, 16));
    addChannelBtn->onClick = [=]() {
      if (heap_caps_get_free_size(MALLOC_CAP_8BIT) < 5000)
        app()->messageBox("Out of memory!", "I'm sorry, there is no more memory available!", "OK");
      else
        new ChannelFrame(parent);
    };

    uiButton * resetBtn = new uiButton(this, "Reset", Point(7, 45), Size(80, 16));
    resetBtn->onClick = [=]() {
      if (app()->messageBox("Reset ESP32", "Are you sure?", "Yes", "No", nullptr, uiMessageBoxIcon::Question) == uiMessageBoxResult::Button1)
        ESP.restart();
    };

    // main volume slider
    uiLabel * volumeLabel = new uiLabel(this, "", Point(90, 182));
    uiSlider * volumeSlider = new uiSlider(this, Point(100, 20), Size(17, 160), uiOrientation::Vertical);
    volumeSlider->onChange = [=]() {
      volumeLabel->setTextFmt("Vol %d", volumeSlider->position());
      soundGenerator.setVolume(volumeSlider->position());
    };
    volumeSlider->setup(0, 127, 16);
    volumeSlider->setPosition(100);


  }

};


class MyApp : public uiApp {

  void init() {

    rootWindow()->frameStyle().backgroundColor = RGB888(0, 0, 64);

    new ChannelFrame(rootWindow());
    new ChannelFrame(rootWindow());
    new ChannelFrame(rootWindow());
    new ChannelFrame(rootWindow());

    new CommandsFrame(rootWindow());

    soundGenerator.play(true);

  }
} app;


void loop()
{
  app.run(&DisplayController);
}






