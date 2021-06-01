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
 * @brief This file contains fabgl::Terminal definition.
 */


#ifdef ARDUINO
  #include "Arduino.h"
  #include "Stream.h"
#endif

#include <ctype.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "fabglconf.h"
#include "canvas.h"
#include "devdrivers/keyboard.h"
#include "terminfo.h"
#include "devdrivers/soundgen.h"



/**
 * @page vttest VTTEST VT100/VT102 Compatibility Test Score Sheet
 *
 * Results of remote execution on 640x350 screen using default 80 and 132 column fonts:
 *
 *     1. Test of cursor movements
 *
 *     [X]   1. Text inside frame of E's inside frame of *'s and +'s, 80 columns
 *     [X]   2. Text inside frame of E's inside frame of *'s and +'s, 132 columns
 *     [X]   3. Cursor-control chars inside ESC sequences
 *     [X]   4. Leading 0's in ESC sequences
 *
 *     2. Test of screen features
 *
 *     [X]   5. Three identical lines of *'s (test of wrap mode)
 *     [X]   6. Test of tab setting/resetting
 *     [X]   7. 132-column mode, light background
 *     [X]   8. 80-column mode, light background
 *     [X]   9. 132-column mode, dark background
 *     [X]  10. 80-column mode, dark background
 *     [X]  11. Soft scroll down
 *     [X]  12. Soft scroll up / down
 *     [X]  13. Jump scroll down
 *     [X]  14. Jump scroll up / down
 *     [X]  15. Origin mode test (2 parts)
 *
 *          Graphic Rendition test pattern, dark background
 *
 *     [X]  16. Normal ("vanilla")
 *     [X]  17. Normal underlined distinct from normal
 *     [X]  18. Normal blink distinct from all above
 *     [X]  19. Normal underline blink distinct from all above
 *     [X]  20. Normal reverse ("negative") distinct from all above
 *     [X]  21. Normal underline reverse distinct from all above
 *     [X]  22. Normal blink reverse distinct from all above
 *     [X]  23. Normal underline blink reverse distinct from all above
 *     [X]  24. Bold distinct from all above
 *     [X]  25. Bold underlined distinct from all above
 *     [X]  26. Bold blink distinct from all above
 *     [X]  27. Bold underline blink distinct from all above
 *     [X]  28. Bold reverse ("negative") distinct from all above
 *     [X]  29. Bold underline reverse distinct from all above
 *     [X]  30. Bold blink reverse distinct from all above
 *     [X]  31. Bold underline blink reverse distinct from all above
 *
 *          Graphic Rendition test pattern, light background
 *
 *     [X]  32. Normal ("vanilla")
 *     [X]  33. Normal underlined distinct from normal
 *     [X]  34. Normal blink distinct from all above
 *     [X]  35. Normal underline blink distinct from all above
 *     [X]  36. Normal reverse ("negative") distinct from all above
 *     [X]  37. Normal underline reverse distinct from all above
 *     [X]  38. Normal blink reverse distinct from all above
 *     [X]  39. Normal underline blink reverse distinct from all above
 *     [X]  40. Bold distinct from all above
 *     [X]  41. Bold underlined distinct from all above
 *     [X]  42. Bold blink distinct from all above
 *     [X]  43. Bold underline blink distinct from all above
 *     [X]  44. Bold reverse ("negative") distinct from all above
 *     [X]  45. Bold underline reverse distinct from all above
 *     [X]  46. Bold blink reverse distinct from all above
 *     [X]  47. Bold underline blink reverse distinct from all above
 *
 *          Save/Restore Cursor
 *
 *     [X]  48. AAAA's correctly placed
 *     [X]  49. Lines correctly rendered (middle of character cell)
 *     [X]  50. Diamonds correctly rendered
 *
 *     3. Test of character sets
 *
 *     [X]  51. UK/National shows Pound Sterling sign in 3rd position
 *     [X]  52. US ASCII shows number sign in 3rd position
 *     [X]  53. SO/SI works (right columns identical with left columns)
 *     [X]  54. True special graphics & line drawing chars, not simulated by ASCII
 *
 *     4. Test of double-sized chars
 *
 *          Test 1 in 80-column mode:
 *
 *     [X]  55. Left margin correct
 *     [X]  56. Width correct
 *
 *          Test 2 in 80-column mode:
 *
 *     [X]  57. Left margin correct
 *     [X]  58. Width correct
 *
 *          Test 1 in 132-column mode:
 *
 *     [X]  59. Left margin correct
 *     [X]  60. Width correct
 *
 *          Test 2 in 132-column mode:
 *
 *     [X]  61. Left margin correct
 *     [X]  62. Width correct
 *
 *     [X]  63. "The man programmer strikes again" test pattern
 *     [X]  64. "Exactly half the box should remain"
 *
 *     5. Test of keyboard
 *
 *     [X]  65. LEDs.
 *     [X]  66. Autorepeat
 *     [X]  67. "Press each key" (ability to send each ASCII graphic char)
 *     [X]  68. Arrow keys (ANSI/Cursor key mode reset)
 *     [X]  69. Arrow keys (ANSI/Cursor key mode set)
 *     [X]  70. Arrow keys VT52 mode
 *     [ ]  71. PF keys numeric mode
 *     [ ]  72. PF keys application mode
 *     [ ]  73. PF keys VT52 numeric mode
 *     [ ]  74. PF keys VT52 application mode
 *     [ ]  75. Send answerback message from keyboard
 *     [X]  76. Ability to send every control character
 *
 *     6. Test of Terminal Reports
 *
 *     [ ]  77. Respond to ENQ with answerback
 *     [X]  78. Newline mode set
 *     [X]  79. Newline mode reset
 *     [X]  80. Device status report 5
 *     [X]  81. Device status report 6
 *     [ ]  82. Device attributes report
 *     [ ]  83. Request terminal parameters 0
 *     [ ]  84. Request terminal parameters 1
 *
 *     7. Test of VT52 submode
 *
 *     [X]  85. Centered rectangle
 *     [X]  86. Normal character set
 *     [X]  87. Graphics character set
 *     [X]  88. Identify query
 *
 *     8. VT102 Features
 *
 *     [X]  89. Insert/delete line, 80 columns
 *     [X]  90. Insert (character) mode, 80 columns
 *     [X]  91. Delete character, 80 columns
 *     [X]  92. Right column staggered by 1 (normal chars), 80 columns
 *     [X]  93. Right column staggered by 1 (double-wide chars), 80 columns
 *     [X]  94. ANSI insert character, 80 columns
 *     [X]  95. Insert/delete line, 132 columns
 *     [X]  96. Insert (character) mode, 132 columns
 *     [X]  97. Delete character, 132 columns
 *     [X]  98. Right column staggered by 1 (normal chars), 132 columns
 *     [X]  99. Right column staggered by 1 (double-wide chars), 132 columns
 *     [X] 100. ANSI insert character, 132 columns
 *
 *     9. Extra credit
 *
 *     [X] 101. True soft (smooth) scroll
 *     [X] 102. True underline
 *     [X] 103. True blink
 *     [X] 104. True double-high/wide lines, not simulated
 *     [ ] 105. Reset terminal (*)
 *     [ ] 106. Interpret controls (debug mode) (*)
 *     [ ] 107. Send BREAK (250 msec) (*)
 *     [ ] 108. Send Long BREAK (1.5 sec) (*)
 *     [ ] 109. Host-controlled transparent / controller print (*)
 *     [ ] 110. Host-controlled autoprint (*) 
 */


 /**
 * @page specialTermEscapes FabGL Specific Terminal Sequences
 *
 * This is a list of FabGL specific terminal sequences. These are used to get access to features like graphics/audio/etc not else available using standard escape sequences.<br>
 * Specific sequences are also useful when you don't know which terminal emulation has been set and you still want to control screen using known escape sequences.<br>
 * Specific sequences begin with an ESC (ASCII 27h, 0x1B) plus an underscore ("_", ASCII 90h, 0x5f). Follows the actual command which is composed by a single letter.<br>
 * After the command letter it is possible to specify parameters (if required) separated by semicolons (";"). An ending dollar sign ($, ASCII 36h, 0x24) ends the escape sequence.<br>
 * Some commands return a response sequence: this has a fixed length and starts with a dollar sign ($).<br><br>
 *
 * <br>
 * <hr>
 *
 * <br> <b> Setup Analog to Digital Converter (ADC) </b>
 *
 *     Sequence:
 *       ESC "_A" resolution ";" attenuation ";" gpio "$"
 *
 *     Parameters:
 *       resolution:
 *           "9", "10", "11", "12"
 *       attenuation:
 *           "0" = 0dB   (reduced to 1/1), full-scale voltage 1.1 V, accurate between 100 and 950 mV
 *           "1" = 2.5dB (reduced to 1/1.34), full-scale voltage 1.5 V, accurate between 100 and 1250 mV
 *           "2" = 6dB   (reduced to 1/2), full-scale voltage 2.2 V, accurate between 150 to 1750 mV
 *           "3" = 11dB  (reduced to 1/3.6), full-scale voltage 3.9 V (maximum volatage is still 3.3V!!), accurate between 150 to 2450 mV
 *       gpio:
 *           "32"..."39"
 *
 *     Example:
 *       // setup GPIO number 36 as analog input, 11dB attenuation (3) and 12 bit resolution
 *       Terminal.write("\e_A12;3;36$");
 *
 * <br> <b> Read analog input (ADC) from specified gpio </b>
 *
 *     Sequence:
 *       ESC "_C" gpio "$"
 *
 *     Parameters:
 *       gpio:
 *           "32"..."39"
 *
 *     Returns:
 *       "$"
 *       hex value (3 characters)
 *
 *     Example:
 *       // Request to read ADC from GPIO number 36
 *       Terminal.write("\e_C36$");  // will return something like "$1A5" (value 421)
 *
 * <br> <b> Setup digital pin for input or output </b>
 *
 *     Sequence:
 *       ESC "_D" mode gpio "$"
 *
 *     Parameters:
 *       mode:
 *           "-" = disable input/output
 *           "I" = input only
 *           "O" = output only
 *           "D" = output only with open-drain
 *           "E" = output and input with open-drain
 *           "X" = output and input
 *       gpio:
 *           "0"..."39" (not all usable!)
 *
 *     Example:
 *       // Setup gpio number 12 as output (O)
 *       Terminal.write("\e_DO12$");
 *
 * <br> <b> Set digital output pin state </b>
 *
 *     Sequence:
 *       ESC "_W" value gpio "$"
 *
 *     Parameters:
 *       value:
 *           0 or '0' or 'L' = low (and others)
 *           1 or '1' or 'H' = high
 *       gpio:
 *           "0"..."39" (not all usable!)
 *
 *     Example:
 *       // Set gpio 12 to High
 *       Terminal.write("\e_WH12$");
 *       // Set gpio 12 to Low
 *       Terminal.write("\e_WL12$");
 *
 * <br> <b> Read digital input pin state </b>
 *
 *     Sequence:
 *       ESC "_R" gpio "$"
 *
 *     Parameters:
 *       gpio:
 *           "0"..."39" (not all usable!)
 *
 *     Returns:
 *       "$"
 *       '0' = low, '1' = high
 *
 *     Example:
 *       // Read state of gpio 12
 *       Terminal.write("\e_R12$");  // you will get "$0" or "$1"
 *
 * <br> <b> Clear terminal area with background color </b>
 *
 *     Sequence:
 *       ESC "_B" "$"
 *
 *     Example:
 *       // Clear terminal area
 *       Terminal.write("\e_B$");
 *
 * <br> <b> Enable or disable cursor </b>
 *
 *     Sequence:
 *       ESC "_E" state "$"
 *
 *     Parameters:
 *       state:
 *           "0" = disable cursor
 *           "1" = enable cursor
 *
 *     Example:
 *       // Disable cursor
 *       Terminal.write("\e_E0$");
 *
 * <br> <b> Set cursor position </b>
 *
 *     Sequence:
 *       ESC "_F" column ";" row "$"
 *
 *     Parameters:
 *       column:
 *           column (1 = first column)
 *       row:
 *           row (1 = first row)
 *
 *     Example:
 *       // Print "Hello" at column 1 of row 10
 *       Terminal.write("\e_F1;10$");
 *       Terminal.write("Hello");
 *
 * <br> <b> Enable/disable mouse </b>
 *
 *     Sequence:
 *       ESC "_H" value "$"
 *
 *     Parameters:
 *       value:
 *           '0' (and others) = enable mouse
 *           '1' = disable mouse
 *
 *     Example:
 *       // Enable mouse
 *       Terminal.write("\e_H1$");
 *
 * <br> <b> Get mouse position </b>
 *
 *     Sequence:
 *       ESC "_M" "$"
 *
 *     Returns:
 *       "$"
 *       X position: 3 hex digits
 *       ";"
 *       Y position: 3 hex digits
 *       ";"
 *       Scroll wheel delta: 1 hex digit (0..F)
 *       ";"
 *       Pressed buttons: 1 hex digit, composed as follow:
 *           bit 1 = left button
 *           bit 2 = middle button
 *           bit 3 = right button
 *
 *     Example:
 *       // Get mouse status
 *       Terminal.write("\e_M$"); // you will get something like "$1AB;08A;0;1"
 *
 * <br> <b> Delay milliseconds </b>
 *
 *     Sequence:
 *       ESC "_Y" value "$"
 *
 *     Parameters:
 *       value:
 *           number (milliseconds)
 *
 *     Returns:
 *       "$": returns the dollar sign after specified number of milliseconds
 *
 *     Example:
 *       // Get mouse status
 *       Terminal.write("\e_Y500$"); // you will get "$" after 500 milliseconds
 *
 * <br> <b> Play sound </b>
 *
 *     Sequence:
 *       ESC "_S" waveform ";" frequency ";" duration ";" volume "$"
 *
 *     Parameters:
 *       waveform:
 *           "0" = SINE
 *           "1" = SQUARE
 *           "2" = TRIANGLE
 *           "3" = SAWTOOTH
 *           "4" = NOISE
 *           "5" = VIC NOISE
 *       frequency:
 *           frequency in Hertz
 *       duration:
 *           duration in milliseconds
 *       volume:
 *           volume (min is 0, max is 127)
 *
 *     Example:
 *       // play Sine waveform at 800 Hz, for 1000ms at volume 100
 *       Terminal.write("\e_S0;800;1000;100$");
 *
 * <br> <b> Clear graphics screen with background color and reset scrolling region </b>
 *
 *     Sequence:
 *       ESC "_GCLEAR" "$"
 *
 *     Example:
 *       // clear graphics screen, filling with dark blue
 *       Terminal.write("\e_GBRUSH0;0;128$");
 *       Terminal.write("\e_GCLEAR$");
 *
 * <br> <b> Set brush color for graphics </b>
 *
 *     Sequence:
 *       ESC "_GBRUSH" red ";" green ";" blue "$"
 *
 *     Parameters:
 *       red:   '0'..'255'
 *       green: '0'..'255'
 *       blue:  '0'..'255'
 *
 *     Example:
 *       // set pure red (255,0,0) as background color for graphics
 *       Terminal.write("\e_GBRUSH255;0;0$");
 *
 * <br> <b> Set pen color for graphics </b>
 *
 *     Sequence:
 *       ESC "_GPEN" red ";" green ";" blue "$"
 *
 *     Parameters:
 *       red:   '0'..'255'
 *       green: '0'..'255'
 *       blue:  '0'..'255'
 *
 *     Example:
 *       // set yellow (255,255,0) as pen color for graphics
 *       Terminal.write("\e_GPEN255;255;0$");
 *
 * <br> <b> Set pen width </b>
 *
 *     Sequence:
 *       ESC "_GPENW" width "$"
 *
 *     Parameters:
 *       width: pen width (from 1)
 *
 *     Example:
 *       // set pen width to 2
 *       Terminal.write("\e_GPENW2$");
 *
 * <br> <b> Set specified pixel using pen color </b>
 *
 *     Sequence:
 *       ESC "_GPIXEL" X ";" Y "$"
 *
 *     Parameters:
 *       X: horizontal coordinate
 *       Y: vertical coordinate
 *
 *     Example:
 *       // set pixel at 89, 31 to blue
 *       Terminal.write("\e_GPEN0;0;255$"); // pen = blue
 *       Terminal.write("\e_GPIXEL89;31$"); // draw pixel
 *
 * <br> <b> Draw a line using pen color</b>
 *
 *     Sequence:
 *       ESC "_GLINE" X1 ";" Y1 ";" X2 ";" Y2 "$"
 *
 *     Parameters:
 *       X1: starting horizontal coordinate
 *       Y1: starting vertical coordinate
 *       X2: ending horizontal coordinate
 *       Y2: ending vertical coordinate
 *
 *     Example:
 *       // draw a red line from 10, 10 to 150,150
 *       Terminal.write("\e_GPEN255;0;0$");        // pen = red
 *       Terminal.write("\e_GLINE10;10;150;150$"); // draw line
 *
 * <br> <b> Draw a rectangle using pen color</b>
 *
 *     Sequence:
 *       ESC "_GRECT" X1 ";" Y1 ";" X2 ";" Y2 "$"
 *
 *     Parameters:
 *       X1: starting horizontal coordinate
 *       Y1: starting vertical coordinate
 *       X2: ending horizontal coordinate
 *       Y2: ending vertical coordinate
 *
 *     Example:
 *       // draw a white rectangle from 10, 10 to 150,150
 *       Terminal.write("\e_GPEN255;255;255$");    // pen = white
 *       Terminal.write("\e_GRECT10;10;150;150$"); // draw rectangle
 *
 * <br> <b> Fill a rectangle using brush color</b>
 *
 *     Sequence:
 *       ESC "_GFILLRECT" X1 ";" Y1 ";" X2 ";" Y2 "$"
 *
 *     Parameters:
 *       X1: starting horizontal coordinate
 *       Y1: starting vertical coordinate
 *       X2: ending horizontal coordinate
 *       Y2: ending vertical coordinate
 *
 *     Example:
 *       // fill a yellow rectangle from 10, 10 to 150,150
 *       Terminal.write("\e_GBRUSH255;255;0$");         // brush = yellow
 *       Terminal.write("\e_GFILLRECT10;10;150;150$");  // fill rectangle
 *
 * <br> <b> Draw an ellipse using pen color</b>
 *
 *     Sequence:
 *       ESC "_GELLIPSE" X ";" Y ";" width ";" height "$"
 *
 *     Parameters:
 *       X:      horizontal coordinate of ellipse center
 *       Y:      vertical coordinate of ellipse center
 *       with:   ellipse width
 *       height: ellipse height
 *
 *     Example:
 *       // draw a green ellipse at 100,120 with 50 horizontal size and 80 vertical size
 *       Terminal.write("\e_GPEN0;255;0$");            // pen = green
 *       Terminal.write("\e_GELLIPSE100;120;50;80$");  // draw ellipse
 *
 * <br> <b> Fill an ellipse using brush color</b>
 *
 *     Sequence:
 *       ESC "_GFILLELLIPSE" X ";" Y ";" width ";" height "$"
 *
 *     Parameters:
 *       X:      horizontal coordinate of ellipse center
 *       Y:      vertical coordinate of ellipse center
 *       with:   ellipse width
 *       height: ellipse height
 *
 *     Example:
 *       // fill a red ellipse at 100,120 with 50 horizontal size and 80 vertical size
 *       Terminal.write("\e_GBRUSH255;0;0$");              // brush = red
 *       Terminal.write("\e_GFILLELLIPSE100;120;50;80$");  // fill ellipse
 *
 * <br> <b> Draw a polygon (path) using pen color</b>
 *
 *     Sequence:
 *       ESC "_GPATH" X1 ";" Y1 ";" X2 ";" Y2 [";" Xn ";" Yn...] "$"
 *
 *     Parameters:
 *       X1: first horizontal coordinate
 *       Y1: first vertical coordinate
 *       X2: second horizontal coordinate
 *       Y2: second vertical coordinate
 *       Xn: optional "n" horizontal coordinate
 *       Yn: optional "n" vertical coordinate
 *
 *     Notes:
 *       Maximum number of points is 32 (configurable in terminal.h)
 *
 *     Example:
 *       // draw a red triangle at (5,5)-(12,18)-(6,16)
 *       Terminal.write("\e_GPEN255;0;0$");                // pen = red
 *       Terminal.write("\e_GPATH5;5;12;18;6;16$");        // draw path
 *
 * <br> <b> Fill a polygon (path) using brush color </b>
 *
 *     Sequence:
 *       ESC "_GFILLPATH" X1 ";" Y1 ";" X2 ";" Y2 [";" Xn ";" Yn...] "$"
 *
 *     Parameters:
 *       X1: first horizontal coordinate
 *       Y1: first vertical coordinate
 *       X2: second horizontal coordinate
 *       Y2: second vertical coordinate
 *     [Xn]: optional "n" horizontal coordinate
 *     [Yn]: optional "n" vertical coordinate
 *
 *     Notes:
 *       Maximum number of points is 32 (configurable in terminal.h)
 *
 *     Example:
 *       // fill a green triangle at (5,5)-(12,18)-(6,16)
 *       Terminal.write("\e_GBRUSH0;255;0$");              // brush = green
 *       Terminal.write("\e_GFILLPATH5;5;12;18;6;16$");    // fill path
 *
 * <br> <b> Set number of sprites to allocate </b>
 *
 *     Sequence:
 *       ESC "_GSPRITECOUNT" count "$"
 *
 *     Parameters:
 *       count: number of sprites that will be defined by "_GSPRITEDEF"
 *
 *     Example:
 *       // allocates two sprites
 *       Terminal.write("\e_GSPRITECOUNT2$");
 *
 * <br> <b> Add a bitmap to an allocated sprite </b>
 *
 *     Sequence:
 *       ESC "_GSPRITEDEF" spriteIndex ";" width ";" height ";" format ";" [R ";" G ";" B ";"] data... "$"
 *
 *     Parameters:
 *       spriteIndex: sprite index (0...)
 *       width:       bitmap width
 *       height:      bitmap height
 *       format:
 *           "M" = bitmap format is monochrome (1 bit per pixel)
 *           "2" = bitmap format is 64 colors (6 bits per pixel, 2 bits per channel with transparency)
 *           "8" = bitmap format is true color (32 bits per pixel, 8 bits per channel with transparency)
 *      [R]:          red channel when bitmap format is monochrome
 *      [G]:          green channel when bitmap format is monochrome
 *      [B]:          blue channel when bitmap format is monochrome
 *       data:        bitmap data data as a sequence of 2 digits hex numbers (ie 002A3BFF2C...).
 *                    each bitmap row is always byte aligned
 *
 *     Example:
 *       // allocates one sprite and assign a 8x4 monochrome bitmap, colored with red
 *       Terminal.write("\e_GSPRITECOUNT1$");
 *       Terminal.write("\e_GSPRITEDEF0;8;4;M;255;0;0;AABBCCDD$");
 *
 * <br> <b> Set sprite visibility, position and current frame (bitmap) </b>
 *
 *     Sequence:
 *       ESC "_GSPRITESET" spriteIndex ";" visible ";" frameIndex ";" X ";" Y "$"
 *
 *     Parameters:
 *       spriteIndex: sprite index (0...)
 *       visible:     "H" = hidden, "V" = visible
 *       frameIndex:  current frame (bitmap) to show (0...)
 *       X:           horizontal position
 *       Y:           vertical position
 *
 *     Example:
 *       // make sprite 0 visible at position 50,120 with first added bitmap
 *       Terminal.write("\e_GSPRITESET0;V;0;50;120$");
 *
 * <br> <b> Scroll screen at pixel level </b>
 *
 *     Sequence:
 *       ESC "_GSCROLL" offsetX ";" offsetY "$"
 *
 *     Parameters:
 *       offsetX: number of pixels to scroll (<0 = scroll left, >0 scroll right)
 *       offsetY: nunber of pixels to scroll (<0 = scroll up, >0 scroll down)
 *
 *     Example:
 *       // scroll left by 8 pixels
 *       Terminal.write("\e_GSCROLL-8;0$");
 *
 * <br><br>
 */


