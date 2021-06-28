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


#include <stdarg.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_attr.h"
#if __has_include("esp32/rom/uart.h")
  #include "esp32/rom/uart.h"
#else
  #include "rom/uart.h"
#endif
#include "soc/uart_reg.h"
#include "soc/uart_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"
#include "soc/rtc.h"
#include "esp_intr_alloc.h"


#include "fabutils.h"
#include "terminal.h"
#include "devdrivers/mouse.h"



#pragma GCC optimize ("O2")


namespace fabgl {


// Terminal identification ID
// 64 = VT420
// 1 = support for 132 columns
// 6 = selective erase
// 22 = color
const char TERMID[] = "?64;1;6;22c";

// to send 8 bit (S8C1T) or 7 bit control characters
const char CSI_7BIT[] = "\e[";
const char CSI_8BIT[] = "\x9B";
const char DCS_7BIT[] = "\eP";
const char DCS_8BIT[] = "\x90";
const char SS2_7BIT[] = "\eN";
const char SS2_8BIT[] = "\x8E";
const char SS3_7BIT[] = "\eO";
const char SS3_8BIT[] = "\x8F";
const char ST_7BIT[]  = "\e\\";
const char ST_8BIT[]  = "\x9C";
const char OSC_7BIT[] = "\e]";
const char OSC_8BIT[] = "\x9D";



#define ISCTRLCHAR(c) ((c) <= ASCII_US || (c) == ASCII_DEL)


// Map "DEC Special Graphics Character Set" to CP437
static const uint8_t DECGRAPH_TO_CP437[255] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                                               50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
                                               74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
                                               /* 95 */ 32, /* 96 */ 4, /* 97 */ 177, /* 98 (not implemented) */ 63, /* 99 (not implemented) */ 63,
                                               /* 100 (not implemented) */ 63, /* 101 (not implemented) */ 63, /* 102 */ 248, /* 103 */ 241,
                                               /* 104 (not implemented) */ 63, /* 105 (not implemented) */ 63, /* 106 */ 217, /* 107 */ 191, /* 108 */ 218,
                                               /* 109 */ 192, /* 110 */ 197, /* 111 (not implemented) */ 63, /* 112 (not implemented) */ 63, /* 113 */ 196,
                                               /* 114 (not implemented) */ 63, /* 115 (not implemented) */ 63, /* 116 */ 195, /* 117 */ 180, /* 118 */ 193,
                                               /* 119 */ 194, /* 120 */ 179, /* 121 */ 243, /* 122 */ 242, /* 123 */ 227, /* 124 (not implemented) */ 63,
                                               /* 125 */ 156, /* 126 */ 249};


const char * CTRLCHAR_TO_STR[] = {"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BELL", "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI", "DLE", "XON", "DC2",
                                  "XOFF", "DC4", "NAK", "SYN", "ETB", "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US", "SPC"};


// '_' (APC, ESC+0x5f is DEC-ANSI code for APC - Application Program Command)
// it should terminate with "ST", but we don't!
#define FABGLEXT_STARTCODE          '_'
#define FABGLEXT_CMD                "\e_"
#define FABGLEXT_ENDCODE            '$'
#define FABGLEXT_REPLYCODE          '$'


// sub commands of FABGLEXT_STARTCODE (user command):
#define FABGLEXT_USERSEQ        '#'
// sub commands of FABGLEXT_STARTCODE (binary encoded parameters):
#define FABGLEXTB_GETCURSORPOS   'a'
#define FABGLEXTB_GETCURSORCOL   'b'
#define FABGLEXTB_GETCURSORROW   'c'
#define FABGLEXTB_SETCURSORPOS   'd'
#define FABGLEXTB_INSERTSPACE    'e'
#define FABGLEXTB_DELETECHAR     'f'
#define FABGLEXTB_CURSORLEFT     'g'
#define FABGLEXTB_CURSORRIGHT    'h'
#define FABGLEXTB_SETCHAR        'i'
#define FABGLEXTB_DISABLEFABSEQ  'j'
#define FABGLEXTB_SETTERMTYPE    'k'
#define FABGLEXTB_ISVKDOWN       'K'
#define FABGLEXTB_SETFGCOLOR     'l'
#define FABGLEXTB_SETBGCOLOR     'm'
#define FABGLEXTB_SETCHARSTYLE   'n'
// sub commands of FABGLEXT_STARTCODE (text encoded parameters):
#define FABGLEXTX_SETUPADC       'A'
#define FABGLEXTX_CLEAR          'B'
#define FABGLEXTX_READADC        'C'
#define FABGLEXTX_SETUPGPIO      'D'
#define FABGLEXTX_ENABLECURSOR   'E'
#define FABGLEXTX_SETCURSORPOS   'F'
#define FABGLEXTX_GRAPHICSCMD    'G'
#define FABGLEXTX_SHOWMOUSE      'H'
#define FABGLEXTX_GETMOUSEPOS    'M'
#define FABGLEXTX_GETGPIO        'R'
#define FABGLEXTX_SOUND          'S'
#define FABGLEXTX_SETGPIO        'W'
#define FABGLEXTX_DELAY          'Y'

// maximum length of sub commands (includes ending zero)
#define FABGLEXT_MAXSUBCMDLEN   16

// sub commands of FABGLEXTX_GRAPHICSCMD
#define FABGLEXT_GCLEAR         "CLEAR"
#define FABGLEXT_GSETBRUSHCOLOR "BRUSH"
#define FABGLEXT_GSETPENCOLOR   "PEN"
#define FABGLEXT_GSETPIXEL      "PIXEL"
#define FABGLEXT_GSCROLL        "SCROLL"
#define FABGLEXT_GPENWIDTH      "PENW"
#define FABGLEXT_GLINE          "LINE"
#define FABGLEXT_GRECT          "RECT"
#define FABGLEXT_GFILLRECT      "FILLRECT"
#define FABGLEXT_GELLIPSE       "ELLIPSE"
#define FABGLEXT_GFILLELLIPSE   "FILLELLIPSE"
#define FABGLEXT_GPATH          "PATH"
#define FABGLEXT_GFILLPATH      "FILLPATH"
#define FABGLEXT_GSPRITECOUNT   "SPRITECOUNT"
#define FABGLEXT_GSPRITEDEF     "SPRITEDEF"
#define FABGLEXT_GSPRITESET     "SPRITESET"


// specifies color attribute index for m_coloredAttributesColor[]
#define COLORED_ATTRIBUTE_BOLD      0
#define COLORED_ATTRIBUTE_FAINT     1
#define COLORED_ATTRIBUTE_ITALIC    2
#define COLORED_ATTRIBUTE_UNDERLINE 3
#define COLORED_ATTRIBUTE_INVALID   4




Terminal * Terminal::s_activeTerminal = nullptr;


int Terminal::inputQueueSize = FABGLIB_DEFAULT_TERMINAL_INPUT_QUEUE_SIZE;

int Terminal::inputConsumerTaskStackSize = FABGLIB_DEFAULT_TERMINAL_INPUT_CONSUMER_TASK_STACK_SIZE;

int Terminal::keyboardReaderTaskStackSize = FABGLIB_DEFAULT_TERMINAL_KEYBOARD_READER_TASK_STACK_SIZE;



Terminal::Terminal()
  : m_canvas(nullptr),
    m_mutex(nullptr),
    m_uartRXEnabled(true),
    m_soundGenerator(nullptr),
    m_sprites(nullptr),
    m_spritesCount(0)
{
  if (s_activeTerminal == nullptr)
    s_activeTerminal = this;
}


Terminal::~Terminal()
{
  // end() called?
  if (m_mutex)
    end();

  if (m_soundGenerator)
    delete m_soundGenerator;

  freeSprites();
}


void Terminal::activate(TerminalTransition transition)
{
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  if (s_activeTerminal != this) {

    if (s_activeTerminal && transition != TerminalTransition::None) {
      if (m_bitmappedDisplayController) {
        // bitmapped controller, use Canvas to perform the animation
        s_activeTerminal = nullptr;
        switch (transition) {
          case TerminalTransition::LeftToRight:
            for (int x = 0; x < m_columns; ++x) {
              m_canvas->scroll(m_font.width, 0);
              m_canvas->setOrigin(-m_font.width * (m_columns - x - 1), 0);
              for (int y = 0; y < m_rows; ++y)
                m_canvas->renderGlyphsBuffer(m_columns - x - 1, y, &m_glyphsBuffer);
              m_canvas->waitCompletion(false);
              vTaskDelay(2 / portTICK_PERIOD_MS);
            }
            break;
          case TerminalTransition::RightToLeft:
            for (int x = 0; x < m_columns; ++x) {
              m_canvas->scroll(-m_font.width, 0);
              m_canvas->setOrigin(m_font.width * (m_columns - x - 1), 0);
              for (int y = 0; y < m_rows; ++y)
                m_canvas->renderGlyphsBuffer(x, y, &m_glyphsBuffer);
              m_canvas->waitCompletion(false);
              vTaskDelay(2 / portTICK_PERIOD_MS);
            }
            break;
          default:
            break;
        }
      } else {
        // textual controller, use temporary buffer to perform animation
        auto txtCtrl = static_cast<TextualDisplayController*>(m_displayController);
        auto map = (uint32_t*) heap_caps_malloc(sizeof(uint32_t) * m_columns * m_rows, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        memcpy(map, s_activeTerminal->m_glyphsBuffer.map, sizeof(uint32_t) * m_columns * m_rows);
        txtCtrl->enableCursor(false);
        txtCtrl->setTextMap(map, m_rows);
        switch (transition) {
          case TerminalTransition::LeftToRight:
            for (int x = 0; x < m_columns; ++x) {
              for (int y = 0; y < m_rows; ++y) {
                memmove(map + y * m_columns + 1, map + y * m_columns, sizeof(uint32_t) * (m_columns - 1));
                map[y * m_columns] = m_glyphsBuffer.map[y * m_columns + m_columns - x - 1];
              }
              vTaskDelay(5);
            }
            break;
          case TerminalTransition::RightToLeft:
            for (int x = 0; x < m_columns; ++x) {
              for (int y = 0; y < m_rows; ++y) {
                memmove(map + y * m_columns, map + y * m_columns + 1, sizeof(uint32_t) * (m_columns - 1));
                map[y * m_columns + m_columns - 1] = m_glyphsBuffer.map[y * m_columns + x];
              }
              vTaskDelay(5);
            }
            break;
          default:
            break;
        }
        txtCtrl->setTextMap(m_glyphsBuffer.map, m_rows);
        free(map);
      }
    }

    s_activeTerminal = this;
    vTaskResume(m_keyboardReaderTaskHandle);
    syncDisplayController();
  }
  xSemaphoreGive(m_mutex);
}


void Terminal::deactivate()
{
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  if (s_activeTerminal == this) {
    s_activeTerminal = nullptr;
  }
  xSemaphoreGive(m_mutex);
}


// synchronize display controller (and canvas) state
void Terminal::syncDisplayController()
{
  if (m_bitmappedDisplayController) {
    // restore canvas state for bitmapped display controller
    m_canvas->reset();
    m_canvas->setGlyphOptions(m_glyphOptions);
    m_canvas->setBrushColor(m_emuState.backgroundColor);
    m_canvas->setPenColor(m_emuState.foregroundColor);
  } else {
    // restore textual display controller state
    auto txtCtrl = static_cast<TextualDisplayController*>(m_displayController);
    txtCtrl->setTextMap(m_glyphsBuffer.map, m_rows);
    txtCtrl->setCursorBackground(m_emuState.backgroundColor);
    txtCtrl->setCursorForeground(m_emuState.foregroundColor);
    txtCtrl->setCursorPos(m_emuState.cursorY - 1, m_emuState.cursorX - 1);
    txtCtrl->enableCursor(m_emuState.cursorEnabled);
  }
  updateCanvasScrollingRegion();
  refresh();
}


bool Terminal::begin(BaseDisplayController * displayController, int maxColumns, int maxRows, Keyboard * keyboard)
{
  m_displayController = displayController;
  m_bitmappedDisplayController = (m_displayController->controllerType() == DisplayControllerType::Bitmapped);

  m_maxColumns = maxColumns;
  m_maxRows    = maxRows;

  if (m_bitmappedDisplayController) {
    m_canvas = new Canvas(static_cast<BitmappedDisplayController*>(m_displayController));
  } else {
    m_canvas = nullptr;
    static_cast<TextualDisplayController*>(m_displayController)->adjustMapSize(&m_maxColumns, &m_maxRows);
  }

  m_keyboard = keyboard;
  if (m_keyboard == nullptr && PS2Controller::initialized()) {
    // get default keyboard from PS/2 controller
    m_keyboard = PS2Controller::keyboard();
  }

  m_logStream = nullptr;

  m_glyphsBuffer = (GlyphsBuffer){0, 0, nullptr, 0, 0, nullptr};

  m_emuState.tabStop = nullptr;
  m_font.data = nullptr;

  m_savedCursorStateList = nullptr;

  m_alternateScreenBuffer = false;
  m_alternateMap = nullptr;

  m_autoXONOFF = false;
  m_XOFF = false;

  m_lastWrittenChar = 0;

  m_writeDetectedFabGLSeq = false;

  // conformance level
  m_emuState.conformanceLevel = 4; // VT400
  m_emuState.ctrlBits = 7;

  // cursor setup
  m_cursorState = false;
  m_emuState.cursorEnabled = false;

  // colored attributes
  m_coloredAttributesMaintainStyle = true;
  m_coloredAttributesMask = 0;

  m_mutex = xSemaphoreCreateMutex();

  set132ColumnMode(false);

  // blink support
  m_blinkTimer = xTimerCreate("", pdMS_TO_TICKS(FABGLIB_DEFAULT_BLINK_PERIOD_MS), pdTRUE, this, blinkTimerFunc);
  xTimerStart(m_blinkTimer, portMAX_DELAY);

  // queue and task to consume input characters
  m_inputQueue = xQueueCreate(Terminal::inputQueueSize, sizeof(uint8_t));
  xTaskCreate(&charsConsumerTask, "", Terminal::inputConsumerTaskStackSize, this, FABGLIB_CHARS_CONSUMER_TASK_PRIORITY, &m_charsConsumerTaskHandle);

  m_defaultBackgroundColor = Color::Black;
  m_defaultForegroundColor = Color::White;

  #ifdef ARDUINO
  m_serialPort = nullptr;
  #endif

  m_keyboardReaderTaskHandle = nullptr;
  m_uart = false;

  m_outputQueue = nullptr;

  m_termInfo = nullptr;

  bool success = (m_glyphsBuffer.map != nullptr);

  if (success)
    reset();

  return success;
}


void Terminal::end()
{
  if (m_keyboardReaderTaskHandle)
    vTaskDelete(m_keyboardReaderTaskHandle);

  xTimerDelete(m_blinkTimer, portMAX_DELAY);

  clearSavedCursorStates();

  vTaskDelete(m_charsConsumerTaskHandle);
  vQueueDelete(m_inputQueue);

  if (m_outputQueue)
    vQueueDelete(m_outputQueue);

  freeFont();
  freeTabStops();
  freeGlyphsMap();

  vSemaphoreDelete(m_mutex);
  m_mutex = nullptr;

  delete m_canvas;

  if (isActive())
    s_activeTerminal = nullptr;
}


#ifdef ARDUINO
void Terminal::connectSerialPort(HardwareSerial & serialPort, bool autoXONXOFF)
{
  if (m_serialPort)
    vTaskDelete(m_keyboardReaderTaskHandle);
  m_serialPort = &serialPort;
  m_autoXONOFF = autoXONXOFF;

  m_serialPort->setRxBufferSize(Terminal::inputQueueSize);

  if (!m_keyboardReaderTaskHandle && m_keyboard->isKeyboardAvailable())
    xTaskCreate(&keyboardReaderTask, "", Terminal::keyboardReaderTaskStackSize, this, FABGLIB_KEYBOARD_READER_TASK_PRIORITY, &m_keyboardReaderTaskHandle);

  // just in case a reset occurred after an XOFF
  if (m_autoXONOFF)
    send(ASCII_XON);
}
#endif


// returns number of bytes received (in the UART2 rx fifo buffer)
inline int uartGetRXFIFOCount()
{
  uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
  return uart->status.rxfifo_cnt | ((int)(uart->mem_cnt_status.rx_cnt) << 8);
}


// flushes TX buffer of UART2
static void uartFlushTXFIFO()
{
  uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
  while (uart->status.txfifo_cnt || uart->status.st_utx_out)
    ;
}


// flushes RX buffer of UART2
static void uartFlushRXFIFO()
{
  uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
  while (uartGetRXFIFOCount() != 0 || uart->mem_rx_status.wr_addr != uart->mem_rx_status.rd_addr)
    uart->fifo.rw_byte;
}


// look into input queue (m_inputQueue): if there is space for new incoming bytes send XON and reenable uart RX interrupts
void Terminal::uartCheckInputQueueForFlowControl()
{
  if (m_autoXONOFF) {
    uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
    if (uxQueueMessagesWaiting(m_inputQueue) == 0 && uart->int_ena.rxfifo_full == 0) {
      if (m_XOFF) {
        m_XOFF = false;
        uart->flow_conf.send_xon = 1; // send XON
      }
      uart->int_ena.rxfifo_full = 1;
    }
  }
}


// connect to UART2
void Terminal::connectSerialPort(uint32_t baud, uint32_t config, int rxPin, int txPin, FlowControl flowControl, bool inverted)
{
  uart_dev_t * uart = (volatile uart_dev_t *) DR_REG_UART2_BASE;

  bool initialSetup = !m_uart;

  if (initialSetup) {
    // uart not configured, configure now

    #ifdef ARDUINO
    Serial2.end();
    #endif

    m_uart = true;

    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);

    // flush
    uartFlushTXFIFO();
    uartFlushRXFIFO();

    // TX/RX Pin direction
    configureGPIO(int2gpio(rxPin), GPIO_MODE_INPUT);
    configureGPIO(int2gpio(txPin), GPIO_MODE_OUTPUT);

    // RX interrupt
    uart->conf1.rxfifo_full_thrhd = 1;  // an interrupt for each character received
    uart->conf1.rx_tout_thrhd = 2;      // actually not used
    uart->conf1.rx_tout_en    = 0;      // timeout not enabled
    uart->int_ena.rxfifo_full = 1;      // interrupt on FIFO full (1 character - see rxfifo_full_thrhd)
    uart->int_ena.frm_err     = 1;      // interrupt on frame error
    uart->int_ena.rxfifo_tout = 0;      // no interrupt on rx timeout (see rx_tout_en and rx_tout_thrhd)
    uart->int_ena.parity_err  = 1;      // interrupt on rx parity error
    uart->int_ena.rxfifo_ovf  = 1;      // interrupt on rx overflow
    uart->int_clr.val = 0xffffffff;
    esp_intr_alloc(ETS_UART2_INTR_SOURCE, 0, uart_isr, this, nullptr);

    // setup FIFOs size
    uart->mem_conf.rx_size = 3;  // RX: 384 bytes (this is the max for UART2)
    uart->mem_conf.tx_size = 1;  // TX: 128 bytes

    if (!m_keyboardReaderTaskHandle && m_keyboard->isKeyboardAvailable())
      xTaskCreate(&keyboardReaderTask, "", Terminal::keyboardReaderTaskStackSize, this, FABGLIB_KEYBOARD_READER_TASK_PRIORITY, &m_keyboardReaderTaskHandle);
  }

  m_autoXONOFF = (flowControl == FlowControl::Software);

  // set baud rate
  uint32_t clk_div = (getApbFrequency() << 4) / baud;
  uart->clk_div.div_int  = clk_div >> 4;
  uart->clk_div.div_frag = clk_div & 0xf;

  // frame
  uart->conf0.val = config;
  if (uart->conf0.stop_bit_num == 0x3) {
    uart->conf0.stop_bit_num = 1;
    uart->rs485_conf.dl1_en  = 1;
  }

  // TX/RX Pin logic
  gpio_matrix_in(rxPin, U2RXD_IN_IDX, inverted);
  gpio_matrix_out(txPin, U2TXD_OUT_IDX, inverted, false);

  // Flow Control
  uart->flow_conf.sw_flow_con_en = 0;
  uart->flow_conf.xonoff_del     = 0;
  if (flowControl == FlowControl::Software) {
    // we actually use manual software control, using send_xon/send_xoff bits to send control characters
    // because we have to check both RX-FIFO and input queue
    uart->swfc_conf.xon_threshold  = 0;
    uart->swfc_conf.xoff_threshold = 0;
    uart->swfc_conf.xon_char  = ASCII_XON;
    uart->swfc_conf.xoff_char = ASCII_XOFF;
    if (initialSetup) {
      // send an XON right now
      m_XOFF = true;
      uart->flow_conf.send_xon = 1;
    }
  }

  // APB Change callback (TODO?)
  //addApbChangeCallback(this, uart_on_apb_change);
}


