/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
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



#pragma once


/**
 * @file
 *
 * @brief This file is the all in one include file. Application can just include this file to use FabGL library.
 *
 */



/**
 * @mainpage FabGL Library
 *
 * @htmlonly
 * <a href="https://www.github.com/fdivitto/fabgl"> <img src="github.png" style="width:230px;height:80px;border:0;"> </a>
 * @endhtmlonly
 *
 * [www.FabGL.com](http://www.fabgl.com) - 2019 by Fabrizio Di Vittorio (fdivitto2013@gmail.com)
 *
 * - - -
 *
 * This is a VGA Controller, PS/2 Keyboard and Mouse Controller, Graphics Library, Audio Engine, Graphical User Interface (GUI), Game Engine and ANSI/VT Terminal for the ESP32.<br>
 * This library works with ESP32 revision 1 or upper.
 *
 * VGA output requires a digital to analog converter (DAC): it can be done by three 270 Ohm resistors to have 8 colors, or by 6 resistors to have 64 colors.
 *
 * Three fixed width fonts are embedded to best represents 80x25 or 132x25 text screen, at 640x350 resolution. However other fonts are embedded, even with variable width.
 *
 * Sprites can have up to 64 colors (RGB, 2 bits per channel + transparency).<br>
 * A sprite has associated one o more bitmaps, even of different size. Bitmaps (frames) can be selected in sequence to create animations.<br>
 * Unlimited number of sprites are supported. However big sprites and a large amount of them reduces the frame rate and could generate flickering.
 *
 * When there is enough memory (on low resolutions like 320x200), it is possible to allocate two screen buffers, so to implement double buffering.<br>
 * In this case drawing primitives always draw on the back buffer.
 *
 * Except for double buffering or when explicitly disabled, all drawings are performed on vertical retracing, so no flickering is visible.<br>
 * If the queue of primitives to draw is not processed before the vertical retracing ends, then it is interrupted and continued at next retracing.
 *
 * There is a graphical user interface (GUI) with overlapping windows and mouse handling and a lot of widgets (buttons, editboxes, checkboxes, comboboxes, listboxes, etc..).
 *
 * Finally, there is a sound engine, with multiple channels mixed to a mono output. Each channel can generate sine waveforms, square, etc... or custom sampled data.
 *
 * - - -
 *
 * The main classes of FabGL library are:
 *    * fabgl::VGAControllerClass (instanced as \b VGAController), that controls the hardware. Use to setup GPIOs, screen resolution and adjust the screen position.
 *    * fabgl::CanvasClass (instanced as \b Canvas), that provides a set of drawing primitives (lines, rectangles, text...).
 *    * fabgl::TerminalClass, that emulates an ANSI/VT100/VT102 and up terminal (look at @ref vttest "vttest score").
 *    * fabgl::KeyboardClass (instanced as \b Keyboard), that controls a PS2 keyboard and translates scancodes to virtual keys or ASCII/ANSI codes.
 *    * fabgl::MouseClass (instanced as \b Mouse), that controls a PS2 mouse.
 *    * fabgl::Scene abstract class that handles sprites, timings and collision detection.
 *    * fabgl::uiApp base class to build Graphical User Interface applications
 *    * fabgl::SoundGenerator to generate sound and music.
 *
 * See @ref confVGA "Configuring VGA outputs" for VGA connection sample schema.
 *
 * See @ref confPS2 "Configuring PS/2 port" for PS/2 connection sample schema.
 *
 * See @ref confAudio "Configuring Audio port" for audio connection sample schema.
 *
 * - - -
 * <CENTER> @link SpaceInvaders/SpaceInvaders.ino Space Invaders Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/LL8J7tjxeXA?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link GraphicalUserInterface/GraphicalUserInterface.ino Graphical User Interface - GUI Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/84ytGdiOih0?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link Audio/Audio.ino Audio output demo @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/RQtKFgU7OYI?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link SimpleTerminalOut/SimpleTerminalOut.ino Simple Terminal Out Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/AmXN0SIRqqU?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link Altair8800/Altair8800.ino Altair 8800 Emulator @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/y0opVifEyS8?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link NetworkTerminal/NetworkTerminal.ino Network Terminal Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/n5c27-y5tm4?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link ModelineStudio/ModelineStudio.ino Modeline Studio Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/Urp0rPukjzE?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link LoopbackTerminal/LoopbackTerminal.ino Loopback Terminal Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/hQhU5hgWdcU?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link DoubleBuffer/DoubleBuffer.ino Double Buffering Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/TRQcIiWQCJw?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link CollisionDetection/CollisionDetection.ino Collision Detection Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/q3OPSq4HhDE?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 *
 * Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com> <br>
 * Copyright (c) 2019 Fabrizio Di Vittorio. <br>
 * All rights reserved. <br>
 *
 * This file is part of FabGL Library.
 *
 * FabGL is free software: you can redistribute it and/or modify<br>
 * it under the terms of the GNU General Public License as published by<br>
 * the Free Software Foundation, either version 3 of the License, or<br>
 * (at your option) any later version.<br>
 *
 * FabGL is distributed in the hope that it will be useful,<br>
 * but WITHOUT ANY WARRANTY; without even the implied warranty of<br>
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the<br>
 * GNU General Public License for more details.<br>
 *
 * You should have received a copy of the GNU General Public License<br>
 * along with FabGL.  If not, see <http://www.gnu.org/licenses/>.<br>
 */



