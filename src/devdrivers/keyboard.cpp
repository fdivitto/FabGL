/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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
    m_SCodeToVKConverterTask(nullptr),
    m_virtualKeyQueue(nullptr),
    m_scancodeSet(2),
    m_lastDeadKey(VK_NONE),
    m_codepage(nullptr)
{
}


Keyboard::~Keyboard()
{
  enableVirtualKeys(false, false);
}


void Keyboard::begin(bool generateVirtualKeys, bool createVKQueue, int PS2Port)
{
  PS2Device::begin(PS2Port);

  m_CTRL       = false;
  m_LALT       = false;
  m_RALT       = false;
  m_SHIFT      = false;
  m_CAPSLOCK   = false;
  m_GUI        = false;
  m_NUMLOCK    = false;
  m_SCROLLLOCK = false;

  m_numLockLED     = false;
  m_capsLockLED    = false;
  m_scrollLockLED  = false;

  m_uiApp = nullptr;

  reset();

  enableVirtualKeys(generateVirtualKeys, createVKQueue);
}


void Keyboard::begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO, bool generateVirtualKeys, bool createVKQueue)
{
  PS2Controller::begin(clkGPIO, dataGPIO);
  PS2Controller::setKeyboard(this);
  begin(generateVirtualKeys, createVKQueue, 0);
}


void Keyboard::enableVirtualKeys(bool generateVirtualKeys, bool createVKQueue)
{
  PS2DeviceLock lock(this);

  if (createVKQueue)
    generateVirtualKeys = true;

  // create task and queue?

  if (!m_virtualKeyQueue && createVKQueue)
    m_virtualKeyQueue = xQueueCreate(FABGLIB_KEYBOARD_VIRTUALKEY_QUEUE_SIZE, sizeof(VirtualKeyItem));

  if (!m_SCodeToVKConverterTask && generateVirtualKeys)
    xTaskCreate(&SCodeToVKConverterTask, "", Keyboard::scancodeToVirtualKeyTaskStackSize, this, FABGLIB_SCODETOVK_TASK_PRIORITY, &m_SCodeToVKConverterTask);

  // destroy in reverse order

  if (m_SCodeToVKConverterTask && !generateVirtualKeys) {
    vTaskDelete(m_SCodeToVKConverterTask);
    m_SCodeToVKConverterTask = nullptr;
  }

  if (m_virtualKeyQueue && !createVKQueue) {
    vQueueDelete(m_virtualKeyQueue);
    m_virtualKeyQueue = nullptr;
  }
}


// reset keyboard, set scancode 2 and US layout
bool Keyboard::reset()
{
  memset(m_VKMap, 0, sizeof(m_VKMap));

  // sets default layout
  setLayout(&USLayout);

  // 350ms keyboard poweron delay (look at NXP M68HC08 designer reference manual)
  vTaskDelay(350 / portTICK_PERIOD_MS);

  // tries up to three times to reset keyboard
  for (int i = 0; i < 3; ++i) {
    m_keyboardAvailable = send_cmdReset();
    if (m_keyboardAvailable)
      break;
    vTaskDelay(350 / portTICK_PERIOD_MS);
  }
  // give the time to the device to be fully initialized
  vTaskDelay(200 / portTICK_PERIOD_MS);

  send_cmdSetScancodeSet(2);

  return m_keyboardAvailable;
}


bool Keyboard::setScancodeSet(int value)
{
  if (m_SCodeToVKConverterTask) {
    // virtual keys enabled, just 1 and 2 are allowed
    if (value == 1 || value == 2) {
      m_scancodeSet = value;
      return true;
    }
  } else {
    // no virtual keys enabled, just try to tell keyboard which set we need
    if (send_cmdSetScancodeSet(value)) {
      m_scancodeSet = value;
      return true;
    }
  }
  return false;
}


bool Keyboard::setLEDs(bool numLock, bool capsLock, bool scrollLock)
{
  m_numLockLED    = numLock;
  m_capsLockLED   = capsLock;
  m_scrollLockLED = scrollLock;
  return send_cmdLEDs(numLock, capsLock, scrollLock);
}


void Keyboard::getLEDs(bool * numLock, bool * capsLock, bool * scrollLock)
{
  *numLock    = m_numLockLED;
  *capsLock   = m_capsLockLED;
  *scrollLock = m_scrollLockLED;
}


