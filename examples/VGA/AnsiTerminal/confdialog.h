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

#include <Preferences.h>

#include "fabui.h"
#include "uistyle.h"
#include "progsdialog.h"
#include "restartdialog.h"


Preferences preferences;


#define TERMVERSION_MAJ 1
#define TERMVERSION_MIN 4


static const char * BAUDRATES_STR[] = { "110", "300", "600", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200", "128000", "230400", "250000", "256000", "500000", "1000000", "2000000" };
static const int    BAUDRATES_INT[] = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000, 230400, 250000, 256000, 500000, 1000000, 2000000 };
constexpr int       BAUDRATES_COUNT = sizeof(BAUDRATES_INT) / sizeof(int);

static const char * DATALENS_STR[]  = { "5 bits", "6 bits", "7 bits", "8 bits" };
static const char * PARITY_STR[]    = { "None", "Even", "Odd" };
static const char * STOPBITS_STR[]  = { "1 bit", "1.5 bits", "2 bits" };
static const char * FLOWCTRL_STR[]  = { "None", "Software" };

constexpr int RESOLUTION_DEFAULT           = 5;
static const char * RESOLUTIONS_STR[]      = { "1280x768, B&W",           // 0
                                               "1024x768, 4 Colors",      // 1
                                               "800x600, 8 Colors",       // 2
                                               "720x576, 16 Colors",      // 3
                                               "640x480 73Hz, 16 C.",     // 4
                                               "640x480 60Hz, 16 C.",     // 5
                                               "640x350, 64 Colors",      // 6
                                               "512x384, 64 Colors",      // 7
                                               "400x300, 64 Colors",      // 8
                                              };
static const char * RESOLUTIONS_CMDSTR[]   = { "1280x768x2",              // 0
                                               "1024x768x4",              // 1
                                               "800x600x8",               // 2
                                               "720x576x16",              // 3
                                               "640x480@73x16",           // 4
                                               "640x480@60x16",           // 5
                                               "640x350x64",              // 6
                                               "512x384x64",              // 7
                                               "400x300x64",              // 8
                                            };
enum class ResolutionController { VGAController, VGA16Controller, VGA8Controller, VGA2Controller, VGA4Controller };
static const ResolutionController RESOLUTIONS_CONTROLLER[] = { ResolutionController::VGA2Controller,     // 0
                                                               ResolutionController::VGA4Controller,     // 1
                                                               ResolutionController::VGA8Controller,     // 2
                                                               ResolutionController::VGA16Controller,    // 3
                                                               ResolutionController::VGA16Controller,    // 4
                                                               ResolutionController::VGA16Controller,    // 5
                                                               ResolutionController::VGAController,      // 6
                                                               ResolutionController::VGAController,      // 7
                                                               ResolutionController::VGAController,      // 8
                                                             };
static const char * RESOLUTIONS_MODELINE[] = { SVGA_1280x768_50Hz,        // 0
                                               SVGA_1024x768_75Hz,        // 1
                                               SVGA_800x600_56Hz,         // 2
                                               PAL_720x576_50Hz,          // 3
                                               VGA_640x480_73Hz,          // 4
                                               VGA_640x480_60Hz,          // 5
                                               VGA_640x350_70HzAlt1,      // 6
                                               VGA_512x384_60Hz,          // 7
                                               VGA_400x300_60Hz,          // 8
};
constexpr int RESOLUTIONS_COUNT            = sizeof(RESOLUTIONS_STR) / sizeof(char const *);

static const char * FONTS_STR[]            = { "Auto", "VGA 4x6", "VGA 5x7", "VGA 5x8", "VGA 6x8", "VGA 6x9", "VGA 6x10", "VGA 6x12", "VGA 6x13",
                                                "VGA 7x13", "VGA 7x14", "VGA 8x8", "VGA 8x9", "VGA 8x13", "VGA 8x14", "VGA 8x16", "VGA 8x19", "VGA 9x15",
                                                "VGA 9x18", "VGA 10x20", "BigSerif 8x14", "BigSerif 8x16", "Block 8x14", "Broadway 8x14",
                                                "Computer 8x14", "Courier 8x14", "LCD 8x14", "Old English 8x16", "Sans Serif 8x14", "Sans Serif 8x16",
                                                "Slant 8x14", "Wiggly 8x16" };
