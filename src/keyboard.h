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


#ifndef _KEYBOARD_H_INCLUDED
#define _KEYBOARD_H_INCLUDED


/**
 * @file
 *
 * @brief This file contains fabgl::KeyboardClass definition and the Keyboard instance.
 */


#include "freertos/FreeRTOS.h"

#include "fabglconf.h"


namespace fabgl {


// ASCII control characters
#define ASCII_NUL  0x00   // Null
#define ASCII_SOH  0x01   // Start of Heading
#define ASCII_STX  0x02   // Start of Text
#define ASCII_ETX  0x03   // End Of Text
#define ASCII_EOT  0x04   // End Of Transmission
#define ASCII_ENQ  0x05   // Enquiry
#define ASCII_ACK  0x06   // Acknowledge
#define ASCII_BELL 0x07   // Bell
#define ASCII_BS   0x08   // Backspace
#define ASCII_HT   0x09   // Horizontal Tab
#define ASCII_LF   0x0A   // Line Feed
#define ASCII_VT   0x0B   // Vertical Tab
#define ASCII_FF   0x0C   // Form Feed
#define ASCII_CR   0x0D   // Carriage Return
#define ASCII_SO   0x0E   // Shift Out
#define ASCII_SI   0x0F   // Shift In
#define ASCII_DLE  0x10   // Data Link Escape
#define ASCII_DC1  0x11   // Device Control 1
#define ASCII_XON  0x11   // Transmission On
#define ASCII_DC2  0x12   // Device Control 2
#define ASCII_DC3  0x13   // Device Control 3
#define ASCII_XOFF 0x13   // Transmission Off
#define ASCII_DC4  0x14   // Device Control 4
#define ASCII_NAK  0x15   // Negative Acknowledge
#define ASCII_SYN  0x16   // Synchronous Idle
#define ASCII_ETB  0x17   // End-of-Transmission-Block
#define ASCII_CAN  0x18   // Cancel
#define ASCII_EM   0x19   // End of Medium
#define ASCII_SUB  0x1A   // Substitute
#define ASCII_ESC  0x1B   // Escape
#define ASCII_FS   0x1C   // File Separator
#define ASCII_GS   0x1D   // Group Separator
#define ASCII_RS   0x1E   // Record Separator
#define ASCII_US   0x1F   // Unit Separator
#define ASCII_SPC  0x20   // Space
#define ASCII_DEL  0x7F   // Delete



/**
 * @brief Represents the type of device attached to PS/2 port.
 */
enum PS2Device {
  UnknownPS2Device,             /**< Unknown device or unable to connect to the device */
  OldATKeyboard,                /**< Old AT keyboard */
  MouseStandard,                /**< Standard mouse */
  MouseWithScrollWheel,         /**< Mouse with Scroll Wheel */
  Mouse5Buttons,                /**< Mouse with 5 buttons */
  MF2KeyboardWithTranslation,   /**< Standard MF2 keyboard with translation */
  M2Keyboard,                   /**< Standard MF2 keyboard. This is the most common value returned by USB/PS2 modern keyboards */
};


/**
 * @brief Represents each possible real or derived (SHIFT + real) key.
 */
enum VirtualKey {
  VK_NONE,            /**< No character (marks the first virtual key) */

