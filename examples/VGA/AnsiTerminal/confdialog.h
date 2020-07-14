#pragma once

#include <Preferences.h>

#include "fabui.h"


Preferences preferences;



static const char * BAUDRATES_STR[] = { "110", "300", "600", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200", "128000", "230400", "250000", "256000", "500000", "1000000", "2000000" };
static const int    BAUDRATES_INT[] = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000, 230400, 250000, 256000, 500000, 1000000, 2000000 };
constexpr int       BAUDRATES_COUNT = sizeof(BAUDRATES_INT) / sizeof(int);

static const char * DATALENS_STR[]  = { "5 bits", "6 bits", "7 bits", "8 bits" };
static const char * PARITY_STR[]    = { "None", "Even", "Odd" };
static const char * STOPBITS_STR[]  = { "1 bit", "1.5 bits", "2 bits" };
static const char * FLOWCTRL_STR[]  = { "None", "Software" };



#define STYLE_FRAME_ID     1
#define STYLE_LABEL_ID     2
#define STYLE_LABELHELP_ID 3
#define STYLE_BUTTON_ID    4
#define STYLE_COMBOBOX     5

#define BACKGROUND_COLOR RGB888(64, 64, 64)
#define BUTTONS_BACKGROUND_COLOR RGB888(128, 128, 64);


struct ConfDialogStyle : uiStyle {
  void setStyle(uiObject * object, uint32_t styleClassID) {
    switch (styleClassID) {
      case STYLE_FRAME_ID:
        ((uiFrame*)object)->windowStyle().activeBorderColor         = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().activeTitleBackgroundColor = RGB888(128, 128, 255);
        ((uiFrame*)object)->frameStyle().backgroundColor            = BACKGROUND_COLOR;
        break;
      case STYLE_LABEL_ID:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_12;
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 255);
        break;
      case STYLE_LABELHELP_ID:
        ((uiLabel*)object)->labelStyle().textFont                   = &fabgl::FONT_std_14;
        ((uiLabel*)object)->labelStyle().backgroundColor            = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                  = RGB888(255, 255, 0);
        break;
      case STYLE_BUTTON_ID:
        ((uiButton*)object)->windowStyle().borderColor              = RGB888(0, 0, 0);
        ((uiButton*)object)->buttonStyle().backgroundColor          = BUTTONS_BACKGROUND_COLOR;
        break;
      case STYLE_COMBOBOX:
        ((uiFrame*)object)->windowStyle().borderColor               = RGB888(255, 255, 255);
        ((uiFrame*)object)->windowStyle().borderSize                = 1;
        break;
    }
  }
} confDialogStyle;



class ConfDialogApp : public uiApp {

  uiFrame *         frame;
  uiComboBox *      termComboBox;
  uiComboBox *      kbdComboBox;
  uiComboBox *      baudRateComboBox;
  uiComboBox *      datalenComboBox;
  uiComboBox *      parityComboBox;
  uiComboBox *      stopBitsComboBox;
  uiComboBox *      flowCtrlComboBox;
  uiColorComboBox * bgColorComboBox;
  uiColorComboBox * fgColorComboBox;