namespace fabgl {




/** \ingroup Enumerations
 * @brief This enum defines various serial port flow control methods
 */
enum class FlowControl {
  None,              /**< No flow control */
  Software,          /**< Software flow control. Use XON and XOFF control characters */
};


// used by saveCursorState / restoreCursorState
struct TerminalCursorState {
  TerminalCursorState *   next;
  int16_t                 cursorX;
  int16_t                 cursorY;
  uint8_t *               tabStop;
  bool                    cursorPastLastCol;
  bool                    originMode;
  GlyphOptions            glyphOptions;
  uint8_t                 characterSetIndex;
  uint8_t                 characterSet[4];
};


enum KeypadMode {
  Application,  // DECKPAM
  Numeric,      // DECKPNM
};


/** \ingroup Enumerations
 * @brief This enum defines a character style
 */
enum CharStyle {
  Bold,               /**< Bold */
  ReducedLuminosity,  /**< Reduced luminosity */
  Italic,             /**< Italic */
  Underline,          /**< Underlined */
  Blink,              /**< Blinking text */
  Blank,              /**< Invisible text */
  Inverse,            /**< Swap background and foreground colors */
};


/** \ingroup Enumerations
 * @brief This enum defines terminal transition effect
 */
enum class TerminalTransition {
  None,
  LeftToRight,    /**< Left to right */
  RightToLeft,    /**< Right to left */
};


struct EmuState {