  VK_SPACE,           /**< Space */
  VK_0,               /**< Number 0 */
  VK_1,               /**< Number 1 */
  VK_2,               /**< Number 2 */
  VK_3,               /**< Number 3 */
  VK_4,               /**< Number 4 */
  VK_5,               /**< Number 5 */
  VK_6,               /**< Number 6 */
  VK_7,               /**< Number 7 */
  VK_8,               /**< Number 8 */
  VK_9,               /**< Number 9 */
  VK_KP_0,            /**< Keypad number 0 */
  VK_KP_1,            /**< Keypad number 1 */
  VK_KP_2,            /**< Keypad number 2 */
  VK_KP_3,            /**< Keypad number 3 */
  VK_KP_4,            /**< Keypad number 4 */
  VK_KP_5,            /**< Keypad number 5 */
  VK_KP_6,            /**< Keypad number 6 */
  VK_KP_7,            /**< Keypad number 7 */
  VK_KP_8,            /**< Keypad number 8 */
  VK_KP_9,            /**< Keypad number 9 */
  VK_a,               /**< Lower case letter 'a' */
  VK_b,               /**< Lower case letter 'b' */
  VK_c,               /**< Lower case letter 'c' */
  VK_d,               /**< Lower case letter 'd' */
  VK_e,               /**< Lower case letter 'e' */
  VK_f,               /**< Lower case letter 'f' */
  VK_g,               /**< Lower case letter 'g' */
  VK_h,               /**< Lower case letter 'h' */
  VK_i,               /**< Lower case letter 'i' */
  VK_j,               /**< Lower case letter 'j' */
  VK_k,               /**< Lower case letter 'k' */
  VK_l,               /**< Lower case letter 'l' */
  VK_m,               /**< Lower case letter 'm' */
  VK_n,               /**< Lower case letter 'n' */
  VK_o,               /**< Lower case letter 'o' */
  VK_p,               /**< Lower case letter 'p' */
  VK_q,               /**< Lower case letter 'q' */
  VK_r,               /**< Lower case letter 'r' */
  VK_s,               /**< Lower case letter 's' */
  VK_t,               /**< Lower case letter 't' */
  VK_u,               /**< Lower case letter 'u' */
  VK_v,               /**< Lower case letter 'v' */
  VK_w,               /**< Lower case letter 'w' */
  VK_x,               /**< Lower case letter 'x' */
  VK_y,               /**< Lower case letter 'y' */
  VK_z,               /**< Lower case letter 'z' */
  VK_A,               /**< Upper case letter 'A' */
  VK_B,               /**< Upper case letter 'B' */
  VK_C,               /**< Upper case letter 'C' */
  VK_D,               /**< Upper case letter 'D' */
  VK_E,               /**< Upper case letter 'E' */
  VK_F,               /**< Upper case letter 'F' */
  VK_G,               /**< Upper case letter 'G' */
  VK_H,               /**< Upper case letter 'H' */
  VK_I,               /**< Upper case letter 'I' */
  VK_J,               /**< Upper case letter 'J' */
  VK_K,               /**< Upper case letter 'K' */
  VK_L,               /**< Upper case letter 'L' */
  VK_M,               /**< Upper case letter 'M' */
  VK_N,               /**< Upper case letter 'N' */
  VK_O,               /**< Upper case letter 'O' */
  VK_P,               /**< Upper case letter 'P' */
  VK_Q,               /**< Upper case letter 'Q' */
  VK_R,               /**< Upper case letter 'R' */
  VK_S,               /**< Upper case letter 'S' */
  VK_T,               /**< Upper case letter 'T' */
  VK_U,               /**< Upper case letter 'U' */
  VK_V,               /**< Upper case letter 'V' */
  VK_W,               /**< Upper case letter 'W' */
  VK_X,               /**< Upper case letter 'X' */
  VK_Y,               /**< Upper case letter 'Y' */
  VK_Z,               /**< Upper case letter 'Z' */
  VK_GRAVEACCENT,     /**< Grave accent: ` */
  VK_ACUTEACCENT,     /**< Acute accent: ´ */
  VK_QUOTE,           /**< Quote: ' */
  VK_QUOTEDBL,        /**< Double quote: " */
  VK_EQUALS,          /**< Equals: = */
  VK_MINUS,           /**< Minus: - */
  VK_KP_MINUS,        /**< Keypad minus: - */
  VK_PLUS,            /**< Plus: + */
  VK_KP_PLUS,         /**< Keypad plus: + */
  VK_KP_MULTIPLY,     /**< Keypad multiply: * */
  VK_ASTERISK,        /**< Asterisk: * */
  VK_BACKSLASH,       /**< Backslash: \ */
  VK_KP_DIVIDE,       /**< Keypad divide: / */
  VK_SLASH,           /**< Slash: / */
  VK_KP_PERIOD,       /**< Keypad period: . */
  VK_PERIOD,          /**< Period: . */
  VK_COLON,           /**< Colon: : */
  VK_COMMA,           /**< Comma: , */
  VK_SEMICOLON,       /**< Semicolon: ; */
  VK_AMPERSAND,       /**< Ampersand: & */
  VK_VERTICALBAR,     /**< Vertical bar: | */
  VK_HASH,            /**< Hash: # */
  VK_AT,              /**< At: @ */
  VK_CARET,           /**< Caret: ^ */
  VK_DOLLAR,          /**< Dollar: $ */
  VK_POUND,           /**< Pound: £ */
  VK_EURO,            /**< Euro: € */
  VK_PERCENT,         /**< Percent: % */
  VK_EXCLAIM,         /**< Exclamation mark: ! */
  VK_QUESTION,        /**< Question mark: ? */
  VK_LEFTBRACE,       /**< Left brace: { */
  VK_RIGHTBRACE,      /**< Right brace: } */
  VK_LEFTBRACKET,     /**< Left bracket: [ */
  VK_RIGHTBRACKET,    /**< Right bracket: ] */
  VK_LEFTPAREN,       /**< Left parenthesis: ( */
  VK_RIGHTPAREN,      /**< Right parenthesis: ) */
  VK_LESS,            /**< Less: < */
  VK_GREATER,         /**< Greater: > */
  VK_UNDERSCORE,      /**< Underscore: _ */
  VK_DEGREE,          /**< Degree: ° */
  VK_SECTION,         /**< Section: § */
  VK_TILDE,           /**< Tilde: ~ */
  VK_NEGATION,        /**< Negation: ¬ */
  VK_LSHIFT,          /**< Left SHIFT */
  VK_RSHIFT,          /**< Right SHIFT */
  VK_LALT,            /**< Left ALT */
  VK_RALT,            /**< Right ALT */
  VK_LCTRL,           /**< Left CTRL */
  VK_RCTRL,           /**< Right CTRL */
  VK_LGUI,            /**< Left GUI */
  VK_RGUI,            /**< Right GUI */
  VK_ESCAPE,          /**< ESC */
  VK_PRINTSCREEN1,    /**< PRINTSCREEN is translated as separated VK_PRINTSCREEN1 and VK_PRINTSCREEN2. VK_PRINTSCREEN2 is also generated by CTRL or SHIFT + PRINTSCREEN. So pressing PRINTSCREEN both VK_PRINTSCREEN1 and VK_PRINTSCREEN2 are generated, while pressing CTRL+PRINTSCREEN or SHIFT+PRINTSCREEN only VK_PRINTSCREEN2 is generated. */
  VK_PRINTSCREEN2,    /**< PRINTSCREEN. See VK_PRINTSCREEN1 */
  VK_SYSREQ,          /**< SYSREQ */
  VK_INSERT,          /**< INS */
  VK_KP_INSERT,       /**< Keypad INS */
  VK_DELETE,          /**< DEL */
  VK_KP_DELETE,       /**< Keypad DEL */
  VK_BACKSPACE,       /**< Backspace */
  VK_HOME,            /**< HOME */
  VK_KP_HOME,         /**< Keypad HOME */
  VK_END,             /**< END */
  VK_KP_END,          /**< Keypad END */
  VK_PAUSE,           /**< PAUSE */
  VK_BREAK,           /**< CTRL + PAUSE */
  VK_SCROLLLOCK,      /**< SCROLLLOCK */
  VK_NUMLOCK,         /**< NUMLOCK */
  VK_CAPSLOCK,        /**< CAPSLOCK */
  VK_TAB,             /**< TAB */
  VK_RETURN,          /**< RETURN */
  VK_KP_ENTER,        /**< Keypad ENTER */
  VK_APPLICATION,     /**< APPLICATION / MENU key */
  VK_PAGEUP,          /**< PAGEUP */
  VK_KP_PAGEUP,       /**< Keypad PAGEUP */
  VK_PAGEDOWN,        /**< PAGEDOWN */
  VK_KP_PAGEDOWN,     /**< Keypad PAGEDOWN */
  VK_UP,              /**< Cursor UP */
  VK_KP_UP,           /**< Keypad cursor UP  */
  VK_DOWN,            /**< Cursor DOWN */
  VK_KP_DOWN,         /**< Keypad cursor DOWN */
  VK_LEFT,            /**< Cursor LEFT */
  VK_KP_LEFT,         /**< Keypad cursor LEFT */
  VK_RIGHT,           /**< Cursor RIGHT */
  VK_KP_RIGHT,        /**< Keypad cursor RIGHT */
  VK_KP_CENTER,       /**< Keypad CENTER key */
  VK_F1,              /**< F1 function key */
  VK_F2,              /**< F2 function key */
  VK_F3,              /**< F3 function key */
  VK_F4,              /**< F4 function key */
  VK_F5,              /**< F5 function key */
  VK_F6,              /**< F6 function key */
  VK_F7,              /**< F7 function key */
  VK_F8,              /**< F8 function key */
  VK_F9,              /**< F9 function key */
  VK_F10,             /**< F10 function key */
  VK_F11,             /**< F11 function key */
  VK_F12,             /**< F12 function key */
  VK_GRAVE_a,         /**< Grave 'a': à */
  VK_GRAVE_e,         /**< Grave 'e': è */
  VK_ACUTE_e,         /**< Acute 'e': é */
  VK_GRAVE_i,         /**< Grave 'i': ì */
  VK_GRAVE_o,         /**< Grave 'o': ò */
  VK_GRAVE_u,         /**< Grave 'u': ù */
  VK_CEDILLA_c,       /**< Cedilla 'c': ç */
  VK_ESZETT,          /**< Eszett: ß */
  VK_UMLAUT_u,        /**< Umlaut 'u': ü */
  VK_UMLAUT_o,        /**< Umlaut 'o': ö */
  VK_UMLAUT_a,        /**< Umlaut 'a': ä */