void Keyboard::updateLEDs()
{
  send_cmdLEDs(m_NUMLOCK, m_CAPSLOCK, m_SCROLLLOCK);
  m_numLockLED    = m_NUMLOCK;
  m_capsLockLED   = m_CAPSLOCK;
  m_scrollLockLED = m_SCROLLLOCK;
}


int Keyboard::scancodeAvailable()
{
  return dataAvailable();
}


int Keyboard::getNextScancode(int timeOutMS, bool requestResendOnTimeOut)
{
  while (true) {
    int r = getData(timeOutMS);
    if (r == -1 && CLKTimeOutError()) {
      // try to recover a stall sending a re-enable scanning command
      send_cmdEnableScanning();
    }
    if (r == -1 && requestResendOnTimeOut) {
      requestToResendLastByte();
      continue;
    }
    return r;
  }
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
                             "VK_LGUI", "VK_RGUI", "VK_ESCAPE", "VK_PRINTSCREEN", "VK_SYSREQ", "VK_INSERT", "VK_KP_INSERT", "VK_DELETE", "VK_KP_DELETE", "VK_BACKSPACE", "VK_HOME", "VK_KP_HOME", "VK_END", "VK_KP_END", "VK_PAUSE", "VK_BREAK",
                             "VK_SCROLLLOCK", "VK_NUMLOCK", "VK_CAPSLOCK", "VK_TAB", "VK_RETURN", "VK_KP_ENTER", "VK_APPLICATION", "VK_PAGEUP", "VK_KP_PAGEUP", "VK_PAGEDOWN", "VK_KP_PAGEDOWN", "VK_UP", "VK_KP_UP",
                             "VK_DOWN", "VK_KP_DOWN", "VK_LEFT", "VK_KP_LEFT", "VK_RIGHT", "VK_KP_RIGHT", "VK_KP_CENTER", "VK_F1", "VK_F2", "VK_F3", "VK_F4", "VK_F5", "VK_F6", "VK_F7", "VK_F8", "VK_F9", "VK_F10", "VK_F11", "VK_F12",
                             "VK_GRAVE_a", "VK_GRAVE_e", "VK_ACUTE_e", "VK_GRAVE_i", "VK_GRAVE_o", "VK_GRAVE_u", "VK_CEDILLA_c", "VK_ESZETT", "VK_UMLAUT_u",
                             "VK_UMLAUT_o", "VK_UMLAUT_a", "VK_CEDILLA_C", "VK_TILDE_n", "VK_TILDE_N", "VK_UPPER_a", "VK_ACUTE_a", "VK_ACUTE_i", "VK_ACUTE_o", "VK_ACUTE_u", "VK_UMLAUT_i", "VK_EXCLAIM_INV", "VK_QUESTION_INV",
                             "VK_ACUTE_A","VK_ACUTE_E","VK_ACUTE_I","VK_ACUTE_O","VK_ACUTE_U", "VK_GRAVE_A","VK_GRAVE_E","VK_GRAVE_I","VK_GRAVE_O","VK_GRAVE_U", "VK_INTERPUNCT", "VK_DIAERESIS",
                             "VK_UMLAUT_e", "VK_UMLAUT_A", "VK_UMLAUT_E", "VK_UMLAUT_I", "VK_UMLAUT_O", "VK_UMLAUT_U", "VK_CARET_a", "VK_CARET_e", "VK_CARET_i", "VK_CARET_o", "VK_CARET_u", "VK_CARET_A", "VK_CARET_E",
                             "VK_CARET_I", "VK_CARET_O", "VK_CARET_U", "VK_ASCII",
                          };
  return VKTOSTR[virtualKey];
}
#endif