  // Index of characterSet[], 0 = G0 (Standard)  1 = G1 (Alternate),  2 = G2,  3 = G3
  uint8_t      characterSetIndex;

  // 0 = DEC Special Character and Line Drawing   1 = United States (USASCII)
  uint8_t      characterSet[4];

  Color        foregroundColor;
  Color        backgroundColor;

  // cursor position (topleft = 1,1)
  int          cursorX;
  int          cursorY;

  bool         cursorPastLastCol;

  bool         originMode;

  bool         wraparound;

  // top and down scrolling regions (1 = first row)
  int          scrollingRegionTop;
  int          scrollingRegionDown;

  bool         cursorEnabled;

  // true = blinking cursor, false = steady cursor
  bool         cursorBlinkingEnabled;

  // 0,1,2 = block  3,4 = underline  5,6 = bar
  int          cursorStyle;

  // column 1 at m_emuState.tabStop[0], column 2 at m_emuState.tabStop[1], etc...  0=no tab stop,  1 = tab stop
  uint8_t *    tabStop;

  // IRM (Insert Mode)
  bool         insertMode;

  // NLM (Automatic CR LF)
  bool         newLineMode;

  // DECSCLM (Smooth scroll)
  // Smooth scroll is effective only when vertical sync refresh is enabled,
  // hence must be BitmappedDisplayController.enableBackgroundPrimitiveExecution(true),
  // that is the default.
  bool         smoothScroll;