  VK_LAST,            // marks the last virtual key
};


/**
 * @brief Associates scancode to virtualkey.
 */
struct VirtualKeyDef {
  uint8_t      scancode;    /**< Raw scancode received from the Keyboard device */
  VirtualKey   virtualKey;  /**< Real virtualkey (non shifted) associated to the scancode */
};


/**
 * @brief Associates a virtualkey and various shift states (ctrl, alt, etc..) to another virtualkey.
 */
struct AltVirtualKeyDef {
  VirtualKey reqVirtualKey; /**< Source virtualkey translated using VirtualKeyDef. */
  struct {
    uint8_t ctrl     : 1;   /**< CTRL needs to be down. */
    uint8_t alt      : 1;   /**< ALT needs to be down. */
    uint8_t shift    : 1;   /**< SHIFT needs to be down (OR-ed with capslock). */
    uint8_t capslock : 1;   /**< CAPSLOCK needs to be down (OR-ed with shift). */
    uint8_t numlock  : 1;   /**< NUMLOCK needs to be down. */
  };
  VirtualKey virtualKey;  /**< Generated virtualkey. */
};


/** @brief All in one structure to fully represent a keyboard layout */
struct KeyboardLayout {
  const char *             name;                /**< Layout name. */
  KeyboardLayout const *   inherited;           /**< Inherited layout. Useful to avoid to repeat the same scancode-virtualkeys associations. */
  VirtualKeyDef            scancodeToVK[92];    /**< Direct one-byte-scancode->virtualkey associations. */
  VirtualKeyDef            exScancodeToVK[32];  /**< Direct extended-scancode->virtualkey associations. Extended scancodes begin with 0xE0. */
  AltVirtualKeyDef         alternateVK[64];     /**< Virtualkeys generated by other virtualkeys and shift combinations. */
};


/** @brief Predefined US layout. Often used as inherited layout for other layouts. */
extern const KeyboardLayout USLayout;

/** @brief UK keyboard layout */
extern const KeyboardLayout UKLayout;

/** @brief German keyboard layout */
extern const KeyboardLayout GermanLayout;

/** @brief Italian keyboard layout */
extern const KeyboardLayout ItalianLayout;


/**
 * @brief The PS2 Keyboard controller class.
 *
 * KeyboardClass interfaces directly to PS2 Controller class (fabgl::PS2ControllerClass) and provides the logic
 * that converts scancodes to virtual keys or ASCII (and ANSI) codes.<br>
 * It optionally creates a task that waits for scan codes from the PS2 device and puts virtual keys in a queue.<br>
 * The PS2 controller uses ULP coprocessor and RTC slow memory to communicate with the PS2 device.<br>
 *
 * It is possible to specify an international keyboard layout. The default is US-layout.<br>
 * There are three predefined kayboard layouts: US (USA), UK (United Kingdom), DE (German) and IT (Italian). Other layout can be added
 * inheriting from US or from any other layout.
 *
 * Applications do not need to create an instance of KeyboardClass because an instance named Keyboard is created automatically.
 *
 * Example:
 *
 *     // Setup pins GPIO33 for CLK and GPIO32 for DATA
 *     Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);  // clk, dat
 *
 *     // Prints name of received virtual keys
 *     while (true)
 *       Serial.printf("VirtualKey = %s\n", Keyboard.virtualKeyToString(Keyboard.getNextVirtualKey()));
 *
 */
class KeyboardClass {

public:

