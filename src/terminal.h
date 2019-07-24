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
 * @brief This file contains fabgl::TerminalClass definition.
 */



#include "Arduino.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "fabglconf.h"
#include "canvas.h"
#include "keyboard.h"
#include "terminfo.h"

#include "Stream.h"



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


namespace fabgl {




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
  // hence must be VGAController.enableBackgroundPrimitiveExecution(true),
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
};



/**
 * @brief An ANSI-VT100 compatible display terminal.
 *
 * Implements most of common ANSI, VT52, VT100, VT200, VT300, VT420 and VT500 escape codes, like non-CSI codes (RIS, IND, DECID, DECDHL, etc..),<br>
 * like CSI codes (private modes, CUP, TBC, etc..), like CSI-SGR codes (bold, italic, blinking, etc...) and like DCS codes (DECRQSS, etc..).<br>
 * Supports convertion from PS/2 keyboard virtual keys to ANSI or VT codes (keypad, cursor keys, function keys, etc..).<br>
 *
 * TerminalClass can receive codes to display from Serial Port or it can be controlled directly from the application. In the same way
 * TerminalClass can send keyboard codes to a Serial Port or directly to the application.<br>
 *
 * For default it supports 80x25 or 132x25 characters at 640x350. However any custom resolution and text buffer size is supported
 * specifying a custom font.<br>
 *
 * There are three cursors styles (block, underlined and bar), blinking or not blinking.
 *
 * TerminalClass inherits from Stream so applications can use all Stream and Print input and output methods.
 *
 * TerminalClass passes 95/110 of VTTEST VT100/VT102 Compatibility Test Score Sheet.
 *
 *
 *
 * Example 1:
 *
 *     TerminalClass Terminal;
 *
 *     // Setup 80x25 columns loop-back terminal (send what you type on keyboard to the display)
 *     void setup() {
 *       Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);  // GPIOs for keyboard (CLK, DATA)
 *
 *       // GPIOs for VGA (RED0, RED1, GREEN0, GREEN1, BLUE0, BLUE1, HSYNC, VSYNC)
 *       VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
 *       VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350); // 640x350, 80x25 columns
 *
 *       Terminal.begin();
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
 *     TerminalClass Terminal;
 *
 *     // Setup 80x25 columns terminal using UART2 to communicate with the server,
 *     // VGA to display output and PS2 device as keyboard input
 *     void setup() {
 *       Serial2.begin(115200);
 *
 *       Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32); // GPIOs for keyboard (CLK, DATA)
 *
 *       // GPIOs for VGA (RED0, RED1, GREEN0, GREEN1, BLUE0, BLUE1, HSYNC, VSYNC)
 *       VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
 *       VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350); // 640x350, 80x25 columns
 *
 *       Terminal.begin();
 *       Terminal.connectSerialPort(Serial2);
 *       Terminal.enableCursor(true);
 *     }
 *
 *     void loop() {
 *       Terminal.pollSerialPort();
 *     }
 */
class TerminalClass : public Stream {

public:

  /**
   * @brief Initializes the terminal.
   *
   * Applications should call this method before any other method call or after resolution has been set.
   */
  void begin();

  /**
   * @brief Finalizes the terminal.
   *
   * Applications should call this method before screen resolution changes.
   */
  void end();

  /**
   * @brief Connects a remove host using the specified serial port.
   *
   * When serial port is set, the typed keys on PS/2 keyboard are encoded
   * as ANSI/VT100 codes and then sent to the specified serial port.<br>
   * Also replies to terminal queries like terminal identification, cursor position, etc.. will be
   * sent to the serial port.<br>
   * Call TerminalClass.pollSerialPort() to send codes from serial port to the display.
   *
   * @param serialPort The serial port to use.
   * @param autoXONXOFF If true uses software flow control (XON/XOFF).
   *
   * Example:
   *
   *       Terminal.begin();
   *       Terminal.connectSerialPort(Serial);
   */
  void connectSerialPort(HardwareSerial & serialPort, bool autoXONXOFF = true);

  /**
   * @brief Pools the serial port for incoming data.
   *
   * Tnis method needs to be called in the application main loop to check if new data
   * is coming from the current serial port (specified using TerminalClass.connectSerialPort).
   *
   * Example:
   *
   *       void loop()
   *       {
   *         Terminal.pollSerialPort();
   *       }
   */
  void pollSerialPort();

  /**
   * @brief Permits using of terminal locally.
   *
   * Create a queue where to put ANSI keys decoded from keyboard or as replies to terminal queries.<br>
   * This queue is accessible with read(), available() and peek() methods.
   *
   * Example:
   *
   *        Terminal.begin();
   *        Terminal.connectLocally();
   *        // from here you can use Terminal.read() to receive keys from keyboard
   *        // and Terminal.write() to control the display.
   */
  void connectLocally();

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
   * @brief Sets the stream where to output debugging logs.
   *
   * Logging info sents to the logging stream are detailed by FABGLIB_TERMINAL_DEBUG_REPORT_.... macros in fabglconf.h configuration file.
   *
   * @param stream The logging stream.
   *
   * Example:
   *
   *        Serial.begin(115200);
   *        Terminal.begin();
   *        Terminal.setLogStream(Serial);
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
   * Particular cases require setting custom fonts, so applications can use TerminalClass.loadFont().
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
   *        Terminal.setBackgroundColor(Color::Black);
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
   *        Terminal.setForegroundColor(Color::White);
   */
  void setForegroundColor(Color color, bool setAsDefault = true);