  // DECKPAM (Keypad Application Mode)
  // DECKPNM (Keypad Numeric Mode)
  KeypadMode   keypadMode;

  // DECCKM (Cursor Keys Mode)
  bool         cursorKeysMode;

  // DESSCL (1 = VT100 ... 5 = VT500)
  int          conformanceLevel;

  // two values allowed: 7 and 8
  int          ctrlBits;

  bool         keyAutorepeat;

  bool         allow132ColumnMode;

  bool         reverseWraparoundMode;

  // DECBKM (false = BACKSPACE sends BS, false BACKSPACE sends DEL)
  bool         backarrowKeyMode;

  // DECANM (false = VT52 mode, true = ANSI mode)
  bool         ANSIMode;

  // VT52 Graphics Mode
  bool         VT52GraphicsMode;

  // Allow FabGL specific sequences (ESC FABGLEXT_STARTCODE .....)
  int          allowFabGLSequences;  // >0 allow, 0 = don't allow
};


#ifndef ARDUINO

struct Print {
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t * buffer, size_t size);
  size_t write(const char *str) {
    if (str == NULL)
        return 0;
    return write((const uint8_t *)str, strlen(str));
  }
  void printf(const char * format, ...) {
    va_list ap;
    va_start(ap, format);
    int size = vsnprintf(nullptr, 0, format, ap) + 1;
    if (size > 0) {
      va_end(ap);
      va_start(ap, format);
      char buf[size + 1];
      auto l = vsnprintf(buf, size, format, ap);
      write((uint8_t*)buf, l);
    }
    va_end(ap);
  }
};

struct Stream : public Print{
};

#endif  // ifdef ARDUINO



/**
 * @brief An ANSI-VT100 compatible display terminal.
 *
 * Implements most of common ANSI, VT52, VT100, VT200, VT300, VT420 and VT500 escape codes, like non-CSI codes (RIS, IND, DECID, DECDHL, etc..),<br>
 * like CSI codes (private modes, CUP, TBC, etc..), like CSI-SGR codes (bold, italic, blinking, etc...) and like DCS codes (DECRQSS, etc..).<br>
 * Supports convertion from PS/2 keyboard virtual keys to ANSI or VT codes (keypad, cursor keys, function keys, etc..).<br>
 *
 * Terminal can receive codes to display from Serial Port or it can be controlled directly from the application. In the same way
 * Terminal can send keyboard codes to a Serial Port or directly to the application.<br>
 *
 * For default it supports 80x25 or 132x25 characters at 640x350. However any custom resolution and text buffer size is supported
 * specifying a custom font.<br>
 *
 * There are three cursors styles (block, underlined and bar), blinking or not blinking.
 *
 * Terminal inherits from Stream so applications can use all Stream and Print input and output methods.
 *
 * Terminal passes 95/110 of VTTEST VT100/VT102 Compatibility Test Score Sheet.
 *
 *
 *
 * Example 1:
 *
 *     fabgl::VGAController VGAController;
 *     fabgl::PS2Controller PS2Controller;
 *     fabgl::Terminal      Terminal;
 *
 *     // Setup 80x25 columns loop-back terminal (send what you type on keyboard to the display)
 *     void setup() {
 *       PS2Controller.begin(PS2Preset::KeyboardPort0);
 *
 *       VGAController.begin();
 *       VGAController.setResolution(VGA_640x350_70HzAlt1);
 *
 *       Terminal.begin(&VGAController);
 *       Terminal.connectLocally();      // to use Terminal.read(), available(), etc..
 *       Terminal.enableCursor(true);
 *     }
 *
 *     void loop() {
 *       if (Terminal.available()) {
 *         char c = Terminal.read();
 *         switch (c) {
 *           case 0x7F:       // DEL -> backspace + ESC[K
 *             Terminal.write("\b\e[K");
 *             break;
 *           case 0x0D:       // CR  -> CR + LF
 *             Terminal.write("\r\n");
 *             break;
 *           case 32 ... 126: // printable chars
 *             Terminal.write(c);
 *             break;
 *         }
 *       }
 *     }
 *
 *
 * Example 2:
 *
 *     fabgl::VGAController VGAController;
 *     fabgl::PS2Controller PS2Controller;
 *     fabgl::Terminal      Terminal;
 *
 *     // Setup 80x25 columns terminal using UART2 to communicate with the server,
 *     // VGA to display output and PS2 device as keyboard input
 *     void setup() {
 *       Serial2.begin(115200);
 *
 *       PS2Controller.begin(PS2Preset::KeyboardPort0);
 *
 *       VGAController.begin();
 *       VGAController.setResolution(VGA_640x350_70HzAlt1);
 *
 *       Terminal.begin(&VGAController);
 *       Terminal.connectSerialPort(Serial2);
 *       Terminal.enableCursor(true);
 *     }
 *
 *     void loop() {
 *       Terminal.pollSerialPort();
 *     }
 */
class Terminal : public Stream {

public:

  Terminal();

  ~Terminal();

  /**
   * @brief Initializes the terminal.
   *
   * Applications should call this method before any other method call and after resolution has been set.
   *
   * @param displayController The output display controller
   * @param maxColumns Maximum number of columns (-1 = depends by the display horizontal resolution)
   * @param maxRows Maximum number of rows (-1 = depends by the display vertical resolution)
   * @param keyboard Keyboard device. nullptr = gets from PS2Controller
   *
   * @return False on fail to allocate required memory
   */
  bool begin(BaseDisplayController * displayController, int maxColumns = -1, int maxRows = -1, Keyboard * keyboard = nullptr);