static const fabgl::FontInfo * FONTS_INFO[] = { nullptr, &fabgl::FONT_4x6, &fabgl::FONT_5x7, &fabgl::FONT_5x8, &fabgl::FONT_6x8, &fabgl::FONT_6x9,
                                               &fabgl::FONT_6x10, &fabgl::FONT_6x12, &fabgl::FONT_6x13, &fabgl::FONT_7x13, &fabgl::FONT_7x14, &fabgl::FONT_8x8,
                                               &fabgl::FONT_8x9, &fabgl::FONT_8x13, &fabgl::FONT_8x14, &fabgl::FONT_8x16, &fabgl::FONT_8x19, &fabgl::FONT_9x15,
                                               &fabgl::FONT_9x18, &fabgl::FONT_10x20, &fabgl::FONT_BIGSERIF_8x14, &fabgl::FONT_BIGSERIF_8x16, &fabgl::FONT_BLOCK_8x14,
                                               &fabgl::FONT_BROADWAY_8x14, &fabgl::FONT_COMPUTER_8x14, &fabgl::FONT_COURIER_8x14,
                                               &fabgl::FONT_LCD_8x14, &fabgl::FONT_OLDENGL_8x16, &fabgl::FONT_SANSERIF_8x14, &fabgl::FONT_SANSERIF_8x16,
                                               &fabgl::FONT_SLANT_8x14, &fabgl::FONT_WIGGLY_8x16 };
constexpr int       FONTS_COUNT             = sizeof(FONTS_STR) / sizeof(char const *);

static const char * COLUMNS_STR[] = { "Max", "80", "132" };
static const int    COLUMNS_INT[] = { 0, 80, 132 };
constexpr int       COLUMNS_COUNT = sizeof(COLUMNS_STR) / sizeof(char const *);

static const char * ROWS_STR[]    = { "Max", "24", "25" };
static const int    ROWS_INT[]    = { 0, 24, 25 };
constexpr int       ROWS_COUNT    = sizeof(ROWS_STR) / sizeof(char const *);

constexpr int       BOOTINFO_DISABLED     = 0;
constexpr int       BOOTINFO_ENABLED      = 1;
constexpr int       BOOTINFO_TEMPDISABLED = 2;

constexpr int       SERCTL_DISABLED     = 0;
constexpr int       SERCTL_ENABLED      = 1;


struct ConfDialogApp : public uiApp {

  Rect              frameRect;
  int               progToInstall;

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
  uiComboBox *      resolutionComboBox;
  uiComboBox *      fontComboBox;
  uiComboBox *      columnsComboBox;
  uiComboBox *      rowsComboBox;
  uiCheckBox *      infoCheckBox;
  uiCheckBox *      serctlCheckBox;

