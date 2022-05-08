/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
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


#pragma once

#include <Preferences.h>

#include "fabui.h"
#include "uistyle.h"
#include "progsdialog.h"
#include "restartdialog.h"


Preferences preferences;


#define TERMVERSION_MAJ 1
#define TERMVERSION_MIN 5


static const char * BAUDRATES_STR[]  = { "110", "300", "600", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200", "128000", "230400", "250000", "256000", "500000", "1000000", "2000000" };
static const int    BAUDRATES_INT[]  = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000, 230400, 250000, 256000, 500000, 1000000, 2000000 };
constexpr int       BAUDRATES_COUNT  = sizeof(BAUDRATES_INT) / sizeof(int);

static const char * DATALENS_STR[]   = { "5 bits", "6 bits", "7 bits", "8 bits" };
static const int    DATALENS_INT[]   = { 5, 6, 7, 8 };

static const char * PARITY_STR[]     = { "None", "Even", "Odd" };
static const char   PARITY_CHAR[]    = { 'N', 'E', 'O' };

static const char * STOPBITS_STR[]   = { "1", "1.5", "2", "3" };
static const float  STOPBITS_FLOAT[] = { 1.0, 1.5, 2.0, 3.0 };

static const char * FLOWCTRL_STR[]   = { "None", "XON/XOFF", "RTS/CTS", "Combi" };

constexpr int RESOLUTION_DEFAULT           = 5;
static const char * RESOLUTIONS_STR[]      = { "1280x768, B&W",           // 0
                                               "1024x720, 4 Colors",      // 1
                                               "800x600, 8 Colors",       // 2
                                               "720x520, 16 Colors",      // 3
                                               "640x480 73Hz, 16 C.",     // 4
                                               "640x480 60Hz, 16 C.",     // 5
                                               "640x350, 16 Colors",      // 6
                                               "512x384, 64 Colors",      // 7
                                               "400x300, 64 Colors",      // 8
                                              };