  /**
   * @brief Finalizes the terminal.
   *
   * Applications should call this method before screen resolution changes.
   */
  void end();

  /**
   * @brief Connects a remote host using the specified serial port.
   *
   * When serial port is set, the typed keys on PS/2 keyboard are encoded
   * as ANSI/VT100 codes and then sent to the specified serial port.<br>
   * Also replies to terminal queries like terminal identification, cursor position, etc.. will be
   * sent to the serial port.<br>
   * Call Terminal.pollSerialPort() to send codes from serial port to the display.
   *
   * This method requires continous polling of the serial port and is very inefficient. Use the second overload
   * to directly handle serial port using interrupts.
   *
   * @param serialPort The serial port to use.
   * @param autoXONXOFF If true uses software flow control (XON/XOFF).
   *
   * Example:
   *
   *     Terminal.begin(&VGAController);
   *     Terminal.connectSerialPort(Serial);
   */
  #ifdef ARDUINO
  void connectSerialPort(HardwareSerial & serialPort, bool autoXONXOFF = true);
  #endif

  /**
   * @brief Connects a remote host using UART
   *
   * When serial port is set, the typed keys on PS/2 keyboard are encoded
   * as ANSI/VT100 codes and then sent to the specified serial port.<br>
   * Also replies to terminal queries like terminal identification, cursor position, etc.. will be
   * sent to the serial port.<br>
   * This method setups the UART2 with specified parameters. Received characters are handlded using interrupts freeing main
   * loop to do something other.<br>
   * <br>
   * This is the preferred way to connect the Terminal with a serial port.<br>
   * You may call connectSerialPort whenever a parameters needs to be changed (except for rx and tx pins).
   *
   * @param baud Baud rate.
   * @param config Defines word length, parity and stop bits. Example: SERIAL_8N1.
   * @param rxPin UART RX pin GPIO number.
   * @param txPin UART TX pin GPIO number.
   * @param flowControl Flow control. When set to FlowControl::Software, XON and XOFF characters are automatically sent.
   * @param inverted If true RX and TX signals are inverted.
   *
   * Example:
   *
   *     Terminal.begin(&DisplayController);
   *     Terminal.connectSerialPort(115200, SERIAL_8N1, 34, 2, FlowControl::Software);
   */
  void connectSerialPort(uint32_t baud, uint32_t config, int rxPin, int txPin, FlowControl flowControl, bool inverted = false);

  /**
   * @brief Pools the serial port for incoming data.
   *
   * This method needs to be called in the application main loop to check if new data
   * is coming from the current serial port (specified using Terminal.connectSerialPort).
   *
   * Example:
   *
   *     void loop() {
   *       Terminal.pollSerialPort();
   *     }
   */
  #ifdef ARDUINO
  void pollSerialPort();
  #endif

  /**
   * @brief Disables/Enables serial port RX
   *
   * This method temporarily disables RX from serial port, discarding all incoming data.
   *
   * @param value If True RX is disabled. If False RX is re-enabled.
   */
  void disableSerialPortRX(bool value)         { m_uartRXEnabled = !value; }

  /**
   * @brief Permits using of terminal locally.
   *
   * Create a queue where to put ANSI keys decoded from keyboard or as replies to terminal queries.<br>
   * This queue is accessible with read(), available() and peek() methods.
   *
   * Example:
   *
   *     Terminal.begin(&VGAController);
   *     Terminal.connectLocally();
   *     // from here you can use Terminal.read() to receive keys from keyboard
   *     // and Terminal.write() to control the display.
   */
  void connectLocally();

  /**
   * @brief Avoids using of terminal locally.
   *
   * This is the opposite of connectLocally().
   */
  void disconnectLocally();

  /**
   * @brief Injects keys into the keyboard queue.
   *
   * Characters added with localWrite() will be received with read(), available() and peek() methods.
   *
   * @param c ASCII code to inject into the queue.
   */
  void localWrite(uint8_t c);

  /**
   * @brief Injects a string of keys into the keyboard queue.
   *
   * Characters added with localWrite() will be received with read(), available() and peek() methods.
   *
   * @param str A string of ASCII codes to inject into the queue.
   */
  void localWrite(char const * str);

  /**
   * @brief Injects keys into the keyboard queue.
   *
   * Characters inserted with localInsert() will be received with read(), available() and peek() methods.
   *
   * @param c ASCII code to inject into the queue.
   */
  void localInsert(uint8_t c);

  /**
   * @brief Injects keys into the keyboard queue.
   *
   * This is the same of localInsert().
   * Characters inserted with localWrite() will be received with read(), available() and peek() methods.
   *
   * @param c ASCII code to inject into the queue.
   */
  void unRead(uint8_t c) { localInsert(c); }

  /**
   * @brief Sets the stream where to output debugging logs.
   *
   * Logging info sents to the logging stream are detailed by FABGLIB_TERMINAL_DEBUG_REPORT_.... macros in fabglconf.h configuration file.
   *
   * @param stream The logging stream.
   *
   * Example:
   *
   *     Serial.begin(115200);
   *     Terminal.begin(&VGAController);
   *     Terminal.setLogStream(Serial);
   */
  void setLogStream(Stream & stream) { m_logStream = &stream; }

  void logFmt(const char * format, ...);
  void log(const char * txt);
  void log(char c);

  /**
   * @brief Sets the font to use.
   *
   * Terminal automatically choises the best font considering screen resolution and required
   * number of columns and rows.<br>
   * Particular cases require setting custom fonts, so applications can use Terminal.loadFont().
   * Only fixed width fonts are supported for terminals.
   *
   * @param font Specifies font info for the font to set.
   */
  void loadFont(FontInfo const * font);

  /**
   * @brief Sets the background color.
   *
   * Sets the background color sending an SGR ANSI code and optionally the
   * default background color (used resetting the terminal).
   *
   * @param color Current or default background color.
   * @param setAsDefault If true the specified color is also used as default.
   *
   * Example:
   *
   *     Terminal.setBackgroundColor(Color::Black);
   */
  void setBackgroundColor(Color color, bool setAsDefault = true);

  /**
   * @brief Sets the foreground color.
   *
   * Sets the foreground color sending an SGR ANSI code and optionally the
   * default foreground color (used resetting the terminal).
   *
   * @param color Current or default foreground color.
   * @param setAsDefault If true the specified color is also used as default.
   *
   * Example:
   *
   *     Terminal.setForegroundColor(Color::White);
   */
  void setForegroundColor(Color color, bool setAsDefault = true);

  /**
   * @brief Clears the screen.
   *
   * Clears the screen sending "CSI 2 J" command to the screen.
   *
   * @param moveCursor If True moves cursor at position 1, 1.
   *
   * Example:
   *
   *     // Fill the screen with blue
   *     Terminal.setBackgroundColor(Color::Blue);
   *     Terminal.clear();
   */
  void clear(bool moveCursor = true);

  /**
   * @brief Waits for all codes sent to the display has been processed.
   *
   * @param waitVSync If true codes are processed during screen retrace time (starting from VSync up to about first top visible row).
   *                  When false all messages are processed immediately.
   */
  void flush(bool waitVSync);

  /**
   * @brief Returns the number of columns.
   *
   * @return The number of columns (in characters).
   */
  int getColumns() { return m_columns; }

  /**
   * @brief Returns the number of lines.
   *
   * @return The number of lines (in characters).
   */
  int getRows()    { return m_rows; }

  /**
   * @brief Enables or disables cursor.
   *
   * @param value If true the cursor becomes visible.
   */
  void enableCursor(bool value);

  /**
   * @brief Determines number of codes that the display input queue can still accept.
   *
   * @return The size (in characters) of remaining space in the display queue.
   */
  int availableForWrite();

  /**
   * @brief Sets the terminal type to emulate
   *
   * @param value A terminal to emulate
   *
   * Default and native is ANSI/VT100 mode. Other terminals are emulated translating to native mode.
   */
  void setTerminalType(TermType value);

  /**
   * @brief Determines current terminal type
   *
   * @return Terminal type
   */
  TermInfo const & terminalType() { return *m_termInfo; }


  //////////////////////////////////////////////
  //// Stream abstract class implementation ////

