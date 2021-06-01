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


#pragma once


#include "fabui.h"
#include "uistyle.h"



struct RebootDialog : public uiFrame {
  uiLabel *  label;
  uiButton * button;
  int        counter;

  RebootDialog(uiFrame * parent)
    : uiFrame(parent, "Terminal restart required", UIWINDOW_PARENTCENTER, Size(230, 60)) {
    frameProps().resizeable        = false;
    frameProps().moveable          = false;
    frameProps().hasCloseButton    = false;
    frameProps().hasMaximizeButton = false;
    frameProps().hasMinimizeButton = false;

    label = new uiLabel(this, "", Point(5, 30));

    button = new uiButton(this, "Reboot Now!", Point(132, 27), Size(80, 20));
    button->onClick = [&]() {
      ESP.restart();
    };

    counter = 3;
    app()->setTimer(this, 1000);
    onTimer = [&](uiTimerHandle tHandle) { countDown(); };
    countDown();
  }

  void countDown() {
    app()->setFocusedWindow(button);
    if (counter < 0)
      ESP.restart();
    label->setTextFmt("Rebooting in %d seconds...", counter--);
  }
};


// an app just to show reboot message and reboot
class RebootDialogApp : public uiApp {
  void init() {
    rootWindow()->frameProps().fillBackground = false;
    auto rebootDialog = new RebootDialog(rootWindow());
    showModalWindow(rebootDialog);  // no return!
  }
};

