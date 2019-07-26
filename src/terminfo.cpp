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


#include "terminfo.h"



namespace fabgl {



const TermInfoKbdConv kbdConv_Generic[] = {
  // Cursor Up => CTRL-E, WordStar left
  { VK_UP, "\x05" },

  // Cursor Down => CTRL-X, WordStar Down
  { VK_DOWN, "\x18" },

  // Cursor Left => CTRL-S, WordStar Left
  { VK_LEFT, "\x13" },

  // Cursor Right => CTRL-D, WordStar right
  { VK_RIGHT, "\x04" },

  // Home => CTRL-Q S, WordStar Home
  { VK_HOME, "\x11" "S" },

  // End => CTRL-Q D, WordStar End
  { VK_END, "\x11" "D" },

  // PageUp => CTRL-C, WordStar PageUp
  { VK_PAGEUP, "\x12" },

  // PageDown => CTRL-R, WordStar PageDown
  { VK_PAGEDOWN, "\x03" },

  // Backspace => CTRL-H, CP/M delete char left (but WordStar just moves cursor left)
  { VK_BACKSPACE, "\x08" },

  // Delete => CTRL-G, WordStar delete char right
  { VK_DELETE, "\x07" },

  // Last item marker
  { VK_NONE, nullptr },
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lear Siegler ADM-3A


// sorted by TermSeq name
const TermInfoVideoConv videoConv_ADM3A[] = {
  // BS => Cursor Left
  { "\x08", 1, { ConvCtrl::CursorLeft, ConvCtrl::END } },

  // VT => Cursor Up
  { "\x0b", 1, { ConvCtrl::CursorUp, ConvCtrl::END} },

  // FF => Cursor Right
  { "\x0c", 1, { ConvCtrl::CursorRight, ConvCtrl::END} },

  // ETB => Erase to end of screen
  { "\x17", 1, { ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // CAN => Erase to end of line
  { "\x18", 1, { ConvCtrl::EraseToEndOfLine, ConvCtrl::END} },

  // SUB => Cursor home and Clear screen
  { "\x1a", 1, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // RS => Cursor Home
  { "\x1e", 1, { ConvCtrl::CursorHome, ConvCtrl::END} },

  // 'ESC G 0' => Char Attribute: Normal video
  { "\eG0", 3, { ConvCtrl::AttrNormal, ConvCtrl::END} },

  // 'ESC G 1' => Char Attribute: Blank
  { "\eG1", 3, { ConvCtrl::AttrBlank, ConvCtrl::END} },

  // 'ESC G 2' => Char Attribute: Blink
  { "\eG2", 3, { ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G 4' => Char Attribute: Reverse
  { "\eG4", 3, { ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC G 6' => Char Attribute: Reverse blinking
  { "\eG6", 3, { ConvCtrl::AttrReverse, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G 8' => Char Attribute: Underline
  { "\eG8", 3, { ConvCtrl::AttrUnderline, ConvCtrl::END} },

  // 'ESC G :' => Char Attribute: Underline blinking
  { "\eG:", 3, { ConvCtrl::AttrUnderline, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G <' => Char Attribute: Underline reverse
  { "\eG<", 3, { ConvCtrl::AttrUnderline, ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC G >' => Char Attribute: Underline, reverse, bliking
  { "\eG>", 3, { ConvCtrl::AttrUnderline, ConvCtrl::AttrReverse, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G @' => Char Attribute: Reduce
  { "\eG@", 3, { ConvCtrl::AttrReduce, ConvCtrl::END} },

  // 'ESC G B' => Char Attribute: Reduce blinking
  { "\eGB", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G D' => Char Attribute: Reduce reverse
  { "\eGD", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC G F' => Char Attribute: Reduce reverse blinking
  { "\eGF", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrReverse, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G H' => Char Attribute: Reduce underline
  { "\eGH", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrUnderline, ConvCtrl::END} },

  // 'ESC G J' => Char Attribute: Reduce underline blinking
  { "\eGJ", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrUnderline, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G L' => Char Attribute: Reduce underline reverse
  { "\eGL", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrUnderline, ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC G N' => Char Attribute: Reduce underline reverse blinking
  { "\eGN", 3, { ConvCtrl::AttrReduce, ConvCtrl::AttrUnderline, ConvCtrl::AttrReverse, ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC = y x' => Cursor Position (cursorX = x-31, cursorY = y-31)
  { "\e=\xff\xff", 4, { ConvCtrl::CursorPos, ConvCtrl::END} },

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_ADM3A = {
  "",
  videoConv_ADM3A,
  kbdConv_Generic
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lear Siegler ADM-31


// sorted by TermSeq name
const TermInfoVideoConv videoConv_ADM31[] = {
  // BS => Cursor Left
  { "\x08", 1, { ConvCtrl::CursorLeft, ConvCtrl::END } },

  // VT => Cursor Up
  { "\x0b", 1, { ConvCtrl::CursorUp, ConvCtrl::END} },

  // FF => Cursor Right
  { "\x0c", 1, { ConvCtrl::CursorRight, ConvCtrl::END} },

  // SUB => Cursor home and Clear screen
  { "\x1a", 1, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // RS => Cursor Home
  { "\x1e", 1, { ConvCtrl::CursorHome, ConvCtrl::END} },

  // US => New line
  { "\x1f", 1, { ConvCtrl::CarriageReturn, ConvCtrl::LineFeed, ConvCtrl::END} },

  // 'ESC = y x' => Cursor Position (cursorX = x-31, cursorY = y-31)
  { "\e=\xff\xff", 4, { ConvCtrl::CursorPos, ConvCtrl::END} },

  // 'ESC G 4' => Char Attribute: Reverse
  { "\eG4", 3, { ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC G 3' => Char Attribute: Underline
  { "\eG3", 3, { ConvCtrl::AttrUnderline, ConvCtrl::END} },

  // 'ESC G 2' => Char Attribute: Blink
  { "\eG2", 3, { ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC G 0' => Char Attribute: Normal video
  { "\eG0", 3, { ConvCtrl::AttrNormal, ConvCtrl::END} },

  // 'ESC )' => Char Attribute: Half intensity ON
  { "\e)", 2, { ConvCtrl::AttrReduce, ConvCtrl::END} },

  // 'ESC (' => Char Attribute: Half intensity OFF
  { "\e(", 2, { ConvCtrl::AttrReduceOff, ConvCtrl::END} },

  // 'ESC E' => Insert Line
  { "\eE", 2, { ConvCtrl::InsertLine, ConvCtrl::END} },

  // 'ESC Q' => Insert Character
  { "\eQ", 2, { ConvCtrl::InsertChar, ConvCtrl::END} },

  // 'ESC R' => Delete Line
  { "\eR", 2, { ConvCtrl::DeleteLine, ConvCtrl::END} },

  // 'ESC W' => Delete Character
  { "\eW", 2, { ConvCtrl::DeleteCharacter, ConvCtrl::END} },

  // 'ESC T' => Erase to end of line
  { "\eT", 2, { ConvCtrl::EraseToEndOfLine, ConvCtrl::END} },

  // 'ESC Y' => Erase to end of screen
  { "\eY", 2, { ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // 'ESC *' => Cursor home and Clear screen
  { "\e*", 2, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_ADM31 = {
  "",
  videoConv_ADM31,
  kbdConv_Generic
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hazeltine 1500


// sorted by TermSeq name
const TermInfoVideoConv videoConv_Hazeltine1500[] = {
  // '~ VT' => Cursor Down
  { "~\x0b", 2, { ConvCtrl::LineFeed, ConvCtrl::END} },

  // '~ FF' => Cursor Up
  { "~\x0c", 2, { ConvCtrl::CursorUp, ConvCtrl::END} },

  // DLE => Cursor Right
  { "\x10", 1, { ConvCtrl::CursorRight, ConvCtrl::END} },

  // '~ SI' => Erase to end of line
  { "~\x0f", 2, { ConvCtrl::EraseToEndOfLine, ConvCtrl::END} },

  // '~ DC1 x y' => Cursor Position (cursorX = x+1, cursorY = y+1)
  { "~\x11\xff\xff", 4, { ConvCtrl::CursorPos2, ConvCtrl::END} },

  // '~ DC2' => Cursor Home
  { "~\x12", 2, { ConvCtrl::CursorHome, ConvCtrl::END} },

  // '~ DC3' => Delete Line
  { "~\x13", 2, { ConvCtrl::DeleteLine, ConvCtrl::END} },

  // '~ CAN' => Clear to end of Screen
  { "~\x18", 2, { ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // '~ ETB' => Clear to end of Screen
  { "~\x17", 2, { ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // '~ FS' => Cursor home and Clear screen
  { "~\x1c", 2, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // '~ SUB' => Insert Line
  { "~\x1a", 2, { ConvCtrl::InsertLine, ConvCtrl::END} },

  // '~ EM' => Char Attribute: Half intensity ON
  { "~\x19", 2, { ConvCtrl::AttrReduce, ConvCtrl::END} },

  // '~ US' => Char Attribute: Half intensity oFF
  { "~\x1f", 2, { ConvCtrl::AttrReduceOff, ConvCtrl::END} },

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_Hazeltine1500 = {
  "",
  videoConv_Hazeltine1500,
  kbdConv_Generic
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Osborne I


// sorted by TermSeq name
const TermInfoVideoConv videoConv_Osborne[] = {
  // BS => Cursor Left
  { "\x08", 1, { ConvCtrl::CursorLeft, ConvCtrl::END } },

  // 'VT' => Cursor Up
  { "\x0b", 1, { ConvCtrl::CursorUp, ConvCtrl::END} },

  // 'FF' => Cursor Right
  { "\x0c", 1, { ConvCtrl::CursorRight, ConvCtrl::END} },

  // SUB => Cursor home and Clear screen
  { "\x1a", 1, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // RS => Cursor Home
  { "\x1e", 1, { ConvCtrl::CursorHome, ConvCtrl::END} },

  // 'ESC )' => Char Attribute: Half intensity ON
  { "\e)", 2, { ConvCtrl::AttrReduce, ConvCtrl::END} },

  // 'ESC (' => Char Attribute: Half intensity OFF
  { "\e(", 2, { ConvCtrl::AttrReduceOff, ConvCtrl::END} },

  // 'ESC E' => Insert Line
  { "\eE", 2, { ConvCtrl::InsertLine, ConvCtrl::END} },

  // 'ESC l' => Char Attribute: Underline
  { "\el", 3, { ConvCtrl::AttrUnderline, ConvCtrl::END} },

  // 'ESC m' => Char Attribute: Underline OFF
  { "\el", 3, { ConvCtrl::AttrUnderlineOff, ConvCtrl::END} },

  // 'ESC Q' => Insert Character
  { "\eQ", 2, { ConvCtrl::InsertChar, ConvCtrl::END} },

  // 'ESC R' => Delete Line
  { "\eR", 2, { ConvCtrl::DeleteLine, ConvCtrl::END} },

  // 'ESC T' => Erase to end of line
  { "\eT", 2, { ConvCtrl::EraseToEndOfLine, ConvCtrl::END} },

  // 'ESC W' => Delete Character
  { "\eW", 2, { ConvCtrl::DeleteCharacter, ConvCtrl::END} },

  // 'ESC = y x' => Cursor Position (cursorX = x-31, cursorY = y-31)
  { "\e=\xff\xff", 4, { ConvCtrl::CursorPos, ConvCtrl::END} },

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_Osborne = {
  "",
  videoConv_Osborne,
  kbdConv_Generic
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Kaypro


// sorted by TermSeq name
const TermInfoVideoConv videoConv_Kaypro[] = {

  // BS => Cursor Left
  { "\x08", 1, { ConvCtrl::CursorLeft, ConvCtrl::END } },

  // 'FF' => Cursor Right
  { "\x0c", 1, { ConvCtrl::CursorRight, ConvCtrl::END} },

  // 'VT' => Cursor Up
  { "\x0b", 1, { ConvCtrl::CursorUp, ConvCtrl::END} },

  // ETB => Erase to end of screen
  { "\x17", 1, { ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // CAN => Erase to end of line
  { "\x18", 1, { ConvCtrl::EraseToEndOfLine, ConvCtrl::END} },

  // SUB => Cursor home and Clear screen
  { "\x1a", 1, { ConvCtrl::CursorHome, ConvCtrl::EraseToEndOfScreen, ConvCtrl::END} },

  // RS => Cursor Home
  { "\x1e", 1, { ConvCtrl::CursorHome, ConvCtrl::END} },

  // 'ESC E' => Insert Line
  { "\eE", 2, { ConvCtrl::InsertLine, ConvCtrl::END} },

  // 'ESC R' => Delete Line
  { "\eR", 2, { ConvCtrl::DeleteLine, ConvCtrl::END} },

  // 'ESC = y x' => Cursor Position (cursorX = x-31, cursorY = y-31)
  { "\e=\xff\xff", 4, { ConvCtrl::CursorPos, ConvCtrl::END} },

  // 'ESC B 0' => Char Attribute: Reverse
  { "\eB0", 3, { ConvCtrl::AttrReverse, ConvCtrl::END} },

  // 'ESC C 0' => Char Attribute: Reverse Off
  { "\eC0", 3, { ConvCtrl::AttrReverseOff, ConvCtrl::END} },

  // 'ESC B 1' => Char Attribute: Reduce
  { "\eB1", 3, { ConvCtrl::AttrReduce, ConvCtrl::END} },

  // 'ESC C 1' => Char Attribute: Reduce Off
  { "\eC1", 3, { ConvCtrl::AttrReduceOff, ConvCtrl::END} },

  // 'ESC B 2' => Char Attribute: Blink
  { "\eB2", 3, { ConvCtrl::AttrBlink, ConvCtrl::END} },

  // 'ESC C 2' => Char Attribute: Blink Off
  { "\eC2", 3, { ConvCtrl::AttrBlinkOff, ConvCtrl::END} },

  // 'ESC B 3' => Char Attribute: Underline
  { "\eB3", 3, { ConvCtrl::AttrUnderline, ConvCtrl::END} },

  // 'ESC C 3' => Char Attribute: Underline Off
  { "\eC3", 3, { ConvCtrl::AttrUnderlineOff, ConvCtrl::END} },

  // 'ESC B 4' => Cursor On
  { "\eB4", 4, { ConvCtrl::CursorOn, ConvCtrl::END} },

  // 'ESC C 4' => Cursor Off
  { "\eC4", 4, { ConvCtrl::CursorOff, ConvCtrl::END} },

  // 'ESC B 6' => Save Cursor
  { "\eB6", 4, { ConvCtrl::SaveCursor, ConvCtrl::END} },

  // 'ESC C 6' => Restore Cursor
  { "\eC6", 4, { ConvCtrl::RestoreCursor, ConvCtrl::END} },

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_Kaypro = {
  "",
  videoConv_Kaypro,
  kbdConv_Generic
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VT52


// sorted by TermSeq name
const TermInfoVideoConv videoConv_VT52[] = {

  // Last item marker
  { nullptr, 0, { } },
};


const TermInfo term_VT52 = {
  "\e[?2l",         // <= set VT52 mode
  videoConv_VT52,
  kbdConv_Generic
};


}