void Terminal::connectLocally()
{
  m_outputQueue = xQueueCreate(FABGLIB_TERMINAL_OUTPUT_QUEUE_SIZE, sizeof(uint8_t));
  if (!m_keyboardReaderTaskHandle && m_keyboard->isKeyboardAvailable())
    xTaskCreate(&keyboardReaderTask, "", Terminal::keyboardReaderTaskStackSize, this, FABGLIB_KEYBOARD_READER_TASK_PRIORITY, &m_keyboardReaderTaskHandle);
}


void Terminal::disconnectLocally()
{
  if (m_outputQueue)
    vQueueDelete(m_outputQueue);
  m_outputQueue = nullptr;
}


void Terminal::logFmt(const char * format, ...)
{
  if (m_logStream) {
    va_list ap;
    va_start(ap, format);
    int size = vsnprintf(nullptr, 0, format, ap) + 1;
    if (size > 0) {
      va_end(ap);
      va_start(ap, format);
      char buf[size + 1];
      vsnprintf(buf, size, format, ap);
      m_logStream->write(buf);
    }
    va_end(ap);
  }
}


void Terminal::log(const char * txt)
{
  if (m_logStream)
    m_logStream->write(txt);
}


void Terminal::log(char c)
{
  if (m_logStream)
    m_logStream->write(c);
}


void Terminal::freeFont()
{
  #if FABGLIB_CACHE_FONT_IN_RAM
  if (m_font.data) {
    free((void*) m_font.data);
    m_font.data = nullptr;
  }
  #endif
}


void Terminal::freeTabStops()
{
  if (m_emuState.tabStop) {
    free(m_emuState.tabStop);
    m_emuState.tabStop = nullptr;
  }
}


void Terminal::freeGlyphsMap()
{
  if (m_glyphsBuffer.map) {
    free((void*) m_glyphsBuffer.map);
    m_glyphsBuffer.map = nullptr;
  }
  if (m_alternateMap) {
    free((void*) m_alternateMap);
    m_alternateMap = nullptr;
  }
}


void Terminal::reset()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("reset()\n");
  #endif

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_resetRequested = false;

  m_emuState.originMode            = false;
  m_emuState.wraparound            = true;
  m_emuState.insertMode            = false;
  m_emuState.newLineMode           = false;
  m_emuState.smoothScroll          = false;
  m_emuState.keypadMode            = KeypadMode::Numeric;
  m_emuState.cursorKeysMode        = false;
  m_emuState.keyAutorepeat         = true;
  m_emuState.cursorBlinkingEnabled = true;
  m_emuState.cursorStyle           = 0;
  m_emuState.allow132ColumnMode    = false;
  m_emuState.reverseWraparoundMode = false;
  m_emuState.backarrowKeyMode      = false;
  m_emuState.ANSIMode              = true;
  m_emuState.VT52GraphicsMode      = false;
  m_emuState.allowFabGLSequences   = 1;  // enabled for default
  m_emuState.characterSetIndex     = 0;  // Select G0
  for (int i = 0; i < 4; ++i)
    m_emuState.characterSet[i] = 1;     // G0, G1, G2 and G3 = USASCII

  m_lastPressedKey = VK_NONE;

  m_blinkingTextVisible = false;
  m_blinkingTextEnabled = true;

  m_cursorState = false;

  m_convMatchedCount = 0;
  m_convMatchedItem  = nullptr;

  // this also restore cursor at top-left
  setScrollingRegion(1, m_rows);

  resetTabStops();

  m_glyphOptions = (GlyphOptions) {{
    .fillBackground   = 1,
    .bold             = 0,
    .reduceLuminosity = 0,
    .italic           = 0,
    .invert           = 0,
    .blank            = 0,
    .underline        = 0,
    .doubleWidth      = 0,
    .userOpt1         = 0,    // blinking
    .userOpt2         = 0,    // 0 = erasable by DECSED or DECSEL,  1 = not erasable by DECSED or DECSEL
  }};
  if (m_canvas)
    m_canvas->setGlyphOptions(m_glyphOptions);

  m_paintOptions = PaintOptions();

  reverseVideo(false);

  int_setBackgroundColor(m_defaultBackgroundColor);
  int_setForegroundColor(m_defaultForegroundColor);

  clearSavedCursorStates();

  int_clear();

  xSemaphoreGive(m_mutex);
}


void Terminal::loadFont(FontInfo const * font)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("loadFont()\n");
  #endif

  int codepage = 0;

  if (m_bitmappedDisplayController) {

    // bitmapped display controller

    freeFont();

    m_font = *font;
  #if FABGLIB_CACHE_FONT_IN_RAM
    int size = m_font.height * 256 * ((m_font.width + 7) / 8);
    m_font.data = (uint8_t const*) malloc(size);
    memcpy((void*)m_font.data, font->data, size);
  #else
    m_font.data = font->data;
  #endif

    m_columns = m_canvas->getWidth() / m_font.width;
    m_rows    = m_canvas->getHeight() / m_font.height;

    m_glyphsBuffer.glyphsWidth  = m_font.width;
    m_glyphsBuffer.glyphsHeight = m_font.height;
    m_glyphsBuffer.glyphsData   = m_font.data;

    codepage = m_font.codepage;

  } else {

    // textual display controller

    auto txtctrl = static_cast<TextualDisplayController*>(m_displayController);

    m_columns = txtctrl->getColumns();
    m_rows    = txtctrl->getRows();
    codepage  = txtctrl->getFont()->codepage;

  }

  if (m_keyboard) {
    m_keyboard->setCodePage(CodePages::get(codepage));
    //Serial.printf("set codepage %d\n", codepage);
  }

  // check maximum columns and rows
  if (m_maxColumns > 0 && m_maxColumns < m_columns)
    m_columns = m_maxColumns;
  if (m_maxRows > 0 && m_maxRows < m_rows)
    m_rows = m_maxRows;

  freeTabStops();
  m_emuState.tabStop = (uint8_t*) malloc(m_columns);
  resetTabStops();

  freeGlyphsMap();
  m_glyphsBuffer.columns      = m_columns;
  m_glyphsBuffer.rows         = m_rows;
  while (true) {
    m_glyphsBuffer.map = (uint32_t*) heap_caps_malloc(sizeof(uint32_t) * m_columns * m_rows, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (m_glyphsBuffer.map)
      break;
    // no enough memory, reduce m_rows
    --m_rows;
  }

  m_alternateMap = nullptr;
  m_alternateScreenBuffer = false;

  if (!m_bitmappedDisplayController && isActive()) {
    // associate map with textual display controller
    static_cast<TextualDisplayController*>(m_displayController)->setTextMap(m_glyphsBuffer.map, m_rows);
  }

  setScrollingRegion(1, m_rows);
  int_clear();
}


void Terminal::flush(bool waitVSync)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("flush()\n");
  #endif
  if (m_bitmappedDisplayController && isActive()) {
    while (uxQueueMessagesWaiting(m_inputQueue) > 0)
      ;
    m_canvas->waitCompletion(waitVSync);
  }
}


// false = setup for 80 columns mode
// true = setup for 132 columns mode
void Terminal::set132ColumnMode(bool value)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("set132ColumnMode()\n");
  #endif

  if (m_bitmappedDisplayController) {
    if (value) {
      // 132x25 is forced because ANSI compatibility
      m_maxColumns = 132;
      m_maxRows    = 25;
    }
    loadFont(getPresetFontInfo(m_canvas->getWidth(), m_canvas->getHeight(), (value ? 132 : 80), 25));
  } else
    loadFont(nullptr);
}


void Terminal::setBackgroundColor(Color color, bool setAsDefault)
{
  if (setAsDefault)
    m_defaultBackgroundColor = color;
  TerminalController(this).setBackgroundColor(color);
}


void Terminal::int_setBackgroundColor(Color color)
{
  m_emuState.backgroundColor = color;
  if (isActive()) {
    if (m_bitmappedDisplayController)
      m_canvas->setBrushColor(color);
    else
      static_cast<TextualDisplayController*>(m_displayController)->setCursorBackground(color);
  }
}


void Terminal::setForegroundColor(Color color, bool setAsDefault)
{
  if (setAsDefault)
    m_defaultForegroundColor = color;
  TerminalController(this).setForegroundColor(color);
}


void Terminal::int_setForegroundColor(Color color)
{
  m_emuState.foregroundColor = color;
  if (isActive()) {
    if (m_bitmappedDisplayController)
      m_canvas->setPenColor(color);
    else
      static_cast<TextualDisplayController*>(m_displayController)->setCursorForeground(color);
  }
}


int CharStyleToColorAttributeIndex(CharStyle attribute)
{
  switch (attribute) {
    case CharStyle::Bold:
      return COLORED_ATTRIBUTE_BOLD;
    case CharStyle::ReducedLuminosity:
      return COLORED_ATTRIBUTE_FAINT;
    case CharStyle::Italic:
      return COLORED_ATTRIBUTE_ITALIC;
    case CharStyle::Underline:
      return COLORED_ATTRIBUTE_UNDERLINE;
    default:
      return COLORED_ATTRIBUTE_INVALID;
  }
}


void Terminal::setColorForAttribute(CharStyle attribute, Color color, bool maintainStyle)
{
  int cindex = CharStyleToColorAttributeIndex(attribute);
  if (cindex != COLORED_ATTRIBUTE_INVALID) {
    m_coloredAttributesMask |= 1 << cindex;
    m_coloredAttributesColor[cindex] = color;
    m_coloredAttributesMaintainStyle = maintainStyle;
  }
}


void Terminal::setColorForAttribute(CharStyle attribute)
{
  int cindex = CharStyleToColorAttributeIndex(attribute);
  if (cindex != COLORED_ATTRIBUTE_INVALID)
    m_coloredAttributesMask &= ~(1 << cindex);
}


void Terminal::reverseVideo(bool value)
{
  if (m_paintOptions.swapFGBG != value) {
    m_paintOptions.swapFGBG = value;
    if (isActive()) {
      if (m_bitmappedDisplayController) {
        m_canvas->setPaintOptions(m_paintOptions);
        m_canvas->swapRectangle(0, 0, m_canvas->getWidth() - 1, m_canvas->getHeight() - 1);
      }
    }
  }
}


void Terminal::clear(bool moveCursor)
{
  TerminalController(this).clear();
  if (moveCursor)
    TerminalController(this).setCursorPos(1, 1);
}


void Terminal::int_clear()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("int_clear()\n");
  #endif

  if (m_bitmappedDisplayController && isActive())
    m_canvas->clear();
  clearMap(m_glyphsBuffer.map);
}


void Terminal::clearMap(uint32_t * map)
{
  uint32_t itemValue = GLYPHMAP_ITEM_MAKE(ASCII_SPC, m_emuState.backgroundColor, m_emuState.foregroundColor, m_glyphOptions);
  uint32_t * mapItemPtr = map;
  for (int row = 0; row < m_rows; ++row)
    for (int col = 0; col < m_columns; ++col, ++mapItemPtr)
      *mapItemPtr = itemValue;
}


// return True if scroll down required
bool Terminal::moveUp()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("moveUp()\n");
  #endif

  if (m_emuState.cursorY == m_emuState.scrollingRegionTop)
    return true;
  setCursorPos(m_emuState.cursorX, m_emuState.cursorY - 1);
  return false;
}


// return True if scroll up required
bool Terminal::moveDown()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("moveDown()\n");
  #endif

  if (m_emuState.cursorY == m_emuState.scrollingRegionDown || m_emuState.cursorY == m_rows)
    return true;
  setCursorPos(m_emuState.cursorX, m_emuState.cursorY + 1);
  return false;
}


// Move cursor at left or right, wrapping lines if necessary
void Terminal::move(int offset)
{
  int pos = m_emuState.cursorX - 1 + (m_emuState.cursorY - 1) * m_columns + offset;  // pos is zero based
  int newY = pos / m_columns + 1;
  int newX = pos % m_columns + 1;
  if (newY < m_emuState.scrollingRegionTop) {
    newX = 1;
    newY = m_emuState.scrollingRegionTop;
  }
  if (newY > m_emuState.scrollingRegionDown) {
    newX = m_columns;
    newY = m_emuState.scrollingRegionDown;
  }
  setCursorPos(newX, newY);
}


void Terminal::setCursorPos(int X, int Y)
{
  m_emuState.cursorX = tclamp(X, 1, (int)m_columns);
  m_emuState.cursorY = tclamp(Y, 1, (int)m_rows);
  m_emuState.cursorPastLastCol = false;

  if (!m_bitmappedDisplayController && isActive())
    static_cast<TextualDisplayController*>(m_displayController)->setCursorPos(m_emuState.cursorY - 1, m_emuState.cursorX - 1);

  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCSALL
  logFmt("setCursorPos(%d, %d) => set to (%d, %d)\n", X, Y, m_emuState.cursorX, m_emuState.cursorY);
  #endif
}


int Terminal::getAbsoluteRow(int Y)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("getAbsoluteRow(%d)\n", Y);
  #endif

  if (m_emuState.originMode) {
    Y += m_emuState.scrollingRegionTop - 1;
    Y = tclamp(Y, m_emuState.scrollingRegionTop, m_emuState.scrollingRegionDown);
  }
  return Y;
}


void Terminal::enableCursor(bool value)
{
  TerminalController(this).enableCursor(value);
}


bool Terminal::int_enableCursor(bool value)
{
  bool prev = m_emuState.cursorEnabled;
  if (m_emuState.cursorEnabled != value) {
    m_emuState.cursorEnabled = value;
    if (m_bitmappedDisplayController) {
      // bitmapped display, simulated cursor
      if (m_emuState.cursorEnabled) {
        if (uxQueueMessagesWaiting(m_inputQueue) == 0) {
          // just to show the cursor before next blink
          blinkCursor();
        }
      } else {
        if (m_cursorState) {
          // make sure cursor is hidden
          blinkCursor();
        }
      }
    } else if (isActive()) {
      // textual display, native cursor
      static_cast<TextualDisplayController*>(m_displayController)->enableCursor(value);
    }
  }
  return prev;
}


bool Terminal::enableBlinkingText(bool value)
{
  bool prev = m_blinkingTextEnabled;
  m_blinkingTextEnabled = value;
  return prev;
}


// callback for blink timer
void Terminal::blinkTimerFunc(TimerHandle_t xTimer)
{
  Terminal * term = (Terminal*) pvTimerGetTimerID(xTimer);

  if (term->isActive() && xSemaphoreTake(term->m_mutex, 0) == pdTRUE) {
    // cursor blink
    if (term->m_emuState.cursorEnabled && term->m_emuState.cursorBlinkingEnabled)
      term->blinkCursor();

    // text blink
    if (term->m_blinkingTextEnabled)
      term->blinkText();

    xSemaphoreGive(term->m_mutex);

  }
}


void Terminal::blinkCursor()
{
  if (m_bitmappedDisplayController && isActive()) {
    m_cursorState = !m_cursorState;
    int X = (m_emuState.cursorX - 1) * m_font.width;
    int Y = (m_emuState.cursorY - 1) * m_font.height;
    switch (m_emuState.cursorStyle) {
      case 0 ... 2:
        // block cursor
        m_canvas->swapRectangle(X, Y, X + m_font.width - 1, Y + m_font.height - 1);
        break;
      case 3 ... 4:
        // underline cursor
        m_canvas->swapRectangle(X, Y + m_font.height - 2, X + m_font.width - 1, Y + m_font.height - 1);
        break;
      case 5 ... 6:
        // bar cursor
        m_canvas->swapRectangle(X, Y, X + 1, Y + m_font.height - 1);
        break;
    }
  }
}


void Terminal::blinkText()
{
  if (isActive()) {
    m_blinkingTextVisible = !m_blinkingTextVisible;
    bool keepEnabled = false;
    int rows = m_rows;
    int cols = m_columns;
    if (m_bitmappedDisplayController)
      m_canvas->beginUpdate();
    for (int y = 0; y < rows; ++y) {
      uint32_t * itemPtr = m_glyphsBuffer.map + y * cols;
      for (int x = 0; x < cols; ++x, ++itemPtr) {
        // character to blink?
        GlyphOptions glyphOptions = glyphMapItem_getOptions(itemPtr);
        if (glyphOptions.userOpt1) {
          glyphOptions.blank = !m_blinkingTextVisible;
          glyphMapItem_setOptions(itemPtr, glyphOptions);
          refresh(x + 1, y + 1);
          keepEnabled = true;
        }
      }
      if (m_bitmappedDisplayController)
        m_canvas->waitCompletion(false);
    }
    if (m_bitmappedDisplayController)
      m_canvas->endUpdate();
    if (!keepEnabled)
      m_blinkingTextEnabled = false;
  }
}


void Terminal::nextTabStop()
{
  int actualColumns = m_columns;

  // if double width for current line then consider half columns
  if (getGlyphOptionsAt(1, m_emuState.cursorY).doubleWidth)
    actualColumns /= 2;

  int x = m_emuState.cursorX;
  while (x < actualColumns) {
    ++x;
    if (m_emuState.tabStop[x - 1])
      break;
  }
  setCursorPos(x, m_emuState.cursorY);
}


// setup tab stops. One every 8.
void Terminal::resetTabStops()
{
  for (int i = 0; i < m_columns; ++i)
    m_emuState.tabStop[i] = (i > 0 && (i % 8) == 0 ? 1 : 0);
}


// if column = 0 then clear all tab stops
void Terminal::setTabStop(int column, bool set)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("setTabStop %d %d\n", column, (int)set);
  #endif

  if (column == 0)
    memset(m_emuState.tabStop, 0, m_columns);
  else
    m_emuState.tabStop[column - 1] = set ? 1 : 0;
}


void Terminal::scrollDown()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("scrollDown\n");
  #endif

  if (m_bitmappedDisplayController && isActive()) {
    // scroll down using canvas
    if (m_emuState.smoothScroll) {
      for (int i = 0; i < m_font.height; ++i)
        m_canvas->scroll(0, 1);
    } else
      m_canvas->scroll(0, m_font.height);
  }

  // move down screen buffer
  for (int y = m_emuState.scrollingRegionDown - 1; y > m_emuState.scrollingRegionTop - 1; --y)
    memcpy(m_glyphsBuffer.map + y * m_columns, m_glyphsBuffer.map + (y - 1) * m_columns, m_columns * sizeof(uint32_t));

  // insert a blank line in the screen buffer
  uint32_t itemValue = GLYPHMAP_ITEM_MAKE(ASCII_SPC, m_emuState.backgroundColor, m_emuState.foregroundColor, m_glyphOptions);
  uint32_t * itemPtr = m_glyphsBuffer.map + (m_emuState.scrollingRegionTop - 1) * m_columns;
  for (int x = 0; x < m_columns; ++x, ++itemPtr)
    *itemPtr = itemValue;

}


// startingRow: represents an absolute value, not related to the scrolling region
void Terminal::scrollDownAt(int startingRow)
{
  int prevScrollingRegionTop = m_emuState.scrollingRegionTop;
  setScrollingRegion(startingRow, m_emuState.scrollingRegionDown, false);

  scrollDown();

  setScrollingRegion(prevScrollingRegionTop, m_emuState.scrollingRegionDown, false);
}