  /**
   * @brief Gets the number of codes available in the keyboard queue.
   *
   * Keyboard queue is available only after Terminal.connectLocally() call.
   *
   * @return The number of codes available to read.
   */
  int available();

  /**
   * @brief Reads codes from keyboard.
   *
   * Keyboard queue is available only after Terminal.connectLocally() call.
   *
   * @return The first code of incoming data available (or -1 if no data is available).
   */
  int read();

  /**
   * @brief Reads codes from keyboard specyfing timeout.
   *
   * Keyboard queue is available only after Terminal.connectLocally() call.
   *
   * @param timeOutMS Timeout in milliseconds. -1 = no timeout (infinite wait).
   *
   * @return The first code of incoming data available (or -1 if no data is available after timeout has expired).
   */
  int read(int timeOutMS);

  /**
   * @brief Wait for a specific code from keyboard, discarding all previous codes.
   *
   * Keyboard queue is available only after Terminal.connectLocally() call.
   *
   * @param value Char code to wait for.
   * @param timeOutMS Timeout in milliseconds. -1 = no timeout (infinite wait).
   *
   * @return True if specified value has been received within timeout time.
   */
  bool waitFor(int value, int timeOutMS = -1);

  /**
   * @brief Reads a code from the keyboard without advancing to the next one.
   *
   * Keyboard queue is available only after Terminal.connectLocally() call.
   *
   * @return The next code, or -1 if none is available.
   */
  int peek();

  /**
   * @brief Waits for all codes sent to the display has been processed.
   *
   * Codes are processed during screen retrace time (starting from VSync up to about first top visible row).
   */
  void flush();

  /**
   * @brief Sends specified number of codes to the display.
   *
   * Codes can be ANSI/VT codes or ASCII characters.
   *
   * @param buffer Pointer to codes buffer.
   * @param size Number of codes in the buffer.
   *
   * @return The number of codes written.
   *
   * Example:
   *
   *     // Clear the screen and print "Hello World!"
   *     Terminal.write("\e[2J", 4);
   *     Terminal.write("Hellow World!\r\n", 15);
   *
   *     // The same without size specified
   *     Terminal.write("\e[2J");
   *     Terminal.write("Hellow World!\r\n");
   */
  size_t write(const uint8_t * buffer, size_t size);

  /**
   * @brief Sends a single code to the display.
   *
   * Code can be only of the ANSI/VT codes or ASCII characters.
   *
   * @param c The code to send.
   *
   * @return The number of codes written.
   */
  size_t write(uint8_t c);

  using Print::write;

  /**
   * @brief Like localWrite() but sends also to serial port if connected
   *
   * @param c Character code to send
   */
  void send(uint8_t c);

  /**
   * @brief Like localWrite() but sends also to serial port if connected
   *
   * @param str String to send
   */
  void send(char const * str);


  /**
   * @brief Gets associated keyboard object.
   *
   * @return The Keyboard object.
   */
  Keyboard * keyboard() { return m_keyboard; }

  /**
   * @brief Gets associated canvas object.
   *
   * @return The Canvas object.
   */
  Canvas * canvas() { return m_canvas; }

  /**
   * @brief Activates this terminal for input and output.
   *
   * Only one terminal can be active at the time, for input and output.
   * Use this method to activate a terminal. This method de-activates currently active terminal.
   *
   * @param transition Optional transition effect
   */
  void activate(TerminalTransition transition = TerminalTransition::None);

  /**
   * @brief Deactivates this terminal.
   */
  void deactivate();

  /**
   * @brief Determines if this terminal is active or not.
   *
   * @return True is this terminal is active for input and output.
   */
  bool isActive() { return s_activeTerminal == this; }

  /**
   * @brief Selects a color for the specified attribute
   *
   * This method allows to indicate a color when the terminal prints a character with a specific attribute.
   * If a character has multiple attributes then the resulting color is undefined.
   * To disable attribute color call the other setColorForAttribute() overload.
   *
   * @param attribute Style/attribute to set color. Only CharStyle::Bold, CharStyle::ReducedLuminosity, CharStyle::Italic and CharStyle::Underline are supported
   * @param color Color of the attribute
   * @param maintainStyle If True style is applied. If False just the specified color is applied.
   */
  void setColorForAttribute(CharStyle attribute, Color color, bool maintainStyle);

  /**
   * @brief Disables color for the specified attribute
   *
   * This method disables color specification for the specified attribute.
   *
   * @param attribute Style/attribute to disable color. Only CharStyle::Bold, CharStyle::ReducedLuminosity, CharStyle::Italic and CharStyle::Underline are supported
   */
  void setColorForAttribute(CharStyle attribute);

  /**
   * @brief Gets embedded sound generator
   *
   * @return SoundGenerator object
   */
  SoundGenerator * soundGenerator();


  //// Delegates ////

  /**
   * @brief Delegate called whenever a new virtual key is received from keyboard
   *
   * First parameter is a pointer to the decoded virtual key
   * Second parameter specifies if the key is Down (true) or Up (false)
   */
  Delegate<VirtualKey *, bool> onVirtualKey;


  /**
   * @brief Delegate called whenever a new virtual key is received from keyboard, including shift states
   *
   * The parameter is a pointer to the decoded virtual key info
   */
  Delegate<VirtualKeyItem *> onVirtualKeyItem;


  /**
   * @brief Delegate called whenever a new user sequence has been received
   *
   * The parameter contains a zero terminated string containing the user sequence payload.
   * User sequence starts with ESC + "_#" and terminates with "$". For example, when then terminal
   * receives "ESC_#mycommand$", the delegate receives "mycommand" string.
   */
  Delegate<char const *> onUserSequence;



  // statics (used for common default properties)


  /**
   * @brief Number of characters the terminal can "write" without pause (increase if you have loss of characters in serial port).
   *
   * Application should change before begin() method.
   *
   * Default value is FABGLIB_DEFAULT_TERMINAL_INPUT_QUEUE_SIZE (defined in fabglconf.h)
   */
  static int inputQueueSize;

  /**
   * @brief Stack size of the task that processes Terminal input stream.
   *
   * Application should change before begin() method.
   *
   * Default value is FABGLIB_DEFAULT_TERMINAL_INPUT_CONSUMER_TASK_STACK_SIZE (defined in fabglconf.h)
   */
  static int inputConsumerTaskStackSize;

  /**
   * @brief Stack size of the task that reads keys from keyboard and send ANSI/VT codes to output stream in Terminal.
   *
   * Application should change before begin() method.
   *
   * Default value is FABGLIB_DEFAULT_TERMINAL_KEYBOARD_READER_TASK_STACK_SIZE (defined in fabglconf.h)
   */
  static int keyboardReaderTaskStackSize;


private:

  void reset();
  void int_clear();
  void clearMap(uint32_t * map);

  void freeFont();
  void freeTabStops();
  void freeGlyphsMap();

  void set132ColumnMode(bool value);

  bool moveUp();
  bool moveDown();
  void move(int offset);
  void setCursorPos(int X, int Y);
  int getAbsoluteRow(int Y);

  void int_setBackgroundColor(Color color);
  void int_setForegroundColor(Color color);

  void syncDisplayController();

  // tab stops
  void nextTabStop();
  void setTabStop(int column, bool set);
  void resetTabStops();

  // scroll control
  void scrollDown();
  void scrollDownAt(int startingRow);
  void scrollUp();
  void scrollUpAt(int startingRow);
  void setScrollingRegion(int top, int down, bool resetCursorPos = true);
  void updateCanvasScrollingRegion();

  // multilevel save/restore cursor state
  void saveCursorState();
  void restoreCursorState();
  void clearSavedCursorStates();

  void erase(int X1, int Y1, int X2, int Y2, uint8_t c, bool maintainDoubleWidth, bool selective);

  void consumeInputQueue();
  void consumeESC();
  void consumeCSI();
  void consumeOSC();
  void consumeFabGLSeq();
  void consumeFabGLGraphicsSeq();
  void consumeCSIQUOT(int * params, int paramsCount);
  void consumeCSISPC(int * params, int paramsCount);
  uint8_t consumeParamsAndGetCode(int * params, int * paramsCount, bool * questionMarkFound);
  void consumeDECPrivateModes(int const * params, int paramsCount, uint8_t c);
  void consumeDCS();
  void execSGRParameters(int const * params, int paramsCount);
  void consumeESCVT52();