  KeyboardClass();

  /**
   * @brief Initialize KeyboardClass specifying CLOCK and DATA GPIOs.
   *
   * A reset command (KeyboardClass.reset() method) is automatically sent to the keyboard.<br>
   * This method also initializes the PS2ControllerClass, calling its begin() method.
   *
   * @param clkGPIO The GPIO number of Clock line
   * @param dataGPIO The GPIO number of Data line
   * @param generateVirtualKeys If true creates a task which consumes scancodes and produces virtual keys,
   *                            so you can call KeyboardClass.isVKDown().
   * @param createVKQueue If true creates a task which consunes scancodes and produces virtual keys
   *                      and put them in a queue, so you can call KeyboardClass.isVKDown(), KeyboardClass.scancodeAvailable()
   *                      and KeyboardClass.getNextScancode().
   *
   * Example:
   *
   *     // Setup pins GPIO33 for CLK and GPIO32 for DATA
   *     Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);  // clk, dat
   */
  void begin(gpio_num_t clkGPIO, gpio_num_t dataGPIO, bool generateVirtualKeys = true, bool createVKQueue = true);

  /**
   * @brief Send a Reset command to the keyboard.
   *
   * @return True if the keyboard is correctly initialized.
   */
  bool reset();

  /**
   * @brief Return true if a keyboard has been detected and correctly initialized.
   *
   * isKeyboardAvailable() returns a valid value only after KeyboardClass.begin() or KeyboardClass.reset() has been called.
   *
   * @return True if the keyboard is correctly initialized.
   */
  bool isKeyboardAvailable() { return m_keyboardAvailable; }