void Terminal::scrollUp()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("scrollUp\n");
  #endif

  auto ldown = m_emuState.scrollingRegionDown;
  if (m_emuState.cursorY == m_rows) {
    // scrolling occurs always when at bottom row, even if it is out of scrolling region (xterm has the same behaviour)
    m_emuState.scrollingRegionDown = m_rows;
    updateCanvasScrollingRegion();
  }

  if (m_bitmappedDisplayController && isActive()) {
    // scroll up using canvas
    if (m_emuState.smoothScroll) {
      for (int i = 0; i < m_font.height; ++i)
        m_canvas->scroll(0, -1);
    } else
      m_canvas->scroll(0, -m_font.height);
  }

  // move up screen buffer
  for (int y = m_emuState.scrollingRegionTop - 1; y < m_emuState.scrollingRegionDown - 1; ++y)
    memcpy(m_glyphsBuffer.map + y * m_columns, m_glyphsBuffer.map + (y + 1) * m_columns, m_columns * sizeof(uint32_t));

  // insert a blank line in the screen buffer
  uint32_t itemValue = GLYPHMAP_ITEM_MAKE(ASCII_SPC, m_emuState.backgroundColor, m_emuState.foregroundColor, m_glyphOptions);
  uint32_t * itemPtr = m_glyphsBuffer.map + (m_emuState.scrollingRegionDown - 1) * m_columns;
  for (int x = 0; x < m_columns; ++x, ++itemPtr)
    *itemPtr = itemValue;

  if (ldown != m_emuState.scrollingRegionDown) {
    m_emuState.scrollingRegionDown = ldown;
    updateCanvasScrollingRegion();
  }
}


void Terminal::scrollUpAt(int startingRow)
{
  int prevScrollingRegionTop = m_emuState.scrollingRegionTop;
  setScrollingRegion(startingRow, m_emuState.scrollingRegionDown, false);

  scrollUp();

  setScrollingRegion(prevScrollingRegionTop, m_emuState.scrollingRegionDown, false);
}


void Terminal::setScrollingRegion(int top, int down, bool resetCursorPos)
{
  m_emuState.scrollingRegionTop  = tclamp(top, 1, (int)m_rows);
  m_emuState.scrollingRegionDown = tclamp(down, 1, (int)m_rows);
  updateCanvasScrollingRegion();

  if (resetCursorPos)
    setCursorPos(1, m_emuState.originMode ? m_emuState.scrollingRegionTop : 1);

  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("setScrollingRegion: %d %d => %d %d\n", top, down, m_emuState.scrollingRegionTop, m_emuState.scrollingRegionDown);
  #endif
}


void Terminal::updateCanvasScrollingRegion()
{
  if (m_bitmappedDisplayController && isActive())
    m_canvas->setScrollingRegion(0, (m_emuState.scrollingRegionTop - 1) * m_font.height, m_canvas->getWidth() - 1, m_emuState.scrollingRegionDown * m_font.height - 1);
}


// Insert a blank space at current position, moving next "charsToMove" characters to the right (even on multiple lines).
// Characters after "charsToMove" length are overwritter.
// Vertical scroll may occurs (in this case returns "True")
bool Terminal::multilineInsertChar(int charsToMove)
{
  bool scrolled = false;
  int col = m_emuState.cursorX;
  int row = m_emuState.cursorY;
  if (m_emuState.cursorPastLastCol) {
    ++row;
    col = 1;
  }
  uint32_t lastColItem = 0;
  while (charsToMove > 0) {
    uint32_t * rowPtr = m_glyphsBuffer.map + (row - 1) * m_columns;
    uint32_t lItem = rowPtr[m_columns - 1];
    insertAt(col, row, 1);
    if (row > m_emuState.cursorY) {
      rowPtr[0] = lastColItem;
      refresh(1, row);
    }
    lastColItem = lItem;
    charsToMove -= m_columns - col;
    col = 1;
    if (charsToMove > 0 && row == m_emuState.scrollingRegionDown) {
      scrolled = true;
      scrollUp();
      setCursorPos(m_emuState.cursorX, m_emuState.cursorY - 1);
    } else {
      ++row;
    }
    if (m_bitmappedDisplayController && isActive())
      m_canvas->waitCompletion(false);
  }
  return scrolled;
}


// inserts specified number of blank spaces at the specified row and column. Characters moved past the right border are lost.
void Terminal::insertAt(int column, int row, int count)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("insertAt(%d, %d, %d)\n", column, row, count);
  #endif

  count = tmin((int)m_columns, count);

  if (m_bitmappedDisplayController && isActive()) {
    // move characters on the right using canvas
    int charWidth = getCharWidthAt(row);
    m_canvas->setScrollingRegion((column - 1) * charWidth, (row - 1) * m_font.height, charWidth * getColumnsAt(row) - 1, row * m_font.height - 1);
    m_canvas->scroll(count * charWidth, 0);
    updateCanvasScrollingRegion();  // restore original scrolling region
  }

  // move characters in the screen buffer
  uint32_t * rowPtr = m_glyphsBuffer.map + (row - 1) * m_columns;
  for (int i = m_columns - 1; i >= column + count - 1; --i)
    rowPtr[i] = rowPtr[i - count];

  // fill blank characters
  GlyphOptions glyphOptions = m_glyphOptions;
  glyphOptions.doubleWidth = glyphMapItem_getOptions(rowPtr).doubleWidth;
  uint32_t itemValue = GLYPHMAP_ITEM_MAKE(ASCII_SPC, m_emuState.backgroundColor, m_emuState.foregroundColor, glyphOptions);
  for (int i = 0; i < count; ++i)
    rowPtr[column + i - 1] = itemValue;
}


void Terminal::multilineDeleteChar(int charsToMove)
{
  int col = m_emuState.cursorX;
  int row = m_emuState.cursorY;
  if (m_emuState.cursorPastLastCol) {
    ++row;
    col = 1;
  }

  // at least one char must be deleted
  if (charsToMove == 0)
    deleteAt(col, row, 1);

  while (charsToMove > 0) {
    deleteAt(col, row, 1);
    charsToMove -= m_columns - col;
    if (charsToMove > 0) {
      if (m_bitmappedDisplayController && isActive())
        m_canvas->waitCompletion(false);
      uint32_t * lastItem  = m_glyphsBuffer.map + (row - 1) * m_columns + (m_columns - 1);
      lastItem[0] = lastItem[1];
      refresh(m_columns, row);
    }
    col = 1;
    ++row;
    if (m_bitmappedDisplayController && isActive())
      m_canvas->waitCompletion(false);
  }
}


// deletes "count" characters from specified "row", starting from "column", scrolling left remaining characters
void Terminal::deleteAt(int column, int row, int count)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("deleteAt(%d, %d, %d)\n", column, row, count);
  #endif

  count = imin(m_columns - column + 1, count);

  if (m_bitmappedDisplayController && isActive()) {
    // move characters on the right using canvas
    int charWidth = getCharWidthAt(row);
    m_canvas->setScrollingRegion((column - 1) * charWidth, (row - 1) * m_font.height, charWidth * getColumnsAt(row) - 1, row * m_font.height - 1);
    m_canvas->scroll(-count * charWidth, 0);
    updateCanvasScrollingRegion();  // restore original scrolling region
  }

  // move characters in the screen buffer
  uint32_t * rowPtr = m_glyphsBuffer.map + (row - 1) * m_columns;
  int itemsToMove = m_columns - column - count + 1;
  for (int i = 0; i < itemsToMove; ++i)
    rowPtr[column - 1 + i] = rowPtr[column - 1 + i + count];

  // fill blank characters
  GlyphOptions glyphOptions = m_glyphOptions;
  glyphOptions.doubleWidth = glyphMapItem_getOptions(rowPtr).doubleWidth;
  uint32_t itemValue = GLYPHMAP_ITEM_MAKE(ASCII_SPC, m_emuState.backgroundColor, m_emuState.foregroundColor, glyphOptions);
  for (int i = m_columns - count + 1 ; i <= m_columns; ++i)
    rowPtr[i - 1] = itemValue;
}


// Coordinates are cursor coordinates (1,1  = top left)
// maintainDoubleWidth = true: Maintains line attributes (double width)
// selective = true: erase only characters with userOpt1 = 0
void Terminal::erase(int X1, int Y1, int X2, int Y2, uint8_t c, bool maintainDoubleWidth, bool selective)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("erase(%d, %d, %d, %d, %d, %d)\n", X1, Y1, X2, Y2, (int)c, (int)maintainDoubleWidth);
  #endif

  X1 = tclamp(X1 - 1, 0, (int)m_columns - 1);
  Y1 = tclamp(Y1 - 1, 0, (int)m_rows - 1);
  X2 = tclamp(X2 - 1, 0, (int)m_columns - 1);
  Y2 = tclamp(Y2 - 1, 0, (int)m_rows - 1);

  if (m_bitmappedDisplayController && isActive()) {
    if (c == ASCII_SPC && !selective) {
      int charWidth = getCharWidthAt(m_emuState.cursorY);
      m_canvas->fillRectangle(X1 * charWidth, Y1 * m_font.height, (X2 + 1) * charWidth - 1, (Y2 + 1) * m_font.height - 1);
    }
  }

  GlyphOptions glyphOptions = {.value = 0};
  glyphOptions.fillBackground = 1;

  for (int y = Y1; y <= Y2; ++y) {
    uint32_t * itemPtr = m_glyphsBuffer.map + X1 + y * m_columns;
    for (int x = X1; x <= X2; ++x, ++itemPtr) {
      if (selective && glyphMapItem_getOptions(itemPtr).userOpt2)  // bypass if protected item
        continue;
      glyphOptions.doubleWidth = maintainDoubleWidth ? glyphMapItem_getOptions(itemPtr).doubleWidth : 0;
      *itemPtr = GLYPHMAP_ITEM_MAKE(c, m_emuState.backgroundColor, m_emuState.foregroundColor, glyphOptions);
    }
  }
  if (c != ASCII_SPC || selective)
    refresh(X1 + 1, Y1 + 1, X2 + 1, Y2 + 1);
}


void Terminal::enableFabGLSequences(bool value)
{
  m_emuState.allowFabGLSequences += value ? 1 : -1;
  if (m_emuState.allowFabGLSequences < 0)
    m_emuState.allowFabGLSequences = 0;
}


void Terminal::clearSavedCursorStates()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("clearSavedCursorStates()\n");
  #endif

  for (TerminalCursorState * curItem = m_savedCursorStateList, * next; curItem; curItem = next) {
    next = curItem->next;
    free(curItem->tabStop);
    free(curItem);
  }
  m_savedCursorStateList = nullptr;
}


void Terminal::saveCursorState()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("saveCursorState()\n");
  #endif

  TerminalCursorState * s = (TerminalCursorState*) malloc(sizeof(TerminalCursorState));

  if (s) {
    *s = (TerminalCursorState) {
      .next              = m_savedCursorStateList,
      .cursorX           = (int16_t) m_emuState.cursorX,
      .cursorY           = (int16_t) m_emuState.cursorY,
      .tabStop           = (uint8_t*) malloc(m_columns),
      .cursorPastLastCol = m_emuState.cursorPastLastCol,
      .originMode        = m_emuState.originMode,
      .glyphOptions      = m_glyphOptions,
      .characterSetIndex = m_emuState.characterSetIndex,
      .characterSet      = {m_emuState.characterSet[0], m_emuState.characterSet[1], m_emuState.characterSet[2], m_emuState.characterSet[3]},
    };
    if (s->tabStop)
      memcpy(s->tabStop, m_emuState.tabStop, m_columns);
    m_savedCursorStateList = s;
  } else {
    #if FABGLIB_TERMINAL_DEBUG_REPORT_ERRORS
    log("ERROR: Unable to alloc TerminalCursorState\n");
    #endif
  }
}


void Terminal::restoreCursorState()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("restoreCursorState()\n");
  #endif

  if (m_savedCursorStateList) {
    m_emuState.cursorX = m_savedCursorStateList->cursorX;
    m_emuState.cursorY = m_savedCursorStateList->cursorY;
    m_emuState.cursorPastLastCol = m_savedCursorStateList->cursorPastLastCol;
    m_emuState.originMode = m_savedCursorStateList->originMode;
    if (m_savedCursorStateList->tabStop)
      memcpy(m_emuState.tabStop, m_savedCursorStateList->tabStop, m_columns);
    m_glyphOptions = m_savedCursorStateList->glyphOptions;
    if (m_bitmappedDisplayController && isActive())
      m_canvas->setGlyphOptions(m_glyphOptions);
    m_emuState.characterSetIndex = m_savedCursorStateList->characterSetIndex;
    for (int i = 0; i < 4; ++i)
      m_emuState.characterSet[i] = m_savedCursorStateList->characterSet[i];

    TerminalCursorState * next = m_savedCursorStateList->next;

    free(m_savedCursorStateList->tabStop);
    free(m_savedCursorStateList);
    m_savedCursorStateList = next;
  }
}


void Terminal::useAlternateScreenBuffer(bool value)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("useAlternateScreenBuffer: %d\n", value);
  #endif
  if (m_alternateScreenBuffer != value) {
    m_alternateScreenBuffer = value;
    if (!m_alternateMap) {
      // first usage, need to setup the alternate screen
      m_alternateMap = (uint32_t*) heap_caps_malloc(sizeof(uint32_t) * m_columns * m_rows, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      clearMap(m_alternateMap);
      m_alternateCursorX = 1;
      m_alternateCursorY = 1;
      m_alternateScrollingRegionTop  = 1;
      m_alternateScrollingRegionDown = m_rows;
      m_alternateCursorBlinkingEnabled = true;
    }
    tswap(m_alternateMap, m_glyphsBuffer.map);
    tswap(m_emuState.cursorX, m_alternateCursorX);
    tswap(m_emuState.cursorY, m_alternateCursorY);
    tswap(m_emuState.scrollingRegionTop, m_alternateScrollingRegionTop);
    tswap(m_emuState.scrollingRegionDown, m_alternateScrollingRegionDown);
    tswap(m_emuState.cursorBlinkingEnabled, m_alternateCursorBlinkingEnabled);
    setScrollingRegion(m_emuState.scrollingRegionTop, m_emuState.scrollingRegionDown, false);
    m_emuState.cursorPastLastCol = false;
    refresh();
  }
}


void Terminal::localInsert(uint8_t c)
{
  if (m_outputQueue)
    xQueueSendToFront(m_outputQueue, &c, portMAX_DELAY);
}


void Terminal::localWrite(uint8_t c)
{
  if (m_outputQueue)
    xQueueSendToBack(m_outputQueue, &c, portMAX_DELAY);
}


void Terminal::localWrite(char const * str)
{
  if (m_outputQueue) {
    while (*str) {
      xQueueSendToBack(m_outputQueue, str, portMAX_DELAY);

      #if FABGLIB_TERMINAL_DEBUG_REPORT_OUT_CODES
      logFmt("=> %02X  %s%c\n", (int)*str, (*str <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)(*str)] : ""), (*str > ASCII_SPC ? *str : ASCII_SPC));
      #endif

      ++str;
    }
  }
}


int Terminal::available()
{
  return m_outputQueue ? uxQueueMessagesWaiting(m_outputQueue) : 0;
}


int Terminal::read()
{
  return read(-1);
}


int Terminal::read(int timeOutMS)
{
  if (m_outputQueue) {
    uint8_t c;
    xQueueReceive(m_outputQueue, &c, msToTicks(timeOutMS));
    return c;
  } else
    return -1;
}


bool Terminal::waitFor(int value, int timeOutMS)
{
  TimeOut timeout;
  while (!timeout.expired(timeOutMS)) {
    int c = read(timeOutMS);
    if (c == value)
      return true;
  }
  return false;
}


// not implemented
int Terminal::peek()
{
  return -1;
}


void Terminal::flush()
{
  flush(true);
}


#ifdef ARDUINO
void Terminal::pollSerialPort()
{
  while (true) {
    int avail = m_serialPort->available();

    if (m_autoXONOFF) {
      if (m_XOFF) {
        // XOFF already sent, need to send XON?
        if (avail < FABGLIB_TERMINAL_XON_THRESHOLD) {
          send(ASCII_XON);
          m_XOFF = false;
        }
      } else {
        // XOFF not sent, need to send XOFF?
        if (avail >= FABGLIB_TERMINAL_XOFF_THRESHOLD) {
          send(ASCII_XOFF);
          m_XOFF = true;
        }
      }
    }

    if (!avail)
      break;

    auto r = m_serialPort->read();
    if (m_uartRXEnabled)
      write(r);
  }
}
#endif


void IRAM_ATTR Terminal::uart_isr(void *arg)
{
  Terminal * term = (Terminal*) arg;
  uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);

  // look for overflow or RX errors
  if (uart->int_st.rxfifo_ovf || uart->int_st.frm_err || uart->int_st.parity_err) {
    // reset RX-FIFO, because hardware bug rxfifo_rst cannot be used, so just flush
    uartFlushRXFIFO();
    // reset interrupt flags
    uart->int_clr.rxfifo_ovf = 1;
    uart->int_clr.frm_err    = 1;
    uart->int_clr.parity_err = 1;
    return;
  }

  // software flow control?
  if (term->m_autoXONOFF) {
    // send XOFF/XON looking at RX FIFO occupation
    int count = uartGetRXFIFOCount();
    if (count > 300 && !term->m_XOFF) {
      uart->flow_conf.send_xoff = 1; // send XOFF
      term->m_XOFF = true;
    } else if (count < 20 && term->m_XOFF) {
      uart->flow_conf.send_xon = 1;  // send XON
      term->m_XOFF = false;
    }
  }

  // main receive loop
  while (uartGetRXFIFOCount() != 0 || uart->mem_rx_status.wr_addr != uart->mem_rx_status.rd_addr) {
    // look for enough room in input queue
    if (term->m_autoXONOFF && xQueueIsQueueFullFromISR(term->m_inputQueue)) {
      if (!term->m_XOFF) {
        uart->flow_conf.send_xoff = 1;  // send XOFF
        term->m_XOFF = true;
      }
      // block further interrupts
      uart->int_ena.rxfifo_full = 0;
      break;
    }
    // add to input queue
    auto r = uart->fifo.rw_byte;
    if (term->m_uartRXEnabled)
      term->write(r, true);
  }

  // clear interrupt flag
  uart->int_clr.rxfifo_full = 1;
}


// send a character to m_serialPort or m_outputQueue
void Terminal::send(uint8_t c)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_OUT_CODES
  logFmt("=> %02X  %s%c\n", (int)c, (c <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)c] : ""), (c > ASCII_SPC ? c : ASCII_SPC));
  #endif

  #ifdef ARDUINO
  if (m_serialPort) {
    while (m_serialPort->availableForWrite() == 0)
      vTaskDelay(1 / portTICK_PERIOD_MS);
    m_serialPort->write(c);
  }
  #endif

  if (m_uart) {
    uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
    while (uart->status.txfifo_cnt == 0x7F)
      ;
    uart->fifo.rw_byte = c;
  }

  localWrite(c);  // write to m_outputQueue
}


// send a string to m_serialPort or m_outputQueue
void Terminal::send(char const * str)
{
  #ifdef ARDUINO
  if (m_serialPort) {
    while (*str) {
      while (m_serialPort->availableForWrite() == 0)
        vTaskDelay(1 / portTICK_PERIOD_MS);
      m_serialPort->write(*str);

      #if FABGLIB_TERMINAL_DEBUG_REPORT_OUT_CODES
      logFmt("=> %02X  %s%c\n", (int)*str, (*str <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)(*str)] : ""), (*str > ASCII_SPC ? *str : ASCII_SPC));
      #endif

      ++str;
    }
  }
  #endif

  if (m_uart) {
    uart_dev_t * uart = (volatile uart_dev_t *)(DR_REG_UART2_BASE);
    while (*str) {
      while (uart->status.txfifo_cnt == 0x7F)
        ;
      uart->fifo.rw_byte = *str++;
    }
  }

  localWrite(str);  // write to m_outputQueue
}


void Terminal::sendCSI()
{
  send(m_emuState.ctrlBits == 7 ? CSI_7BIT : CSI_8BIT);
}


void Terminal::sendDCS()
{
  send(m_emuState.ctrlBits == 7 ? DCS_7BIT : DCS_8BIT);
}


void Terminal::sendSS3()
{
  send(m_emuState.ctrlBits == 7 ? SS3_7BIT : SS3_8BIT);
}


int Terminal::availableForWrite()
{
  return uxQueueSpacesAvailable(m_inputQueue);
}


bool Terminal::addToInputQueue(uint8_t c, bool fromISR)
{
  if (fromISR)
    return xQueueSendToBackFromISR(m_inputQueue, &c, nullptr);
  else
    return xQueueSendToBack(m_inputQueue, &c, portMAX_DELAY);
}