  void execCtrlCode(uint8_t c);

  static void charsConsumerTask(void * pvParameters);
  static void keyboardReaderTask(void * pvParameters);

  static void blinkTimerFunc(TimerHandle_t xTimer);
  void blinkText();
  bool enableBlinkingText(bool value);
  void blinkCursor();
  bool int_enableCursor(bool value);

  static void IRAM_ATTR uart_isr(void *arg);

  uint8_t getNextCode(bool processCtrlCodes);

  bool setChar(uint8_t c);
  GlyphOptions getGlyphOptionsAt(int X, int Y);

  void insertAt(int column, int row, int count);
  void deleteAt(int column, int row, int count);

  bool multilineInsertChar(int charsToMove);
  void multilineDeleteChar(int charsToMove);

  void reverseVideo(bool value);

  void refresh();
  void refresh(int X, int Y);
  void refresh(int X1, int Y1, int X2, int Y2);

  void setLineDoubleWidth(int row, int value);
  int getCharWidthAt(int row);
  int getColumnsAt(int row);

  void useAlternateScreenBuffer(bool value);

  void sendCSI();
  void sendDCS();
  void sendSS3();
  void sendCursorKeyCode(uint8_t c);
  void sendKeypadCursorKeyCode(uint8_t applicationCode, const char * numericCode);

  void ANSIDecodeVirtualKey(VirtualKeyItem const & item);
  void VT52DecodeVirtualKey(VirtualKeyItem const & item);

  void convHandleTranslation(uint8_t c, bool fromISR);
  void convSendCtrl(ConvCtrl ctrl, bool fromISR);
  void convQueue(const char * str, bool fromISR);
  void TermDecodeVirtualKey(VirtualKeyItem const & item);

  bool addToInputQueue(uint8_t c, bool fromISR);
  bool insertToInputQueue(uint8_t c, bool fromISR);

  void write(uint8_t c, bool fromISR);

  //static void uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

  void uartCheckInputQueueForFlowControl();

  void enableFabGLSequences(bool value);

  void int_setTerminalType(TermType value);
  void int_setTerminalType(TermInfo const * value);

  void sound(int waveform, int frequency, int duration, int volume);

  uint8_t extGetByteParam();
  int extGetIntParam();
  void extGetCmdParam(char * cmd);

  void freeSprites();

  uint32_t makeGlyphItem(uint8_t c, GlyphOptions * glyphOptions, Color * newForegroundColor);

  // indicates which is the active terminal when there are multiple instances of Terminal
  static Terminal *  s_activeTerminal;


  BaseDisplayController * m_displayController;
  Canvas *           m_canvas;
  bool               m_bitmappedDisplayController;  // true = bitmapped, false = textual

  Keyboard *         m_keyboard;

  Stream *           m_logStream;

  // characters, characters attributes and characters colors container
  // you may also call this the "text screen buffer"
  GlyphsBuffer       m_glyphsBuffer;

  // used to implement alternate screen buffer
  uint32_t *         m_alternateMap;

  // true when m_alternateMap and m_glyphBuffer.map has been swapped
  bool               m_alternateScreenBuffer;

  // just to restore some properties when swapping screens (alternate screen)
  int                m_alternateCursorX;
  int                m_alternateCursorY;
  int                m_alternateScrollingRegionTop;
  int                m_alternateScrollingRegionDown;
  bool               m_alternateCursorBlinkingEnabled;

  FontInfo           m_font;

  PaintOptions       m_paintOptions;
  GlyphOptions       m_glyphOptions;

  EmuState           m_emuState;

  Color              m_defaultForegroundColor;
  Color              m_defaultBackgroundColor;

  // states of cursor and blinking text before consumeInputQueue()
  bool               m_prevCursorEnabled;
  bool               m_prevBlinkingTextEnabled;

  // task that reads and processes incoming characters
  TaskHandle_t       m_charsConsumerTaskHandle;

  // task that reads keyboard input and send ANSI/VT100 codes to serial port
  TaskHandle_t       m_keyboardReaderTaskHandle;

  // true = cursor in reverse state (visible), false = cursor invisible
  volatile bool      m_cursorState;

  // timer used to blink
  TimerHandle_t      m_blinkTimer;

  // main terminal mutex
  volatile SemaphoreHandle_t m_mutex;

  volatile bool      m_blinkingTextVisible;    // true = blinking text is currently visible
  volatile bool      m_blinkingTextEnabled;

  volatile int       m_columns;
  volatile int       m_rows;

  // checked in loadFont() to limit m_columns and m_rows (-1 = not checked)
  int                m_maxColumns;
  int                m_maxRows;

  #ifdef ARDUINO
  // optional serial port
  // data from serial port is processed and displayed
  // keys from keyboard are processed and sent to serial port
  HardwareSerial *          m_serialPort;
  #endif

  // optional serial port (directly handled)
  // data from serial port is processed and displayed
  // keys from keyboard are processed and sent to serial port
  volatile bool             m_uart;

  // if false all inputs from UART are discarded
  volatile bool             m_uartRXEnabled;

  // contains characters to be processed (from write() calls)
  volatile QueueHandle_t    m_inputQueue;

  // contains characters received and decoded from keyboard (or as replyes from ANSI-VT queries)
  QueueHandle_t             m_outputQueue;

  // linked list that contains saved cursor states (first item is the last added)
  TerminalCursorState *     m_savedCursorStateList;

  // a reset has been requested
  bool                      m_resetRequested;

  volatile bool             m_autoXONOFF;
  volatile bool             m_XOFF;       // true = XOFF sent

  // used to implement m_emuState.keyAutorepeat
  VirtualKey                m_lastPressedKey;

  uint8_t                   m_convMatchedCount;
  uint8_t                   m_convMatchedChars[EmuTerminalMaxChars];
  TermInfoVideoConv const * m_convMatchedItem;
  TermInfo const *          m_termInfo;

  // last char added with write()
  volatile uint8_t          m_lastWrittenChar;

  // when a FabGL sequence has been detected in write()
  volatile bool             m_writeDetectedFabGLSeq;

  // used by extGetIntParam(), extGetCmdParam(), extGetByteParam() to store next item (to avoid insertToInputQueue() which can cause dead-locks)
  int                       m_extNextCode; // -1 = no code

  SoundGenerator *          m_soundGenerator;

  Sprite *                  m_sprites;
  int                       m_spritesCount;

  bool                      m_coloredAttributesMaintainStyle;
  int                       m_coloredAttributesMask;    // related bit 1 if enabled
  Color                     m_coloredAttributesColor[4];

};



//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
// TerminalController


/**
 * @brief TerminalController allows direct controlling of the Terminal object without using escape sequences
 *
 * TerminalController needs FabGL specific sequences to be enabled (this is the default), and always works
 * despite the selected terminal emulation.
 *
 * Example:
 *
 *     // Writes "Hello" at 10, 10
 *     TerminalController termctrl(&Terminal);
 *     termctrl.setCursorPos(10, 10);
 *     Terminal.write("Hello");
 */
class TerminalController {

public:

  /**
   * @brief Object constructor
   *
   * @param terminal The Terminal instance to control. If not specified you have to set delegates.
   */
  TerminalController(Terminal * terminal = nullptr);

  ~TerminalController();

  /**
   * @brief Clears screen
   */
  void clear();

  /**
   * @brief Sets destination terminal
   *
   * @param terminal The Terminal instance to control. If not specified you have to set delegates.
   */
  void setTerminal(Terminal * terminal = nullptr);

  /**
   * @brief Enables/disables cursor
   *
   * @param value True enables cursor, False disables cursor
   */
  void enableCursor(bool value);

  /**
   * @brief Sets current cursor position
   *
   * @param col Cursor column (1 = left-most position).
   * @param row Cursor row (1 = top-most position).
   */
  void setCursorPos(int col, int row);

  /**
   * @brief Gets current cursor position
   *
   * @param col Pointer to a variable where to store current cursor column (1 = left-most position).
   * @param row Pointer to a variable where to store current cursor row (1 = top-most position).
   *
   * Example:
   *     int col, row;
   *     termctrl.getCursorPos(&col, &row);
   */
  void getCursorPos(int * col, int * row);

