/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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



#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "keyboard.h"


#pragma GCC optimize ("O2")


namespace fabgl {




int Keyboard::scancodeToVirtualKeyTaskStackSize = FABGLIB_DEFAULT_SCODETOVK_TASK_STACK_SIZE;



Keyboard::Keyboard()
  : m_keyboardAvailable(false),
    m_lastDeadKey(VK_NONE)
{
}


void Keyboard::begin(bool generateVirtualKeys, bool createVKQueue, int PS2Port)
{
  PS2Device::begin(PS2Port);

  m_CTRL       = false;
  m_ALT        = false;
  m_SHIFT      = false;
  m_CAPSLOCK   = false;
  m_NUMLOCK    = false;
  m_SCROLLLOCK = false;

  m_numLockLED     = false;
  m_capsLockLED    = false;
  m_scrollLockLED  = false;

  m_SCodeToVKConverterTask = nullptr;
  m_virtualKeyQueue        = nullptr;

  m_uiApp = nullptr;

  reset();

  if (generateVirtualKeys || createVKQueue) {
    if (createVKQueue)
      m_virtualKeyQueue = xQueueCreate(FABGLIB_KEYBOARD_VIRTUALKEY_QUEUE_SIZE, sizeof(VirtualKeyItem));
    xTaskCreate(&SCodeToVKConverterTask, "", Keyboard::scancodeToVirtualKeyTaskStackSize, this, FABGLIB_SCODETOVK_TASK_PRIORITY, &m_SCodeToVKConverterTask);
  }
}


void Keyboard::begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO, bool generateVirtualKeys, bool createVKQueue)
{
  PS2Controller * PS2 = PS2Controller::instance();
  PS2->begin(clkGPIO, dataGPIO);
  PS2->setKeyboard(this);
  begin(generateVirtualKeys, createVKQueue, 0);
}