/**
 * @page confVGA Configuring VGA outputs
 *
 * VGA output can be configured such as 8 colors or 64 colors are displayed.
 * Eight colors require 5 outputs (R, G, B, H and V), while sixty-four colors require 8 outputs (R0, R1, G0, G1, B0, B1, H and V).
 *
 * Following is an example of outputs configuration and a simple digital to analog converter circuit:
 *
 *
 *       === 8 colors, 1 bit per channel, 3 bit per pixel ===
 *
 *       Sample connection scheme:
 *                            -----------
 *        GPIO22 (red0) ------|R 270 Ohm|---- VGA_R
 *                            -----------
 *
 *                            -----------
 *        GPIO21 (green0) ----|R 270 Ohm|---- VGA_G
 *                            -----------
 *
 *                            -----------
 *        GPIO19 (blue0) -----|R 270 Ohm|---- VGA_B
 *                            -----------
 *
 *        GPIO18 ---------------------------- VGA_HSYNC
 *
 *        GPIO5  ---------------------------- VGA_VSYNC
 *
 *       Using above GPIOs the VGA Controller may be initialized in this way:
 *         VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
 *
 *       === 64 colors, 2 bit per channel, 6 bit per pixel ===
 *
 *            One resistor for each R0, R1, G0, G1, B0 and B1. Low bit (LSB) should have
 *            twice resistance value than high bit (MSB), for example 800Ohm (LSB) and 400Ohm (MSB).
 *
 *                            ------------
 *        GPIO22 (red1) ------|R 400 Ohm |------*---- VGA_R
 *                            ------------      |
 *                            ------------      |
 *        GPIO21 (red0) ------|R 800 Ohm |------*
 *                            ------------
 *
 *                            ------------
 *        GPIO19 (green1) ----|R 400 Ohm |------*---- VGA_G
 *                            ------------      |
 *                            ------------      |
 *        GPIO18 (green0) ----|R 800 Ohm |------*
 *                            ------------
 *
 *                            ------------
 *        GPIO5 (blue1) ------|R 400 Ohm |------*---- VGA_B
 *                            ------------      |
 *                            ------------      |
 *        GPIO4 (blue0) ------|R 800 Ohm |------*
 *                            ------------
 *
 *        GPIO23 ------------------------------------ VGA_HSYNC
 *
 *        GPIO15 ------------------------------------ VGA_VSYNC
 *
 *       Using above GPIOs the VGA Controller may be initialized in this way:
 *         VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
 *
 *     Note: Do not connect GPIO_NUM_2 (led) to the VGA signals.
 *
 */



/**
 * @page confPS2 Configuring PS/2 port
 *
 * PS2 Keyboard connection uses two GPIOs (data and clock) and requires one 120 Ohm series resistor and one 2K Ohm pullup resistor for each signal:
 *
 *                                             +5V
 *                                              |
 *                                              |
 *                                              *-----+
 *                                              |     |
 *                                             ---   ---
 *                                             | |   | |
 *                                             |R|   |R|
 *                                             |2|   |2|
 *                                             |K|   |K|
 *                                             | |   | |
 *                                             ---   ---
 *                            ------------      |     |
 *        GPIO33 (CLK)    ----|R 120 Ohm |------*--------- PS/2 KEYBOARD CLK
 *                            ------------            |
 *                            ------------            |
 *        GPIO32 (DAT)    ----|R 120 Ohm |------------*--- PS/2 KEYBOARD DAT
 *                            ------------
 *
 *       Using above GPIOs the PS2 Keyboard Controller may be initialized in this way:
 *         Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);  // clk, dat
 *
 *
 *
 * PS2 Mouse connection also uses two GPIOs (data and clock) and requires one 120 Ohm series resistor and one 2K Ohm pullup resistor for each signal:
 *
 *                                             +5V
 *                                              |
 *                                              |
 *                                              *-----+
 *                                              |     |
 *                                             ---   ---
 *                                             | |   | |
 *                                             |R|   |R|
 *                                             |2|   |2|
 *                                             |K|   |K|
 *                                             | |   | |
 *                                             ---   ---
 *                            ------------      |     |
 *        GPIO26 (CLK)    ----|R 120 Ohm |------*--------- PS/2 MOUSE CLK
 *                            ------------            |
 *                            ------------            |
 *        GPIO27 (DAT)    ----|R 120 Ohm |------------*--- PS/2 MOUSE DAT
 *                            ------------
 *
 *       Using above GPIOs the PS2 Mouse Controller may be initialized in this way:
 *         Mouse.begin(GPIO_NUM_26, GPIO_NUM_27);  // clk, dat
 *
 *
 *       When both a mouse and a keyboard are connected initialization must be done directly on PS2Controller, in this way:
 *         // port 0 (keyboard) CLK and DAT, port 1 (mouse) CLK and DAT
 *         PS2Controller.begin(GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_26, GPIO_NUM_27);
 *         // initialize keyboard on port 0 (GPIO33=CLK, GPIO32=DAT)
 *         Keyboard.begin(true, true, 0);
 *         // initialize mouse on port 1 (GPIO26=CLK, GPIO27=DAT)
 *         Mouse.begin(1);
 */