// -1 = virtual key cannot be translated to ASCII
int Keyboard::virtualKeyToASCII(VirtualKey virtualKey)
{
  VirtualKeyItem item;
  item.vk         = virtualKey;
  item.down       = true;
  item.CTRL       = m_CTRL;
  item.LALT       = m_LALT;
  item.RALT       = m_RALT;
  item.SHIFT      = m_SHIFT;
  item.GUI        = m_GUI;
  item.CAPSLOCK   = m_CAPSLOCK;
  item.NUMLOCK    = m_NUMLOCK;
  item.SCROLLLOCK = m_SCROLLLOCK;
  return fabgl::virtualKeyToASCII(item, m_codepage);
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

  // manage keypad
  // NUMLOCK ON, SHIFT OFF => generate VK_KP_number
  // NUMLOCK ON, SHIFT ON  => generate VK_KP_cursor_control (as when NUMLOCK is OFF)
  // NUMLOCK OFF           => generate VK_KP_cursor_control
  if (m_NUMLOCK & !m_SHIFT) {
    switch (vk) {
      case VK_KP_DELETE:
        vk = VK_KP_PERIOD;
        break;
      case VK_KP_INSERT:
        vk = VK_KP_0;
        break;
      case VK_KP_END:
        vk = VK_KP_1;
        break;
      case VK_KP_DOWN:
        vk = VK_KP_2;
        break;
      case VK_KP_PAGEDOWN:
        vk = VK_KP_3;
        break;
      case VK_KP_LEFT:
        vk = VK_KP_4;
        break;
      case VK_KP_CENTER:
        vk = VK_KP_5;
        break;
      case VK_KP_RIGHT:
        vk = VK_KP_6;
        break;
      case VK_KP_HOME:
        vk = VK_KP_7;
        break;
      case VK_KP_UP:
        vk = VK_KP_8;
        break;
      case VK_KP_PAGEUP:
        vk = VK_KP_9;
        break;
      default:
        break;
    }
  }

  return vk;
}


