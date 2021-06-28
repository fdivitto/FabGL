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


#include "codepages.h"


#pragma GCC optimize ("O2")



namespace fabgl {



///////////////////////////////////////////////////////////////////////////////////
// Codepage 437

const VirtualKeyToASCII VK2ASCII437[] = {

  { VK_GRAVE_a,      0x85 },    // à
  { VK_GRAVE_e,      0x8a },    // è
  { VK_ACUTE_e,      0x82 },    // é
  { VK_GRAVE_i,      0x8d },    // ì
  { VK_GRAVE_o,      0x95 },    // ò
  { VK_GRAVE_u,      0x97 },    // ù
  { VK_CEDILLA_c,    0x87 },    // ç
  { VK_ESZETT,       0xe1 },    // ß
  { VK_UMLAUT_u,     0x81 },    // ü
  { VK_UMLAUT_o,     0x94 },    // ö
  { VK_UMLAUT_a,     0x84 },    // ä
  { VK_ACUTEACCENT,  0x27 },    // ´ -> '  (not in cp437)
  { VK_POUND,        0x9c },    // £
  { VK_EURO,         0xee },    // € -> ε  (not in cp437)
  { VK_DEGREE,       0xf8 },    // °
  { VK_NEGATION,     0xaa },    // ¬
  { VK_SQUARE,       0xfd },    // ²
  { VK_MU,           0xe6 },    // µ
  { VK_CEDILLA_C,    0x80 },    // Ç
  { VK_TILDE_n,      0xa4 },    // ñ
  { VK_TILDE_N,      0xa5 },    // Ñ
  { VK_UPPER_a,      0xa6 },    // ª
  { VK_ACUTE_a,      0xa0 },    // á
  { VK_ACUTE_o,      0xa2 },    // ó
  { VK_ACUTE_u,      0xa3 },    // ú
  { VK_UMLAUT_i,     0x8b },    // ï
  { VK_EXCLAIM_INV,  0xad },    // ¡
  { VK_QUESTION_INV, 0xa8 },    // ¿
  { VK_ACUTE_A,      0x41 },    // Á -> A  (not in cp437)
  { VK_ACUTE_E,      0x90 },    // É
  { VK_ACUTE_I,      0x49 },    // Í -> I  (not in cp437)
  { VK_ACUTE_O,      0xe0 },    // Ó -> O  (not in cp437)
  { VK_ACUTE_U,      0x55 },    // Ú -> U  (not in cp437)
  { VK_GRAVE_A,      0x41 },    // À -> A  (not in cp437)
  { VK_GRAVE_E,      0x45 },    // È -> E  (not in cp437)
  { VK_GRAVE_I,      0x49 },    // Ì -> I  (not in cp437)
  { VK_GRAVE_O,      0x4f },    // Ò -> O  (not in cp437)
  { VK_GRAVE_U,      0x55 },    // Ù -> U  (not in cp437)
  { VK_INTERPUNCT,   0xfa },    // ·
  { VK_UMLAUT_e,     0x89 },    // ë
  { VK_UMLAUT_A,     0x8e },    // Ä
  { VK_UMLAUT_E,     0x45 },    // Ë -> E  (not in cp437)
  { VK_UMLAUT_I,     0x49 },    // Ï -> I  (not in cp437)
  { VK_UMLAUT_O,     0x99 },    // Ö
  { VK_UMLAUT_U,     0x9a },    // Ü
  { VK_CARET_a,      0x83 },    // â
  { VK_CARET_e,      0x88 },    // ê
  { VK_CARET_i,      0x8c },    // î
  { VK_CARET_o,      0x93 },    // ô
  { VK_CARET_u,      0x96 },    // û
  { VK_CARET_A,      0x41 },    // Â -> A  (not in cp437)
  { VK_CARET_E,      0x45 },    // Ê -> E  (not in cp437)
  { VK_CARET_I,      0x49 },    // Î -> I  (not in cp437)
  { VK_CARET_O,      0x4f },    // Ô -> O  (not in cp437)
  { VK_CARET_U,      0x55 },    // Û -> U  (not in cp437)
  { VK_TILDE_a,      0x61 },    // ã -> a  (not in cp437)
  { VK_TILDE_A,      0x41 },    // Ã -> A  (not in cp437)
  { VK_TILDE_o,      0x6f },    // õ -> o  (not in cp437)
  { VK_TILDE_O,      0x4f },    // Õ -> O  (not in cp437)
  { VK_GRAVE_y,      0x79 },    // ỳ -> y  (not in cp437)
  { VK_GRAVE_Y,      0x59 },    // Ỳ -> Y  (not in cp437)
  { VK_ACUTE_y,      0x79 },    // ý -> y  (not in cp437)
  { VK_ACUTE_Y,      0x59 },    // Ý -> Y  (not in cp437)
  { VK_CARET_y,      0x79 },    // ŷ -> y  (not in cp437)
  { VK_CARET_Y,      0x59 },    // Ŷ -> Y  (not in cp437)
  { VK_UMLAUT_y,     0x98 },    // ÿ
  { VK_UMLAUT_Y,     0x59 },    // Ÿ -> Y  (not in cp437)

  { VK_NONE,         0x00 },    // end of table
};

const CodePage CodePage437 = {
  437,
  VK2ASCII437,
};



///////////////////////////////////////////////////////////////////////////////////
// Codepage 1252

const VirtualKeyToASCII VK2ASCII1252[] = {
  { VK_GRAVE_a,      0xe0 },    // à
  { VK_GRAVE_e,      0xe8 },    // è
  { VK_ACUTE_e,      0xe9 },    // é
  { VK_GRAVE_i,      0xec },    // ì
  { VK_GRAVE_o,      0xf2 },    // ò
  { VK_GRAVE_u,      0xf9 },    // ù
  { VK_CEDILLA_c,    0xe7 },    // ç
  { VK_ESZETT,       0xdf },    // ß
  { VK_UMLAUT_u,     0xfc },    // ü
  { VK_UMLAUT_o,     0xf6 },    // ö
  { VK_UMLAUT_a,     0xe4 },    // ä
  { VK_ACUTEACCENT,  0xb4 },    // ´
  { VK_POUND,        0xa3 },    // £
  { VK_EURO,         0x80 },    // €
  { VK_DEGREE,       0xb0 },    // °
  { VK_SECTION,      0xa7 },    // §
  { VK_NEGATION,     0xac },    // ¬
  { VK_SQUARE,       0xb2 },    // ²
  { VK_MU,           0xb5 },    // µ
  { VK_CEDILLA_C,    0xc7 },    // Ç
  { VK_TILDE_n,      0xf1 },    // ñ
  { VK_TILDE_N,      0xd1 },    // Ñ
  { VK_UPPER_a,      0xaa },    // ª
  { VK_ACUTE_a,      0xe1 },    // á
  { VK_ACUTE_o,      0xf3 },    // ó
  { VK_ACUTE_u,      0xfa },    // ú
  { VK_UMLAUT_i,     0xef },    // ï
  { VK_EXCLAIM_INV,  0xa1 },    // ¡
  { VK_QUESTION_INV, 0xbf },    // ¿
  { VK_ACUTE_A,      0xc1 },    // Á
  { VK_ACUTE_E,      0xc9 },    // É
  { VK_ACUTE_I,      0xcd },    // Í
  { VK_ACUTE_O,      0xd3 },    // Ó
  { VK_ACUTE_U,      0xda },    // Ú
  { VK_GRAVE_A,      0xc0 },    // À
  { VK_GRAVE_E,      0xc8 },    // È
  { VK_GRAVE_I,      0xcc },    // Ì
  { VK_GRAVE_O,      0xd2 },    // Ò
  { VK_GRAVE_U,      0xd9 },    // Ù
  { VK_INTERPUNCT,   0xb7 },    // ·
  { VK_DIAERESIS,    0xa8 },    // ¨
  { VK_UMLAUT_e,     0xeb },    // ë
  { VK_UMLAUT_A,     0xc4 },    // Ä
  { VK_UMLAUT_E,     0xcb },    // Ë
  { VK_UMLAUT_I,     0xcf },    // Ï
  { VK_UMLAUT_O,     0xd6 },    // Ö
  { VK_UMLAUT_U,     0xdc },    // Ü
  { VK_CARET_a,      0xe2 },    // â
  { VK_CARET_e,      0xea },    // ê
  { VK_CARET_i,      0xee },    // î
  { VK_CARET_o,      0xf4 },    // ô
  { VK_CARET_u,      0xfb },    // û
  { VK_CARET_A,      0xc2 },    // Â
  { VK_CARET_E,      0xca },    // Ê
  { VK_CARET_I,      0xce },    // Î
  { VK_CARET_O,      0xd4 },    // Ô
  { VK_CARET_U,      0xdb },    // Û
  { VK_TILDE_a,      0xe3 },    // ã
  { VK_TILDE_A,      0xc3 },    // Ã
  { VK_TILDE_o,      0xf5 },    // õ
  { VK_TILDE_O,      0xd5 },    // Õ
  { VK_GRAVE_y,      0x79 },    // ỳ -> y  (not in cp1252)
  { VK_GRAVE_Y,      0x59 },    // Ỳ -> Y  (not in cp1252)
  { VK_ACUTE_y,      0xfd },    // ý
  { VK_ACUTE_Y,      0xdd },    // Ý
  { VK_CARET_y,      0x79 },    // ŷ -> y  (not in cp1252)
  { VK_CARET_Y,      0x59 },    // Ŷ -> Y  (not in cp1252)
  { VK_UMLAUT_y,     0xff },    // ÿ
  { VK_UMLAUT_Y,     0x9f },    // Ÿ

  { VK_NONE,         0x00 },    // end of table
};

const CodePage CodePage1252 = {
  1252,
  VK2ASCII1252,
};




///////////////////////////////////////////////////////////////////////////////////
// virtualKeyToASCII


// -1 = virtual key cannot be translated to ASCII
// of VirtualKeyItem uses:
//    - .vk
//    - .CTRL
//    - .SHIFT
//    - .SCROLLLOCK
int virtualKeyToASCII(VirtualKeyItem const & item, CodePage const * codepage)
{
  int r = -1;

  if (item.CTRL) {

      // CTRL + ....

      switch (item.vk) {

        case VK_SPACE:
          r = ASCII_NUL;                    // CTRL SPACE = NUL
          break;

        case VK_BACKSPACE:
          r = ASCII_DEL;                    // CTRL BACKSPACE = DEL
          break;

        case VK_DELETE:
        case VK_KP_DELETE:
          r = ASCII_DEL;                    // DELETE = dEL
          break;

        case VK_RETURN:
        case VK_KP_ENTER:
          r = ASCII_LF;                     // CTRL ENTER = LF
          break;

        case VK_ESCAPE:
          r = ASCII_ESC;                    // CTRL ESCAPE = ESC
          break;

        case VK_2:
          r = ASCII_NUL;                    // CTRL 2 = NUL
          break;

        case VK_6:
          r = ASCII_RS;                     // CTRL 6 = RS
          break;

        case VK_a ... VK_z:
          r = item.vk - VK_a + ASCII_SOH ;  // CTRL alpha = SOH (a) ...SUB (z)
          break;

        case VK_A ... VK_Z:
          r = item.vk - VK_A + ASCII_SOH;   // CTRL alpha = SOH (A) ...SUB (Z)
          break;

        case VK_MINUS:
          r = ASCII_US;                     // CTRL - = US
          break;

        case VK_BACKSLASH:
          r = ASCII_FS;                     // CTRL \ = FS
          break;

        case VK_QUESTION:
          r = ASCII_US;                     // CTRL ? = US
          break;

        case VK_LEFTBRACKET:
          r = ASCII_ESC;                    // CTRL [ = ESC
          break;

        case VK_RIGHTBRACKET:
          r = ASCII_GS;                     // CTRL ] = GS
          break;

        case VK_TILDE:
          r = ASCII_RS;                     // CTRL ~ = RS
          break;

        default:
          break;

      }

  } else {

      switch (item.vk) {

        case VK_BACKSPACE:
          r = ASCII_BS;                     // BACKSPACE = BS
          break;

        case VK_RETURN:
        case VK_KP_ENTER:
          r = ASCII_CR;                     // ENTER = CR
          break;

        case VK_TAB:
          if (!item.SHIFT)
            r = ASCII_HT;                   // TAB = HT
          break;

        case VK_ESCAPE:
          r = ASCII_ESC;                    // ESCAPE = ESC
          break;

        case VK_SCROLLLOCK:
          r = item.SCROLLLOCK ? ASCII_XOFF : ASCII_XON; // SCROLLLOCK = switch XOFF / XON
          break;

        case VK_0 ... VK_9:
          r = item.vk - VK_0 + '0';         // digits
          break;

        case VK_KP_0 ... VK_KP_9:
          r = item.vk - VK_KP_0 + '0';      // keypad digits
          break;

        case VK_a ... VK_z:
          r = item.vk - VK_a + 'a';         // lowercase alpha
          break;

        case VK_A ... VK_Z:
          r = item.vk - VK_A + 'A';         // uppercase alpha
          break;

        case VK_SPACE:
          r = ASCII_SPC;                    // SPACE
          break;

        case VK_QUOTE:
          r = '\'';                         // '
          break;

        case VK_QUOTEDBL:
          r = '"';                          // "
          break;

        case VK_EQUALS:
          r = '=';                          // =
          break;

        case VK_MINUS:
        case VK_KP_MINUS:
          r = '-';                          // -
          break;

        case VK_PLUS:
        case VK_KP_PLUS:
          r = '+';                          // +
          break;

        case VK_KP_MULTIPLY:
        case VK_ASTERISK:
          r = '*';                          // *
          break;

        case VK_KP_DIVIDE:
        case VK_SLASH:
          r = '/';                          // /
          break;

        case VK_KP_PERIOD:
        case VK_PERIOD:
          r = '.';                          // .
          break;

        case VK_COLON:
          r = ':';                          // :
          break;

        case VK_COMMA:
          r = ',';                          // ,
          break;

        case VK_SEMICOLON:
          r = ';';                          // ;
          break;

        case VK_AMPERSAND:
          r = '&';                          // &
          break;

        case VK_VERTICALBAR:
          r = '|';                          // |
          break;

        case VK_HASH:
          r = '#';                          // #
          break;

        case VK_AT:
          r = '@';                          // @
          break;

        case VK_CARET:
          r = '^';                          // ^
          break;

        case VK_DOLLAR:
          r = '$';                          // $
          break;

        case VK_GRAVEACCENT:
          r = 0x60;                         // `
          break;

        case VK_PERCENT:
          r = '%';                          // %
          break;

        case VK_EXCLAIM:
          r = '!';                          // !
          break;

        case VK_LEFTBRACE:
          r = '{';                          // {
          break;

        case VK_RIGHTBRACE:
          r = '}';                          // }
          break;

        case VK_LEFTPAREN:
          r = '(';                          // (
          break;

        case VK_RIGHTPAREN:
          r = ')';                          // )
          break;

        case VK_LESS:
          r = '<';                          // <
          break;

        case VK_GREATER:
          r = '>';                          // >
          break;

        case VK_UNDERSCORE:
          r = '_';                          // _
          break;

        case VK_BACKSLASH:
          r = '\\';                         // backslash
          break;

        case VK_QUESTION:
          r = '?';                          // ?
          break;

        case VK_LEFTBRACKET:
          r = '[';                          // [
          break;

        case VK_RIGHTBRACKET:
          r = ']';                          // ]
          break;

        case VK_TILDE:
          r = '~';                          // ~
          break;

        default:
          // look into codepage table
          if (codepage) {
            for (auto cpitem = codepage->convTable; cpitem->vk != VK_NONE; ++cpitem) {
              if (item.vk == cpitem->vk) {
                r = cpitem->ASCII;
                break;
              }
            }
          }
          break;

      }

  }

  return r;
}




} // fabgl namespace