/**
 * @page confAudio Configuring Audio port
 *
 * Audio output connection uses GPIO 25 and requires a simple low-pass filter and peak limiter. This works for me (you may do better):
 *
 *
 *                                                                    10uF
 *                           -------------                          + | | -
 *          GPIO25 ----------| R 270 Ohm |-------*---------*----------| |-------> OUT AUX LINE
 *                           -------------       |         |          | |
 *                                               |         |
 *                                               |         |
 *                                               |        ---
 *                                               |        |R|
 *                                      100nF  -----      | |
 *                                             -----      |1|
 *                                               |        |5|
 *                                               |        |0|
 *                                               |        ---
 *                                               |         |
 *                                               |         |
 *                                               +----*----+
 *                                                    |
 *                                                    |
 *                                                  ----- GND
 *                                                   ---
 *                                                    -
 *
 */




/**
 * @example AnsiTerminal/AnsiTerminal.ino Serial VT/ANSI Terminal
 * @example CollisionDetection/CollisionDetection.ino fabgl::Scene, sprites and collision detection example
 * @example DoubleBuffer/DoubleBuffer.ino Show double buffering usage
 * @example Altair8800/Altair8800.ino Altair 8800 Emulator - with ADM-31, ADM-3A, Kaypro, Hazeltine 1500 and Osborne I terminal emulation
 * @example KeyboardStudio/KeyboardStudio.ino PS/2 keyboard full example (scancodes, virtual keys, LEDs control...)
 * @example LoopbackTerminal/LoopbackTerminal.ino Loopback VT/ANSI Terminal
 * @example ModelineStudio/ModelineStudio.ino Test VGA output at predefined resolutions or custom resolution by specifying linux-like modelines
 * @example MouseStudio/MouseStudio.ino PS/2 mouse events
 * @example MouseOnScreen/MouseOnScreen.ino PS/2 mouse and mouse pointer on screen
 * @example NetworkTerminal/NetworkTerminal.ino Network VT/ANSI Terminal
 * @example SimpleTerminalOut/SimpleTerminalOut.ino Simple terminal - output only
 * @example SpaceInvaders/SpaceInvaders.ino Space invaders full game
 * @example SquareWaveGenerator/SquareWaveGenerator.ino Show usage of fabgl::SquareWaveGeneratorClass to generate square waves at various frequencies
 * @example GraphicalUserInterface/GraphicalUserInterface.ino Graphical User Interface - GUI demo
 * @example Audio/Audio.ino Audio demo
 */


/*!
    \defgroup Enumerations
        Enumeration types
*/


#include "fabutils.h"
#include "terminal.h"
#include "vgacontroller.h"
#include "ps2controller.h"
#include "keyboard.h"
#include "mouse.h"
#include "scene.h"
#include "collisiondetector.h"
#include "soundgen.h"



using fabgl::Color;
using fabgl::ScreenBlock;
using fabgl::GlyphOptions;
using fabgl::Scene;
using fabgl::RGB;
using fabgl::Bitmap;
using fabgl::Sprite;
using fabgl::CollisionDetector;
using fabgl::Point;
using fabgl::Size;
using fabgl::Rect;
using fabgl::MouseDelta;
using fabgl::MouseStatus;
using fabgl::CursorName;
using fabgl::TerminalClass;
using fabgl::uiButtonKind;
using fabgl::uiTimerHandle;
using fabgl::uiTextEdit;
using fabgl::uiApp;
using fabgl::uiFrame;
using fabgl::uiButton;
using fabgl::uiLabel;
using fabgl::uiImage;
using fabgl::uiPanel;
using fabgl::uiMessageBoxIcon;
using fabgl::uiPaintBox;
using fabgl::uiOrientation;
using fabgl::uiListBox;
using fabgl::uiComboBox;
using fabgl::uiCheckBox;
using fabgl::uiCheckBoxKind;
using fabgl::uiSlider;
using fabgl::SoundGenerator;
using fabgl::uiMessageBoxResult;
using fabgl::SineWaveformGenerator;
using fabgl::SquareWaveformGenerator;
using fabgl::NoiseWaveformGenerator;
using fabgl::TriangleWaveformGenerator;
using fabgl::SawtoothWaveformGenerator;
using fabgl::SamplesGenerator;
using fabgl::WaveformGenerator;
using fabgl::TermType;
using fabgl::PS2Preset;
using fabgl::KbdMode;
using fabgl::VirtualKey;
using fabgl::uiKeyEventInfo;
using fabgl::uiCustomListBox;
using fabgl::uiFileBrowser;
using fabgl::FileBrowser;
using fabgl::ModalWindowState;