bool Terminal::insertToInputQueue(uint8_t c, bool fromISR)
{
  if (fromISR)
    return xQueueSendToFrontFromISR(m_inputQueue, &c, nullptr);
  else
    return xQueueSendToFront(m_inputQueue, &c, portMAX_DELAY);
}


void Terminal::write(uint8_t c, bool fromISR)
{
  if (m_termInfo == nullptr || m_writeDetectedFabGLSeq)
    addToInputQueue(c, fromISR);  // send unprocessed
  else
    convHandleTranslation(c, fromISR);

  // this is necessary to avoid to call convHandleTranslation() in the middle of FabGL specific sequence (which can have binary data inside)
  if (m_writeDetectedFabGLSeq) {
    if (c == FABGLEXT_ENDCODE)
      m_writeDetectedFabGLSeq = false;
  } else if (m_emuState.allowFabGLSequences && m_lastWrittenChar == ASCII_ESC && c == FABGLEXT_STARTCODE) {
    m_writeDetectedFabGLSeq = true;
  }

  m_lastWrittenChar = c;

  #if FABGLIB_TERMINAL_DEBUG_REPORT_IN_CODES
  logFmt("<= %02X  %s%c\n", (int)c, (c <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)c] : ""), (c > ASCII_SPC ? c : ASCII_SPC));
  #endif
}


size_t Terminal::write(uint8_t c)
{
  write(c, false);
  return 1;
}


size_t Terminal::write(const uint8_t * buffer, size_t size)
{
  for (int i = 0; i < size; ++i)
    write(*(buffer++));
  return size;
}


void Terminal::setTerminalType(TermType value)
{
  // doesn't set it immediately, serialize into the queue
  TerminalController(this).setTerminalType(value);
}


void Terminal::int_setTerminalType(TermInfo const * value)
{
  // disable VT52 mode
  m_emuState.ANSIMode = true;
  m_emuState.conformanceLevel = 4;

  m_termInfo = nullptr;

  if (value != nullptr) {
    // need to "insert" initString in reverse order
    auto s = value->initString;
    for (int i = strlen(s) - 1; i >= 0; --i)
      insertToInputQueue(s[i], false);

    m_termInfo = value;
  }
}


void Terminal::int_setTerminalType(TermType value)
{
  switch (value) {
    case TermType::ANSI_VT:
      int_setTerminalType(nullptr);
      break;
    case TermType::ADM3A:
      int_setTerminalType(&term_ADM3A);
      break;
    case TermType::ADM31:
      int_setTerminalType(&term_ADM31);
      break;
    case TermType::Hazeltine1500:
      int_setTerminalType(&term_Hazeltine1500);
      break;
    case TermType::Osborne:
      int_setTerminalType(&term_Osborne);
      break;
    case TermType::Kaypro:
      int_setTerminalType(&term_Kaypro);
      break;
    case TermType::VT52:
      int_setTerminalType(&term_VT52);
      break;
    case TermType::ANSILegacy:
      int_setTerminalType(&term_ANSILegacy);
      break;
  }
}


void Terminal::convHandleTranslation(uint8_t c, bool fromISR)
{
  if (m_convMatchedCount > 0 || c < 32 || c == 0x7f || c == '~') {

    m_convMatchedChars[m_convMatchedCount] = c;

    if (m_convMatchedItem == nullptr)
      m_convMatchedItem = m_termInfo->videoCtrlSet;

    for (auto item = m_convMatchedItem; item->termSeq; ++item) {
      if (item != m_convMatchedItem) {
        // check if this item can be a new candidate
        if (m_convMatchedCount == 0 || (item->termSeqLen > m_convMatchedCount && strncmp(item->termSeq, m_convMatchedItem->termSeq, m_convMatchedCount) == 0))
          m_convMatchedItem = item;
        else
          continue;
      }
      // here (item == m_convMatchedItem) is always true
      if (item->termSeq[m_convMatchedCount] == 0xFF || item->termSeq[m_convMatchedCount] == c) {
        // are there other chars to process?
        ++m_convMatchedCount;
        if (item->termSeqLen == m_convMatchedCount) {
          // full match, send related ANSI sequences (and resets m_convMatchedCount and m_convMatchedItem)
          for (ConvCtrl const * ctrl = item->convCtrl; *ctrl != ConvCtrl::END; ++ctrl)
            convSendCtrl(*ctrl, fromISR);
        }
        return;
      }
    }

    // no match, send received stuff as is
    convQueue(nullptr, fromISR);
  } else
    addToInputQueue(c, fromISR);
}


void Terminal::convSendCtrl(ConvCtrl ctrl, bool fromISR)
{
  switch (ctrl) {
    case ConvCtrl::CarriageReturn:
      convQueue("\x0d", fromISR);
      break;
    case ConvCtrl::LineFeed:
      convQueue("\x0a", fromISR);
      break;
    case ConvCtrl::CursorLeft:
      convQueue("\e[D", fromISR);
      break;
    case ConvCtrl::CursorUp:
      convQueue("\e[A", fromISR);
      break;
    case ConvCtrl::CursorRight:
      convQueue("\e[C", fromISR);
      break;
    case ConvCtrl::EraseToEndOfScreen:
      convQueue("\e[J", fromISR);
      break;
    case ConvCtrl::EraseToEndOfLine:
      convQueue("\e[K", fromISR);
      break;
    case ConvCtrl::CursorHome:
      convQueue("\e[H", fromISR);
      break;
    case ConvCtrl::AttrNormal:
      convQueue("\e[0m", fromISR);
      break;
    case ConvCtrl::AttrBlank:
      convQueue("\e[8m", fromISR);
      break;
    case ConvCtrl::AttrBlink:
      convQueue("\e[5m", fromISR);
      break;
    case ConvCtrl::AttrBlinkOff:
      convQueue("\e[25m", fromISR);
      break;
    case ConvCtrl::AttrReverse:
      convQueue("\e[7m", fromISR);
      break;
    case ConvCtrl::AttrReverseOff:
      convQueue("\e[27m", fromISR);
      break;
    case ConvCtrl::AttrUnderline:
      convQueue("\e[4m", fromISR);
      break;
    case ConvCtrl::AttrUnderlineOff:
      convQueue("\e[24m", fromISR);
      break;
    case ConvCtrl::AttrReduce:
      convQueue("\e[2m", fromISR);
      break;
    case ConvCtrl::AttrReduceOff:
      convQueue("\e[22m", fromISR);
      break;
    case ConvCtrl::InsertLine:
      convQueue("\e[L", fromISR);
      break;
    case ConvCtrl::InsertChar:
      convQueue("\e[@", fromISR);
      break;
    case ConvCtrl::DeleteLine:
      convQueue("\e[M", fromISR);
      break;
    case ConvCtrl::DeleteCharacter:
      convQueue("\e[P", fromISR);
      break;
    case ConvCtrl::CursorOn:
      convQueue("\e[?25h", fromISR);
      break;
    case ConvCtrl::CursorOff:
      convQueue("\e[?25l", fromISR);
      break;
    case ConvCtrl::SaveCursor:
      convQueue("\e[?1048h", fromISR);
      break;
    case ConvCtrl::RestoreCursor:
      convQueue("\e[?1048l", fromISR);
      break;
    case ConvCtrl::CursorPos:
    case ConvCtrl::CursorPos2:
    {
      char s[11];
      int y = (ctrl == ConvCtrl::CursorPos ? m_convMatchedChars[2] - 31 : m_convMatchedChars[3] + 1);
      int x = (ctrl == ConvCtrl::CursorPos ? m_convMatchedChars[3] - 31 : m_convMatchedChars[2] + 1);
      sprintf(s, "\e[%d;%dH", y, x);
      convQueue(s, fromISR);
      break;
    }

    default:
      break;
  }
}


// queue m_termMatchedChars[] or specified string
void Terminal::convQueue(const char * str, bool fromISR)
{
  if (str) {
    for (; *str; ++str)
      addToInputQueue(*str, fromISR);
  } else {
    for (int i = 0; i <= m_convMatchedCount; ++i) {
      addToInputQueue(m_convMatchedChars[i], fromISR);
    }
  }
  m_convMatchedCount = 0;
  m_convMatchedItem = nullptr;
}


uint32_t Terminal::makeGlyphItem(uint8_t c, GlyphOptions * glyphOptions, Color * newForegroundColor)
{
  *newForegroundColor = m_emuState.foregroundColor;

  if (glyphOptions->bold && (m_coloredAttributesMask & (1 << COLORED_ATTRIBUTE_BOLD))) {
    *newForegroundColor = m_coloredAttributesColor[COLORED_ATTRIBUTE_BOLD];
    if (!m_coloredAttributesMaintainStyle)
      glyphOptions->bold = 0;
  }

  if (glyphOptions->reduceLuminosity && (m_coloredAttributesMask & (1 << COLORED_ATTRIBUTE_FAINT))) {
    *newForegroundColor = m_coloredAttributesColor[COLORED_ATTRIBUTE_FAINT];
    if (!m_coloredAttributesMaintainStyle)
      glyphOptions->reduceLuminosity = 0;
  }

  if (glyphOptions->italic && (m_coloredAttributesMask & (1 << COLORED_ATTRIBUTE_ITALIC))) {
    *newForegroundColor = m_coloredAttributesColor[COLORED_ATTRIBUTE_ITALIC];
    if (!m_coloredAttributesMaintainStyle)
      glyphOptions->italic = 0;
  }

  if (glyphOptions->underline && (m_coloredAttributesMask & (1 << COLORED_ATTRIBUTE_UNDERLINE))) {
    *newForegroundColor = m_coloredAttributesColor[COLORED_ATTRIBUTE_UNDERLINE];
    if (!m_coloredAttributesMaintainStyle)
      glyphOptions->underline = 0;
  }

  return GLYPHMAP_ITEM_MAKE(c, m_emuState.backgroundColor, *newForegroundColor, *glyphOptions);
}


// set specified character at current cursor position
// return true if vertical scroll happened
bool Terminal::setChar(uint8_t c)
{
  bool vscroll = false;

  if (m_emuState.cursorPastLastCol) {
    if (m_emuState.wraparound) {
      setCursorPos(1, m_emuState.cursorY); // this sets m_emuState.cursorPastLastCol = false
      if (moveDown()) {
        scrollUp();
        vscroll = true;
      }
    }
  }

  if (m_emuState.insertMode)
    insertAt(m_emuState.cursorX, m_emuState.cursorY, 1);

  GlyphOptions glyphOptions = m_glyphOptions;

  // doubleWidth must be maintained
  uint32_t * mapItemPtr = m_glyphsBuffer.map + (m_emuState.cursorX - 1) + (m_emuState.cursorY - 1) * m_columns;
  glyphOptions.doubleWidth = glyphMapItem_getOptions(mapItemPtr).doubleWidth;
  Color newForegroundColor;
  *mapItemPtr = makeGlyphItem(c, &glyphOptions, &newForegroundColor);

  if (m_bitmappedDisplayController && isActive()) {

    // Instead of this boring stuff we may also just call:
    //   m_canvas->renderGlyphsBuffer(m_emuState.cursorX - 1, m_emuState.cursorY - 1, &m_glyphsBuffer);
    // and then *synch*, but it may be slower. Further tests required.

    if (glyphOptions.value != m_glyphOptions.value)
      m_canvas->setGlyphOptions(glyphOptions);
    if (newForegroundColor != m_emuState.foregroundColor)
      m_canvas->setPenColor(newForegroundColor);

    int x = (m_emuState.cursorX - 1) * m_font.width * (glyphOptions.doubleWidth ? 2 : 1);
    int y = (m_emuState.cursorY - 1) * m_font.height;
    m_canvas->drawGlyph(x, y, m_font.width, m_font.height, m_font.data, c);

    if (newForegroundColor != m_emuState.foregroundColor)
      m_canvas->setPenColor(m_emuState.foregroundColor);
    if (glyphOptions.value != m_glyphOptions.value)
      m_canvas->setGlyphOptions(m_glyphOptions);
  }

  // blinking text?
  if (m_glyphOptions.userOpt1)
    m_prevBlinkingTextEnabled = true; // consumeInputQueue() will set the value

  if (m_emuState.cursorX == m_columns) {
    m_emuState.cursorPastLastCol = true;
  } else {
    setCursorPos(m_emuState.cursorX + 1, m_emuState.cursorY);
  }

  return vscroll;
}


void Terminal::refresh()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  log("refresh()\n");
  #endif

  refresh(1, 1, m_columns, m_rows);
}


// do not call waitCompletion!!
void Terminal::refresh(int X, int Y)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("refresh(%d, %d)\n", X, Y);
  #endif

  if (m_bitmappedDisplayController && isActive())
    m_canvas->renderGlyphsBuffer(X - 1, Y - 1, &m_glyphsBuffer);
}


void Terminal::refresh(int X1, int Y1, int X2, int Y2)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("refresh(%d, %d, %d, %d)\n", X1, Y1, X2, Y2);
  #endif

  if (m_bitmappedDisplayController && isActive()) {
    for (int y = Y1 - 1; y < Y2; ++y) {
      for (int x = X1 - 1; x < X2; ++x)
        m_canvas->renderGlyphsBuffer(x, y, &m_glyphsBuffer);
      m_canvas->waitCompletion(false);
    }
  }
}


// value: 0 = normal, 1 = double width, 2 = double width - double height top, 3 = double width - double height bottom
void Terminal::setLineDoubleWidth(int row, int value)
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCS
  logFmt("setLineDoubleWidth(%d, %d)\n", row, value);
  #endif

  uint32_t * mapItemPtr = m_glyphsBuffer.map + (row - 1) * m_columns;
  for (int i = 0; i < m_columns; ++i, ++mapItemPtr) {
    GlyphOptions glyphOptions = glyphMapItem_getOptions(mapItemPtr);
    glyphOptions.doubleWidth = value;
    glyphMapItem_setOptions(mapItemPtr, glyphOptions);
  }

  refresh(1, row, m_columns, row);
}


int Terminal::getCharWidthAt(int row)
{
  return glyphMapItem_getOptions(m_glyphsBuffer.map + (row - 1) * m_columns).doubleWidth ? m_font.width * 2 : m_font.width;
}


int Terminal::getColumnsAt(int row)
{
  return glyphMapItem_getOptions(m_glyphsBuffer.map + (row - 1) * m_columns).doubleWidth ? m_columns / 2 : m_columns;
}


GlyphOptions Terminal::getGlyphOptionsAt(int X, int Y)
{
  return glyphMapItem_getOptions(m_glyphsBuffer.map + (X - 1) + (Y - 1) * m_columns);
}


// blocking operation
uint8_t Terminal::getNextCode(bool processCtrlCodes)
{
  while (true) {
    uint8_t c;
    xQueueReceive(m_inputQueue, &c, portMAX_DELAY);

    #if FABGLIB_TERMINAL_DEBUG_REPORT_INQUEUE_CODES
    logFmt("<= %02X  %s%c\n", (int)c, (c <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)c] : ""), (c > ASCII_SPC ? c : ASCII_SPC));
    #endif

    if (m_uart)
      uartCheckInputQueueForFlowControl();

    // inside an ESC sequence we may find control characters!
    if (processCtrlCodes && ISCTRLCHAR(c))
      execCtrlCode(c);
    else
      return c;
  }
}


void Terminal::charsConsumerTask(void * pvParameters)
{
  Terminal * term = (Terminal*) pvParameters;

  while (true)
    term->consumeInputQueue();
}


void Terminal::consumeInputQueue()
{
  uint8_t c = getNextCode(false);  // blocking call. false: do not process ctrl chars

  xSemaphoreTake(m_mutex, portMAX_DELAY);

  m_prevCursorEnabled = int_enableCursor(false);
  m_prevBlinkingTextEnabled = enableBlinkingText(false);

  // ESC
  // Start escape sequence
  if (c == ASCII_ESC)
    consumeESC();

  else if (ISCTRLCHAR(c))
    execCtrlCode(c);

  else {
    if (m_emuState.characterSet[m_emuState.characterSetIndex] == 0 || (!m_emuState.ANSIMode && m_emuState.VT52GraphicsMode))
      c = DECGRAPH_TO_CP437[(uint8_t)c];
    setChar(c);
  }

  enableBlinkingText(m_prevBlinkingTextEnabled);
  int_enableCursor(m_prevCursorEnabled);

  xSemaphoreGive(m_mutex);

  if (m_resetRequested)
    reset();
}


void Terminal::execCtrlCode(uint8_t c)
{
  switch (c) {

    // BS
    // backspace, move cursor one position to the left (without wrap).
    case ASCII_BS:
      if (m_emuState.cursorX > 1)
        setCursorPos(m_emuState.cursorX - 1, m_emuState.cursorY);
      else if (m_emuState.reverseWraparoundMode) {
        int newX = m_columns;
        int newY = m_emuState.cursorY - 1;
        if (newY == 0)
          newY = m_rows;
        setCursorPos(newX, newY);
      }
      break;

    // HT
    // Go to next tabstop or end of line if no tab stop
    case ASCII_HT:
      nextTabStop();
      break;

    // LF
    // Line feed
    case ASCII_LF:
      if (!m_emuState.cursorPastLastCol) {
        if (m_emuState.newLineMode)
          setCursorPos(1, m_emuState.cursorY);
        if (moveDown())
          scrollUp();
      }
      break;

    // VT (vertical tab)
    // FF (form feed)
    // Move cursor down
    case ASCII_VT:
    case ASCII_FF:
      if (moveDown())
        scrollUp();
      break;

    // CR
    // Carriage return. Move cursor at the beginning of the line.
    case ASCII_CR:
      setCursorPos(1, m_emuState.cursorY);
      break;

    // SO
    // Shift Out. Switch to Alternate Character Set (G1)
    case ASCII_SO:
      m_emuState.characterSetIndex = 1;
      break;

    // SI
    // Shift In. Switch to Standard Character Set (G0)
    case ASCII_SI:
      m_emuState.characterSetIndex = 0;
      break;

    case ASCII_DEL:
      // nothing to do
      break;

    // BELL
    case ASCII_BEL:
      sound('1', 800, 250, 100);  // square wave, 800 Hz, 250ms, volume 100
      break;

    default:
      break;
  }
}