// reset keyboard, set scancode 2 and US layout
bool Keyboard::reset()
{
  memset(m_VKMap, 0, sizeof(m_VKMap));

  // sets default layout
  setLayout(&USLayout);

  // tries up to three times to reset keyboard
  for (int i = 0; i < 3; ++i) {
    m_keyboardAvailable = send_cmdReset() && send_cmdSetScancodeSet(2);
    if (m_keyboardAvailable)
      break;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  return m_keyboardAvailable;
}


bool Keyboard::setScancodeSet(int value)
{
  return send_cmdSetScancodeSet(value);
}


void Keyboard::getLEDs(bool * numLock, bool * capsLock, bool * scrollLock)
{
  *numLock    = m_numLockLED;
  *capsLock   = m_capsLockLED;
  *scrollLock = m_scrollLockLED;
}


int Keyboard::scancodeAvailable()
{
  return dataAvailable();
}


int Keyboard::getNextScancode(int timeOutMS, bool requestResendOnTimeOut)
{
  while (true) {
    int r = getData(timeOutMS);
    if (r == -1 && requestResendOnTimeOut) {
      requestToResendLastByte();
      continue;
    }
    return r;
  }
}


void Keyboard::updateLEDs()
{
  send_cmdLEDs(m_NUMLOCK, m_CAPSLOCK, m_SCROLLLOCK);
  m_numLockLED    = m_NUMLOCK;
  m_capsLockLED   = m_CAPSLOCK;
  m_scrollLockLED = m_SCROLLLOCK;
}


void Keyboard::setLayout(const KeyboardLayout * layout)
{
  m_layout = layout;
}


#if FABGLIB_HAS_VirtualKeyO_STRING
char const * Keyboard::virtualKeyToString(VirtualKey virtualKey)
{
  char const * VKTOSTR[] = { "VK_NONE", "VK_SPACE", "VK_0", "VK_1", "VK_2", "VK_3", "VK_4", "VK_5", "VK_6", "VK_7", "VK_8", "VK_9", "VK_KP_0", "VK_KP_1", "VK_KP_2",
                             "VK_KP_3", "VK_KP_4", "VK_KP_5", "VK_KP_6", "VK_KP_7", "VK_KP_8", "VK_KP_9", "VK_a", "VK_b", "VK_c", "VK_d", "VK_e", "VK_f", "VK_g", "VK_h",
                             "VK_i", "VK_j", "VK_k", "VK_l", "VK_m", "VK_n", "VK_o", "VK_p", "VK_q", "VK_r", "VK_s", "VK_t", "VK_u", "VK_v", "VK_w", "VK_x", "VK_y", "VK_z",
                             "VK_A", "VK_B", "VK_C", "VK_D", "VK_E", "VK_F", "VK_G", "VK_H", "VK_I", "VK_J", "VK_K", "VK_L", "VK_M", "VK_N", "VK_O", "VK_P", "VK_Q", "VK_R",
                             "VK_S", "VK_T", "VK_U", "VK_V", "VK_W", "VK_X", "VK_Y", "VK_Z", "VK_GRAVEACCENT", "VK_ACUTEACCENT", "VK_QUOTE", "VK_QUOTEDBL", "VK_EQUALS", "VK_MINUS", "VK_KP_MINUS",
                             "VK_PLUS", "VK_KP_PLUS", "VK_KP_MULTIPLY", "VK_ASTERISK", "VK_BACKSLASH", "VK_KP_DIVIDE", "VK_SLASH", "VK_KP_PERIOD", "VK_PERIOD", "VK_COLON",
                             "VK_COMMA", "VK_SEMICOLON", "VK_AMPERSAND", "VK_VERTICALBAR", "VK_HASH", "VK_AT", "VK_CARET", "VK_DOLLAR", "VK_POUND", "VK_EURO", "VK_PERCENT",
                             "VK_EXCLAIM", "VK_QUESTION", "VK_LEFTBRACE", "VK_RIGHTBRACE", "VK_LEFTBRACKET", "VK_RIGHTBRACKET", "VK_LEFTPAREN", "VK_RIGHTPAREN", "VK_LESS",
                             "VK_GREATER", "VK_UNDERSCORE", "VK_DEGREE", "VK_SECTION", "VK_TILDE", "VK_NEGATION", "VK_LSHIFT", "VK_RSHIFT", "VK_LALT", "VK_RALT", "VK_LCTRL", "VK_RCTRL",
                             "VK_LGUI", "VK_RGUI", "VK_ESCAPE", "VK_PRINTSCREEN1", "VK_PRINTSCREEN2", "VK_SYSREQ", "VK_INSERT", "VK_KP_INSERT", "VK_DELETE", "VK_KP_DELETE", "VK_BACKSPACE", "VK_HOME", "VK_KP_HOME", "VK_END", "VK_KP_END", "VK_PAUSE", "VK_BREAK",
                             "VK_SCROLLLOCK", "VK_NUMLOCK", "VK_CAPSLOCK", "VK_TAB", "VK_RETURN", "VK_KP_ENTER", "VK_APPLICATION", "VK_PAGEUP", "VK_KP_PAGEUP", "VK_PAGEDOWN", "VK_KP_PAGEDOWN", "VK_UP", "VK_KP_UP",
                             "VK_DOWN", "VK_KP_DOWN", "VK_LEFT", "VK_KP_LEFT", "VK_RIGHT", "VK_KP_RIGHT", "VK_KP_CENTER", "VK_F1", "VK_F2", "VK_F3", "VK_F4", "VK_F5", "VK_F6", "VK_F7", "VK_F8", "VK_F9", "VK_F10", "VK_F11", "VK_F12",
                             "VK_GRAVE_a", "VK_GRAVE_e", "VK_ACUTE_e", "VK_GRAVE_i", "VK_GRAVE_o", "VK_GRAVE_u", "VK_CEDILLA_c", "VK_ESZETT", "VK_UMLAUT_u",
                             "VK_UMLAUT_o", "VK_UMLAUT_a", "VK_CEDILLA_C", "VK_TILDE_n", "VK_TILDE_N", "VK_UPPER_a", "VK_ACUTE_a", "VK_ACUTE_i", "VK_ACUTE_o", "VK_ACUTE_u", "VK_UMLAUT_i", "VK_EXCLAIM_INV", "VK_QUESTION_INV",
                             "VK_ACUTE_A","VK_ACUTE_E","VK_ACUTE_I","VK_ACUTE_O","VK_ACUTE_U", "VK_GRAVE_A","VK_GRAVE_E","VK_GRAVE_I","VK_GRAVE_O","VK_GRAVE_U", "VK_INTERPUNCT", "VK_DIAERESIS",
                             "VK_UMLAUT_e", "VK_UMLAUT_A", "VK_UMLAUT_E", "VK_UMLAUT_I", "VK_UMLAUT_O", "VK_UMLAUT_U", "VK_CARET_a", "VK_CARET_e", "VK_CARET_i", "VK_CARET_o", "VK_CARET_u", "VK_CARET_A", "VK_CARET_E","VK_CARET_I","VK_CARET_O","VK_CARET_U",
                          };
  return VKTOSTR[virtualKey];
}
#endif


// -1 = virtual key cannot be translated to ASCII
int Keyboard::virtualKeyToASCII(VirtualKey virtualKey)
{
  switch (virtualKey) {
    case VK_SPACE:
      return m_CTRL ? ASCII_NUL : ASCII_SPC;   // CTRL + SPACE = NUL, otherwise 0x20

    case VK_0 ... VK_9:
      return virtualKey - VK_0 + '0';

    case VK_KP_0 ... VK_KP_9:
      return virtualKey - VK_KP_0 + '0';

    case VK_a ... VK_z:
      return virtualKey - VK_a + (m_CTRL ? ASCII_SOH : 'a');  // CTRL + letter = SOH (a) ...SUB (z), otherwise the lower letter

    case VK_A ... VK_Z:
      return virtualKey - VK_A + (m_CTRL ? ASCII_SOH : 'A');  // CTRL + letter = SOH (A) ...SUB (Z), otherwise the lower letter

    case VK_GRAVE_a:
      return 0xE0;  // à

    case VK_GRAVE_e:
      return 0xE8;  // è

    case VK_ACUTE_e:
      return 0xE9;  // é

    case VK_GRAVE_i:
      return 0xEC;  // ì

    case VK_GRAVE_o:
      return 0xF2;  // ò

    case VK_GRAVE_u:
      return 0xF9;  // ù

    case VK_CEDILLA_c:
      return 0x87;  // ç

    case VK_ESZETT:
      return 0xDF;  // ß

    case VK_UMLAUT_u:
      return 0xFC;  // ü

    case VK_UMLAUT_o:
      return 0xF6;  // ö

    case VK_UMLAUT_a:
      return 0xE4;  // ä

    case VK_GRAVEACCENT:
      return 0x60;  // "`"

    case VK_ACUTEACCENT:
      return '\'';  // 0xB4

    case VK_QUOTE:
      return '\'';  // "'"

    case VK_QUOTEDBL:
      return '"';

    case VK_EQUALS:
      return '=';

    case VK_MINUS:
    case VK_KP_MINUS:
      return '-';

    case VK_PLUS:
    case VK_KP_PLUS:
      return '+';

    case VK_KP_MULTIPLY:
    case VK_ASTERISK:
      return '*';

    case VK_BACKSLASH:
      return m_CTRL ? ASCII_FS : '\\';  // CTRL \ = FS, otherwise '\'

    case VK_KP_DIVIDE:
    case VK_SLASH:
      return '/';

    case VK_KP_PERIOD:
    case VK_PERIOD:
      return '.';

    case VK_COLON:
      return ':';

    case VK_COMMA:
      return ',';

    case VK_SEMICOLON:
      return ';';

    case VK_AMPERSAND:
      return '&';

    case VK_VERTICALBAR:
      return '|';

    case VK_HASH:
      return '#';

    case VK_AT:
      return '@';

    case VK_CARET:
      return '^';

    case VK_DOLLAR:
      return '$';

    case VK_POUND:
      return 0xA3;  // '£'

    case VK_EURO:
      return 0xEE;  // '€' - non 8 bit ascii

    case VK_PERCENT:
      return '%';

    case VK_EXCLAIM:
      return '!';

    case VK_QUESTION:
      return m_CTRL ? ASCII_US : '?'; // CTRL ? = US, otherwise '?'

    case VK_LEFTBRACE:
      return '{';

    case VK_RIGHTBRACE:
      return '}';

    case VK_LEFTBRACKET:
      return m_CTRL ? ASCII_ESC : '['; // CTRL [ = ESC, otherwise '['

    case VK_RIGHTBRACKET:
      return m_CTRL ? ASCII_GS : ']'; // CTRL ] = GS, otherwise ']'

    case VK_LEFTPAREN:
      return '(';

    case VK_RIGHTPAREN:
      return ')';

    case VK_LESS:
      return '<';

    case VK_GREATER:
      return '>';

    case VK_UNDERSCORE:
      return '_';

    case VK_DEGREE:
      return 0xf8;  // in  ISO/IEC 8859 it should 0xB0, but in code page 437 it is 0xF8

    case VK_SECTION:
      return 0xA7;  // "§"

    case VK_TILDE:
      return m_CTRL ? ASCII_RS : '~';   // CTRL ~ = RS, otherwise "~"

    case VK_NEGATION:
      return 0xAA;  // "¬"

    case VK_BACKSPACE:
      return ASCII_BS;

    case VK_DELETE:
    case VK_KP_DELETE:
      return ASCII_DEL;

    case VK_RETURN:
    case VK_KP_ENTER:
      return ASCII_CR;

    case VK_TAB:
      return ASCII_HT;

    case VK_ESCAPE:
      return ASCII_ESC;

    case VK_SCROLLLOCK:
      return m_SCROLLLOCK ? ASCII_XOFF : ASCII_XON;

    // spanish and catalan symbols

    case VK_CEDILLA_C:
      return 0x80; // 'Ç'

    case VK_TILDE_n:
      return 0xA4; // 'ñ'

    case VK_TILDE_N:
      return 0xA5; // 'Ñ'

    case VK_UPPER_a:
      return 0xA6; // 'ª'

    case VK_ACUTE_a:
      return 0xA0; // 'á'

    case VK_ACUTE_o:
      return 0xA2; // 'ó'

    case VK_ACUTE_u:
      return 0xA3; // 'ú'

    case VK_UMLAUT_i:
      return 0x8B; // 'ï'

    case VK_EXCLAIM_INV:
      return 0xAD; // '¡'

    case VK_QUESTION_INV:
      return 0xA8; // '¿'

    case VK_ACUTE_A:
      return 0xB5; // 'Á'

    case VK_ACUTE_E:
      return 0x90; // 'É'

    case VK_ACUTE_I:
      return 0xD6; // 'Í'

    case VK_ACUTE_O:
      return 0xE0; // 'Ó'

    case VK_ACUTE_U:
      return 0xE9; // 'Ú'

    case VK_GRAVE_A:
      return 0xB7; // 'À'

    case VK_GRAVE_E:
      return 0xD4; // 'È'

    case VK_GRAVE_I:
      return 0xDE; // 'Ì'

    case VK_GRAVE_O:
      return 0xE3; // 'Ò'

    case VK_GRAVE_U:
      return 0xEB; // 'Ù'

    case VK_INTERPUNCT:
      return 0xFA; // '·'

    case VK_DIAERESIS: // '¨'
      return '"';

    case VK_UMLAUT_e: // 'ë'
      return 0xEB;

    case VK_UMLAUT_A: // 'Ä'
      return 0XC4;

    case VK_UMLAUT_E:  // 'Ë'
      return 0XCB;

    case VK_UMLAUT_I:  // 'Ï'
      return  0XCF;

    case VK_UMLAUT_O:  // 'Ö'
      return  0XD6;

    case VK_UMLAUT_U:  // 'Ü'
      return 0XDC;

    case VK_CARET_a:   // 'â'
      return 0XE2;

    case VK_CARET_e:   // 'ê'
      return 0XEA;

    case VK_CARET_i:   // 'î'
      return 0XEE;

    case VK_CARET_o:   // 'ô'
      return 0xF6;

    case VK_CARET_u:   // 'û'
      return 0xFC;

    case VK_CARET_A:   // 'Â'
      return 0XC2;

    case VK_CARET_E:  // 'Ê'
      return 0XCA;

    case VK_CARET_I:   // 'Î'
      return 0XCE;

    case VK_CARET_O:   // 'Ô'
      return 0XD4;

    case VK_CARET_U:  // 'Û'
      return 0XDB;
		
    default:
      return -1;
  }
}


VirtualKey Keyboard::scancodeToVK(uint8_t scancode, bool isExtended, KeyboardLayout const * layout)
{
  VirtualKey vk = VK_NONE;

  if (layout == nullptr)
    layout = m_layout;

  VirtualKeyDef const * def = isExtended ? layout->exScancodeToVK : layout->scancodeToVK;
  for (; def->scancode; ++def)
    if (def->scancode == scancode) {
      vk = def->virtualKey;
      break;
    }

  if (vk == VK_NONE && layout->inherited)
    vk = scancodeToVK(scancode, isExtended, layout->inherited);

  return vk;
}


VirtualKey Keyboard::VKtoAlternateVK(VirtualKey in_vk, bool down, KeyboardLayout const * layout)
{
  VirtualKey vk = VK_NONE;

  if (layout == nullptr)
    layout = m_layout;

  // this avoids releasing a required key when SHIFT has been pressed after the key but before releasing
  if (!down && isVKDown(in_vk))
    vk = in_vk;

  if (vk == VK_NONE) {
    // handle this case:
    //   - derived KEY up without any SHIFT (because released before the KEY, ie SHIFT+"1" => "!", but you release the SHIFT before "1")
    // this avoid to maintain a KEY DOWN when you release the SHIFT key before the KEY ()
    for (AltVirtualKeyDef const * def = layout->alternateVK; def->reqVirtualKey != VK_NONE; ++def) {
      if (def->reqVirtualKey == in_vk && isVKDown(def->virtualKey)) {
        vk = def->virtualKey;
        break;
      }
    }
  }

  if (vk == VK_NONE) {
    // handle these cases:
    //   - KEY down with SHIFTs already down
    //   - KEY up with SHIFTs still down
    for (AltVirtualKeyDef const * def = layout->alternateVK; def->reqVirtualKey != VK_NONE; ++def) {
      if (def->reqVirtualKey == in_vk && def->ctrl == m_CTRL &&
                                         def->alt == m_ALT &&
                                         (def->shift == m_SHIFT || (def->capslock && def->capslock == m_CAPSLOCK)) &&
                                         def->numlock == m_NUMLOCK) {
        vk = def->virtualKey;
        break;
      }
    }
  }

  if (vk == VK_NONE && layout->inherited)
    vk = VKtoAlternateVK(in_vk, down, layout->inherited);

  return vk == VK_NONE ? in_vk : vk;
}


bool Keyboard::blockingGetVirtualKey(VirtualKeyItem * item)
{
  item->vk   = VK_NONE;
  item->down = true;

  uint8_t * scode = item->scancode;

  *scode = getNextScancode();
  if (*scode == 0xE0) {
    // two bytes scancode
    *(++scode) = getNextScancode(100, true);
    if (*scode == 0xF0) {
      // two bytes scancode key up
      *(++scode) = getNextScancode(100, true);
      item->vk = scancodeToVK(*scode, true);
      item->down = false;
    } else {
      // two bytes scancode key down
      item->vk = scancodeToVK(*scode, true);
    }
  } else if (*scode == 0xE1) {
    // special case: "PAUSE" : 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77
    static const uint8_t PAUSECODES[] = {0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77};
    for (int i = 0; i < sizeof(PAUSECODES); ++i) {
      *(++scode) = getNextScancode(100, true);
      if (*scode != PAUSECODES[i])
        break;
      else if (i == sizeof(PAUSECODES) - 1)
        item->vk = VK_PAUSE;
    }
  } else if (*scode == 0xF0) {
    // one byte scancode, key up
    *(++scode) = getNextScancode(100, true);
    item->vk = scancodeToVK(*scode, false);
    item->down = false;
  } else {
    // one byte scancode, key down
    item->vk = scancodeToVK(*scode, false);
  }

  if (item->vk != VK_NONE) {

    // alternate VK (virtualkeys modified by shift, alt, ...)
    item->vk = VKtoAlternateVK(item->vk, item->down);

    // update shift, alt, ctrl, capslock, numlock and scrollock states and LEDs
    switch (item->vk) {
      case VK_LCTRL:
      case VK_RCTRL:
        m_CTRL = item->down;
        break;
      case VK_LALT:
      case VK_RALT:
        m_ALT = item->down;
        break;
      case VK_LSHIFT:
      case VK_RSHIFT:
        m_SHIFT = item->down;
        break;
      case VK_CAPSLOCK:
        if (!item->down) {
          m_CAPSLOCK = !m_CAPSLOCK;
          updateLEDs();
        }
        break;
      case VK_NUMLOCK:
        if (!item->down) {
          m_NUMLOCK = !m_NUMLOCK;
          updateLEDs();
        }
        break;
      case VK_SCROLLLOCK:
        if (!item->down) {
          m_SCROLLLOCK = !m_SCROLLLOCK;
          updateLEDs();
        }
        break;
      default:
        break;
    }

  }

  // manage dead keys - Implemented by Carles Oriol (https://github.com/carlesoriol)
  for (VirtualKey const * dk = m_layout->deadKeysVK; *dk != VK_NONE; ++dk) {
    if (item->vk == *dk) {
      m_lastDeadKey = item->vk;
      item->vk = VK_NONE;
    }
  }
  if (item->vk != m_lastDeadKey && item->vk != VK_NONE) {
    for (DeadKeyVirtualKeyDef const * dk = m_layout->deadkeysToVK; dk->deadKey != VK_NONE; ++dk) {
      if (item->vk == dk->reqVirtualKey && m_lastDeadKey == dk->deadKey) {
        item->vk = dk->virtualKey;
        break;
      }
    }
    if (!item->down && (item->vk != m_lastDeadKey) && (item->vk != VK_RSHIFT) && (item->vk != VK_LSHIFT))
      m_lastDeadKey = VK_NONE;
  }

  // ending zero to item->scancode
  if (scode < item->scancode + sizeof(VirtualKeyItem::scancode) - 1)
    *(++scode) = 0;

  return item->vk != VK_NONE;
}


void Keyboard::injectVirtualKey(VirtualKeyItem const & item, bool insert)
{
  // update m_VKMap
  if (item.down)
    m_VKMap[(int)item.vk >> 3] |= 1 << ((int)item.vk & 7);
  else
    m_VKMap[(int)item.vk >> 3] &= ~(1 << ((int)item.vk & 7));

  // has VK queue? Insert VK into it.
  if (m_virtualKeyQueue) {
    auto ticksToWait = (m_uiApp ? 0 : portMAX_DELAY);  // 0, and not portMAX_DELAY to avoid uiApp locks
    if (insert)
      xQueueSendToFront(m_virtualKeyQueue, &item, ticksToWait);
    else
      xQueueSendToBack(m_virtualKeyQueue, &item, ticksToWait);
  }
}


void Keyboard::injectVirtualKey(VirtualKey virtualKey, bool keyDown, bool insert)
{
  VirtualKeyItem item;
  item.vk          = virtualKey;
  item.down        = keyDown;
  item.scancode[0] = 0;  // this is a manual insert, not scancode associated
  injectVirtualKey(item, insert);
}


void Keyboard::SCodeToVKConverterTask(void * pvParameters)
{
  Keyboard * keyboard = (Keyboard*) pvParameters;

  while (true) {

    VirtualKeyItem item;

    if (keyboard->blockingGetVirtualKey(&item)) {

      // onVirtualKey may set item.vk = VK_NONE!
      keyboard->onVirtualKey(&item.vk, item.down);

      if (item.vk != VK_NONE) {

        // add into m_virtualKeyQueue and update m_VKMap
        keyboard->injectVirtualKey(item, false);

        // need to send events to uiApp?
        if (keyboard->m_uiApp) {
          uiEvent evt = uiEvent(nullptr, item.down ? UIEVT_KEYDOWN : UIEVT_KEYUP);
          evt.params.key.VK    = item.vk;
          evt.params.key.LALT  = keyboard->isVKDown(VK_LALT);
          evt.params.key.RALT  = keyboard->isVKDown(VK_RALT);
          evt.params.key.CTRL  = keyboard->isVKDown(VK_LCTRL)  || keyboard->isVKDown(VK_RCTRL);
          evt.params.key.SHIFT = keyboard->isVKDown(VK_LSHIFT) || keyboard->isVKDown(VK_RSHIFT);
          evt.params.key.GUI   = keyboard->isVKDown(VK_LGUI)   || keyboard->isVKDown(VK_RGUI);
          keyboard->m_uiApp->postEvent(&evt);
        }

      }

    }

  }

}


void Keyboard::suspendVirtualKeyGeneration(bool value)
{
  if (value)
    vTaskSuspend(m_SCodeToVKConverterTask);
  else
    vTaskResume(m_SCodeToVKConverterTask);
}


bool Keyboard::isVKDown(VirtualKey virtualKey)
{
  bool r = m_VKMap[(int)virtualKey >> 3] & (1 << ((int)virtualKey & 7));

  // VK_PAUSE is never released (no scancode sent from keyboard on key up), so when queried it is like released
  if (virtualKey == VK_PAUSE)
    m_VKMap[(int)virtualKey >> 3] &= ~(1 << ((int)virtualKey & 7));

  return r;
}


bool Keyboard::getNextVirtualKey(VirtualKeyItem * item, int timeOutMS)
{
  return (m_SCodeToVKConverterTask && item && xQueueReceive(m_virtualKeyQueue, item, msToTicks(timeOutMS)) == pdTRUE);
}


VirtualKey Keyboard::getNextVirtualKey(bool * keyDown, int timeOutMS)
{
  VirtualKeyItem item;
  if (getNextVirtualKey(&item, timeOutMS)) {
    if (keyDown)
      *keyDown = item.down;
    return item.vk;
  }
  return VK_NONE;
}


int Keyboard::virtualKeyAvailable()
{
  return m_virtualKeyQueue ? uxQueueMessagesWaiting(m_virtualKeyQueue) : 0;
}


void Keyboard::emptyVirtualKeyQueue()
{
  xQueueReset(m_virtualKeyQueue);
}




} // end of namespace
