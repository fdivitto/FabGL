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
 * @brief This file contains fabgl::GPIOStream definition.
 */


#include <stdint.h>
#include <stddef.h>


#if __has_include("esp32/rom/lldesc.h")
  #include "esp32/rom/lldesc.h"
#else
  #include "rom/lldesc.h"
#endif

#include "fabglconf.h"





namespace fabgl {



/*
 * This is a square wave generator or DMA->GPIO stream generator that uses APLL internal Audio PLL clock.
 *
 * When FABGLIB_USE_APLL_AB_COEF = 0 (the default) the frequency range is 2651514 Hz to 62500000 Hz.<br>
 * Average error is 21 Hz, Minimum error is 0, Maximum error is 1000 Hz except for range 41666667 Hz to 42708333 Hz while
 * frequency remains fixed at 41666666 Hz (error from 0 Hz to 1041666 Hz) and except for range 42708334 hz to 43748999 Hz while
 * frequency remains fixed at 43750000 Hz (error from 750001 Hz to 1041666 Hz).
 *
 * When FABGLIB_USE_APLL_AB_COEF = 1 the frequency range is 82500 Hz to 62500000 Hz. Unfortunately the output has lot of frequency jittering.<br>
 * Average error is about 7 Hz, Minimum error is 0, Maximum error is 6349 Hz.
 */
class GPIOStream {

public:

  void begin();

  /*
   * Initializes GPIOStream and associate GPIOs to the outputs.
   *
   * div1_onGPIO0 If true the undivided frequency is delivered on GPIO0.
   * div2 Specifies the GPIO where to send frequency / 2 (set GPIO_UNUSED to disable output).
   * div4 Specifies the GPIO where to send frequency / 4 (set GPIO_UNUSED to disable output).
   * div8 Specifies the GPIO where to send frequency / 8 (set GPIO_UNUSED to disable output).
   * div16 Specifies the GPIO where to send frequency / 16 (set GPIO_UNUSED to disable output).
   * div32 Specifies the GPIO where to send frequency / 32 (set GPIO_UNUSED to disable output).
   * div64 Specifies the GPIO where to send frequency / 64 (set GPIO_UNUSED to disable output).
   * div128 Specifies the GPIO where to send frequency / 128 (set GPIO_UNUSED to disable output).
   * div256 Specifies the GPIO where to send frequency / 256 (set GPIO_UNUSED to disable output).
   *
   * Example:
   *
   *     // Outputs 25Mhz on GPIO0 and 6.25Mhz on GPIO5, for 5 seconds
   *     GPIOStream.begin(true, GPIO_UNUSED, GPIO_NUM_5);
   *     GPIOStream.play(25000000);
   *     delay(5000);
   *     // Outputs 20Mhz on GPIO and 5Mhz on GPIO5, for 10 seconds
   *     GPIOStream.play(20000000);
   *     delay(10000);
   *     GPIOStream.stop();
   */
  void begin(bool div1_onGPIO0, gpio_num_t div2 = GPIO_UNUSED, gpio_num_t div4 = GPIO_UNUSED, gpio_num_t div8 = GPIO_UNUSED, gpio_num_t div16 = GPIO_UNUSED, gpio_num_t div32 = GPIO_UNUSED, gpio_num_t div64 = GPIO_UNUSED, gpio_num_t div128 = GPIO_UNUSED, gpio_num_t div256 = GPIO_UNUSED);

  /*
   * Disables all outputs.
   */
  void end();

  /*
   * Sets the main frequency.
   *
   * freq Frequency in Hertz.
   * dmaBuffers Use only to provide custom DMA buffers.
   *
   * Example:
   *
   *     // Set 25MHz as main frequency
   *     GPIOStream.play(25000000);
   */
  void play(int freq, lldesc_t volatile * dmaBuffers = nullptr);

  /*
   * Disables all outputs.
   */
  void stop();

private:

  void setupClock(int freq);
  static void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode);

  bool                m_DMAStarted;
  volatile lldesc_t * m_DMABuffer;
  volatile uint8_t *  m_DMAData;
};





} // end of namespace