  /**
   * @brief Moves cursor to the left
   *
   * Cursor movement may cross lines.
   *
   * @param count Amount of positions to move.
   */
  void cursorLeft(int count);

  /**
   * @brief Moves cursor to the right
   *
   * Cursor movement may cross lines.
   *
   * @param count Amount of positions to move.
   */
  void cursorRight(int count);

  /**
   * @brief Gets current cursor column
   *
   * @return Cursor column (1 = left-most position).
   */
  int getCursorCol();

  /**
   * @brief Gets current cursor row
   *
   * @return Cursor row (1 = top-most position).
   */
  int getCursorRow();

  /**
   * @brief Inserts a blank character and move specified amount of characters to the right
   *
   * Moving characters to the right may cross multiple lines.
   *
   * @param charsToMove Amount of characters to move to the right, starting from current cursor position.
   *
   * @return True if vertical scroll occurred.
   */
  bool multilineInsertChar(int charsToMove);

  /**
   * @brief Deletes a character moving specified amount of characters to the left
   *
   * Moving characters to the left may cross multiple lines.
   *
   * @param charsToMove Amount of characters to move to the left, starting from current cursor position.
   */
  void multilineDeleteChar(int charsToMove);

  /**
   * @brief Sets a raw character at current cursor position
   *
   * Cursor position is moved by one position to the right.
   *
   * @param c Raw character code to set. Raw character is not interpreted as control character or escape.
   *
   * @return True if vertical scroll occurred.
   */
  bool setChar(uint8_t c);

  /**
   * @brief Checks if a virtual key is currently down
   *
   * @param vk Virtual key code
   *
   * @return True is the specified key is pressed
   */
  bool isVKDown(VirtualKey vk);

  /**
   * @brief Disables FabGL specific sequences
   */
  void disableFabGLSequences();

  /**
   * @brief Sets the terminal type to emulate
   *
   * @param value A terminal to emulate
   */
  void setTerminalType(TermType value);

  /**
   * @brief Sets foreground color
   *
   * @param value Foreground color
   */
  void setForegroundColor(Color value);

  /**
   * @brief Sets background color
   *
   * @param value Background color
   */
  void setBackgroundColor(Color value);

  /**
   * @brief Enables or disables specified character style
   *
   * @param style Style to enable or disable
   * @param enabled If true the style is enabled, if false the style is disabled
   */
  void setCharStyle(CharStyle style, bool enabled);


  //// Delegates ////

  /**
   * @brief Read character delegate
   *
   * This delegate is called whenever a character needs to be read, and no Terminal has been specified.
   * The delegate should block until a character is received.
   *
   * First parameter represents a pointer to the receiving character code.
   */
  Delegate<int *> onRead;

  /**
   * @brief Write character delegate
   *
   * This delegate is called whenever a character needs to be written, and no Terminal has been specified.
   *
   * First parameter represents the character code to send.
   */
  Delegate<int> onWrite;


private:

  void waitFor(int value);
  void write(uint8_t c);
  void write(char const * str);
  int read();


  Terminal * m_terminal;
};



//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
// LineEditor


/** \ingroup Enumerations
 * @brief Special character specified in on values from LineEditor::onSpecialChar delegate
 */
enum class LineEditorSpecialChar {
  CursorUp,    /**< Cursor Up */
  CursorDown,  /**< Cursor Down */
};


/**
 * @brief LineEditor is a single-line / multiple-rows editor which uses the Terminal object as input and output.
 *
 * The editor supports following control keys:
 *
 * \li <b>Left and Right Arrow keys</b>: Move cursor left and right, even across rows
 * \li <b>CTRL + Left and Right Arrow keys</b>: Move cursor at the begining of prevous or next word
 * \li <b>Home key</b>: Move cursor at the beginning of the line
 * \li <b>End key</b>: Move cursor at the end of the line
 * \li <b>Delete key</b>: Delete character at cursor
 * \li <b>Insert key</b>: Enable/disable insert mode
 * \li <b>Backspace key</b>: Delete character at left of the cursor
 * \li <b>Enter/Return</b>: Move cursor at the beginning of the next line and exit editor
 *
 * Example:
 *
 *     Terminal.write("> ");  // show prompt
 *     LineEditor ed(&Terminal);
 *     char * txt = ed.get();
 *     Terminal.printf("Your input is: %s\r\n", txt);
 */
class LineEditor {

public:

  /**
   * @brief Object constructor
   *
   * @param terminal Pointer to Terminal object
   */
  LineEditor(Terminal * terminal);

  ~LineEditor();

  /**
   * @brief Sets initial text
   *
   * Call this method if the input must have some text already inserted.
   * This method can be also called during editing to replace current text.
   *
   * @param text Initial text.
   * @param moveCursor If true the cursor is moved at the end of initial text.
   *
   * Example:
   * 
   *     LineEditor ed(&Terminal);
   *     ed.setText("Initial text ");
   *     char * txt = ed.get();
   *     Terminal.printf("Your input is: %s\r\n", txt);
   */
  void setText(char const * text, bool moveCursor = true);

  /**
   * @brief Sets initial text specifying length
   *
   * Call this method if the input must have some text already inserted.
   * This method can be also called during editing to replace current text.
   *
   * @param text Initial text.
   * @param length Text length
   * @param moveCursor If true the cursor is moved at the end of initial text.
   */
  void setText(char const * text, int length, bool moveCursor = true);

  /**
   * @brief Simulates user typing
   *
   * This method simulates user typing. Unlike setText, this methods allows control characters and generates onChar events.
   *
   * @param text Text to type
   */
  void typeText(char const * text);

  /**
   * @brief Reads user input and return the inserted line
   *
   * This method returns when user press ENTER/RETURN.
   *
   * @param maxLength Maximum amount of character the user can type. 0 = unlimited.
   *
   * @return Returns what user typed and edited.
   */
  char const * edit(int maxLength = 0);

  /**
   * @brief Gets current content
   *
   * @return Returns what user typed.
   */
  char const * get() { return m_text; }

  /**
   * @brief Sets insert mode state
   *
   * @param value If True insert mode is enabled (default), if False insert mode is disabled.
   */
  void setInsertMode(bool value) { m_insertMode = value; }


  // delegates

  /**
   * @brief Read character delegate
   *
   * This delegate is called whenever a character needs to be read, and no Terminal has been specified.
   * The delegate should block until a character is received.
   *
   * First parameter represents a pointer to the receiving character code.
   */
  Delegate<int *> onRead;

  /**
   * @brief Write character delegate
   *
   * This delegate is called whenever a character needs to be written, and no Terminal has been specified.
   *
   * First parameter represents the character code to send.
   */
  Delegate<int> onWrite;

  /**
   * @brief A delegate called whenever a character has been received
   *
   * First parameter represents a pointer to the receiving character code.
   */
  Delegate<int *>  onChar;

  /**
   * @brief A delegate called whenever carriage return has been pressed
   *
   * First parameter specifies the action to perform when ENTER has been pressed:
   *
   *    0 = new line and end editing (default)
   *    1 = end editing
   *    2 = continue editing
   */
  Delegate<int *> onCarriageReturn;

  /**
   * @brief A delegate called whenever a special character has been pressed
   *
   * First parameter receives the special key pressed. The type is LineEditorSpecialChar.
   */
  Delegate<LineEditorSpecialChar> onSpecialChar;


private:

  void beginInput();
  void endInput();
  void setLength(int newLength);

  void write(uint8_t c);
  int read();

  void performCursorUp();
  void performCursorDown();
  void performCursorLeft();
  void performCursorRight();
  void performCursorHome();
  void performCursorEnd();
  void performDeleteRight();
  void performDeleteLeft();


  Terminal *          m_terminal;
  TerminalController  m_termctrl;
  char *              m_text;
  int                 m_textLength;
  int                 m_allocated;
  int16_t             m_inputPos;
  int16_t             m_state;     // -1 = begin input, 0 = normal input, 1 = ESC, 2 = CTRL-Q, >=31 = CSI (actual value specifies the third char if present)
  int16_t             m_homeCol;
  int16_t             m_homeRow;
  bool                m_insertMode;
  char *              m_typeText;
  int                 m_typingIndex;
};



} // end of namespace

