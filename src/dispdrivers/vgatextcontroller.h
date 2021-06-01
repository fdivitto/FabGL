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
 * @brief This file contains fabgl::VGATextController definition.
 */


#include <stdint.h>
#include <stddef.h>

#include "driver/gpio.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "devdrivers/swgenerator.h"
#include "dispdrivers/vgacontroller.h"
#include "fonts/font_8x14.h"


namespace fabgl {


#define VGATextController_CHARWIDTH      8    // max 8
#define VGATextController_CHARWIDTHBYTES ((VGATextController_CHARWIDTH + 7) / 8)
#define VGATextController_CHARHEIGHT     14
#define VGATextController_COLUMNS        80
#define VGATextController_ROWS           34
#define VGATextController_WIDTH          640
#define VGATextController_HEIGHT         480

#define VGATextController_MODELINE       VGA_640x480_60Hz






/**
* @brief Represents the VGA text-only controller
*
* The text only VGA controller allows only text, but requires less than 50K of RAM.
* Resolution is fixed at 640x480, with 80 columns by 34 rows, 16 colors.
*
* Text only output is very CPU intensive process and consumes up to 30% of one CPU core. Anyway this allows to have
* more than 290K free for your application.
*
* Graphics (Canvas) aren't possible. Also, some character styles aren't also possible (double size, 132 columns, italic).
*
*
* This example initializes VGA Text Controller with 64 colors (16 usable):
*
*     fabgl::VGATextController VGAController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     VGAController.begin();
*     VGAController.setResolution();
*/
class VGATextController  : public TextualDisplayController {

public:

  VGATextController();
  ~VGATextController();

  /**
   * @brief This is the 8 colors (5 GPIOs) initializer.
   *
   * One GPIO per channel, plus horizontal and vertical sync signals.
   *
   * @param redGPIO GPIO to use for red channel.
   * @param greenGPIO GPIO to use for green channel.
   * @param blueGPIO GPIO to use for blue channel.
   * @param HSyncGPIO GPIO to use for horizontal sync signal.
   * @param VSyncGPIO GPIO to use for vertical sync signal.
   *
   * Example:
   *
   *     // Use GPIO 22 for red, GPIO 21 for green, GPIO 19 for blue, GPIO 18 for HSync and GPIO 5 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
   */
  void begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO);

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   *
   * @param red1GPIO GPIO to use for red channel, MSB bit.
   * @param red0GPIO GPIO to use for red channel, LSB bit.
   * @param green1GPIO GPIO to use for green channel, MSB bit.
   * @param green0GPIO GPIO to use for green channel, LSB bit.
   * @param blue1GPIO GPIO to use for blue channel, MSB bit.
   * @param blue0GPIO GPIO to use for blue channel, LSB bit.
   * @param HSyncGPIO GPIO to use for horizontal sync signal.
   * @param VSyncGPIO GPIO to use for vertical sync signal.
   *
   * Example:
   *
   *     // Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
   */
  void begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO);

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer using default pinout.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   * Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *
   * Example:
   *
   *     VGAController.begin();
   */
  void begin();

  /**
   * @brief Sets fixed resolution
   *
   * This call is required, even you cannot set or change resolution.
   */
  void setResolution(char const * modeline = nullptr, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  /**
   * @brief Sets text map to display
   *
   * This is set automatically by the terminal.
   */
  void setTextMap(uint32_t const * map, int rows);

  /**
   * @brief Adjust columns and rows to the controller limits
   *
   * @param columns If > 0 then it is set to 80.
   * @param rows If > 0 then it is limited to 1..34 range.
   */
  void adjustMapSize(int * columns, int * rows);

  int getColumns()                         { return VGATextController_COLUMNS; }
  int getRows()                            { return VGATextController_ROWS; }

  void enableCursor(bool value)            { m_cursorEnabled = value; }
  void setCursorPos(int row, int col)      { m_cursorRow = row; m_cursorCol = col; m_cursorCounter = 0; }
  void setCursorSpeed(int value)           { m_cursorSpeed = value; }
  void setCursorForeground(Color value);
  void setCursorBackground(Color value);


private:

  void setResolution(VGATimings const& timings);
  void init(gpio_num_t VSyncGPIO);
  void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode);
  void freeBuffers();

  void fillDMABuffers();
  uint8_t packHVSync(bool HSync, bool VSync);
  uint8_t preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync);

  uint8_t IRAM_ATTR preparePixel(RGB222 rgb) { return m_HVSync | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT); }

  static void ISRHandler(void * arg);


  static volatile int        s_scanLine;
  static uint32_t            s_blankPatternDWord;
  static uint32_t *          s_fgbgPattern;
  static int                 s_textRow;
  static bool                s_upperRow;
  static lldesc_t volatile * s_frameResetDesc;

  VGATimings                 m_timings;

  GPIOStream                 m_GPIOStream;
  int                        m_bitsPerChannel;  // 1 = 8 colors, 2 = 64 colors, set by begin()
  lldesc_t volatile *        m_DMABuffers;
  int                        m_DMABuffersCount;

  uint32_t *                 m_lines;

  int                        m_rows;

  volatile uint8_t *         m_blankLine; // for vertical porch lines
  volatile uint8_t *         m_syncLine;  // for vertical sync lines

  intr_handle_t              m_isr_handle;

  // contains H and V signals for visible line
  volatile uint8_t           m_HVSync;

  uint8_t *                  m_charData;
  uint32_t const *           m_map;

  // cursor props
  bool                       m_cursorEnabled;
  int                        m_cursorCounter; // trip from -m_cursorSpeed to +m_cursorSpeed (>= cursor is visible)
  int                        m_cursorSpeed;
  int                        m_cursorRow;
  int                        m_cursorCol;
  uint8_t                    m_cursorForeground;
  uint8_t                    m_cursorBackground;

};



}