// consume non-CSI and CSI (ESC is already consumed)
void Terminal::consumeESC()
{

  if (!m_emuState.ANSIMode) {
    consumeESCVT52();
    return;
  }

  uint8_t c = getNextCode(true);   // true: process ctrl chars

  if (c == '[') {
    // ESC [ : start of CSI sequence
    consumeCSI();
    return;
  }

  if (c == ']') {
    // ESC ] : start of OSC sequence
    consumeOSC();
    return;
  }

  if (c == FABGLEXT_STARTCODE && m_emuState.allowFabGLSequences > 0) {
    // ESC FABGLEXT_STARTCODE : FabGL specific sequence
    consumeFabGLSeq();
    return;
  }

  if (c == 'P') {
    // ESC P : start of DCS sequence
    consumeDCS();
    return;
  }

  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  logFmt("ESC%c\n", c);
  #endif

  switch (c) {

    // ESC c : RIS, reset terminal.
    case 'c':
      m_resetRequested = true;  // reset() actually called by consumeInputQueue()
      break;

    // ESC D : IND, line feed.
    case 'D':
      if (moveDown())
        scrollUp();
      break;

    // ESC E : NEL, new line.
    case 'E':
      setCursorPos(1, m_emuState.cursorY);
      if (moveDown())
        scrollUp();
      break;

    // ESC H : HTS, set tab stop at current column.
    case 'H':
      setTabStop(m_emuState.cursorX, true);
      break;

    // ESC M : RI, move up one line keeping column position.
    case 'M':
      if (moveUp())
        scrollDown();
      break;

    // ESC Z : DECID, returns TERMID
    case 'Z':
      //log("DECID\n");
      sendCSI();
      send(TERMID);
      break;

    // ESC 7 : DECSC, save current cursor state (position and attributes)
    case '7':
      saveCursorState();
      break;

    // ESC 8 : DECRC, restore current cursor state (position and attributes)
    case '8':
      restoreCursorState();
      break;

    // ESC #
    case '#':
      c = getNextCode(true);  // true: process ctrl chars
      switch (c) {
        // ESC # 3 : DECDHL, DEC double-height line, top half
        case '3':
          setLineDoubleWidth(m_emuState.cursorY, 2);
          break;
        // ESC # 4 : DECDHL, DEC double-height line, bottom half
        case '4':
          setLineDoubleWidth(m_emuState.cursorY, 3);
          break;
        // ESC # 5 : DECSWL, DEC single-width line
        case '5':
          setLineDoubleWidth(m_emuState.cursorY, 0);
          break;
        // ESC # 6 : DECDWL, DEC double-width line
        case '6':
          setLineDoubleWidth(m_emuState.cursorY, 1);
          break;
        // ESC # 8 :DECALN, DEC screen alignment test - fill screen with E's.
        case '8':
          erase(1, 1, m_columns, m_rows, 'E', false, false);
          break;
      }
      break;

    // Start sequence defining character set:
    // ESC character_set_index character_set
    // character_set_index: '(' = G0  ')' = G1   '*' = G2   '+' = G3
    // character_set: '0' = VT100 graphics mapping    '1' = VT100 graphics mapping    'B' = USASCII
    case '(':
    case ')':
    case '*':
    case '+':
      switch (getNextCode(true)) {
        case '0':
        case '2':
          m_emuState.characterSet[c - '('] = 0; // DEC Special Character and Line Drawing
          break;
        default:  // 'B' and others
          m_emuState.characterSet[c - '('] = 1; // United States (USASCII)
          break;
      }
      break;

    // ESC = : DECKPAM (Keypad Application Mode)
    case '=':
      m_emuState.keypadMode = KeypadMode::Application;
      #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCSALL
      log("Keypad Application Mode\n");
      #endif
      break;

    // ESC > : DECKPNM (Keypad Numeric Mode)
    case '>':
      m_emuState.keypadMode = KeypadMode::Numeric;
      #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCSALL
      log("Keypad Numeric Mode\n");
      #endif
      break;

    case ASCII_SPC:
      switch (getNextCode(true)) {

        // ESC SPC F : S7C1T, Select 7-Bit C1 Control Characters
        case 'F':
          m_emuState.ctrlBits = 7;
          break;

        // ESC SPC G : S8C1T, Select 8-Bit C1 Control Characters
        case 'G':
          if (m_emuState.conformanceLevel >= 2 && m_emuState.ANSIMode)
            m_emuState.ctrlBits = 8;
          break;
      }
      break;

    // consume unknow char
    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      logFmt("Unknown ESC %c\n", c);
      #endif
      break;
  }
}


// a parameter is a number. Parameters are separated by ';'. Example: "5;27;3"
// first parameter has index 0
// params array must have up to FABGLIB_MAX_CSI_PARAMS
uint8_t Terminal::consumeParamsAndGetCode(int * params, int * paramsCount, bool * questionMarkFound)
{
  // get parameters until maximum size reached or a command character has been found
  *paramsCount = 1; // one parameter is always assumed (even if not exists)
  *questionMarkFound = false;
  int * p = params;
  *p = 0;
  while (true) {
    uint8_t c = getNextCode(true);  // true: process ctrl chars

    #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
    log(c);
    #endif

    if (c == '?') {
      *questionMarkFound = true;
      continue;
    }

    // break the loop if a command has been found
    if (!isdigit(c) && c != ';') {

      #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
      log('\n');
      #endif

      // reset non specified parameters
      while (p < params + FABGLIB_MAX_CSI_PARAMS)
        *(++p) = 0;

      return c;
    }

    if (p < params + FABGLIB_MAX_CSI_PARAMS) {
      if (c == ';') {
        ++p;
        *p = 0;
        ++(*paramsCount);
      } else {
        // this is a digit
        *p = *p * 10 + (c - '0');
      }
    }
  }
}


// consume CSI sequence (ESC + '[' already consumed)
void Terminal::consumeCSI()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  log("ESC[");
  #endif

  bool questionMarkFound;
  int params[FABGLIB_MAX_CSI_PARAMS];
  int paramsCount;
  uint8_t c = consumeParamsAndGetCode(params, &paramsCount, &questionMarkFound);

  // ESC [ ? ... h
  // ESC [ ? ... l
  if (questionMarkFound && (c == 'h' || c == 'l')) {
    consumeDECPrivateModes(params, paramsCount, c);
    return;
  }

  // ESC [ SPC ...
  if (c == ASCII_SPC) {
    consumeCSISPC(params, paramsCount);
    return;
  }

  // ESC [ " ...
  if (c == '"') {
    consumeCSIQUOT(params, paramsCount);
    return;
  }

  // process command in "c"
  switch (c) {

    // ESC [ H : CUP, Move cursor to the indicated row, column
    // ESC [ f : HVP, Move cursor to the indicated row, column
    case 'H':
    case 'f':
      setCursorPos(params[1], getAbsoluteRow(params[0]));
      break;

    // ESC [ g : TBC, Clear one or all tab stops
    case 'g':
      switch (params[0]) {
        case 0:
          setTabStop(m_emuState.cursorX, false); // clear tab stop at cursor
          break;
        case 3:
          setTabStop(0, false);         // clear all tab stops
          break;
      }
      break;

    // ESC [ C : CUF, Move cursor right the indicated # of columns.
    case 'C':
      setCursorPos(m_emuState.cursorX + tmax(1, params[0]), m_emuState.cursorY);
      break;

    // ESC [ P : DCH, Delete the indicated # of characters on current line.
    case 'P':
      deleteAt(m_emuState.cursorX, m_emuState.cursorY, tmax(1, params[0]));
      break;

    // ESC [ A : CUU, Move cursor up the indicated # of rows.
    case 'A':
      setCursorPos(m_emuState.cursorX, getAbsoluteRow(m_emuState.cursorY - tmax(1, params[0])));
      break;

    // ESC [ Ps J : ED, Erase display
    // ESC [ ? Ps J : DECSED, Selective Erase in Display
    // Ps = 0 : from cursor to end of display (default)
    // Ps = 1 : erase from start to cursor
    // Ps = 2 : erase whole display
    // Erase also doubleWidth attributes
    case 'J':
      switch (params[0]) {
        case 0:
          erase(m_emuState.cursorX, m_emuState.cursorY, m_columns, m_emuState.cursorY, ASCII_SPC, false, questionMarkFound);
          erase(1, m_emuState.cursorY + 1, m_columns, m_rows, ASCII_SPC, false, questionMarkFound);
          break;
        case 1:
          erase(1, 1, m_columns, m_emuState.cursorY - 1, ASCII_SPC, false, questionMarkFound);
          erase(1, m_emuState.cursorY, m_emuState.cursorX, m_emuState.cursorY, ASCII_SPC, false, questionMarkFound);
          break;
        case 2:
          erase(1, 1, m_columns, m_rows, ASCII_SPC, false, questionMarkFound);
          break;
      }
      break;

    // ESC [ Ps K :  EL, Erase line
    // ESC [ ? Ps K : DECSEL, Selective Erase in line
    // Ps = 0 : from cursor to end of line (default)
    // Ps = 1 : erase from start of line to cursor
    // Ps = 2 : erase whole line
    // Maintain doubleWidth attributes
    case 'K':
      switch (params[0]) {
        case 0:
          erase(m_emuState.cursorX, m_emuState.cursorY, m_columns, m_emuState.cursorY, ASCII_SPC, true, questionMarkFound);
          break;
        case 1:
          erase(1, m_emuState.cursorY, m_emuState.cursorX, m_emuState.cursorY, ASCII_SPC, true, questionMarkFound);
          break;
        case 2:
          erase(1, m_emuState.cursorY, m_columns, m_emuState.cursorY, ASCII_SPC, true, questionMarkFound);
          break;
      }
      break;

    // ESC [ Pn X : ECH, Erase Characters
    // Pn: number of characters to erase from cursor position (default is 1) up to the end of line
    case 'X':
      erase(m_emuState.cursorX, m_emuState.cursorY, tmin((int)m_columns, m_emuState.cursorX + tmax(1, params[0]) - 1), m_emuState.cursorY, ASCII_SPC, true, false);
      break;

    // ESC [ r : DECSTBM, Set scrolling region; parameters are top and bottom row.
    case 'r':
      setScrollingRegion(tmax(params[0], 1), (params[1] < 1 ? m_rows : params[1]));
      break;

    // ESC [ d : VPA, Move cursor to the indicated row, current column.
    case 'd':
      setCursorPos(m_emuState.cursorX, params[0]);
      break;

    // ESC [ G : CHA, Move cursor to indicated column in current row.
    case 'G':
      setCursorPos(params[0], m_emuState.cursorY);
      break;

    // ESC [ S : SU, Scroll up # lines (default = 1)
    case 'S':
      for (int i = tmax(1, params[0]); i > 0; --i)
        scrollUp();
      break;

    // ESC [ T : SD, Scroll down # lines (default = 1)
    case 'T':
      for (int i = tmax(1, params[0]); i > 0; --i)
        scrollDown();
      break;

    // ESC [ D : CUB, Cursor left
    case 'D':
    {
      int newX = m_emuState.cursorX - tmax(1, params[0]);
      if (m_emuState.reverseWraparoundMode && newX < 1) {
        newX = -newX;
        int newY = m_emuState.cursorY - newX / m_columns - 1;
        if (newY < 1)
          newY = m_rows + newY;
        newX = m_columns - (newX % m_columns);
        setCursorPos(newX, newY);
      } else
        setCursorPos(tmax(1, newX), m_emuState.cursorY);
      break;
    }

    // ESC [ B : CUD, Cursor down
    case 'B':
      setCursorPos(m_emuState.cursorX, getAbsoluteRow(m_emuState.cursorY + tmax(1, params[0])));
      break;

    // ESC [ m : SGR, Character Attributes
    case 'm':
      execSGRParameters(params, paramsCount);
      break;

    // ESC [ L : IL, Insert Lines starting at cursor (default 1 line)
    case 'L':
      for (int i = tmax(1, params[0]); i > 0; --i)
        scrollDownAt(m_emuState.cursorY);
      break;

    // ESC [ M : DL, Delete Lines starting at cursor (default 1 line)
    case 'M':
      for (int i = tmax(1, params[0]); i > 0; --i)
        scrollUpAt(m_emuState.cursorY);
      break;

    // ESC [ h : SM, Set Mode
    // ESC [ l : SM, Unset Mode
    case 'h':
    case 'l':
      switch (params[0]) {

        // IRM, Insert Mode
        case 4:
          m_emuState.insertMode = (c == 'h');
          break;

        // LNM, Automatic Newline
        case 20:
          m_emuState.newLineMode = (c == 'h');
          break;

        default:
          #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
          logFmt("Unknown: ESC [ %d %c\n", params[0], c);
          #endif
          break;
      }
      break;

    // ESC [ @ : ICH, Insert Character (default 1 character)
    case '@':
      insertAt(m_emuState.cursorX, m_emuState.cursorY, tmax(1, params[0]));
      break;

    // ESC [ 0 c : Send Device Attributes
    case 'c':
      if (params[0] == 0) {
        sendCSI();
        send(TERMID);
      }
      break;

    // ESC [ Ps q : DECLL, Load LEDs
    case 'q':
      paramsCount = tmax(1, paramsCount);  // default paramater in case no params are provided
      for (int i = 0; i < paramsCount; ++i) {
        bool numLock, capsLock, scrollLock;
        m_keyboard->getLEDs(&numLock, &capsLock, &scrollLock);
        switch (params[i]) {
          case 0:
            numLock = capsLock = scrollLock = false;
            break;
          case 1:
            numLock = true;
            break;
          case 2:
            capsLock = true;
            break;
          case 3:
            scrollLock = true;
            break;
          case 21:
            numLock = false;
            break;
          case 22:
            capsLock = false;
            break;
          case 23:
            scrollLock = false;
            break;
        }
        m_keyboard->setLEDs(numLock, capsLock, scrollLock);
      }
      break;

    // ESC [ Ps n : DSR, Device Status Report
    case 'n':
      switch (params[0]) {
        // Status Report
        case 5:
          sendCSI();
          send("0n");
          break;
        // Report Cursor Position (CPR)
        case 6:
        {
          sendCSI();
          char s[4];
          send(itoa(m_emuState.originMode ? m_emuState.cursorY - m_emuState.scrollingRegionTop + 1 : m_emuState.cursorY, s, 10));
          send(';');
          send(itoa(m_emuState.cursorX, s, 10));
          send('R');
          break;
        }
      }
      break;

    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      log("Unknown: ESC [ ");
      if (questionMarkFound)
        log("? ");
      for (int i = 0; i < paramsCount; ++i)
        logFmt("%d %c ", params[i], i < paramsCount - 1 ? ';' : c);
      log('\n');
      #endif
      break;
  }
}


// consume CSI " sequences
void Terminal::consumeCSIQUOT(int * params, int paramsCount)
{
  uint8_t c = getNextCode(true);  // true: process ctrl chars

  switch (c) {

    // ESC [ P1; P2 " p : DECSCL, Select Conformance Level
    case 'p':
      m_emuState.conformanceLevel = params[0] - 60;
      if (params[0] == 61 || (paramsCount == 2 && params[1] == 1))
        m_emuState.ctrlBits = 7;
      else
        m_emuState.ctrlBits = 8;
      break;

    // ESC [ Ps " q : DECSCA, Select character protection attribute
    case 'q':
      m_glyphOptions.userOpt2 = (params[0] == 1 ? 1 : 0);
      break;

  }
}


// consume CSI SPC sequences
void Terminal::consumeCSISPC(int * params, int paramsCount)
{
  uint8_t c = getNextCode(true);  // true: process ctrl chars

  switch (c) {

    // ESC [ Ps SPC q : DECSCUSR, Set Cursor Style
    // Ps: 0..1 blinking block, 2 steady block, 3 blinking underline, 4 steady underline, 5 blinking bar, 6 steady bar
    case 'q':
      m_emuState.cursorStyle = params[0];
      m_emuState.cursorBlinkingEnabled = (params[0] == 0) || (params[0] & 1);
      break;

    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      log("Unknown: ESC [ ");
      for (int i = 0; i < paramsCount; ++i)
        logFmt("%d %c ", params[i], i < paramsCount - 1 ? ';' : ASCII_SPC);
      logFmt(" %c\n", c);
      #endif
      break;
  }
}


// consume DEC Private Mode (DECSET/DECRST) sequences
// ESC [ ? # h     <- set
// ESC [ ? # l     <- reset
// "c" can be "h" or "l"
void Terminal::consumeDECPrivateModes(int const * params, int paramsCount, uint8_t c)
{
  bool set = (c == 'h');
  switch (params[0]) {

    // ESC [ ? 1 h
    // ESC [ ? 1 l
    // DECCKM (default off): Cursor Keys Mode
    case 1:
      m_emuState.cursorKeysMode = set;
      break;

    // ESC [ ? 2 h
    // ESC [ ? 2 l
    // DECANM (default on): ANSI Mode (off = VT52 Mode)
    case 2:
      m_emuState.ANSIMode = set;
      break;

    // ESC [ ? 3 h
    // ESC [ ? 3 l
    // DECCOLM (default off): 132 Column Mode
    case 3:
      if (m_emuState.allow132ColumnMode) {
        set132ColumnMode(set);
        setCursorPos(1, 1);
      }
      break;

    // ESC [ ? 4 h
    // ESC [ ? 4 l
    // DECSCLM (default off): Scrolling mode (jump or smooth)
    case 4:
      m_emuState.smoothScroll = set;
      break;


    // ESC [ ? 5 h
    // ESC [ ? 5 l
    // DECSCNM (default off): Reverse Video
    case 5:
      reverseVideo(set);
      break;

    // ESC [ ? 6 h
    // ESC [ ? 6 l
    // DECOM (default off): Origin mode
    case 6:
      m_emuState.originMode = set;
      if (set)
        setCursorPos(m_emuState.cursorX, m_emuState.scrollingRegionTop);
      break;

    // ESC [ ? 7 h
    // ESC [ ? 7 l
    // DECAWM (default on): Wraparound mode
    case 7:
      m_emuState.wraparound = set;
      break;

    // ESC [ ? 8 h
    // ESC [ ? 8 l
    // DECARM (default on): Autorepeat mode
    case 8:
      m_emuState.keyAutorepeat = set;
      break;

    // ESC [ ? 12 h
    // ESC [ ? 12 l
    // Start Blinking Cursor
    case 12:
      m_emuState.cursorBlinkingEnabled = set;
      break;

    // ESC [ ? 25 h
    // ESC [ ? 25 l
    // DECTECM (default on): Make cursor visible.
    case 25:
      m_prevCursorEnabled = set;  // consumeInputQueue() will set the value
      break;

    // ESC [ ? 40 h
    // ESC [ ? 40 l
    // Allow 80 -> 132 column mode (default off)
    case 40:
      m_emuState.allow132ColumnMode = set;
      break;

    // ESC [ ? 45 h
    // ESC [ ? 45 l
    // Reverse-wraparound mode (default off)
    case 45:
      m_emuState.reverseWraparoundMode = set;
      break;

    // ESC [ ? 47 h
    // ESC [ ? 47 l
    // ESC [ ? 1047 h
    // ESC [ ? 1047 l
    // Use Alternate Screen Buffer
    case 47:
    case 1047:
      useAlternateScreenBuffer(set);
      break;

    // ESC [ ? 77 h
    // ESC [ ? 77 l
    // DECBKM (default off): Backarrow key Mode
    case 67:
      m_emuState.backarrowKeyMode = set;
      break;

    // ESC [ ? 1048 h  : Save cursor as in DECSC
    // ESC [ ? 1048 l  : Restore cursor as in DECSC
    case 1048:
      if (set)
        saveCursorState();
      else
        restoreCursorState();
      break;

    // ESC [ ? 1049 h
    // ESC [ ? 1049 l
    // Save cursor as in DECSC and use Alternate Screen Buffer
    case 1049:
      if (set) {
        saveCursorState();
        useAlternateScreenBuffer(true);
      } else {
        useAlternateScreenBuffer(false);
        restoreCursorState();
      }
      break;

    // ESC [ ? 7999 h
    // ESC [ ? 7999 l
    // Allows enhanced FabGL sequences (default disabled)
    // This set is "incremental". This is actually disabled when the counter reach 0.
    case 7999:
      enableFabGLSequences(set);
      break;

    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      logFmt("Unknown DECSET/DECRST: %d %c\n", params[0], c);
      #endif
      break;
  }
}


// exec SGR: Select Graphic Rendition
// ESC [ ...params... m
void Terminal::execSGRParameters(int const * params, int paramsCount)
{
  for (; paramsCount; ++params, --paramsCount) {
    switch (*params) {

      // Normal
      case 0:
        m_glyphOptions.bold             = 0;
        m_glyphOptions.reduceLuminosity = 0;  // faint
        m_glyphOptions.italic           = 0;
        m_glyphOptions.underline        = 0;
        m_glyphOptions.userOpt1         = 0;  // blink
        m_glyphOptions.blank            = 0;  // invisible
        m_glyphOptions.invert           = 0;  // inverse
        int_setForegroundColor(m_defaultForegroundColor);
        int_setBackgroundColor(m_defaultBackgroundColor);
        break;

      // Bold
      case 1:
        m_glyphOptions.bold = 1;
        break;

      // Faint (decreased intensity)
      case 2:
        m_glyphOptions.reduceLuminosity = 1;
        break;

      // Select primary font
      case 10:
        m_emuState.characterSetIndex = 0;
        break;

      // Disable Bold and Faint
      case 22:
        m_glyphOptions.bold = m_glyphOptions.reduceLuminosity = 0;
        break;

      // Italic
      case 3:
        m_glyphOptions.italic = 1;
        break;

      // Disable Italic
      case 23:
        m_glyphOptions.italic = 0;
        break;

      // Underline
      case 4:
        m_glyphOptions.underline = 1;
        break;

      // Disable Underline
      case 24:
        m_glyphOptions.underline = 0;
        break;

      // Blink
      case 5:
        m_glyphOptions.userOpt1 = 1;
        break;

      // Disable Blink
      case 25:
        m_glyphOptions.userOpt1 = 0;
        break;

      // Inverse
      case 7:
        m_glyphOptions.invert = 1;
        break;

      // Disable Inverse
      case 27:
        m_glyphOptions.invert = 0;
        break;

      // Invisible
      case 8:
        m_glyphOptions.blank = 1;
        break;

      // Disable Invisible
      case 28:
        m_glyphOptions.blank = 0;
        break;

      // Set Foreground color
      case 30 ... 37:
        int_setForegroundColor( (Color) (*params - 30) );
        break;

      // Set Foreground color to Default
      case 39:
        int_setForegroundColor(m_defaultForegroundColor);
        break;

      // Set Background color
      case 40 ... 47:
        int_setBackgroundColor( (Color) (*params - 40) );
        break;

      // Set Background color to Default
      case 49:
        int_setBackgroundColor(m_defaultBackgroundColor);
        break;

      // Set Bright Foreground color
      case 90 ... 97:
        int_setForegroundColor( (Color) (8 + *params - 90) );
        break;

      // Set Bright Background color
      case 100 ... 107:
        int_setBackgroundColor( (Color) (8 + *params - 100) );
        break;

      default:
        #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
        logFmt("Unknown: ESC [ %d m\n", *params);
        #endif
        break;

    }
  }
  if (m_bitmappedDisplayController && isActive())
    m_canvas->setGlyphOptions(m_glyphOptions);
}