  /**
   * @brief Set keyboard layout.
   *
   * It is possible to specify an international keyboard layout. The default is US-layout.<br>
   * There are three predefined kayboard layouts: US (USA), UK (United Kingdom), DE (German) and IT (Italian). Other layout can be added
   * inheriting from US or from any other layout.
   *
   * @param layout A pointer to the layout structure.
   *
   * Example:
   *
   *     // Set German layout
   *     setLayout(&fabgl::GermanLayout);
   */
  void setLayout(KeyboardLayout const * layout);

  /**
   * @brief Get current keyboard layout.
   *
   * @return The default or last set keyboard layout.
   */
  KeyboardLayout const * getLayout() { return m_layout; }

  /**
   * @brief Get the virtual keys status.
   *
   * This method allows to know the status of each virtual key (Down or Up).<br>
   * Virtual keys are generated from scancodes only if generateVirtualKeys parameter of KeyboardClass.begin() method is true (default).
   *
   * @param virtualKey The Virtual Key to test.
   *
   * @return True if the specified virtual key is down.
   */
  bool isVKDown(VirtualKey virtualKey);

  /**
   * @brief Get the number of virtual keys available in the queue.
   *
   * Virtual keys are generated from scancodes only if generateVirtualKeys parameter is true (default)
   * and createVKQueue parameter is true (default) of KeyboardClass.begin() method.
   *
   * @return The number of virtual keys available to read.
   */
  int virtualKeyAvailable();

