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



/**
 * @file
 *
 * @brief This file contains TFT controllers definitions.
 */


#include <stdint.h>
#include <stddef.h>

#include "fabglconf.h"
#include "fabutils.h"
#include "TFTControllerGeneric.h"




namespace fabgl {



/**
 * @brief Implements ST7789 display driver controller.
 *
 * Example:
 *
 *     fabgl::ST7789Controller DisplayController;
 *
 *     void setup() {
 *       // SCK = 18, MOSI = 23, D/C = 22, RESET = 21, no CS  (WARN: disconnect VGA connector!!)
 *       DisplayController.begin(18, 23, 22, 21, -1, VSPI_HOST);
 *       DisplayController.setResolution(TFT_240x240);
 *
 *       Canvas cv(&DisplayController);
 *       cv.clear();
 *       cv.drawText(0, 0, "Hello World!");
 *     }
 */
class ST7789Controller : public TFTController {
protected:
  void softReset();
};


/**
 * @brief Implements ILI9341 display driver controller.
 *
 * Example:
 *
 *     fabgl::ILI9341Controller DisplayController;
 *
 *     void setup() {
 *       // SCK = 18, MOSI = 23, D/C = 22, RESET = 21, no CS  (WARN: disconnect VGA connector!!)
 *       DisplayController.begin(18, 23, 22, 21, -1, VSPI_HOST);
 *       DisplayController.setResolution(TFT_240x240);
 *
 *       Canvas cv(&DisplayController);
 *       cv.clear();
 *       cv.drawText(0, 0, "Hello World!");
 *     }
 */
class ILI9341Controller : public TFTController {
protected:
  void softReset();
};







} // end of namespace