  /**
   * @brief Clears the screen.
   *
   * Clears the screen sending "CSI 2 J" command to the screen.
   *
   * Example:
   *
   *        // Fill the screen with blue
   *        Terminal.setBackgroundColor(Color::Blue);
   *        Terminal.clear();
   */
  void clear();

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
   * @brief Sets the terminal type to emulate specifying conversion tables
   *
   * @param value Conversione tables for the terminal to emulate. nullptr = native ANSI/VT terminal.
   *
   * Default and native is ANSI/VT100 mode. Other terminals are emulated translating to native mode.
   */
  void setTerminalType(TermInfo const * value);

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
   * Keyboard queue is available only after TerminalClass.connectLocally() call.
   *
   * @return The number of codes available to read.
   */
  int available();

  /**
   * @brief Reads codes from keyboard.
   *
   * Keyboard queue is available only after TerminalClass.connectLocally() call.
   *
   * @return The first code of incoming data available (or -1 if no data is available).
   */
  int read();

  /**
   * @brief Reads a code from the keyboard without advancing to the next one.
   *
   * Keyboard queue is available only after TerminalClass.connectLocally() call.
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
   *        // Clear the screen and print "Hello World!"
   *        Terminal.write("\e[2J", 4);
   *        Terminal.write("Hellow World!\r\n", 15);
   *
   *        // The same without size specified
   *        Terminal.write("\e[2J");
   *        Terminal.write("Hellow World!\r\n");
   */
  int write(const uint8_t * buffer, int size);

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
  void setCursorPos(int X, int Y);
  int getAbsoluteRow(int Y);

  void int_setBackgroundColor(Color color);
  void int_setForegroundColor(Color color);

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

  void erase(int X1, int Y1, int X2, int Y2, char c, bool maintainDoubleWidth, bool selective);

  void consumeInputQueue();
  void consumeESC();
  void consumeCSI();
  void consumeCSIQUOT(int * params, int paramsCount);
  void consumeCSISPC(int * params, int paramsCount);
  char consumeParamsAndGetCode(int * params, int * paramsCount, bool * questionMarkFound);
  void consumeDECPrivateModes(int const * params, int paramsCount, char c);
  void consumeDCS();
  void execSGRParameters(int const * params, int paramsCount);
  void consumeESCVT52();

  void execCtrlCode(char c);

  static void charsConsumerTask(void * pvParameters);
  static void keyboardReaderTask(void * pvParameters);

  static void blinkTimerFunc(TimerHandle_t xTimer);
  void blinkText();
  bool enableBlinkingText(bool value);
  void blinkCursor();
  bool int_enableCursor(bool value);

  char getNextCode(bool processCtrlCodes);

  void setChar(char c);
  GlyphOptions getGlyphOptionsAt(int X, int Y);

  void insertAt(int column, int row, int count);
  void deleteAt(int column, int row, int count);

  void reverseVideo(bool value);

  void refresh();
  void refresh(int X, int Y);
  void refresh(int X1, int Y1, int X2, int Y2);
  void beginRefresh();
  void endRefresh();

  void setLineDoubleWidth(int row, int value);
  int getCharWidthAt(int row);
  int getColumnsAt(int row);

  void useAlternateScreenBuffer(bool value);

  void send(char c);
  void send(char const * str);
  void sendCSI();
  void sendDCS();
  void sendSS3();
  void sendCursorKeyCode(char c);
  void sendKeypadCursorKeyCode(char applicationCode, const char * numericCode);

  void ANSIDecodeVirtualKey(VirtualKey vk);
  void VT52DecodeVirtualKey(VirtualKey vk);

  void convHandleTranslation(uint8_t c);
  void convSendCtrl(ConvCtrl ctrl);
  void convQueue(const char * str = nullptr);
  void TermDecodeVirtualKey(VirtualKey vk);


  Stream *           m_logStream;

  // characters, characters attributes and characters colors container
  // you may also call this the "text screen buffer"
  GlyphsBuffer       m_glyphsBuffer;

  // used to implement alternate screen buffer
  uint32_t *         m_alternateMap;

  // true when m_alternateMap and m_glyphBuffer.map has been swapped
  bool               m_alternateScreenBuffer;

  // just to restore cursor X and Y pos when swapping screens (alternate screen)
  int                m_alternateCursorX;
  int                m_alternateCursorY;

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
  TimerHandle_t              m_blinkTimer;
  volatile SemaphoreHandle_t m_blinkTimerMutex;

  volatile bool      m_blinkingTextVisible;    // true = blinking text is currently visible
  volatile bool      m_blinkingTextEnabled;

  volatile int       m_columns;
  volatile int       m_rows;

  // optional serial port
  // data from serial port is processed and displayed
  // keys from keyboard are processed and sent to serial port
  HardwareSerial *   m_serialPort;

  // contains characters to be processed (from write() calls)
  QueueHandle_t      m_inputQueue;

  // contains characters received and decoded from keyboard (or as replyes from ANSI-VT queries)
  QueueHandle_t      m_outputQueue;

  // linked list that contains saved cursor states (first item is the last added)
  TerminalCursorState * m_savedCursorStateList;

  // a reset has been requested
  bool               m_resetRequested;

  bool               m_autoXONOFF;
  bool               m_XOFF;       // true = XOFF sent

  // used to implement m_emuState.keyAutorepeat
  VirtualKey         m_lastPressedKey;

  uint8_t                   m_convMatchedCount;
  char                      m_convMatchedChars[EmuTerminalMaxChars];
  TermInfoVideoConv const * m_convMatchedItem;
  TermInfo const *          m_termInfo;

};


} // end of namespace