  /**
   * @brief Get a virtual key from the queue.
   *
   * Virtual keys are generated from scancodes only if generateVirtualKeys parameter is true (default)
   * and createVKQueue parameter is true (default) of KeyboardClass.begin() method.
   *
   * @param keyDown A pointer to boolean variable which will contain if the virtual key is depressed (true) or released (false).
   * @param timeOutMS Timeout in milliseconds. -1 means no timeout (infinite time).
   *
   * @return The first virtual key of the queue (VK_NONE if no data is available in the timeout period).
   */
  VirtualKey getNextVirtualKey(bool * keyDown = NULL, int timeOutMS = -1);

  /**
   * @brief Convert virtual key to ASCII.
   *
   * This method converts the specified virtual key to ASCII, if possible.<br>
   * For example VK_A is converted to 'A' (ASCII 0x41), CTRL  + VK_SPACE produces ASCII NUL (0x00), CTRL + letter produces
   * ASCII control codes from SOH (0x01) to SUB (0x1A), CTRL + VK_BACKSLASH produces ASCII FS (0x1C), CTRL + VK_QUESTION produces
   * ASCII US (0x1F), CTRL + VK_LEFTBRACKET produces ASCII ESC (0x1B), CTRL + VK_RIGHTBRACKET produces ASCII GS (0x1D),
   * CTRL + VK_TILDE produces ASCII RS (0x1E) and VK_SCROLLLOCK produces XON or XOFF.
   *
   * @param virtualKey The virtual key to convert.
   *
   * @return The ASCII code of virtual key or -1 if virtual key cannot be translated to ASCII.
   */
  int virtualKeyToASCII(VirtualKey virtualKey);

  /**
   * @brief Get the number of scancodes available in the queue.
   *
   * Scancodes are always generated but they can be consumed by the scancode-to-virtualkeys task. So, in order to use this
   * method KeyboardClass.begin() method should be called with generateVirtualKeys = false and createVKQueue = false.<br>
   * Alternatively it is also possible to suspend the conversion task calling KeyboardClass.suspendVirtualKeyGeneration() method.
   *
   * @return The number of scancodes available to read.
   */
  int scancodeAvailable();

  /**
   * @brief Get a scancode from the queue.
   *
   * Scancodes are always generated but they can be consumed by the scancode-to-virtualkeys task. So, in order to use this
   * method KeyboardClass.begin() method should be called with generateVirtualKeys = false and createVKQueue = false.<br>
   * Alternatively it is also possible to suspend the conversion task calling KeyboardClass.suspendVirtualKeyGeneration() method.
   *
   * @param timeOutMS Timeout in milliseconds. -1 means no timeout (infinite time).
   * @param requestResendOnTimeOut If true and timeout has expired then asks the keyboard to resend the scancode.
   *
   * @return The first scancode of the queue (-1 if no data is available in the timeout period).
   */
  int getNextScancode(int timeOutMS = -1, bool requestResendOnTimeOut = false);

  /**
   * @brief Suspend or resume the virtual key generation task.
   *
   * Use this method to temporarily suspend the scancode to virtual key conversion task. This is useful when
   * scancode are necessary for a limited time.
   *
   * @param value If true conversion task is suspended. If false conversion task is resumed.
   */
  void suspendVirtualKeyGeneration(bool value);