// "ESC P" (DCS) already consumed
// consume from parameters to ST (that is ESC "\")
void Terminal::consumeDCS()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  log("ESC P");
  #endif

  // get parameters
  bool questionMarkFound;
  int params[FABGLIB_MAX_CSI_PARAMS];
  int paramsCount;
  uint8_t c = consumeParamsAndGetCode(params, &paramsCount, &questionMarkFound);

  // get DCS content up to ST
  uint8_t content[FABGLIB_MAX_DCS_CONTENT];
  int contentLength = 0;
  content[contentLength++] = c;
  while (true) {
    uint8_t c = getNextCode(false);  // false: do notprocess ctrl chars, ESC needed here
    if (c == ASCII_ESC) {
      if (getNextCode(false) == '\\')
        break;  // ST found
      else {
        #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
        log("DCS failed, expected ST\n");
        #endif
        return;  // fail
      }
    } else if (contentLength == FABGLIB_MAX_DCS_CONTENT) {
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      log("DCS failed, content too long\n");
      #endif
      return; // fail
    }
    content[contentLength++] = c;
  }

  // $q : DECRQSS, Request Selection or Setting
  if (m_emuState.conformanceLevel >= 3 && contentLength > 2 && content[0] == '$' && content[1] == 'q') {

    // "p : request DECSCL setting, reply with: DCS 1 $ r DECSCL " p ST
    //   where DECSCL is: 6 m_emuState.conformanceLevel ; bits " p
    //   where bits is 0 = 8 bits, 1 = 7 bits
    if (contentLength == 4 && content[2] == '\"' && content[3] == 'p') {
      sendDCS();
      send("1$r6");
      send('0' + m_emuState.conformanceLevel);
      send(';');
      send(m_emuState.ctrlBits == 7 ? '1' : '0');
      send("\"p\e\\");
      return; // processed
    }

  }

  #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
  log("Unknown: ESC P ");
  for (int i = 0; i < paramsCount; ++i)
    logFmt("%d %c ", params[i], i < paramsCount - 1 ? ';' : ASCII_SPC);
  logFmt("%.*s ESC \\\n", contentLength, content);
  #endif
}


void Terminal::consumeESCVT52()
{
  uint8_t c = getNextCode(false);

  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  logFmt("ESC%c\n", c);
  #endif

  // this allows fabgl sequences even in VT52 mode
  if (c == FABGLEXT_STARTCODE && m_emuState.allowFabGLSequences > 0) {
    // ESC FABGLEXT_STARTCODE : FabGL specific sequence
    consumeFabGLSeq();
    return;
  }

  switch (c) {

    // ESC < : Exit VT52 mode, goes to VT100 mode
    case '<':
      m_emuState.ANSIMode = true;
      m_emuState.conformanceLevel = 1;
      break;

    // ESC A : Cursor Up
    case 'A':
      setCursorPos(m_emuState.cursorX, m_emuState.cursorY - 1);
      break;

    // ESC B : Cursor Down
    case 'B':
      setCursorPos(m_emuState.cursorX, m_emuState.cursorY + 1);
      break;

    // ESC C : Cursor Right
    case 'C':
      setCursorPos(m_emuState.cursorX + 1, m_emuState.cursorY);
      break;

    // ESC D : Cursor Left
    case 'D':
      setCursorPos(m_emuState.cursorX -1, m_emuState.cursorY);
      break;

    // ESC H : Cursor to Home Position
    case 'H':
      setCursorPos(1, 1);
      break;

    // ESC I : Reverse Line Feed
    case 'I':
      if (moveUp())
        scrollDown();
      break;

    // ESC J : Erase from cursor to end of screen
    case 'J':
      erase(m_emuState.cursorX, m_emuState.cursorY, m_columns, m_emuState.cursorY, ASCII_SPC, false, false);
      erase(1, m_emuState.cursorY + 1, m_columns, m_rows, ASCII_SPC, false, false);
      break;

    // ESC K : Erase from cursor to end of line
    case 'K':
      erase(m_emuState.cursorX, m_emuState.cursorY, m_columns, m_emuState.cursorY, ASCII_SPC, true, false);
      break;

    // ESC Y row col : Direct Cursor Addressing
    case 'Y':
    {
      int row = getNextCode(false) - 31;
      int col = getNextCode(false) - 31;
      setCursorPos(col, row);
      break;
    }

    // ESC Z : Identify
    case 'Z':
      send("\e/Z");
      break;

    // ESC = : Enter Alternate Keypad Mode
    case '=':
      m_emuState.keypadMode = KeypadMode::Application;
      #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCSALL
      log("Enter Alternate Keypad Mode\n");
      #endif
      break;

    // ESC > : Exit Alternate Keypad Mode
    case '>':
      m_emuState.keypadMode = KeypadMode::Numeric;
      #if FABGLIB_TERMINAL_DEBUG_REPORT_DESCSALL
      log("Exit Alternate Keypad Mode\n");
      #endif
      break;

    // ESC F : Enter Graphics Mode
    case 'F':
      m_emuState.VT52GraphicsMode = true;
      break;

    // ESC G : Exit Graphics Mode
    case 'G':
      m_emuState.VT52GraphicsMode = false;
      break;

    // consume unknow char
    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      logFmt("Unknown ESC %c\n", c);
      #endif
      break;
  }

}


// consume OSC sequence (ESC + ']' already consumed)
// OSC is terminated by ASCII_BEL or ST (ESC + '\')
void Terminal::consumeOSC()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  log("ESC]");
  #endif

  char prevChar = 0;
  while (true) {
    uint8_t c = getNextCode(false);
    #if FABGLIB_TERMINAL_DEBUG_REPORT_OSC_CONTENT
    logFmt("OSC: %02X  %s%c\n", (int)c, (c <= ASCII_SPC ? CTRLCHAR_TO_STR[(int)(c)] : ""), (c > ASCII_SPC ? c : ASCII_SPC));
    #endif
    if (c == ASCII_BEL || (c == '\\' && prevChar == ASCII_ESC))
      break;
    prevChar = c;
  }
}


SoundGenerator * Terminal::soundGenerator()
{
  if (!m_soundGenerator)
    m_soundGenerator = new SoundGenerator;
  return m_soundGenerator;
}


void Terminal::sound(int waveform, int frequency, int duration, int volume)
{
  auto sg = soundGenerator();  // make sure m_soundGenerator is valid
  switch (waveform) {
    case '0':
      sg->playSound(SineWaveformGenerator(), frequency, duration, volume);
      break;
    case '1':
      sg->playSound(SquareWaveformGenerator(), frequency, duration, volume);
      break;
    case '2':
      sg->playSound(TriangleWaveformGenerator(), frequency, duration, volume);
      break;
    case '3':
      sg->playSound(SawtoothWaveformGenerator(), frequency, duration, volume);
      break;
    case '4':
      sg->playSound(NoiseWaveformGenerator(), frequency, duration, volume);
      break;
    case '5':
      sg->playSound(VICNoiseGenerator(), frequency, duration, volume);
      break;
  }
}


// get a single byte from getNextCode() or m_extNextCode
uint8_t Terminal::extGetByteParam()
{
  if (m_extNextCode > -1) {
    auto r = m_extNextCode;
    m_extNextCode = -1;
    return r;
  } else
    return getNextCode(false);
}


// get integer parameter terminated by non digit character
// the integer may have a single space in place of sign (ie "-10", "+10", " 10" are valid values)
int Terminal::extGetIntParam()
{
  int sign = -2;  // -2 = not set
  int val = 0;
  while (true) {
    uint8_t c = extGetByteParam();
    if (!isdigit(c)) {
      // sign?
      if (sign == -2) {
        if (c == '-') {
          sign = -1;
          continue;
        } else if (c == '+' || c == ' ') {
          sign = 1;
          continue;
        }
      }
      // reinsert non digit character and terminate
      m_extNextCode = c;
      break;
    } else if (sign == -2)
      sign = 1; // default positive
    val = val * 10 + c - '0';
  }
  return val * sign;
}

// get a command parameter terminated by non alpha character
// str must be already allocated by FABGLEXT_MAXSUBCMDLEN characters
// max number of characters is specified by
void Terminal::extGetCmdParam(char * cmd)
{
  int len = 0;
  for (; len < FABGLEXT_MAXSUBCMDLEN - 1; ++len) {
    uint8_t c = extGetByteParam();
    if (!isalpha(c)) {
      // reinsert non digit character and terminate
      m_extNextCode = c;
      break;
    }
    cmd[len] = c;
  }
  cmd[len] = 0;
}


// consume FabGL specific sequence (ESC FABGLEXT_STARTCODE ....)
void Terminal::consumeFabGLSeq()
{
  #if FABGLIB_TERMINAL_DEBUG_REPORT_ESC
  log("ESC FABGLEXT_STARTCODE");
  #endif

  m_extNextCode = -1;

  uint8_t c = extGetByteParam();

  // process command in "c"
  switch (c) {

    // Clear terminal area
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTX_CLEAR FABGLEXT_ENDCODE
    // params:
    //   none
    case FABGLEXTX_CLEAR:
      extGetByteParam(); // FABGLEXT_ENDCODE
      syncDisplayController();
      erase(1, 1, m_columns, m_rows, ASCII_SPC, false, false);
      break;

    // Enable/disable cursor
    // Seq:
    //  ESC FABGLEXT_STARTCODE FABGLEXTX_ENABLECURSOR STATE FABGLEXT_ENDCODE
    // params:
    //  STATE (byte): '0' = disable (and others), '1' = enable
    case FABGLEXTX_ENABLECURSOR:
      m_prevCursorEnabled = (extGetByteParam() == '1');
      extGetByteParam(); // FABGLEXT_ENDCODE
      break;

    // Get cursor horizontal position (1 = leftmost pos)
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTB_GETCURSORCOL FABGLEXT_ENDCODE
    // params:
    //    none
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    byte: column
    case FABGLEXTB_GETCURSORCOL:
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(m_emuState.cursorX);
      break;

    // Get cursor vertical position (1 = topmost pos)
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTB_GETCURSORROW FABGLEXT_ENDCODE
    // params:
    //    none
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    byte: row
    case FABGLEXTB_GETCURSORROW:
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(m_emuState.cursorY);
      break;

    // Get cursor position
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTB_GETCURSORPOS FABGLEXT_ENDCODE
    // params:
    //    none
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    byte: column
    //    byte: row
    case FABGLEXTB_GETCURSORPOS:
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(m_emuState.cursorX);
      send(m_emuState.cursorY);
      break;

    // Set cursor position
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_SETCURSORPOS COL ROW FABGLEXT_ENDCODE
    // params:
    //   COL (byte): column (1 = first column)
    //   ROW (byte): row (1 = first row)
    case FABGLEXTB_SETCURSORPOS:
    {
      uint8_t col = extGetByteParam();
      uint8_t row = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      setCursorPos(col, getAbsoluteRow(row));
      break;
    }

    // Set cursor position (textual parameters)
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTX_SETCURSORPOS COL ';' ROW FABGLEXT_ENDCODE
    // params:
    //   COL (text): column (1 = first column)
    //   ROW (text): row (1 = first row)
    case FABGLEXTX_SETCURSORPOS:
    {
      uint8_t col = extGetIntParam();
      extGetByteParam();  // ';'
      uint8_t row = extGetIntParam();
      extGetByteParam();  // FABGLEXT_ENDCODE
      setCursorPos(col, getAbsoluteRow(row));
      break;
    }

    // Insert a blank space at current position, moving next CHARSTOMOVE characters to the right (even on multiple lines).
    // Advances cursor by one position. Characters after CHARSTOMOVE length are overwritter.
    // Vertical scroll may occurs.
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_INSERTSPACE CHARSTOMOVE_L CHARSTOMOVE_H FABGLEXT_ENDCODE
    // params:
    //   CHARSTOMOVE_L, CHARSTOMOVE_H (byte): number of chars to move to the right by one position
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    byte: 0 = vertical scroll not occurred, 1 = vertical scroll occurred
    case FABGLEXTB_INSERTSPACE:
    {
      uint8_t charsToMove_L = extGetByteParam();
      uint8_t charsToMove_H = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      bool scroll = multilineInsertChar(charsToMove_L | charsToMove_H << 8);
      send(FABGLEXT_REPLYCODE);
      send(scroll);
      break;
    }

    // Delete character at current position, moving next CHARSTOMOVE characters to the left (even on multiple lines).
    // Characters after CHARSTOMOVE are filled with spaces.
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_DELETECHAR CHARSTOMOVE_L CHARSTOMOVE_H FABGLEXT_ENDCODE
    // params:
    //   CHARSTOMOVE_L, CHARSTOMOVE_H (byte): number of chars to move to the left by one position
    case FABGLEXTB_DELETECHAR:
    {
      uint8_t charsToMove_L = extGetByteParam();
      uint8_t charsToMove_H = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      multilineDeleteChar(charsToMove_L | charsToMove_H << 8);
      break;
    }

    // Move cursor at left, wrapping lines if necessary
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_CURSORLEFT COUNT_L COUNT_H FABGLEXT_ENDCODE
    // params:
    //   COUNT_L, COUNT_H (byte): number of positions to move to the left
    case FABGLEXTB_CURSORLEFT:
    {
      uint8_t count_L = extGetByteParam();
      uint8_t count_H = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      move(-(count_L | count_H << 8));
      break;
    }

    // Move cursor at right, wrapping lines if necessary
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_CURSORRIGHT COUNT_L COUNT_H FABGLEXT_ENDCODE
    // params:
    //   COUNT (byte): number of positions to move to the right
    case FABGLEXTB_CURSORRIGHT:
    {
      uint8_t count_L = extGetByteParam();
      uint8_t count_H = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      move(count_L | count_H << 8);
      break;
    }

    // Sets char CHAR at current position and advance one position. Scroll if necessary.
    // This do not interpret character as a special code, but just sets it.
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_SETCHAR CHAR FABGLEXT_ENDCODE
    // params:
    //   CHAR (byte): character to set
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    byte: 0 = vertical scroll not occurred, 1 = vertical scroll occurred
    case FABGLEXTB_SETCHAR:
    {
      bool scroll = setChar(extGetByteParam());
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(scroll);
      break;
    }

    // Return virtual key state
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTB_ISVKDOWN VKCODE FABGLEXT_ENDCODE
    // params:
    //    VKCODE : virtual key code to check
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    char: '0' = key is up, '1' = key is down
    case FABGLEXTB_ISVKDOWN:
    {
      VirtualKey vk = (VirtualKey) extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(keyboard()->isVKDown(vk) ? '1' : '0');
      break;
    }

    // Disable FabGL sequences
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_DISABLEFABSEQ FABGLEXT_ENDCODE
    case FABGLEXTB_DISABLEFABSEQ:
      extGetByteParam(); // FABGLEXT_ENDCODE
      enableFabGLSequences(false);
      break;

    // Set terminal type
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTB_SETTERMTYPE TERMINDEX FABGLEXT_ENDCODE
    // params:
    //    TERMINDEX : index of terminal to emulate (TermType)
    case FABGLEXTB_SETTERMTYPE:
    {
      auto termType = (TermType) extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      int_setTerminalType(termType);
      break;
    }

    // Set foreground color
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_SETFGCOLOR COLORINDEX FABGLEXT_ENDCODE
    // params:
    //   COLORINDEX : 0..15 (index of Color enum)
    case FABGLEXTB_SETFGCOLOR:
      int_setForegroundColor((Color) extGetByteParam());
      extGetByteParam(); // FABGLEXT_ENDCODE
      break;

    // Set background color
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_SETBGCOLOR COLORINDEX FABGLEXT_ENDCODE
    // params:
    //   COLORINDEX : 0..15 (index of Color enum)
    case FABGLEXTB_SETBGCOLOR:
      int_setBackgroundColor((Color) extGetByteParam());
      extGetByteParam(); // FABGLEXT_ENDCODE
      break;

    // Set char style
    // Seq:
    //   ESC FABGLEXT_STARTCODE FABGLEXTB_SETCHARSTYLE STYLEINDEX ENABLE FABGLEXT_ENDCODE
    // params:
    //   STYLEINDEX : 0 = bold, 1 = reduce luminosity, 2 = italic, 3 = underline, 4 = blink, 5 = blank, 6 = inverse
    //   ENABLE     : 0 = disable, 1 = enable
    case FABGLEXTB_SETCHARSTYLE:
    {
      int idx = extGetByteParam();
      int val = extGetByteParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      switch (idx) {
        case 0: // bold
          m_glyphOptions.bold = val;
          break;
        case 1: // reduce luminosity
          m_glyphOptions.reduceLuminosity = val;
          break;
        case 2: // italic
          m_glyphOptions.italic = val;
          break;
        case 3: // underline
          m_glyphOptions.underline = val;
          break;
        case 4: // blink
          m_glyphOptions.userOpt1 = val;
          break;
        case 5: // blank
          m_glyphOptions.blank = val;
          break;
        case 6: // inverse
          m_glyphOptions.invert = val;
          break;
      }
      if (m_bitmappedDisplayController && isActive())
        m_canvas->setGlyphOptions(m_glyphOptions);
      break;
    }

    // Setup GPIO
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_SETUPGPIO MODE GPIONUM FABGLEXT_ENDCODE
    // params:
    //    MODE (char) :
    //              '-' = disable input/output
    //              'I' = input only
    //              'O' = output only
    //              'D' = output only with open-drain
    //              'E' = output and input with open-drain
    //              'X' = output and input
    //    GPIONUM (text) : '0'-'39' (not all usable!)
    case FABGLEXTX_SETUPGPIO:
    {
      auto mode = GPIO_MODE_DISABLE;
      switch (extGetByteParam()) {
        case 'I':
          mode = GPIO_MODE_INPUT;
          break;
        case 'O':
          mode = GPIO_MODE_OUTPUT;
          break;
        case 'D':
          mode = GPIO_MODE_OUTPUT_OD;
          break;
        case 'E':
          mode = GPIO_MODE_INPUT_OUTPUT_OD;
          break;
        case 'X':
          mode = GPIO_MODE_INPUT_OUTPUT;
          break;
      }
      auto gpio = (gpio_num_t) extGetIntParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      configureGPIO(gpio, mode);
      break;
    }

    // Set GPIO
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_SETGPIO VALUE GPIONUM FABGLEXT_ENDCODE
    // params:
    //    VALUE (char)   : 0 or '0' or 'L' = low (and others), 1 or '1' or 'H' = high
    //    GPIONUM (text) : '0'-'39' (not all usable!)
    case FABGLEXTX_SETGPIO:
    {
      auto l = extGetByteParam();
      auto level = (l == 1 || l == '1' || l == 'H') ? 1 : 0;
      auto gpio = (gpio_num_t) extGetIntParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      gpio_set_level(gpio, level);
      break;
    }

    // Get GPIO
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_GETGPIO GPIONUM FABGLEXT_ENDCODE
    // params:
    //    GPIONUM (text) : '0'-'39' (not all usable!)
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    char: '0' = low, '1' = high
    case FABGLEXTX_GETGPIO:
    {
      auto gpio = (gpio_num_t) extGetIntParam();
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(gpio_get_level(gpio) ? '1' : '0');
      break;
    }

    // Setup ADC
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_SETUPADC RESOLUTION ';' ATTENUATION ';' GPIONUM FABGLEXT_ENDCODE
    // params:
    //    RESOLUTION (text)  : '9', '10', '11', '12'
    //    ATTENUATION (text) :
    //                   '0' = 0dB   (reduced to 1/1), full-scale voltage 1.1 V, accurate between 100 and 950 mV
    //                   '1' = 2.5dB (reduced to 1/1.34), full-scale voltage 1.5 V, accurate between 100 and 1250 mV
    //                   '2' = 6dB   (reduced to 1/2), full-scale voltage 2.2 V, accurate between 150 to 1750 mV
    //                   '3' = 11dB  (reduced to 1/3.6), full-scale voltage 3.9 V (maximum volatage is still 3.3V!!), accurate between 150 to 2450 mV
    //    GPIONUM (text)     : '32'...'39'
    case FABGLEXTX_SETUPADC:
    {
      auto width   = (adc_bits_width_t) (extGetIntParam() - 9);
      extGetByteParam();  // ';'
      auto atten   = (adc_atten_t) extGetIntParam();
      extGetByteParam();  // ';'
      auto channel = ADC1_GPIO2Channel((gpio_num_t)extGetIntParam());
      extGetByteParam();  // FABGLEXT_ENDCODE
      adc1_config_width(width);
      adc1_config_channel_atten(channel, atten);
      break;
    }

    // Read ADC
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_READADC GPIONUM FABGLEXT_ENDCODE
    // params:
    //    GPIONUM (text) : '32'...'39'
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    char: 1 hex digit of 16 bit value  (most significant nibble)
    //    char: 2 hex digit of 16 bit value
    //    char: 3 hex digit of 16 bit value  (least significant nibble)
    //
    // Example of return value if read value is 160 (0x0A0):
    //       '0'
    //       'A'
    //       '0'
    case FABGLEXTX_READADC:
    {
      auto val = adc1_get_raw(ADC1_GPIO2Channel((gpio_num_t)extGetIntParam()));
      extGetByteParam(); // FABGLEXT_ENDCODE
      send(FABGLEXT_REPLYCODE);
      send(toupper(digit2hex((val & 0xF00) >> 8)));
      send(toupper(digit2hex((val & 0x0F0) >> 4)));
      send(toupper(digit2hex(val & 0x00F)));
      break;
    }

    // Sound
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_SOUND WAVEFORM ';' FREQUENCY ';' DURATION ';' VOLUME FABGLEXT_ENDCODE
    // params:
    //    WAVEFORM (char)  : '0' = SINE, '1' = SQUARE, '2' = TRIANGLE, '3' = SAWTOOTH, '4' = NOISE, '5' = VIC NOISE
    //    FREQUENCY (text) : frequency in Hertz
    //    DURATION (text)  : duration in milliseconds
    //    VOLUME (text)    : volume (max is 127)
    case FABGLEXTX_SOUND:
    {
      char waveform      = extGetByteParam();
      extGetByteParam();  // ';'
      uint16_t frequency = extGetIntParam();
      extGetByteParam();  // ';'
      uint16_t duration  = extGetIntParam();
      extGetByteParam();  // ';'
      uint8_t volume     = extGetIntParam() & 0x7f;
      extGetByteParam();  // FABGLEXT_ENDCODE
      sound(waveform, frequency, duration, volume);
      break;
    }

    // Begin of a graphics command
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_GRAPHICSCMD ...
    case FABGLEXTX_GRAPHICSCMD:
      consumeFabGLGraphicsSeq();
      break;

    // Show or hide mouse pointer
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_SHOWMOUSE VALUE FABGLEXT_ENDCODE
    // params:
    //    VALUE (char) : '1' show mouse, '0' (and others) hide mouse
    case FABGLEXTX_SHOWMOUSE:
    {
      bool value = (extGetByteParam() == '1');
      if (m_bitmappedDisplayController) {
        auto dispctrl = static_cast<BitmappedDisplayController*>(m_displayController);
        auto mouse = PS2Controller::mouse();
        if (mouse && mouse->isMouseAvailable()) {
          if (value) {
            mouse->setupAbsolutePositioner(m_canvas->getWidth(), m_canvas->getHeight(), false, dispctrl);
            dispctrl->setMouseCursor(CursorName::CursorPointerSimpleReduced);
          } else {
            dispctrl->setMouseCursor(nullptr);
            mouse->terminateAbsolutePositioner();
          }
        }
      }
      extGetByteParam();  // FABGLEXT_ENDCODE
      break;
    }

    // Get mouse position
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_GETMOUSEPOS FABGLEXT_ENDCODE
    // params:
    //    none
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    //    3 hex digits : x position
    //    char: ';'
    //    3 hex digits : y position
    //    char: ';'
    //    1 hex digit : scroll wheel delta (0..15)
    //    char: ';'
    //    1 hex digit : pressed button (bit 1 = left button, bit 2 = middle button, bit 3 = right button)
    case FABGLEXTX_GETMOUSEPOS:
    {
      extGetByteParam();  // FABGLEXT_ENDCODE
      if (m_bitmappedDisplayController) {
        auto mouse = PS2Controller::mouse();
        auto x = mouse->status().X;
        auto y = mouse->status().Y;
        send(FABGLEXT_REPLYCODE);
        // x
        send(toupper(digit2hex((x & 0xF00) >> 8)));
        send(toupper(digit2hex((x & 0x0F0) >> 4)));
        send(toupper(digit2hex((x & 0x00F)     )));
        send(';');
        // y
        send(toupper(digit2hex((y & 0xF00) >> 8)));
        send(toupper(digit2hex((y & 0x0F0) >> 4)));
        send(toupper(digit2hex((y & 0x00F)     )));
        send(';');
        // scroll wheel
        send(toupper(digit2hex(mouse->status().wheelDelta & 0xf)));
        send(';');
        // button
        auto b = mouse->status().buttons;
        send(toupper(digit2hex( b.left | (b.middle << 1) | (b.right << 2) )));
      }
      break;
    }

    // Delay for milliseconds (return FABGLEXT_REPLYCODE when time is elapsed)
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXTX_DELAY VALUE FABGLEXT_ENDCODE
    // params:
    //    VALUE (text) : number (milliseconds)
    // return:
    //    byte: FABGLEXT_REPLYCODE   (reply tag)
    case FABGLEXTX_DELAY:
    {
      auto value = extGetIntParam();
      extGetByteParam();  // FABGLEXT_ENDCODE
      vTaskDelay(value / portTICK_PERIOD_MS);
      send(FABGLEXT_REPLYCODE);
      break;
    }

    // User sequence
    // Seq:
    //    ESC FABGLEXT_STARTCODE FABGLEXT_USERSEQ ... FABGLEXT_ENDCODE
    // params:
    //    ... any character different than FABGLEXT_ENDCODE, and up to FABGLEXT_MAXSUBCMDLEN characters
    case FABGLEXT_USERSEQ:
    {
      char usrseq[FABGLEXT_MAXSUBCMDLEN];
      int count = 0;
      while (count < FABGLEXT_MAXSUBCMDLEN) {
        char c = extGetByteParam();
        if (c == FABGLEXT_ENDCODE)
          break;
        usrseq[count++] = c;
      }
      usrseq[count] = 0;
      onUserSequence(usrseq);
      break;
    }

    default:
      #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
      logFmt("Unknown: ESC FABGLEXT_STARTCODE %02x\n", c);
      #endif
      break;
  }
}