VirtualKey Keyboard::manageCAPSLOCK(VirtualKey vk)
{
  if (m_CAPSLOCK) {
    // inverts letters case
    if (vk >= VK_a && vk <= VK_z)
      vk = (VirtualKey)(vk - VK_a + VK_A);
    else if (vk >= VK_A && vk <= VK_Z)
      vk = (VirtualKey)(vk - VK_A + VK_a);
  }
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
                                         def->lalt == m_LALT &&
                                         def->ralt == m_RALT &&
                                         def->shift == m_SHIFT) {
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
  item->vk         = VK_NONE;
  item->down       = true;
  item->CTRL       = m_CTRL;
  item->LALT       = m_LALT;
  item->RALT       = m_RALT;
  item->SHIFT      = m_SHIFT;
  item->GUI        = m_GUI;
  item->CAPSLOCK   = m_CAPSLOCK;
  item->NUMLOCK    = m_NUMLOCK;
  item->SCROLLLOCK = m_SCROLLLOCK;

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

    // manage CAPSLOCK
    item->vk = manageCAPSLOCK(item->vk);

    // alternate VK (virtualkeys modified by shift, alt, ...)
    item->vk = VKtoAlternateVK(item->vk, item->down);

    // update shift, alt, ctrl, capslock, numlock and scrollock states and LEDs
    switch (item->vk) {
      case VK_LCTRL:
      case VK_RCTRL:
        m_CTRL = item->down;
        break;
      case VK_LALT:
        m_LALT = item->down;
        break;
      case VK_RALT:
        m_RALT = item->down;
        break;
      case VK_LSHIFT:
      case VK_RSHIFT:
        m_SHIFT = item->down;
        break;
      case VK_LGUI:
      case VK_RGUI:
        m_GUI = item->down;
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

  // fill ASCII field
  int ascii = fabgl::virtualKeyToASCII(*item, m_codepage);
  item->ASCII = ascii > -1 ? ascii : 0;

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
  item.ASCII       = virtualKeyToASCII(virtualKey);
  item.CTRL        = m_CTRL;
  item.LALT        = m_LALT;
  item.RALT        = m_RALT;
  item.SHIFT       = m_SHIFT;
  item.GUI         = m_GUI;
  item.CAPSLOCK    = m_CAPSLOCK;
  item.NUMLOCK     = m_NUMLOCK;
  item.SCROLLLOCK  = m_SCROLLLOCK;
  injectVirtualKey(item, insert);
}


// inject a virtual key item into virtual key queue calling injectVirtualKey() and into m_uiApp
void Keyboard::postVirtualKeyItem(VirtualKeyItem const & item)
{
  // add into m_virtualKeyQueue and update m_VKMap
  injectVirtualKey(item, false);

  // need to send events to uiApp?
  if (m_uiApp) {
    uiEvent evt = uiEvent(nullptr, item.down ? UIEVT_KEYDOWN : UIEVT_KEYUP);
    evt.params.key.VK    = item.vk;
    evt.params.key.ASCII = item.ASCII;
    evt.params.key.LALT  = item.LALT;
    evt.params.key.RALT  = item.RALT;
    evt.params.key.CTRL  = item.CTRL;
    evt.params.key.SHIFT = item.SHIFT;
    evt.params.key.GUI   = item.GUI;
    m_uiApp->postEvent(&evt);
  }
}


// converts keypad virtual key to number (VK_KP_1 = 1, VK_KP_DOWN = 2, etc...)
// -1 = no convertible
int Keyboard::convKeypadVKToNum(VirtualKey vk)
{
  switch (vk) {
    case VK_KP_0:
    case VK_KP_INSERT:
      return 0;
    case VK_KP_1:
    case VK_KP_END:
      return 1;
    case VK_KP_2:
    case VK_KP_DOWN:
      return 2;
    case VK_KP_3:
    case VK_KP_PAGEDOWN:
      return 3;
    case VK_KP_4:
    case VK_KP_LEFT:
      return 4;
    case VK_KP_5:
    case VK_KP_CENTER:
      return 5;
    case VK_KP_6:
    case VK_KP_RIGHT:
      return 6;
    case VK_KP_7:
    case VK_KP_HOME:
      return 7;
    case VK_KP_8:
    case VK_KP_UP:
      return 8;
    case VK_KP_9:
    case VK_KP_PAGEUP:
      return 9;
    default:
      return -1;
  };
}


void Keyboard::SCodeToVKConverterTask(void * pvParameters)
{
  Keyboard * keyboard = (Keyboard*) pvParameters;

  // manage ALT + Keypad num
  uint8_t ALTNUMValue = 0;  // current value (0 = no value, 0 is not allowed)

  while (true) {

    VirtualKeyItem item;

    if (keyboard->blockingGetVirtualKey(&item)) {

      // onVirtualKey may set item.vk = VK_NONE!
      keyboard->onVirtualKey(&item.vk, item.down);

      if (item.vk != VK_NONE) {

        // manage left-ALT + NUM
        if (!isALT(item.vk) && keyboard->m_LALT) {
          // ALT was down, is this a keypad number?
          int num = convKeypadVKToNum(item.vk);
          if (num >= 0) {
            // yes this is a keypad num, if down update ALTNUMValue
            if (item.down)
              ALTNUMValue = (ALTNUMValue * 10 + num) & 0xff;
          } else {
            // no, back to normal case
            ALTNUMValue = 0;
            keyboard->postVirtualKeyItem(item);
          }
        } else if (ALTNUMValue > 0 && isALT(item.vk) && !item.down) {
          // ALT is up and ALTNUMValue contains a valid value, add it
          keyboard->postVirtualKeyItem(item); // post ALT up
          item.vk          = VK_ASCII;
          item.down        = true;
          item.scancode[0] = 0;
          item.ASCII       = ALTNUMValue;
          keyboard->postVirtualKeyItem(item); // ascii key down
          item.down        = false;
          keyboard->postVirtualKeyItem(item); // ascii key up
          ALTNUMValue = 0;
        } else {
          // normal case
          keyboard->postVirtualKeyItem(item);
        }

      }

    }

  }

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
  bool r = (m_SCodeToVKConverterTask && item && xQueueReceive(m_virtualKeyQueue, item, msToTicks(timeOutMS)) == pdTRUE);
  if (r && m_scancodeSet == 1)
    convertScancode2to1(item);
  return r;
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


void Keyboard::convertScancode2to1(VirtualKeyItem * item)
{
  uint8_t * rpos = item->scancode;
  uint8_t * wpos = rpos;
  uint8_t * epos = rpos + sizeof(VirtualKeyItem::scancode);
  while (*rpos && rpos < epos) {
    if (*rpos == 0xf0) {
      ++rpos;
      *wpos++ = 0x80 | convScancodeSet2To1(*rpos++);
    } else
      *wpos++ = convScancodeSet2To1(*rpos++);
  }
  if (wpos < epos)
    *wpos = 0;
}


uint8_t Keyboard::convScancodeSet2To1(uint8_t code)
{
  // 8042 scancodes set 2 to 1 translation table
  static const uint8_t S2TOS1[256] = {
    0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58, 0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
    0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a, 0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
    0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c, 0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
    0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e, 0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
    0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60, 0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
    0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e, 0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
    0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b, 0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
    0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45, 0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54,
    0x80, 0x81, 0x82, 0x41, 0x54, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
  };
  return S2TOS1[code];
}


} // end of namespace