  void init() {

    setStyle(&confDialogStyle);

    rootWindow()->frameProps().fillBackground = false;

    frame = new uiFrame(rootWindow(), "Terminal Configuration", Point(110, 60), Size(380, 220), true, STYLE_FRAME_ID);

    // ESC : exit without save
    // F10 : save and exit
    frame->onKeyUp = [&](uiKeyEventInfo key) {
      if (key.VK == VirtualKey::VK_ESCAPE)
        quit(0);
      if (key.VK == VirtualKey::VK_F10) {
        saveProps();
        quit(0);
      }
    };

    int y = 24;

    // little help
    new uiLabel(frame, "Use the TAB key to move between fields", Point(90, y), Size(0, 0), true, STYLE_LABELHELP_ID);


    y += 30;

    // select terminal emulation combobox
    new uiLabel(frame, "Terminal Type", Point(10,  y), Size(0, 0), true, STYLE_LABEL_ID);
    termComboBox = new uiComboBox(frame, Point(10, y + 12), Size(85, 20), 80, true, STYLE_COMBOBOX);
    termComboBox->items().append(SupportedTerminals::names(), SupportedTerminals::count());
    termComboBox->selectItem((int)getTermType());

    // select keyboard layout
    new uiLabel(frame, "Keyboard Layout", Point(110, y), Size(0, 0), true, STYLE_LABEL_ID);
    kbdComboBox = new uiComboBox(frame, Point(110, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    kbdComboBox->items().append(SupportedLayouts::names(), SupportedLayouts::count());
    kbdComboBox->selectItem(getKbdLayoutIndex());

    // background color
    new uiLabel(frame, "Background Color", Point(200,  y), Size(0, 0), true, STYLE_LABEL_ID);
    bgColorComboBox = new uiColorComboBox(frame, Point(200, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    bgColorComboBox->selectColor(getBGColor());

    // foreground color
    new uiLabel(frame, "Foreground Color", Point(290,  y), Size(0, 0), true, STYLE_LABEL_ID);
    fgColorComboBox = new uiColorComboBox(frame, Point(290, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    fgColorComboBox->selectColor(getFGColor());


    y += 55;

    // baud rate
    new uiLabel(frame, "Baud Rate", Point(10,  y), Size(0, 0), true, STYLE_LABEL_ID);
    baudRateComboBox = new uiComboBox(frame, Point(10, y + 12), Size(70, 20), 70, true, STYLE_COMBOBOX);
    baudRateComboBox->items().append(BAUDRATES_STR, BAUDRATES_COUNT);
    baudRateComboBox->selectItem(getBaudRateIndex());

    // data length
    new uiLabel(frame, "Data Length", Point(95,  y), Size(0, 0), true, STYLE_LABEL_ID);
    datalenComboBox = new uiComboBox(frame, Point(95, y + 12), Size(60, 20), 70, true, STYLE_COMBOBOX);
    datalenComboBox->items().append(DATALENS_STR, 4);
    datalenComboBox->selectItem(getDataLenIndex());

    // parity
    new uiLabel(frame, "Parity", Point(170,  y), Size(0, 0), true, STYLE_LABEL_ID);
    parityComboBox = new uiComboBox(frame, Point(170, y + 12), Size(45, 20), 50, true, STYLE_COMBOBOX);
    parityComboBox->items().append(PARITY_STR, 3);
    parityComboBox->selectItem(getParityIndex());

    // stop bits
    new uiLabel(frame, "Stop Bits", Point(230,  y), Size(0, 0), true, STYLE_LABEL_ID);
    stopBitsComboBox = new uiComboBox(frame, Point(230, y + 12), Size(55, 20), 50, true, STYLE_COMBOBOX);
    stopBitsComboBox->items().append(STOPBITS_STR, 3);
    stopBitsComboBox->selectItem(getStopBitsIndex() - 1);

    // flow control
    new uiLabel(frame, "Flow Control", Point(300,  y), Size(0, 0), true, STYLE_LABEL_ID);
    flowCtrlComboBox = new uiComboBox(frame, Point(300, y + 12), Size(65, 20), 35, true, STYLE_COMBOBOX);
    flowCtrlComboBox->items().append(FLOWCTRL_STR, 2);
    flowCtrlComboBox->selectItem((int)getFlowCtrl());


    y += 70;

    // exit without save button
    auto exitNoSaveButton = new uiButton(frame, "Quit [ESC]", Point(90, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON_ID);
    exitNoSaveButton->onClick = [&]() {
      quit(0);
    };

    // exit with save button
    auto exitSaveButton = new uiButton(frame, "Save & Quit [F10]", Point(190, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON_ID);
    exitSaveButton->onClick = [&]() {
      saveProps();
      quit(0);
    };


    setActiveWindow(frame);
    setFocusedWindow(exitNoSaveButton);

  }


  void saveProps() {
    preferences.putInt("TermType", termComboBox->selectedItem());
    preferences.putInt("KbdLayout", kbdComboBox->selectedItem());
    preferences.putInt("BaudRate", baudRateComboBox->selectedItem());
    preferences.putInt("DataLen", datalenComboBox->selectedItem());
    preferences.putInt("Parity", parityComboBox->selectedItem());
    preferences.putInt("StopBits", parityComboBox->selectedItem() + 1);
    preferences.putInt("FlowCtrl", flowCtrlComboBox->selectedItem());
    preferences.putInt("BGColor", (int)bgColorComboBox->selectedColor());
    preferences.putInt("FGColor", (int)fgColorComboBox->selectedColor());
    loadConfiguration();
  }


public:

  static TermType getTermType() {
    return (TermType) preferences.getInt("TermType", 7);    // default 7 = ANSILegacy
  }

  static int getKbdLayoutIndex() {
    return preferences.getInt("KbdLayout", 3);              // default 3 = "US"
  }

  static int getBaudRateIndex() {
    return preferences.getInt("BaudRate", 11);              // default 11 = 115200
  }

  static int getDataLenIndex() {
    return preferences.getInt("DataLen", 3);                // default 3 = 8 bits
  }

  static int getParityIndex() {
    return preferences.getInt("Parity", 0);                 // default 0 = no parity
  }

  static int getStopBitsIndex() {
    return preferences.getInt("StopBits", 1);               // default 1 = 1 stop bit
  }

  static FlowControl getFlowCtrl() {
    return (FlowControl) preferences.getInt("FlowCtrl", 0); // default 0 = no flow control
  }

  static Color getBGColor() {
    return (Color) preferences.getInt("BGColor", (int)Color::Black);
  }

  static Color getFGColor() {
    return (Color) preferences.getInt("FGColor", (int)Color::BrightGreen);
  }

  static void loadConfiguration() {
    Terminal.setTerminalType(getTermType());
    Terminal.keyboard()->setLayout(SupportedLayouts::layouts()[getKbdLayoutIndex()]);
    Terminal.connectSerialPort(BAUDRATES_INT[getBaudRateIndex()], fabgl::UARTConf(getParityIndex(), getDataLenIndex(), getStopBitsIndex()), UART_RX, UART_TX, getFlowCtrl());
    Terminal.setBackgroundColor(getBGColor());
    Terminal.setForegroundColor(getFGColor());
  }

};