void Terminal::freeSprites()
{
  for (int i = 0; i < m_spritesCount; ++i) {
    for (int j = 0; j < m_sprites[i].framesCount; ++j) {
      free(m_sprites[i].frames[j]->data);  // free bitmap data
      delete m_sprites[i].frames[j];       // free bitmap struct
    }
  }
  delete [] m_sprites;
  m_sprites = nullptr;
  m_spritesCount = 0;
}



// already received: ESC FABGLEXT_STARTCODE FABGLEXTX_GRAPHICSCMD
void Terminal::consumeFabGLGraphicsSeq()
{
  char cmd[FABGLEXT_MAXSUBCMDLEN];
  extGetCmdParam(cmd);

  if (strcmp(cmd, FABGLEXT_GCLEAR) == 0) {

    // Graphics clear (fill entire screen with canvas brush) and reset scrolling region
    // Seq:
    //    FABGLEXT_GCLEAR FABGLEXT_ENDCODE
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas) {
      m_canvas->reset();
      m_canvas->clear();
    }

  } else if (strcmp(cmd, FABGLEXT_GSETBRUSHCOLOR) == 0) {

    // Graphics set brush color
    // Seq:
    //    FABGLEXT_GSETBRUSHCOLOR RED ';' GREEN ';' BLUE FABGLEXT_ENDCODE
    // params:
    //    RED (text)   : '0'..'255'
    //    GREEN (text) : '0'..'255'
    //    BLUE (text)  : '0'..'255'
    int r = extGetIntParam();
    extGetByteParam();  // ';'
    int g = extGetIntParam();
    extGetByteParam();  // ';'
    int b = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->setBrushColor(r, g, b);

  } else if (strcmp(cmd, FABGLEXT_GSETPENCOLOR) == 0) {

    // Graphics set pen color
    // Seq:
    //    FABGLEXT_GSETPENCOLOR RED ';' GREEN ';' BLUE FABGLEXT_ENDCODE
    // params:
    //    RED (text)   : '0'..'255'
    //    GREEN (text) : '0'..'255'
    //    BLUE (text)  : '0'..'255'
    int r = extGetIntParam();
    extGetByteParam();  // ';'
    int g = extGetIntParam();
    extGetByteParam();  // ';'
    int b = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->setPenColor(r, g, b);

  } else if (strcmp(cmd, FABGLEXT_GSETPIXEL) == 0) {

    // Graphics set pixel
    // Seq:
    //    FABGLEXT_GSETPIXEL X ';' Y FABGLEXT_ENDCODE
    // params:
    //    X (text) : number
    //    Y (text) : number
    int x = extGetIntParam();
    extGetByteParam();  // ';'
    int y = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->setPixel(x, y);

  } else if (strcmp(cmd, FABGLEXT_GSCROLL) == 0) {

    // Graphics scroll
    // Seq:
    //    FABGLEXT_GSCROLL OFFSETX ';' OFFSETY FABGLEXT_ENDCODE
    // params:
    //    OFFSETX (text) : number
    //    OFFSETY (text) : number
    int ox = extGetIntParam();
    extGetByteParam();  // ';'
    int oy = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->scroll(ox, oy);

  } else if (strcmp(cmd, FABGLEXT_GPENWIDTH) == 0) {

    // Graphics set pen width
    // Seq:
    //    FABGLEXT_GPENWIDTH WIDTH FABGLEXT_ENDCODE
    // params:
    //    WIDTH (text) : number
    int w = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->setPenWidth(w);

  } else if (strcmp(cmd, FABGLEXT_GLINE) == 0) {

    // Graphics draw a line
    // Seq:
    //    FABGLEXT_GLINE X1 ';' Y1 ';' X2 ';' Y2 FABGLEXT_ENDCODE
    // params:
    //    X1 (text) : number
    //    Y1 (text) : number
    //    X2 (text) : number
    //    Y2 (text) : number
    int x1 = extGetIntParam();
    extGetByteParam();  // ';'
    int y1 = extGetIntParam();
    extGetByteParam();  // ';'
    int x2 = extGetIntParam();
    extGetByteParam();  // ';'
    int y2 = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->drawLine(x1, y1, x2, y2);

  } else if (strcmp(cmd, FABGLEXT_GRECT) == 0) {

    // Graphics draw a rectangle
    // Seq:
    //    FABGLEXT_GRECT X1 ';' Y1 ';' X2 ';' Y2 FABGLEXT_ENDCODE
    // params:
    //    X1 (text) : number
    //    Y1 (text) : number
    //    X2 (text) : number
    //    Y2 (text) : number
    int x1 = extGetIntParam();
    extGetByteParam();  // ';'
    int y1 = extGetIntParam();
    extGetByteParam();  // ';'
    int x2 = extGetIntParam();
    extGetByteParam();  // ';'
    int y2 = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->drawRectangle(x1, y1, x2, y2);

  } else if (strcmp(cmd, FABGLEXT_GFILLRECT) == 0) {

    // Graphics fill a rectangle
    // Seq:
    //    FABGLEXT_GFILLRECT X1 ';' Y1 ';' X2 ';' Y2 FABGLEXT_ENDCODE
    // params:
    //    X1 (text) : number
    //    Y1 (text) : number
    //    X2 (text) : number
    //    Y2 (text) : number
    int x1 = extGetIntParam();
    extGetByteParam();  // ';'
    int y1 = extGetIntParam();
    extGetByteParam();  // ';'
    int x2 = extGetIntParam();
    extGetByteParam();  // ';'
    int y2 = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->fillRectangle(x1, y1, x2, y2);

  } else if (strcmp(cmd, FABGLEXT_GELLIPSE) == 0) {

    // Graphics draw an ellipse
    // Seq:
    //    FABGLEXT_GELLIPSE X ';' Y ';' WIDTH ';' HEIGHT FABGLEXT_ENDCODE
    // params:
    //    X (text)      : number
    //    Y (text)      : number
    //    WIDTH (text)  : number
    //    HEIGHT (text) : number
    int x = extGetIntParam();
    extGetByteParam();  // ';'
    int y = extGetIntParam();
    extGetByteParam();  // ';'
    int w = extGetIntParam();
    extGetByteParam();  // ';'
    int h = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->drawEllipse(x, y, w, h);

  } else if (strcmp(cmd, FABGLEXT_GFILLELLIPSE) == 0) {

    // Graphics fill an ellipse
    // Seq:
    //    FABGLEXT_GFILLELLIPSE X ';' Y ';' WIDTH ';' HEIGHT FABGLEXT_ENDCODE
    // params:
    //    X (text)      : number
    //    Y (text)      : number
    //    WIDTH (text)  : number
    //    HEIGHT (text) : number
    int x = extGetIntParam();
    extGetByteParam();  // ';'
    int y = extGetIntParam();
    extGetByteParam();  // ';'
    int w = extGetIntParam();
    extGetByteParam();  // ';'
    int h = extGetIntParam();
    extGetByteParam();  // FABGLEXT_ENDCODE
    if (m_canvas)
      m_canvas->fillEllipse(x, y, w, h);

  } else if (strcmp(cmd, FABGLEXT_GPATH) == 0) {

    // Graphics draw path
    // Seq:
    //    FABGLEXT_GPATH X1 ';' Y1 ';' X2 ';' Y2 [';' Xn ';' Yn...] FABGLEXT_ENDCODE
    // params:
    //    X (text)      : number
    //    Y (text)      : number
    // notes:
    //    max 32 points
    constexpr int MAXPOINTS = 32;
    Point pts[MAXPOINTS];
    int count = 0;
    while (count < MAXPOINTS) { // @TODO: what happens if count ends before FABGLEXT_ENDCODE?
      pts[count].X = extGetIntParam();
      extGetByteParam();
      pts[count].Y = extGetIntParam();
      ++count;
      if (extGetByteParam() == FABGLEXT_ENDCODE)
        break;
    }
    if (m_canvas)
      m_canvas->drawPath(pts, count);

  } else if (strcmp(cmd, FABGLEXT_GFILLPATH) == 0) {

    // Graphics fill path
    // Seq:
    //    FABGLEXT_GFILLPATH X1 ';' Y1 ; X2 ';' Y2 [';' Xn ';' Yn...] FABGLEXT_ENDCODE
    // params:
    //    X (text)      : number
    //    Y (text)      : number
    // notes:
    //    max 32 points
    constexpr int MAXPOINTS = 32;
    Point pts[MAXPOINTS];
    int count = 0;
    while (count < MAXPOINTS) { // @TODO: what happens if count ends before FABGLEXT_ENDCODE?
      pts[count].X = extGetIntParam();
      extGetByteParam();
      pts[count].Y = extGetIntParam();
      ++count;
      if (extGetByteParam() == FABGLEXT_ENDCODE)
        break;
    }
    if (m_canvas)
      m_canvas->fillPath(pts, count);

  } else if (strcmp(cmd, FABGLEXT_GSPRITECOUNT) == 0) {

    // Determines number of sprites to define
    // Seq:
    //    FABGLEXT_GSPRITECOUNT COUNT FABGLEXT_ENDCODE
    // params:
    //    COUNT (text) : number of sprites that will be defined by FABGLEXT_GSPRITEDEF (0 = free memory)
    int count = extGetIntParam();
    extGetByteParam();
    if (m_bitmappedDisplayController) {
      static_cast<BitmappedDisplayController*>(m_displayController)->setSprites<Sprite>(nullptr, 0);
      freeSprites();
      if (count > 0) {
        m_spritesCount = count;
        m_sprites = new Sprite[count];
      }
    }

  } else if (strcmp(cmd, FABGLEXT_GSPRITEDEF) == 0) {

    // Add a bitmap to a sprite
    // Seq:
    //    FABGLEXT_GSPRITEDEF SPRITEINDEX ';' WIDTH ';' HEIGHT ';' FORMAT ';' [R ';' G ';' B ';'] DATA... FABGLEXT_ENDCODE
    // params:
    //    SPRITEINDEX (text) : sprite index (0...)
    //    WIDTH (text)       : sprite width
    //    HEIGHT (text)      : sprite height
    //    FORMAT (char)      : 'M' = PixelFormat::Mask, '2' = PixelFormat::RGBA2222, '8' = PixelFormat::RGBA8888
    //    R (text)           : red (0..255) when FORMAT is "MASK"
    //    G (text)           : green (0..255) when FORMAT is "MASK"
    //    B (text)           : blue (0..255) when FORMAT is "MASK"
    //    DATA (text)        : 2 digits hex number    
    int sprite = extGetIntParam();
    extGetByteParam();
    int width = extGetIntParam();
    extGetByteParam();
    int height = extGetIntParam();
    extGetByteParam();
    char cformat = extGetByteParam();
    extGetByteParam();
    int r = 0, g = 0, b = 0;
    int bytes = 0;
    PixelFormat format = PixelFormat::Undefined;
    switch (cformat) {
      case 'M':
        r = extGetIntParam();
        extGetByteParam();
        g = extGetIntParam();
        extGetByteParam();
        b = extGetIntParam();
        extGetByteParam();
        bytes = (width + 7) / 8 * height;
        format = PixelFormat::Mask;
        break;
      case '2':
        bytes = width * height;
        format = PixelFormat::RGBA2222;
        break;
      case '8':
        bytes = width * height * 4;
        format = PixelFormat::RGBA8888;
        break;
    }
    auto data = (uint8_t*) malloc(bytes);
    for (int i = 0; i < bytes + 1; ++i) { // +1 to include ending code
      auto c = extGetByteParam();
      if (c == FABGLEXT_ENDCODE)
        break;
      data[i] = hex2digit(tolower(c)) << 4;
      c = extGetByteParam();
      if (c == FABGLEXT_ENDCODE)
        break;
      data[i] |= hex2digit(tolower(c));
    }
    if (m_bitmappedDisplayController && sprite < m_spritesCount) {
      auto bitmap = new Bitmap(width, height, data, format, RGB888(r, g, b), false);
      m_sprites[sprite].addBitmap(bitmap);
      static_cast<BitmappedDisplayController*>(m_displayController)->setSprites(m_sprites, m_spritesCount);
    } else {
      // error
      free(data);
    }

  } else if (strcmp(cmd, FABGLEXT_GSPRITESET) == 0) {

    // Set sprite visibility, position and frame
    // Seq:
    //    FABGLEXT_GSPRITESET SPRITEINDEX ';' VISIBLE ';' FRAME ';' POSX ';' POSY FABGLEXT_ENDCODE
    // params:
    //    SPRITEINDEX (text) : sprite index (0...)
    //    VISIBLE (char)     : 'H' = hidden, 'V' = visible
    //    FRAME (text)       : frame index (0...)
    //    POSX (text)        : x position
    //    POSY (text)        : y position
    int sprite = extGetIntParam();
    extGetByteParam();
    char visible = extGetByteParam();
    extGetByteParam();
    int frame = extGetIntParam();
    extGetByteParam();
    int posx = extGetIntParam();
    extGetByteParam();
    int posy = extGetIntParam();
    extGetByteParam();
    if (m_bitmappedDisplayController && sprite < m_spritesCount) {
      m_sprites[sprite].visible = (visible == 'V');
      m_sprites[sprite].setFrame(frame);
      m_sprites[sprite].x = posx;
      m_sprites[sprite].y = posy;
      static_cast<BitmappedDisplayController*>(m_displayController)->refreshSprites();
    }

  } else {
    #if FABGLIB_TERMINAL_DEBUG_REPORT_UNSUPPORT
    logFmt("Unknown: ESC FABGLEXT_STARTCODE FABGLEXTX_GRAPHICSCMD %s\n", cmd);
    #endif
  }
}