static const char * RESOLUTIONS_CMDSTR[]   = { "1280x768x2",              // 0
                                               "1024x720x4",              // 1
                                               "800x600x8",               // 2
                                               "720x520x16",              // 3
                                               "640x480@73x16",           // 4
                                               "640x480@60x16",           // 5
                                               "640x350x16",              // 6
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
                                                               ResolutionController::VGA16Controller,    // 6
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
static const int16_t RESOLUTIONS_HEIGHT[] = {   -1,        // 0
                                               720,        // 1
                                                -1,        // 2
                                               520,        // 3
                                                -1,        // 4
                                                -1,        // 5
                                                -1,        // 6
                                                -1,        // 7
                                                -1,        // 8
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

// preferences keys
static const char * PREF_VERMAJ         = "VerMaj";
static const char * PREF_VERMIN         = "VerMin";
static const char * PREF_TERMTYPE       = "TermType";
static const char * PREF_KBDLAYOUT      = "KbdLayout";
static const char * PREF_BAUDRATE       = "BaudRate";
static const char * PREF_DATALEN        = "DataLen";
static const char * PREF_PARITY         = "Parity";
static const char * PREF_STOPBITS       = "StopBits";
static const char * PREF_FLOWCTRL       = "FlowCtrl";
static const char * PREF_BGCOLOR        = "BGColor";
static const char * PREF_FGCOLOR        = "FGColor";
static const char * PREF_RESOLUTION     = "Resolution";
static const char * PREF_FONT           = "Font";
static const char * PREF_COLUMNS        = "Columns";
static const char * PREF_ROWS           = "Rows";
static const char * PREF_BOOTINFO       = "BootInfo";
static const char * PREF_SERCTL         = "SerCtl";
static const char * PREF_TEMPRESOLUTION = "TempResolution";


struct ConfDialogApp : public uiApp {

  Rect              frameRect;
  int               progToInstall;
  bool              lastCTSStatus;

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
  uiLabel *         RTSStatus;
  uiLabel *         CTSStatus;


  void init() {

    setStyle(&dialogStyle);

    rootWindow()->frameProps().fillBackground = false;

    frame = new uiFrame(rootWindow(), "Terminal Configuration", UIWINDOW_PARENTCENTER, Size(380, 287), true, STYLE_FRAME);
    frameRect = frame->rect(fabgl::uiOrigin::Screen);

    frame->frameProps().resizeable        = false;
    frame->frameProps().moveable          = false;
    frame->frameProps().hasCloseButton    = false;
    frame->frameProps().hasMaximizeButton = false;
    frame->frameProps().hasMinimizeButton = false;

    progToInstall = -1;

    // ESC        : exit without save
    // CTRL + ESC : reboot
    // F10        : save and exit
    frame->onKeyUp = [&](uiKeyEventInfo const & key) {
      if (key.VK == VirtualKey::VK_ESCAPE) {
        if (key.CTRL)
          performReboot();  // no return from here!
        quit(0);
      }
      if (key.VK == VirtualKey::VK_F10) {
        saveProps();
        quit(0);
      }
    };

    int y = 19;

    // little help
    new uiStaticLabel(frame, "FabGL Terminal - (c) 2019-2022 by Fabrizio Di Vittorio - fabgl.com", Point(30, y), true, STYLE_LABELHELP2);
    new uiStaticLabel(frame, "Press TAB key to move between fields", Point(100, y + 12), true, STYLE_LABELHELP);
    new uiStaticLabel(frame, "Outside this dialog press CTRL-ALT-F12 to reset settings", Point(52, y + 24), true, STYLE_LABELHELP);


    y += 46;

    // select terminal emulation combobox
    new uiStaticLabel(frame, "Terminal Type", Point(10,  y), true, STYLE_STATICLABEL);
    termComboBox = new uiComboBox(frame, Point(10, y + 12), Size(85, 20), 80, true, STYLE_COMBOBOX);
    termComboBox->items().append(SupportedTerminals::names(), SupportedTerminals::count());
    termComboBox->selectItem((int)getTermType());

    // select keyboard layout
    new uiStaticLabel(frame, "Keyboard Layout", Point(110, y), true, STYLE_STATICLABEL);
    kbdComboBox = new uiComboBox(frame, Point(110, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    kbdComboBox->items().append(SupportedLayouts::names(), SupportedLayouts::count());
    kbdComboBox->selectItem(getKbdLayoutIndex());

    // background color
    new uiStaticLabel(frame, "Background Color", Point(200,  y), true, STYLE_STATICLABEL);
    bgColorComboBox = new uiColorComboBox(frame, Point(200, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    bgColorComboBox->selectColor(getBGColor());

    // foreground color
    new uiStaticLabel(frame, "Foreground Color", Point(290,  y), true, STYLE_STATICLABEL);
    fgColorComboBox = new uiColorComboBox(frame, Point(290, y + 12), Size(75, 20), 70, true, STYLE_COMBOBOX);
    fgColorComboBox->selectColor(getFGColor());


    y += 48;

    // baud rate
    new uiStaticLabel(frame, "Baud Rate", Point(10,  y), true, STYLE_STATICLABEL);
    baudRateComboBox = new uiComboBox(frame, Point(10, y + 12), Size(70, 20), 70, true, STYLE_COMBOBOX);
    baudRateComboBox->items().append(BAUDRATES_STR, BAUDRATES_COUNT);
    baudRateComboBox->selectItem(getBaudRateIndex());

    // data length
    new uiStaticLabel(frame, "Data Length", Point(95,  y), true, STYLE_STATICLABEL);
    datalenComboBox = new uiComboBox(frame, Point(95, y + 12), Size(60, 20), 70, true, STYLE_COMBOBOX);
    datalenComboBox->items().append(DATALENS_STR, 4);
    datalenComboBox->selectItem(getDataLenIndex());

    // parity
    new uiStaticLabel(frame, "Parity", Point(170,  y), true, STYLE_STATICLABEL);
    parityComboBox = new uiComboBox(frame, Point(170, y + 12), Size(45, 20), 50, true, STYLE_COMBOBOX);
    parityComboBox->items().append(PARITY_STR, 3);
    parityComboBox->selectItem(getParityIndex());

    // stop bits
    new uiStaticLabel(frame, "Stop Bits", Point(230,  y), true, STYLE_STATICLABEL);
    stopBitsComboBox = new uiComboBox(frame, Point(230, y + 12), Size(55, 20), 50, true, STYLE_COMBOBOX);
    stopBitsComboBox->items().append(STOPBITS_STR, 4);
    stopBitsComboBox->selectItem(getStopBitsIndex());

    // flow control
    new uiStaticLabel(frame, "Flow Control", Point(300,  y), true, STYLE_STATICLABEL);
    flowCtrlComboBox = new uiComboBox(frame, Point(300, y + 12), Size(65, 20), 35, true, STYLE_COMBOBOX);
    flowCtrlComboBox->items().append(FLOWCTRL_STR, 4);
    flowCtrlComboBox->selectItem((int)getFlowCtrl());


    y += 48;

    // resolution
    new uiStaticLabel(frame, "Resolution", Point(10, y), true, STYLE_STATICLABEL);
    resolutionComboBox = new uiComboBox(frame, Point(10, y + 12), Size(119, 20), 53, true, STYLE_COMBOBOX);
    resolutionComboBox->items().append(RESOLUTIONS_STR, RESOLUTIONS_COUNT);
    resolutionComboBox->selectItem(getResolutionIndex());

    // font
    new uiStaticLabel(frame, "Font", Point(144,  y), true, STYLE_STATICLABEL);
    fontComboBox = new uiComboBox(frame, Point(144, y + 12), Size(110, 20), 70, true, STYLE_COMBOBOX);
    fontComboBox->items().append(FONTS_STR, FONTS_COUNT);
    fontComboBox->selectItem(getFontIndex());

    // columns
    new uiStaticLabel(frame, "Columns", Point(269,  y), true, STYLE_STATICLABEL);
    columnsComboBox = new uiComboBox(frame, Point(269, y + 12), Size(40, 20), 50, true, STYLE_COMBOBOX);
    columnsComboBox->items().append(COLUMNS_STR, COLUMNS_COUNT);
    columnsComboBox->selectItem(getColumnsIndex());

    // rows
    new uiStaticLabel(frame, "Rows", Point(325,  y), true, STYLE_STATICLABEL);
    rowsComboBox = new uiComboBox(frame, Point(324, y + 12), Size(40, 20), 50, true, STYLE_COMBOBOX);
    rowsComboBox->items().append(ROWS_STR, ROWS_COUNT);
    rowsComboBox->selectItem(getRowsIndex());


    y += 48;

    // show boot info
    new uiStaticLabel(frame, "Show Boot Info", Point(10, y), true, STYLE_STATICLABEL);
    infoCheckBox = new uiCheckBox(frame, Point(80, y - 2), Size(16, 16), uiCheckBoxKind::CheckBox, true, STYLE_CHECKBOX);
    infoCheckBox->setChecked(getBootInfo() == BOOTINFO_ENABLED);

    y += 24;

    // set control to usb-serial
    new uiStaticLabel(frame, "USBSerial", Point(10, y), true, STYLE_STATICLABEL);
    serctlCheckBox = new uiCheckBox(frame, Point(80, y - 2), Size(16, 16), uiCheckBoxKind::CheckBox, true, STYLE_CHECKBOX);
    serctlCheckBox->setChecked(getSerCtl() == SERCTL_ENABLED);

    y += 24;

    // Quit button: exit without save
    auto exitNoSaveButton = new uiButton(frame, "Quit [ESC]", Point(10, y), Size(58, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    exitNoSaveButton->onClick = [&]() {
      quit(0);
    };

    // Save & Quit button: exit with save
    auto exitSaveButton = new uiButton(frame, "Save & Quit [F10]", Point(74, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    exitSaveButton->onClick = [&]() {
      saveProps();
      quit(0);
    };

    // reboot button
    auto rebootButton = new uiButton(frame, "Reboot [CTRL+ESC]", Point(170, y), Size(100, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    rebootButton->onClick = [&]() {
      performReboot();
    };

    // install a program
    auto installButton = new uiButton(frame, "Install Programs", Point(276, y), Size(90, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    installButton->onClick = [&]() {
      progToInstall = -1;
      auto progsDialog = new ProgsDialog(rootWindow());
      if (showModalWindow(progsDialog) == 1) {
        progToInstall = progsDialog->progComboBox->selectedItem();
        quit(0);
      }
      destroyWindow(progsDialog);
    };

    // RTS Status (clickable)
    RTSStatus = new uiLabel(frame, "RTS", Point(300, 210), Size(30, 15), true, STYLE_LABELBUTTON);
    setRTSStatus(Terminal.RTSStatus());
    RTSStatus->onClick = [&]() {
      bool newval = !Terminal.RTSStatus();
      Terminal.setRTSStatus(newval);
      setRTSStatus(newval);
    };

    // CTS Status
    CTSStatus = new uiLabel(frame, "CTS", Point(335, 210), Size(30, 15), true, STYLE_LABELBUTTON);
    lastCTSStatus = Terminal.CTSStatus();
    setCTSStatus(lastCTSStatus);

    // timer
    frame->onTimer = [&](uiTimerHandle) {
      // monitor CTS status
      if (Terminal.CTSStatus() != lastCTSStatus) {
        lastCTSStatus = !lastCTSStatus;
        setCTSStatus(lastCTSStatus);
      }
    };
    setTimer(frame, 100);


    setActiveWindow(frame);
    setFocusedWindow(exitNoSaveButton);

  }


  void setRTSStatus(bool status) {
    RTSStatus->labelStyle().backgroundColor = status ? RGB888(0, 128, 0) : RGB888(128, 128, 128);
    RTSStatus->repaint();
  }


  void setCTSStatus(bool status) {
    CTSStatus->labelStyle().backgroundColor = status ? RGB888(128, 0, 0) : RGB888(128, 128, 128);
    CTSStatus->repaint();
  }


  void saveProps() {
    // need reboot?
    bool reboot = resolutionComboBox->selectedItem() != getResolutionIndex() ||
                  fontComboBox->selectedItem()       != getFontIndex()       ||
                  columnsComboBox->selectedItem()    != getColumnsIndex()    ||
                  rowsComboBox->selectedItem()       != getRowsIndex()       ||
                  bgColorComboBox->selectedColor()   != getBGColor();

    preferences.putInt(PREF_TERMTYPE, termComboBox->selectedItem());
    preferences.putInt(PREF_KBDLAYOUT, kbdComboBox->selectedItem());
    preferences.putInt(PREF_BAUDRATE, baudRateComboBox->selectedItem());
    preferences.putInt(PREF_DATALEN, datalenComboBox->selectedItem());
    preferences.putInt(PREF_PARITY, parityComboBox->selectedItem());
    preferences.putInt(PREF_STOPBITS, stopBitsComboBox->selectedItem());
    preferences.putInt(PREF_FLOWCTRL, flowCtrlComboBox->selectedItem());
    preferences.putInt(PREF_BGCOLOR, (int)bgColorComboBox->selectedColor());
    preferences.putInt(PREF_FGCOLOR, (int)fgColorComboBox->selectedColor());
    preferences.putInt(PREF_RESOLUTION, resolutionComboBox->selectedItem());
    preferences.putInt(PREF_FONT, fontComboBox->selectedItem());
    preferences.putInt(PREF_COLUMNS, columnsComboBox->selectedItem());
    preferences.putInt(PREF_ROWS, rowsComboBox->selectedItem());
    preferences.putInt(PREF_BOOTINFO, infoCheckBox->checked() ? BOOTINFO_ENABLED : BOOTINFO_DISABLED);
    preferences.putInt(PREF_SERCTL, serctlCheckBox->checked() ? SERCTL_ENABLED : SERCTL_DISABLED);

    if (reboot)
      performReboot(); // no return from here!

    loadConfiguration();
  }


  void performReboot() {
    auto rebootDialog = new RebootDialog(frame);
    showModalWindow(rebootDialog);  // no return from here!
  }


  ~ConfDialogApp() {
    // this is required, becasue the terminal may not cover the entire screen
    canvas()->reset();
    canvas()->setBrushColor(getBGColor());
    canvas()->fillRectangle(frameRect);
  }


  static TermType getTermType() {
    return (TermType) preferences.getInt(PREF_TERMTYPE, 7);    // default 7 = ANSILegacy
  }


  static int getKbdLayoutIndex() {
    return preferences.getInt(PREF_KBDLAYOUT, 3);              // default 3 = "US"
  }


  static int getBaudRateIndex() {
    return preferences.getInt(PREF_BAUDRATE, 11);              // default 11 = 115200
  }


  static int getDataLenIndex() {
    return preferences.getInt(PREF_DATALEN, 3);                // default 3 = 8 bits
  }


  static int getParityIndex() {
    return preferences.getInt(PREF_PARITY, 0);                 // default 0 = no parity
  }


  static int getStopBitsIndex() {
    return preferences.getInt(PREF_STOPBITS, 0);               // default 0 = 1 stop bit
  }


  static FlowControl getFlowCtrl() {
    return (FlowControl) preferences.getInt(PREF_FLOWCTRL, 0); // default 0 = no flow control
  }


  static Color getBGColor() {
    return (Color) preferences.getInt(PREF_BGCOLOR, (int)Color::Black);
  }


  static Color getFGColor() {
    return (Color) preferences.getInt(PREF_FGCOLOR, (int)Color::BrightGreen);
  }


  static int getResolutionIndex() {
    return preferences.getInt(PREF_RESOLUTION, RESOLUTION_DEFAULT);
  }


  static int getTempResolutionIndex() {
    return preferences.getInt(PREF_TEMPRESOLUTION, -1);         // default -1 = no temp resolution
  }


  static int getFontIndex() {
    return preferences.getInt(PREF_FONT, 0);                    // default 0 = auto
  }


  static int getColumnsIndex() {
    return preferences.getInt(PREF_COLUMNS, 0);                 // default 0 = MAX
  }


  static int getRowsIndex() {
    return preferences.getInt(PREF_ROWS, 0);                    // default 0 = MAX
  }


  static int getBootInfo() {
    return preferences.getInt(PREF_BOOTINFO, BOOTINFO_ENABLED);
  }


  static int getSerCtl() {
    return preferences.getInt(PREF_SERCTL, SERCTL_DISABLED);
  }


  static char const * getSerParamStr() {
    static char outstr[32];
    snprintf(outstr, sizeof(outstr), "%s,%c%c%s flow=%s", BAUDRATES_STR[getBaudRateIndex()],
                                                          DATALENS_STR[getDataLenIndex()][0],
                                                          PARITY_STR[getParityIndex()][0],
                                                          STOPBITS_STR[getStopBitsIndex()],
                                                          FLOWCTRL_STR[(int)getFlowCtrl()]);
    return outstr;
  }


  // if version in preferences doesn't match, reset preferences
  static void checkVersion() {
    if (preferences.getInt(PREF_VERMAJ, 0) != TERMVERSION_MAJ || preferences.getInt(PREF_VERMIN, 0) != TERMVERSION_MIN) {
      preferences.clear();
      preferences.putInt(PREF_VERMAJ, TERMVERSION_MAJ);
      preferences.putInt(PREF_VERMIN, TERMVERSION_MIN);
    }
  }


  static void setupDisplay() {
    // setup display controller
    auto res = getTempResolutionIndex();
    if (res == -1)
      res = getResolutionIndex();
    else
      preferences.putInt(PREF_TEMPRESOLUTION, -1);
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
    DisplayController->setResolution(RESOLUTIONS_MODELINE[res], -1, RESOLUTIONS_HEIGHT[res]);

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
    auto rxPin  = serctl ? UART_URX : UART_SRX;
    auto txPin  = serctl ? UART_UTX : UART_STX;
    
    Terminal.connectSerialPort(BAUDRATES_INT[getBaudRateIndex()], DATALENS_INT[getDataLenIndex()], PARITY_CHAR[getParityIndex()], STOPBITS_FLOAT[getStopBitsIndex()], rxPin, txPin, getFlowCtrl(), false, RTS, CTS);
  }


};
