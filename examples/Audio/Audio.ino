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

#include "speech.h"
#include "loop.h"
#include "mario.h"



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

  //VGAController.enableBackgroundPrimitiveExecution(false);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(20, -2);
  VGAController.moveScreen(-6, 0);

}


SoundGenerator soundGenerator;


class ChannelFrame : public uiFrame {

  SineWaveformGenerator sine;
  SquareWaveformGenerator square;
  NoiseWaveformGenerator noise;
  TriangleWaveformGenerator triangle;
  SawtoothWaveformGenerator sawtooth;
  SamplesGenerator loop = SamplesGenerator(loopSamples, sizeof(loopSamples));
  SamplesGenerator speech = SamplesGenerator(speechSamples, sizeof(speechSamples));
  SamplesGenerator mario = SamplesGenerator(marioSamples, sizeof(marioSamples));
  WaveformSampleGenerator * curGen = nullptr;

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
    genComboBox->listBoxStyle().textFont = Canvas.getPresetFontInfoFromHeight(12, false);
    genComboBox->textEditStyle().textFont = Canvas.getPresetFontInfoFromHeight(12, false);
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
    frameStyle().titleFont = Canvas.getPresetFontInfoFromHeight(12, false);
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

    rootWindow()->frameStyle().backgroundColor = RGB(0, 0, 1);

    /*
    setTimer(this, 1000);
    onTimer = [&](uiTimerHandle tHandle) {
      Serial.printf("Std: %d * 32bit mem: %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_32BIT));
    };
    //*/


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
  app.run();
}