void Terminal::keyboardReaderTask(void * pvParameters)
{
  Terminal * term = (Terminal*) pvParameters;

  while (true) {

    if (!term->isActive())
      vTaskSuspend(NULL);

    VirtualKeyItem item;
    if (term->m_keyboard->getNextVirtualKey(&item)) {

      if (term->isActive()) {

        term->onVirtualKey(&item.vk, item.down);
        term->onVirtualKeyItem(&item);

        if (item.down) {

          if (!term->m_emuState.keyAutorepeat && term->m_lastPressedKey == item.vk)
            continue; // don't repeat
          term->m_lastPressedKey = item.vk;

          xSemaphoreTake(term->m_mutex, portMAX_DELAY);

          if (term->m_termInfo == nullptr) {
            if (term->m_emuState.ANSIMode)
              term->ANSIDecodeVirtualKey(item);
            else
              term->VT52DecodeVirtualKey(item);
          } else
            term->TermDecodeVirtualKey(item);

          xSemaphoreGive(term->m_mutex);

        } else {
          // !keyDown
          term->m_lastPressedKey = VK_NONE;
        }

      } else {
        // not active, reinject back
        term->m_keyboard->injectVirtualKey(item, true);
      }

    }

  }
}


void Terminal::sendCursorKeyCode(uint8_t c)
{
  if (m_emuState.cursorKeysMode)
    sendSS3();
  else
    sendCSI();
  send(c);
}


void Terminal::sendKeypadCursorKeyCode(uint8_t applicationCode, const char * numericCode)
{
  if (m_emuState.keypadMode == KeypadMode::Application) {
    sendSS3();
    send(applicationCode);
  } else {
    sendCSI();
    send(numericCode);
  }
}


void Terminal::ANSIDecodeVirtualKey(VirtualKeyItem const & item)
{
  switch (item.vk) {

    // cursor keys

    case VK_UP:
      sendCursorKeyCode('A');
      break;

    case VK_DOWN:
      sendCursorKeyCode('B');
      break;

    case VK_RIGHT:
      sendCursorKeyCode('C');
      break;

    case VK_LEFT:
      sendCursorKeyCode('D');
      break;

    // cursor keys - on numeric keypad

    case VK_KP_UP:
      sendKeypadCursorKeyCode('x', "A");
      break;

    case VK_KP_DOWN:
      sendKeypadCursorKeyCode('r', "B");
      break;

    case VK_KP_RIGHT:
      sendKeypadCursorKeyCode('v', "C");
      break;

    case VK_KP_LEFT:
      sendKeypadCursorKeyCode('t', "D");
      break;

    // PageUp, PageDown, Insert, Home, Delete, End

    case VK_PAGEUP:
      sendCSI();
      send("5~");
      break;

    case VK_PAGEDOWN:
      sendCSI();
      send("6~");
      break;

    case VK_INSERT:
      sendCSI();
      send("2~");
      break;

    case VK_HOME:
      sendCSI();
      send("1~");
      break;

    case VK_DELETE:
      sendCSI();
      send("3~");
      break;

    case VK_END:
      sendCSI();
      send("4~");
      break;

    // PageUp, PageDown, Insert, Home, Delete, End - on numeric keypad

    case VK_KP_PAGEUP:
      sendKeypadCursorKeyCode('y', "5~");
      break;

    case VK_KP_PAGEDOWN:
      sendKeypadCursorKeyCode('s', "6~");
      break;

    case VK_KP_INSERT:
      sendKeypadCursorKeyCode('p', "2~");
      break;

    case VK_KP_HOME:
      sendKeypadCursorKeyCode('w', "1~");
      break;

    case VK_KP_DELETE:
      sendKeypadCursorKeyCode('n', "3~");
      break;

    case VK_KP_END:
      sendKeypadCursorKeyCode('q', "4~");
      break;

    // Backspace

    case VK_BACKSPACE:
      send(m_emuState.backarrowKeyMode ? ASCII_BS : ASCII_DEL);
      break;

    // Function keys

    case VK_F1:
      sendSS3();
      send('P');
      break;

    case VK_F2:
      sendSS3();
      send('Q');
      break;

    case VK_F3:
      sendSS3();
      send('R');
      break;

    case VK_F4:
      sendSS3();
      send('S');
      break;

    case VK_F5:
      sendCSI();
      send("15~");
      break;

    case VK_F6:
      sendCSI();
      send("17~");
      break;

    case VK_F7:
      sendCSI();
      send("18~");
      break;

    case VK_F8:
      sendCSI();
      send("19~");
      break;

    case VK_F9:
      sendCSI();
      send("20~");
      break;

    case VK_F10:
      sendCSI();
      send("21~");
      break;

    case VK_F11:
      sendCSI();
      send("23~");
      break;

    case VK_F12:
      sendCSI();
      send("24~");
      break;

    // Printable keys

    default:
    {
      switch (item.ASCII) {

        // RETURN (CR)?
        case ASCII_CR:
          if (m_emuState.newLineMode)
            send("\r\n");  // send CR LF  (0x0D 0x0A)
          else
            send('\r');    // send only CR (0x0D)
          break;

        default:
          if (item.ASCII > 0)
            send(item.ASCII);
          break;
      }
      break;
    }

  }
}


void Terminal::VT52DecodeVirtualKey(VirtualKeyItem const & item)
{
  switch (item.vk) {

    // cursor keys

    case VK_UP:
      send("\eA");
      break;

    case VK_DOWN:
      send("\eB");
      break;

    case VK_RIGHT:
      send("\eC");
      break;

    case VK_LEFT:
      send("\eD");
      break;

    // numeric keypad

    case VK_KP_0:
    case VK_KP_INSERT:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?p" : "0");
      break;

    case VK_KP_1:
    case VK_KP_END:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?q" : "1");
      break;

    case VK_KP_2:
    case VK_KP_DOWN:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?r" : "2");
      break;

    case VK_KP_3:
    case VK_KP_PAGEDOWN:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?s" : "3");
      break;

    case VK_KP_4:
    case VK_KP_LEFT:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?t" : "4");
      break;

    case VK_KP_5:
    case VK_KP_CENTER:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?u" : "5");
      break;

    case VK_KP_6:
    case VK_KP_RIGHT:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?v" : "6");
      break;

    case VK_KP_7:
    case VK_KP_HOME:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?w" : "7");
      break;

    case VK_KP_8:
    case VK_KP_UP:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?x" : "8");
      break;

    case VK_KP_9:
    case VK_KP_PAGEUP:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?y" : "9");
      break;

    case VK_KP_PERIOD:
    case VK_KP_DELETE:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?n" : ".");
      break;

    case VK_KP_ENTER:
      send(m_emuState.keypadMode == KeypadMode::Application ? "\e?M" : "\r");
      break;


    // Printable keys

    default:
    {
      if (item.ASCII > 0)
        send(item.ASCII);
      break;
    }

  }
}


void Terminal::TermDecodeVirtualKey(VirtualKeyItem const & item)
{
  for (auto i = m_termInfo->kbdCtrlSet; i->vk != VK_NONE; ++i) {
    if (i->vk == item.vk) {
      send(i->ANSICtrlCode);
      return;
    }
  }

  // default behavior
  if (item.ASCII > 0)
    send(item.ASCII);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TerminalController


TerminalController::TerminalController(Terminal * terminal)
  : m_terminal(terminal)
{
}


TerminalController::~TerminalController()
{
}


void TerminalController::setTerminal(Terminal * terminal)
{
  m_terminal = terminal;
}


void TerminalController::write(uint8_t c)
{
  if (m_terminal)
    m_terminal->write(c);
  else
    onWrite(c);
}


void TerminalController::write(char const * str)
{
  while (*str)
    write(*str++);
}


int TerminalController::read()
{
  if (m_terminal)
    return m_terminal->read(-1);
  else {
    int c;
    onRead(&c);
    return c;
  }
}


void TerminalController::waitFor(int value)
{
  while (true)
    if (read() == value)
      return;
}


void TerminalController::clear()
{
  write(FABGLEXT_CMD);
  write(FABGLEXTX_CLEAR);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::enableCursor(bool value)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTX_ENABLECURSOR);
  write(value ? '1' : '0');
  write(FABGLEXT_ENDCODE);
}


void TerminalController::setCursorPos(int col, int row)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETCURSORPOS);
  write(col);
  write(row);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::cursorLeft(int count)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_CURSORLEFT);
  write(count & 0xff);
  write(count >> 8);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::cursorRight(int count)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_CURSORRIGHT);
  write(count & 0xff);
  write(count >> 8);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::getCursorPos(int * col, int * row)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_GETCURSORPOS);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  *col = read();
  *row = read();
}


int TerminalController::getCursorCol()
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_GETCURSORCOL);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  return read();
}


int TerminalController::getCursorRow()
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_GETCURSORROW);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  return read();
}


bool TerminalController::multilineInsertChar(int charsToMove)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_INSERTSPACE);
  write(charsToMove & 0xff);
  write(charsToMove >> 8);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  return read();
}


void TerminalController::multilineDeleteChar(int charsToMove)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_DELETECHAR);
  write(charsToMove & 0xff);
  write(charsToMove >> 8);
  write(FABGLEXT_ENDCODE);
}


bool TerminalController::setChar(uint8_t c)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETCHAR);
  write(c);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  return read();
}


bool TerminalController::isVKDown(VirtualKey vk)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_ISVKDOWN);
  write((uint8_t)vk);
  write(FABGLEXT_ENDCODE);
  waitFor(FABGLEXT_REPLYCODE);
  return read() == '1';
}


void TerminalController::disableFabGLSequences()
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_DISABLEFABSEQ);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::setTerminalType(TermType value)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETTERMTYPE);
  write((int)value);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::setForegroundColor(Color value)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETFGCOLOR);
  write((int)value);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::setBackgroundColor(Color value)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETBGCOLOR);
  write((int)value);
  write(FABGLEXT_ENDCODE);
}


void TerminalController::setCharStyle(CharStyle style, bool enabled)
{
  write(FABGLEXT_CMD);
  write(FABGLEXTB_SETCHARSTYLE);
  write((int)style);
  write(enabled ? 1 : 0);
  write(FABGLEXT_ENDCODE);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LineEditor


LineEditor::LineEditor(Terminal * terminal)
  : m_terminal(terminal),
    m_termctrl(terminal),
    m_text(nullptr),
    m_textLength(0),
    m_allocated(0),
    m_state(-1),
    m_insertMode(true),
    m_typeText(nullptr),
    m_typingIndex(0)
{
}


LineEditor::~LineEditor()
{
  if (m_typeText)
    free(m_typeText);
  free(m_text);
}


void LineEditor::setLength(int newLength)
{
  if (m_allocated < newLength || m_allocated == 0) {
    int allocated = imax(m_allocated * 2, newLength);
    m_text = (char*) realloc(m_text, allocated + 1);
    memset(m_text + m_allocated, 0, allocated - m_allocated + 1);
    m_allocated = allocated;
  }
  m_textLength = newLength;
}


void LineEditor::typeText(char const * text)
{
  if (m_typeText)
    free(m_typeText);
  m_typeText = strdup(text);
  m_typingIndex = 0;
}


void LineEditor::setText(char const * text, bool moveCursor)
{
  setText(text, strlen(text), moveCursor);
}


void LineEditor::setText(char const * text, int length, bool moveCursor)
{
  if (m_state > -1) {
    // already editing, replace previous text
    m_termctrl.setCursorPos(m_homeCol, m_homeRow);
    for (int i = 0; i < m_textLength; ++i)
      m_termctrl.setChar(' ');
    m_termctrl.setCursorPos(m_homeCol, m_homeRow);
    for (int i = 0; i < length; ++i)
      m_homeRow -= m_termctrl.setChar(text[i]);
  }
  setLength(length);
  memcpy(m_text, text, length);
  m_text[length] = 0;
  m_inputPos = moveCursor ? length : 0;
}


void LineEditor::write(uint8_t c)
{
  if (m_terminal)
    m_terminal->write(c);
  else
    onWrite(c);
}


int LineEditor::read()
{
  if (m_terminal)
    return m_terminal->read(-1);
  else {
    int c;
    onRead(&c);
    return c;
  }
}


void LineEditor::beginInput()
{
  if (m_terminal == nullptr) {
    // in case a terminal has been not specified, we need to use onRead and onWrite delegates
    m_termctrl.onRead  = [&](int * c) { onRead(c); };
    m_termctrl.onWrite = [&](int c)   { onWrite(c); };
  }
  m_homeCol = m_termctrl.getCursorCol();
  m_homeRow = m_termctrl.getCursorRow();
  if (m_text) {
    // m_inputPos already set by setText()
    for (int i = 0, len = strlen(m_text); i < len; ++i)
      m_homeRow -= m_termctrl.setChar(m_text[i]);
    if (m_inputPos == 0)
      m_termctrl.setCursorPos(m_homeCol, m_homeRow);
  } else {
    m_inputPos = 0;
  }
  m_state = 0;
}


void LineEditor::endInput()
{
  m_state = -1;
  if (m_text == nullptr) {
    m_text = (char*) malloc(1);
    m_text[0] = 0;
  }
}


void LineEditor::performCursorUp()
{
  onSpecialChar(LineEditorSpecialChar::CursorUp);
}


void LineEditor::performCursorDown()
{
  onSpecialChar(LineEditorSpecialChar::CursorDown);
}


void LineEditor::performCursorLeft()
{
  if (m_inputPos > 0) {
    int count = 1;
    if (m_termctrl.isVKDown(VK_LCTRL)) {
      // CTRL + Cursor Left => start of previous word
      while (m_inputPos - count > 0 && (m_text[m_inputPos - count] == ASCII_SPC || m_text[m_inputPos - count - 1] != ASCII_SPC))
        ++count;
    }
    m_termctrl.cursorLeft(count);
    m_inputPos -= count;
  }
}


void LineEditor::performCursorRight()
{
  if (m_inputPos < m_textLength) {
    int count = 1;
    if (m_termctrl.isVKDown(VK_LCTRL)) {
      // CTRL + Cursor Right => start of next word
      while (m_text[m_inputPos + count] && (m_text[m_inputPos + count] == ASCII_SPC || m_text[m_inputPos + count - 1] != ASCII_SPC))
        ++count;
    }
    m_termctrl.cursorRight(count);
    m_inputPos += count;
  }
}


void LineEditor::performCursorHome()
{
  m_termctrl.setCursorPos(m_homeCol, m_homeRow);
  m_inputPos = 0;
}


void LineEditor::performCursorEnd()
{
  m_termctrl.cursorRight(m_textLength - m_inputPos);
  m_inputPos = m_textLength;
}


void LineEditor::performDeleteRight()
{
  if (m_inputPos < m_textLength) {
    memmove(m_text + m_inputPos, m_text + m_inputPos + 1, m_textLength - m_inputPos);
    m_termctrl.multilineDeleteChar(m_textLength - m_inputPos - 1);
    --m_textLength;
  }
}


void LineEditor::performDeleteLeft()
{
  if (m_inputPos > 0) {
    m_termctrl.cursorLeft(1);
    m_termctrl.multilineDeleteChar(m_textLength - m_inputPos);
    memmove(m_text + m_inputPos - 1, m_text + m_inputPos, m_textLength - m_inputPos + 1);
    --m_inputPos;
    --m_textLength;
  }
}


char const * LineEditor::edit(int maxLength)
{

  // init?
  if (m_state == -1)
    beginInput();

  while (true) {

    int c;

    if (m_typeText) {
      c = m_typeText[m_typingIndex++];
      if (c == 0) {
        free(m_typeText);
        m_typeText = nullptr;
        continue;
      }
    } else {
      c = read();
    }

    onChar(&c);

    // timeout?
    if (c < 0)
      return nullptr;

    if (m_state == 1) {

      // ESC mode

      switch (c) {

        // "ESC [" => switch to CSI mode
        case '[':
          m_state = 31;
          break;

        default:
          m_state = 0;
          break;

      }

    } else if (m_state == 2) {

      // CTRL-Q mode

      switch (c) {

        // CTRL-Q S => WordStar Home
        case 'S':
          performCursorHome();
          break;

        // CTRL-Q D => WordStar End
        case 'D':
          performCursorEnd();
          break;

      }
      m_state = 0;

    } else if (m_state >= 31) {

      // CSI mode

      switch (c) {

        // "ESC [ A" : cursor Up
        case 'A':
          performCursorUp();
          m_state = 0;
          break;

        // "ESC [ B" : cursor Down
        case 'B':
          performCursorUp();
          m_state = 0;
          break;

        // "ESC [ D" : cursor Left
        case 'D':
          performCursorLeft();
          m_state = 0;
          break;

        // "ESC [ C" : cursor right
        case 'C':
          performCursorRight();
          m_state = 0;
          break;

        // '1'...'6' : special chars (PageUp, Insert, Home...)
        case '1' ... '6':
          // requires ending '~'
          m_state = c;
          break;

        // '~'
        case '~':
          switch (m_state) {

            // Home
            case '1':
              performCursorHome();
              break;

            // End
            case '4':
              performCursorEnd();
              break;

            // Delete
            case '3':
              performDeleteRight();
              break;

            // Insert
            case '2':
              m_insertMode = !m_insertMode;
              break;

          }
          m_state = 0;
          break;

        default:
          m_state = 0;
          break;

      }

    } else {

      // normal mode

      switch (c) {

        // ESC, switch to ESC mode
        case ASCII_ESC:
          m_state = 1;
          break;

        // CTRL-Q, switch to CTRL-Q mode
        case ASCII_CTRLQ:
          m_state = 2;
          break;

        // DEL, delete character at left
        case ASCII_DEL:
        case ASCII_BS:  // alias CTRL-H / backspace
          performDeleteLeft();
          break;

        // CTRL-G, delete character at right
        case ASCII_CTRLG:
          performDeleteRight();
          break;

        // CR, newline and return the inserted text
        case ASCII_CR:
        {
          int op = 0;
          onCarriageReturn(&op);
          if (op < 2) {
            m_termctrl.cursorRight(m_textLength - m_inputPos);
            if (op == 0) {
              write('\r');
              write('\n');
            }
            endInput();
            return m_text;
          } else
            break;
        }

        // CTRL-E, WordStar UP
        case ASCII_CTRLE:
          performCursorUp();
          break;

        // CTRL-X, WordStar DOWN
        case ASCII_CTRLX:
          performCursorDown();
          break;

        // CTRL-S, WordStar LEFT
        case ASCII_CTRLS:
          performCursorLeft();
          break;

        // CTRL-D, WordStar RIGHT
        case ASCII_CTRLD:
          performCursorRight();
          break;

        // insert printable chars
        case 32 ... 126:
        case 128 ... 255:
          // TODO: should we stop input when text length reach the full screen (minus home pos)?
          if (maxLength == 0 || m_inputPos < maxLength) {
            // update internal buffer
            if (m_insertMode || m_inputPos == m_textLength) {
              setLength(m_textLength + 1);
              memmove(m_text + m_inputPos + 1, m_text + m_inputPos, m_textLength - m_inputPos);
            }
            m_text[m_inputPos++] = c;
            // update terminal
            if (m_insertMode && m_inputPos < m_textLength) {
              if (m_termctrl.multilineInsertChar(m_textLength - m_inputPos))
                --m_homeRow;  // scrolled
            }
            if (m_termctrl.setChar(c))
              --m_homeRow;  // scrolled
          }
          break;

      }
    }

  }
}



} // end of namespace
