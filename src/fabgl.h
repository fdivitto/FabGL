/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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
 * [www.FabGL.com](http://www.fabgl.com) - 2019-2021 by Fabrizio Di Vittorio (fdivitto2013@gmail.com)
 *
 * [Demos and Tutorials](https://www.youtube.com/user/fdivitto/videos)
 *
 * - - -
 *
 * FabGL is mainly a Graphics Library for ESP32. It implements several display drivers (for direct VGA output and for I2C and SPI LCD drivers).<br>
 * FabGL can also get input from a PS/2 Keyboard and a Mouse. ULP core handles PS/2 ports communications, leaving main CPU cores free to perform other tasks.<br>
 * FabGL also implements: an Audio Engine, a Graphical User Interface (GUI), a Game Engine and an ANSI/VT Terminal.<br>
 *
 * This library works with ESP32 revision 1 and upper.<br>
 *
 * VGA output requires a digital to analog converter (DAC): it can be done by three 270 Ohm resistors to have 8 colors, or by 6 resistors to have 64 colors.<br>
 *
 * There are several fixed and variable width fonts embedded.
 *
 * Unlimited number of sprites are supported. However big sprites and a large amount of them reduces the frame rate and could generate flickering.<br>
 *
 * When there is enough memory (on low resolutions like 320x200), it is possible to allocate two screen buffers, so to implement double buffering.<br>
 * In this case primitives are always drawn on the back buffer.<br>
 *
 * Except for double buffering or when explicitly disabled, all drawings are performed on vertical retracing (using VGA driver), so no flickering is visible.<br>
 * If the queue of primitives to draw is not processed before the vertical retracing ends, then it is interrupted and continued at next retracing.<br>
 *
 * There is a graphical user interface (GUI) with overlapping windows and mouse handling and a lot of widgets (buttons, editboxes, checkboxes, comboboxes, listboxes, etc..).<br>
 *
 * Finally, there is a sound engine, with multiple channels mixed to a mono output. Each channel can generate sine waveforms, square, etc... or custom sampled data.<br>
 * Audio output, like VGA output, is generated using DMA. CPU just mixes audio channels and prepares waveforms.<br>
 *
 * - - -
 *
 * Main classes of FabGL library:
 *    * fabgl::VGAController, device driver for VGA bitmapped output
 *    * fabgl::VGA2Controller, device driver for VGA 2 colors bitmapped output (low RAM requirements, CPU intensive)
 *    * fabgl::VGA4Controller, device driver for VGA 4 colors bitmapped output (low RAM requirements, CPU intensive)
 *    * fabgl::VGA8Controller, device driver for VGA 8 colors bitmapped output (low RAM requirements, CPU intensive)
 *    * fabgl::VGA16Controller, device driver for VGA 16 colors bitmapped output (low RAM requirements, CPU intensive)
 *    * fabgl::VGATextController, device driver for VGA textual output (low RAM requirements, CPU intensive)
 *    * fabgl::SSD1306Controller, device driver for SSD1306 based OLED displays
 *    * fabgl::ST7789Controller, device driver for ST7789 based TFT displays
 *    * fabgl::ILI9341Controller, device driver for ILI9341 based TFT displays
 *    * fabgl::Canvas, that provides a set of drawing primitives (lines, rectangles, text...)
 *    * fabgl::Terminal, that emulates an ANSI/VT100/VT102/etc and up terminal (look at @ref vttest "vttest score")
 *    * fabgl::Keyboard, that controls a PS2 keyboard and translates scancodes to virtual keys or ASCII/ANSI codes
 *    * fabgl::Mouse, that controls a PS2 mouse
 *    * fabgl::Scene abstract class that handles sprites, timings and collision detection
 *    * fabgl::uiApp base class to build Graphical User Interface applications
 *    * fabgl::SoundGenerator to generate sound and music
 *    * fabgl::InputBox to generate simple UI wizards
 *
 * Devices emulation classes:
 *    * fabgl::Z80, Zilog Z80 CPU emulator
 *    * fabgl::i8080, Intel 8080 CPU emulator
 *    * fabgl::MOS6502, MOS 6502 CPU emulator
 *    * fabgl::VIA6522, VIA 6522 emulator
 *
 * Other classes:
 *    * fabgl::I2C, thread safe I2C (Wire) class
 *    * fabgl::DS3231, Real Time Clock driver which uses the thread safe fabgl::I2C library
 *    * fabgl::MCP23S17, 16 bit SPI port expander
 *
 * See @ref confVGA "VGA output" for VGA connection sample schema.
 *
 * See @ref confPS2 "PS/2 ports" for PS/2 connections sample schema.
 *
 * See @ref confAudio "Configuring Audio port" for audio connection sample schema.
 *
 *
 * - - -
 * <CENTER> Installation Tutorial </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/8OTaPQlSTas?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/PCEmulator/PCEmulator.ino IBM PC Emulator Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/3I1U2nEoxIQ?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/SpaceInvaders/SpaceInvaders.ino Space Invaders Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/LL8J7tjxeXA?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/GraphicalUserInterface/GraphicalUserInterface.ino Graphical User Interface - GUI Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/84ytGdiOih0?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/Audio/Audio.ino Audio output demo @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/RQtKFgU7OYI?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/AnsiTerminal/AnsiTerminal.ino Serial terminal connected to MBC2 Z80 board @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/Ww_pH_ZOLqU?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/SimpleTerminalOut/SimpleTerminalOut.ino Simple Terminal Out Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/AmXN0SIRqqU?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/Altair8800/Altair8800.ino Altair 8800 Emulator @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/y0opVifEyS8?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/VIC20/VIC20.ino Commodore VIC20 Emulator @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/ZW427HVWYys?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/NetworkTerminal/NetworkTerminal.ino Network Terminal Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/n5c27-y5tm4?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/ModelineStudio/ModelineStudio.ino Modeline Studio Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/Urp0rPukjzE?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/LoopbackTerminal/LoopbackTerminal.ino Loopback Terminal Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/hQhU5hgWdcU?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/DoubleBuffer/DoubleBuffer.ino Double Buffering Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/TRQcIiWQCJw?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/CollisionDetection/CollisionDetection.ino Collision Detection Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/q3OPSq4HhDE?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 * <CENTER> @link VGA/MultitaskingCPM/MultitaskingCPM.ino Multitasking CP/M Plus Example @endlink </CENTER>
 * @htmlonly <div align="center"> <iframe width="560" height="349" src="http://www.youtube.com/embed/3UevsxMQZ5w?rel=0&loop=1&autoplay=1&modestbranding=1" frameborder="0" allowfullscreen align="middle"> </iframe> </div> @endhtmlonly
 * - - -
 *
 * Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com> <br>
 * Copyright (c) 2019-2021 Fabrizio Di Vittorio. <br>
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
 * @page confVGA VGA output schema
 *
 * VGA output can be configured such as 8 colors or 64 colors are displayed.
 * Eight colors require 5 outputs (R, G, B, H and V), while sixty-four colors require 8 outputs (R0, R1, G0, G1, B0, B1, H and V).
 *
 * Following is an example of outputs configuration and a simple digital to analog converter circuit, with 64 colors, 2 bit per channel and 6 bit per pixel:
 *
 *
 * \image html schema_VGA64.png width=500cm
 *
 *
 * Using above GPIOs the VGA Controller may be initialized in this way:
 *
 *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
 *
 * Note: Do not use GPIO_NUM_2 (LED) for VGA signals.
 *
 */



/**
 * @page confPS2 PS/2 ports schema
 *
 * PS2 Keyboard or Mouse connection uses two GPIOs (data and clock) and requires one 120 Ohm series resistor and one 2K Ohm pullup resistor for each signal:
 *
 * \image html schema_PS2.png width=500cm
 *
 *
 * Using above GPIOs the PS2 Mouse Controller may be initialized in this way:
 *
 *     Mouse.begin(GPIO_NUM_26, GPIO_NUM_27);  // clk, dat
 *
 * When both a mouse and a keyboard are connected initialization must be done directly on PS2Controller, in this way:
 *
 *     fabgl::PS2Controller PS2Controller;
 *     fabgl::Keyboard Keyboard;
 *     fabgl::Mouse Mouse;
 *
 *     // port 0 (keyboard) CLK and DAT, port 1 (mouse) CLK and DAT
 *     PS2Controller.begin(GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_26, GPIO_NUM_27);
 *     // initialize keyboard on port 0 (GPIO33=CLK, GPIO32=DAT)
 *     Keyboard.begin(true, true, 0);
 *     // initialize mouse on port 1 (GPIO26=CLK, GPIO27=DAT)
 *     Mouse.begin(1);
 *
 * A simplified way to configure Mouse and Keyboard, when you have all GPIOs as before is:
 *
 *     fabgl::PS2Controller PS2Controller;
 *     PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);
 */



/**
 * @page confAudio Configuring Audio port
 *
 * Audio output connection uses GPIO 25 and requires a simple low-pass filter and peak limiter. This works for me (you may do a better job):
 *
 * \image html schema_audio.png width=500cm
 *
 */




/**
 * @example VGA/Altair8800/Altair8800.ino Altair 8800 Emulator - with ADM-31, ADM-3A, Kaypro, Hazeltine 1500 and Osborne I terminal emulation
 * @example VGA/AnsiTerminal/AnsiTerminal.ino Serial VT/ANSI Terminal
 * @example VGA/Audio/Audio.ino Audio demo with GUI
 * @example VGA/ClassicRacer/ClassicRacer.ino Racer game, from Carles Oriol
 * @example VGA/CollisionDetection/CollisionDetection.ino fabgl::Scene, sprites and collision detection example
 * @example VGA/DirectVGA/DirectVGA.ino Sample usage of VGADirectController base class for direct VGA control
 * @example VGA/DirectVGA_ElMuro/DirectVGA_ElMuro.ino Sample breakout-style game using VGADirectController base class, from Carles Oriol
 * @example VGA/DoubleBuffer/DoubleBuffer.ino Show double buffering usage
 * @example VGA/FileBrowser/FileBrowser.ino File browser (SPIFFS and SDCard) with GUI
 * @example VGA/GraphicalUserInterface/GraphicalUserInterface.ino Graphical User Interface - GUI demo
 * @example VGA/HardwareTest/HardwareTest.ino Hardware test with GUI
 * @example VGA/LoopbackTerminal/LoopbackTerminal.ino Loopback VT/ANSI Terminal
 * @example VGA/ModelineStudio/ModelineStudio.ino Test VGA output at predefined resolutions or custom resolution by specifying linux-like modelines
 * @example VGA/MouseOnScreen/MouseOnScreen.ino PS/2 mouse and mouse pointer on screen
 * @example VGA/MultitaskingCPM/MultitaskingCPM.ino Multitasking - Multisession CP/M 3 compatible system. Supports directories and FAT32 file system with SD Cards.
 * @example VGA/NetworkTerminal/NetworkTerminal.ino Network VT/ANSI Terminal
 * @example VGA/PCEmulator/PCEmulator.ino A i8086 based IBM PC emulator (runs FreeDOS, MS-DOS, CPM86, Linux-ELK, Windows 3.0)
 * @example VGA/SimpleTerminalOut/SimpleTerminalOut.ino Simple terminal - output only
 * @example VGA/SimpleTextTerminalOut/SimpleTextTerminalOut.ino Simple terminal text-only mode - output only
 * @example VGA/Songs/Songs.ino Music and sound demo, from Carles Oriol
 * @example VGA/SpaceInvaders/SpaceInvaders.ino Space invaders full game
 * @example VGA/Sprites/Sprites.ino Simple sprites animation
 * @example VGA/VIC20/VIC20.ino Commodore VIC20 Emulator
 * @example VGA/InputBox/InputBox.ino InputBox UI wizard
 *
 * @example SSD1306_OLED/128x32/CollisionDetection/CollisionDetection.ino fabgl::Scene, sprites and collision detection example
 * @example SSD1306_OLED/128x32/SimpleTerminalOut/SimpleTerminalOut.ino Simple terminal - output only
 * @example SSD1306_OLED/128x64/CollisionDetection/CollisionDetection.ino fabgl::Scene, sprites and collision detection example
 * @example SSD1306_OLED/128x64/DoubleBuffer/DoubleBuffer.ino Show double buffering usage
 * @example SSD1306_OLED/128x64/RTClock/RTClock.ino DateTime clock using DS3231 RTC
 * @example SSD1306_OLED/128x64/SimpleTerminalOut/SimpleTerminalOut.ino Simple terminal - output only
 * @example SSD1306_OLED/128x64/NetworkTerminal/NetworkTerminal.ino Network VT/ANSI Terminal
 * @example SSD1306_OLED/128x64/UI/UI.ino Graphic User Interface - GUI demo
 *
 * @example ST7789_TFT/240x240/SimpleTerminalOut/SimpleTerminalOut.ino Simple terminal - output only
 * @example ST7789_TFT/240x240/FileBrowser/FileBrowser.ino File browser (SPIFFS and SDCard) with GUI
 * @example ST7789_TFT/240x240/Sprites/Sprites.ino Simple sprites animation
 * @example ST7789_TFT/240x240/DoubleBuffer/DoubleBuffer.ino Show double buffering usage
 *
 * @example Others/KeyboardScanCodes/KeyboardScanCodes.ino PS/2 keyboard scan codes
 * @example Others/KeyboardVirtualKeys/KeyboardVirtualKeys.ino PS/2 keyboard virtual keys
 * @example Others/MouseStudio/MouseStudio.ino PS/2 mouse events
 */


/*!
    \defgroup Enumerations Enumerations
        Enumeration types
*/


#include "fabutils.h"
#include "fabfonts.h"
#include "terminal.h"
#include "displaycontroller.h"
#include "dispdrivers/vgacontroller.h"
#include "dispdrivers/SSD1306Controller.h"
#include "dispdrivers/TFTControllerSpecif.h"
#include "dispdrivers/vgatextcontroller.h"
#include "dispdrivers/vga2controller.h"
#include "dispdrivers/vga4controller.h"
#include "dispdrivers/vga8controller.h"
#include "dispdrivers/vga16controller.h"
#include "dispdrivers/vgadirectcontroller.h"
#include "fabui.h"
#include "inputbox.h"
#include "comdrivers/ps2controller.h"
#include "comdrivers/tsi2c.h"
#include "devdrivers/keyboard.h"
#include "devdrivers/mouse.h"
#include "devdrivers/DS3231.h"
#include "scene.h"
#include "collisiondetector.h"
#include "devdrivers/soundgen.h"



using fabgl::Color;
using fabgl::VGAScanStart;
using fabgl::GlyphOptions;
using fabgl::Scene;
using fabgl::Bitmap;
using fabgl::Sprite;
using fabgl::CollisionDetector;
using fabgl::Point;
using fabgl::Size;
using fabgl::Rect;
using fabgl::MouseDelta;
using fabgl::MouseStatus;
using fabgl::CursorName;
using fabgl::uiObject;
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
using fabgl::uiStyle;
using fabgl::uiWindowStyle;
using fabgl::uiFrameStyle;
using fabgl::uiScrollableControlStyle;
using fabgl::uiButtonStyle;
using fabgl::uiTextEditStyle;
using fabgl::uiLabelStyle;
using fabgl::uiImageStyle;
using fabgl::uiPanelStyle;
using fabgl::uiPaintBoxStyle;
using fabgl::uiListBoxStyle;
using fabgl::uiComboBoxStyle;
using fabgl::uiCheckBoxStyle;
using fabgl::uiSliderStyle;
using fabgl::uiColorListBox;
using fabgl::uiColorBox;
using fabgl::uiColorComboBox;
using fabgl::uiProgressBar;
using fabgl::SoundGenerator;
using fabgl::uiMessageBoxResult;
using fabgl::SineWaveformGenerator;
using fabgl::SquareWaveformGenerator;
using fabgl::NoiseWaveformGenerator;
using fabgl::VICNoiseGenerator;
using fabgl::TriangleWaveformGenerator;
using fabgl::SawtoothWaveformGenerator;
using fabgl::SamplesGenerator;
using fabgl::WaveformGenerator;
using fabgl::TermType;
using fabgl::SupportedTerminals;
using fabgl::PS2Preset;
using fabgl::PS2DeviceType;
using fabgl::KbdMode;
using fabgl::VirtualKey;
using fabgl::VirtualKeyItem;
using fabgl::uiKeyEventInfo;
using fabgl::uiCustomListBox;
using fabgl::uiFileBrowser;
using fabgl::FileBrowser;
using fabgl::ModalWindowState;
using fabgl::Canvas;
using fabgl::PixelFormat;
using fabgl::RGB222;
using fabgl::RGBA2222;
using fabgl::RGB888;
using fabgl::RGBA8888;
using fabgl::FlowControl;
using fabgl::LineEditorSpecialChar;
using fabgl::LineEditor;
using fabgl::TerminalController;
using fabgl::LineEnds;
using fabgl::CharStyle;
using fabgl::TerminalTransition;
using fabgl::SupportedLayouts;
using fabgl::CoreUsage;
using fabgl::InputResult;
using fabgl::InputBox;