  void init() {

    setStyle(&dialogStyle);

    rootWindow()->frameProps().fillBackground = false;

    frame = new uiFrame(rootWindow(), "Terminal Configuration", UIWINDOW_PARENTCENTER, Size(380, 275), true, STYLE_FRAME);
    frameRect = frame->rect(fabgl::uiOrigin::Screen);

    frame->frameProps().resizeable        = false;
    frame->frameProps().moveable          = false;
    frame->frameProps().hasCloseButton    = false;
    frame->frameProps().hasMaximizeButton = false;
    frame->frameProps().hasMinimizeButton = false;

    progToInstall = -1;

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

    int y = 19;

    // little help
    new uiLabel(frame, "Press TAB key to move between fields", Point(100, y), Size(0, 0), true, STYLE_LABELHELP);
    new uiLabel(frame, "Outside this dialog press CTRL-ALT-F12 to reset settings", Point(52, y + 12), Size(0, 0), true, STYLE_LABELHELP);


    y += 34;

    // select terminal emulation combobox
    new uiLabel(frame, "Terminal Type", Point(10,  y), Size(0, 0), true, STYLE_LABEL);
    termComboBox = new uiComboBox(frame, Point(10, y + 12), Size(85, 20), 80, true, STYLE_COMBOBOX);
    termComboBox->items().append(SupportedTerminals::names(), SupportedTerminals::count());
    termComboBox->selectItem((int)getTermType());

    // select keyboard layout
    new uiLabel(frame, "Keyboard Layout", Point(110, y), Size(0, 0), true, STYLE_LABEL);
    kbdComboBox = new uiComboBox(frame, Point(110, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    kbdComboBox->items().append(SupportedLayouts::names(), SupportedLayouts::count());
    kbdComboBox->selectItem(getKbdLayoutIndex());

    // background color
    new uiLabel(frame, "Background Color", Point(200,  y), Size(0, 0), true, STYLE_LABEL);
    bgColorComboBox = new uiColorComboBox(frame, Point(200, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    bgColorComboBox->selectColor(getBGColor());

    // foreground color
    new uiLabel(frame, "Foreground Color", Point(290,  y), Size(0, 0), true, STYLE_LABEL);
    fgColorComboBox = new uiColorComboBox(frame, Point(290, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    fgColorComboBox->selectColor(getFGColor());


    y += 48;

    // baud rate
    new uiLabel(frame, "Baud Rate", Point(10,  y), Size(0, 0), true, STYLE_LABEL);
    baudRateComboBox = new uiComboBox(frame, Point(10, y + 12), Size(70, 20), 70, true, STYLE_COMBOBOX);
    baudRateComboBox->items().append(BAUDRATES_STR, BAUDRATES_COUNT);
    baudRateComboBox->selectItem(getBaudRateIndex());

    // data length
    new uiLabel(frame, "Data Length", Point(95,  y), Size(0, 0), true, STYLE_LABEL);
    datalenComboBox = new uiComboBox(frame, Point(95, y + 12), Size(60, 20), 70, true, STYLE_COMBOBOX);
    datalenComboBox->items().append(DATALENS_STR, 4);
    datalenComboBox->selectItem(getDataLenIndex());

    // parity
    new uiLabel(frame, "Parity", Point(170,  y), Size(0, 0), true, STYLE_LABEL);
    parityComboBox = new uiComboBox(frame, Point(170, y + 12), Size(45, 20), 50, true, STYLE_COMBOBOX);
    parityComboBox->items().append(PARITY_STR, 3);
    parityComboBox->selectItem(getParityIndex());

    // stop bits
    new uiLabel(frame, "Stop Bits", Point(230,  y), Size(0, 0), true, STYLE_LABEL);
    stopBitsComboBox = new uiComboBox(frame, Point(230, y + 12), Size(55, 20), 50, true, STYLE_COMBOBOX);
    stopBitsComboBox->items().append(STOPBITS_STR, 3);
    stopBitsComboBox->selectItem(getStopBitsIndex() - 1);

    // flow control
    new uiLabel(frame, "Flow Control", Point(300,  y), Size(0, 0), true, STYLE_LABEL);
    flowCtrlComboBox = new uiComboBox(frame, Point(300, y + 12), Size(65, 20), 35, true, STYLE_COMBOBOX);
    flowCtrlComboBox->items().append(FLOWCTRL_STR, 2);
    flowCtrlComboBox->selectItem((int)getFlowCtrl());


    y += 48;

    // resolution
    new uiLabel(frame, "Resolution", Point(10, y), Size(0, 0), true, STYLE_LABEL);
    resolutionComboBox = new uiComboBox(frame, Point(10, y + 12), Size(119, 20), 53, true, STYLE_COMBOBOX);
    resolutionComboBox->items().append(RESOLUTIONS_STR, RESOLUTIONS_COUNT);
    resolutionComboBox->selectItem(getResolutionIndex());

    // font
    new uiLabel(frame, "Font", Point(144,  y), Size(0, 0), true, STYLE_LABEL);
    fontComboBox = new uiComboBox(frame, Point(144, y + 12), Size(110, 20), 70, true, STYLE_COMBOBOX);
    fontComboBox->items().append(FONTS_STR, FONTS_COUNT);
    fontComboBox->selectItem(getFontIndex());

    // columns
    new uiLabel(frame, "Columns", Point(269,  y), Size(0, 0), true, STYLE_LABEL);
    columnsComboBox = new uiComboBox(frame, Point(269, y + 12), Size(40, 20), 50, true, STYLE_COMBOBOX);
    columnsComboBox->items().append(COLUMNS_STR, COLUMNS_COUNT);
    columnsComboBox->selectItem(getColumnsIndex());

    // rows
    new uiLabel(frame, "Rows", Point(325,  y), Size(0, 0), true, STYLE_LABEL);
    rowsComboBox = new uiComboBox(frame, Point(324, y + 12), Size(40, 20), 50, true, STYLE_COMBOBOX);
    rowsComboBox->items().append(ROWS_STR, ROWS_COUNT);
    rowsComboBox->selectItem(getRowsIndex());


    y += 48;

    // show boot info
    new uiLabel(frame, "Show Boot Info", Point(10, y), Size(0, 0), true, STYLE_LABEL);
    infoCheckBox = new uiCheckBox(frame, Point(80, y - 2), Size(16, 16), uiCheckBoxKind::CheckBox, true, STYLE_CHECKBOX);
    infoCheckBox->setChecked(getBootInfo() == BOOTINFO_ENABLED);

    y += 24;

    // set control to usb-serial
    new uiLabel(frame, "USBSerial", Point(10, y), Size(0, 0), true, STYLE_LABEL);
    serctlCheckBox = new uiCheckBox(frame, Point(80, y - 2), Size(16, 16), uiCheckBoxKind::CheckBox, true, STYLE_CHECKBOX);
    serctlCheckBox->setChecked(getSerCtl() == SERCTL_ENABLED);

    y += 24;

    // exit without save button
    auto exitNoSaveButton = new uiButton(frame, "Quit [ESC]", Point(10, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    exitNoSaveButton->onClick = [&]() {
      quit(0);
    };

    // exit with save button
    auto exitSaveButton = new uiButton(frame, "Save & Quit [F10]", Point(110, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    exitSaveButton->onClick = [&]() {
      saveProps();
      quit(0);
    };

    // install a program
    auto installButton = new uiButton(frame, "Install Programs", Point(278, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    installButton->onClick = [&]() {
      progToInstall = -1;
      auto progsDialog = new ProgsDialog(rootWindow());
      if (showModalWindow(progsDialog) == 1) {
        progToInstall = progsDialog->progComboBox->selectedItem();
        quit(0);
      }
      destroyWindow(progsDialog);
    };


    setActiveWindow(frame);
    setFocusedWindow(exitNoSaveButton);

  }


  void saveProps() {
    // need reboot?
    bool reboot = resolutionComboBox->selectedItem() != getResolutionIndex() ||
                  fontComboBox->selectedItem()       != getFontIndex()       ||
                  columnsComboBox->selectedItem()    != getColumnsIndex()    ||
                  rowsComboBox->selectedItem()       != getRowsIndex()       ||
                  bgColorComboBox->selectedColor()   != getBGColor();

    preferences.putInt("TermType", termComboBox->selectedItem());
    preferences.putInt("KbdLayout", kbdComboBox->selectedItem());
    preferences.putInt("BaudRate", baudRateComboBox->selectedItem());
    preferences.putInt("DataLen", datalenComboBox->selectedItem());
    preferences.putInt("Parity", parityComboBox->selectedItem());
    preferences.putInt("StopBits", stopBitsComboBox->selectedItem() + 1);
    preferences.putInt("FlowCtrl", flowCtrlComboBox->selectedItem());
    preferences.putInt("BGColor", (int)bgColorComboBox->selectedColor());
    preferences.putInt("FGColor", (int)fgColorComboBox->selectedColor());
    preferences.putInt("Resolution", resolutionComboBox->selectedItem());
    preferences.putInt("Font", fontComboBox->selectedItem());
    preferences.putInt("Columns", columnsComboBox->selectedItem());
    preferences.putInt("Rows", rowsComboBox->selectedItem());
    preferences.putInt("BootInfo", infoCheckBox->checked() ? BOOTINFO_ENABLED : BOOTINFO_DISABLED);
    preferences.putInt("SerCtl", serctlCheckBox->checked() ? SERCTL_ENABLED : SERCTL_DISABLED);

    if (reboot) {
      auto rebootDialog = new RebootDialog(frame);
      showModalWindow(rebootDialog);  // no return!
    }

    loadConfiguration();
  }


  ~ConfDialogApp() {
    // this is required, becasue the terminal may not cover the entire screen
    canvas()->reset();
    canvas()->setBrushColor(getBGColor());
    canvas()->fillRectangle(frameRect);
  }


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

  static int getResolutionIndex() {
    return preferences.getInt("Resolution", RESOLUTION_DEFAULT);
  }

  static int getTempResolutionIndex() {
    return preferences.getInt("TempResolution", -1);         // default -1 = no temp resolution
  }

  static int getFontIndex() {
    return preferences.getInt("Font", 0);                   // default 0 = auto
  }

  static int getColumnsIndex() {
    return preferences.getInt("Columns", 0);                // default 0 = MAX
  }

  static int getRowsIndex() {
    return preferences.getInt("Rows", 0);                   // default 0 = MAX
  }

  static int getBootInfo() {
    return preferences.getInt("BootInfo", BOOTINFO_ENABLED);
  }

  static int getSerCtl() {
    return preferences.getInt("SerCtl", SERCTL_DISABLED);
  }

  static char const * getSerParamStr() {
    static char outstr[13];
    snprintf(outstr, sizeof(outstr), "%s,%c%c%c", BAUDRATES_STR[getBaudRateIndex()],
                                                  DATALENS_STR[getDataLenIndex()][0],
                                                  PARITY_STR[getParityIndex()][0],
                                                  STOPBITS_STR[getStopBitsIndex() - 1][0]);
    return outstr;
  }
  
  // if version in preferences doesn't match, reset preferences
  static void checkVersion() {
    if (preferences.getInt("VerMaj", 0) != TERMVERSION_MAJ || preferences.getInt("VerMin", 0) != TERMVERSION_MIN) {
      preferences.clear();
      preferences.putInt("VerMaj", TERMVERSION_MAJ);
      preferences.putInt("VerMin", TERMVERSION_MIN);
    }
  }

  static void setupDisplay() {
    // setup display controller
    auto res = getTempResolutionIndex();
    if (res == -1)
      res = getResolutionIndex();
    else
      preferences.putInt("TempResolution", -1);
    switch (RESOLUTIONS_CONTROLLER[res]) {
      case ResolutionController::VGAController:
        DisplayController = new fabgl::VGAController;
        break;
      case ResolutionController::VGA16Controller:
        DisplayController = new fabgl::VGA16Controller;
        break;
      case ResolutionController::VGA2Controller:
        DisplayController = new fabgl::VGA2Controller;
        break;
      case ResolutionController::VGA4Controller:
        DisplayController = new fabgl::VGA4Controller;
        break;
      case ResolutionController::VGA8Controller:
        DisplayController = new fabgl::VGA8Controller;
        break;
    }
    DisplayController->begin();
    DisplayController->setResolution(RESOLUTIONS_MODELINE[res]);

    // setup terminal
    auto cols = COLUMNS_INT[getColumnsIndex()];
    auto rows = ROWS_INT[getRowsIndex()];
    Terminal.begin(DisplayController, (cols ? cols : -1), (rows ? rows : -1));
    // this is required when terminal columns and rows do not cover entire screen
    Terminal.canvas()->setBrushColor(getBGColor());
    Terminal.canvas()->clear();
    if (getFontIndex() == 0) {
      // auto select a font from specified Columns and Rows or from 80x25
      Terminal.loadFont(fabgl::getPresetFontInfo(Terminal.canvas()->getWidth(), Terminal.canvas()->getHeight(), (cols ? cols : 80), (rows ? rows : 25)));
    } else {
      // load specified font
      Terminal.loadFont(FONTS_INFO[getFontIndex()]);
    }
  }

  static void loadConfiguration() {
    Terminal.setTerminalType(getTermType());
    Terminal.keyboard()->setLayout(SupportedLayouts::layouts()[getKbdLayoutIndex()]);
    Terminal.setBackgroundColor(getBGColor());
    Terminal.setForegroundColor(getFGColor());
    // configure serial port
    bool serctl = (getSerCtl() == SERCTL_ENABLED);
    auto rxPin = serctl ? UART_URX : UART_SRX;
    auto txPin = serctl ? UART_UTX : UART_STX;
    Terminal.connectSerialPort(BAUDRATES_INT[getBaudRateIndex()], fabgl::UARTConf(getParityIndex(), getDataLenIndex(), getStopBitsIndex()), rxPin, txPin, getFlowCtrl());
  }

};