  /**
   * @brief Set keyboard LEDs status.
   *
   * Use this method to switch-on or off the NUMLOCK, CAPSLOCK and SCROLLLOCK LEDs.
   *
   * @param numLock When true the NUMLOCK LED is switched on.
   * @param capsLock When true the CAPSLOCK LED is switched on.
   * @param scrollLock When true the SCROLLLOCK LED is switched on.
   *
   * @return True if command has been successfully delivered to the keyboard.
   */
  bool setLEDs(bool numLock, bool capsLock, bool scrollLock) { return send_cmdLEDs(numLock, capsLock, scrollLock); }

  /**
   * @brief Get keyboard LEDs status.
   *
   * Use this method to know the current status of NUMLOCK, CAPSLOCK and SCROLLLOCK LEDs.
   *
   * @param numLock When true the NUMLOCK LED is switched on.
   * @param capsLock When true the CAPSLOCK LED is switched on.
   * @param scrollLock When true the SCROLLLOCK LED is switched on.
   */
  void getLEDs(bool * numLock, bool * capsLock, bool * scrollLock);

  /**
   * @brief Identify the device attached to the PS2 port.
   *
   * @return The identification ID sent by keyboard.
   */
  PS2Device identify() { PS2Device result; send_cmdIdentify(&result); return result; };

  /**
   * @brief Set typematic rate and delay.
   *
   * If the key is kept pressed down, after repeatDelayMS keyboard starts periodically sending codes with frequency repeatRateMS.
   *
   * @param repeatRateMS Repeat rate in milliseconds (in range 33 ms ... 500 ms).
   * @param repeatDelayMS Repeat delay in milliseconds (in range 250 ms ... 1000 ms, steps of 250 ms).
   *
   * @return True if command has been successfully delivered to the keyboard.
   */
  bool setTypematicRateAndDelay(int repeatRateMS, int repeatDelayMS) { return send_cmdTypematicRateAndDelay(repeatRateMS, repeatDelayMS); }

#if FABGLIB_HAS_VirtualKeyO_STRING
  static char const * virtualKeyToString(VirtualKey virtualKey);
#endif

private:

  bool send(uint8_t cmd, uint8_t expectedReply);
  VirtualKey scancodeToVK(uint8_t scancode, bool isExtended, KeyboardLayout const * layout = NULL);
  VirtualKey VKtoAlternateVK(VirtualKey in_vk, KeyboardLayout const * layout = NULL);
  void updateLEDs();
  VirtualKey blockingGetVirtualKey(bool * keyDown);
  static void SCodeToVKConverterTask(void * pvParameters);
  int getReplyCode(int timeOutMS);

  bool send_cmdLEDs(bool numLock, bool capsLock, bool scrollLock);
  bool send_cmdEcho();
  bool send_cmdGetScancodeSet(uint8_t * result);
  bool send_cmdSetScancodeSet(uint8_t scancodeSet);
  bool send_cmdIdentify(PS2Device * result);
  bool send_cmdDisableScanning();
  bool send_cmdEnableScanning();
  bool send_cmdTypematicRateAndDelay(int repeatRateMS, int repeatDelayMS);
  bool send_cmdSetDefaultParams();
  bool send_cmdReset();


  bool                      m_keyboardAvailable;  // self test passed and support for scancode set 2

  // these are valid after a call to generateVirtualKeys(true)
  TaskHandle_t              m_SCodeToVKConverterTask; // Task that converts scancodes to virtual key and populates m_virtualKeyQueue
  QueueHandle_t             m_virtualKeyQueue;

  uint8_t                   m_VKMap[(int)(VK_LAST + 7) / 8];

  KeyboardLayout const *    m_layout;

  bool                      m_CTRL;
  bool                      m_ALT;
  bool                      m_SHIFT;
  bool                      m_CAPSLOCK;
  bool                      m_NUMLOCK;
  bool                      m_SCROLLLOCK;

  // store status of the three LEDs
  bool                      m_numLockLED;
  bool                      m_capsLockLED;
  bool                      m_scrollLockLED;
};








} // end of namespace



extern fabgl::KeyboardClass Keyboard;



#endif


