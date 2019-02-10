/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
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


#include "fabgl.h"


const char * PresetResolutions[] = {
  VGA_320x200_75Hz,
  QVGA_320x240_60Hz,
  VGA_400x300_60Hz,
  VGA_480x300_75Hz,
  VGA_512x384_60Hz,
  VGA_512x448_60Hz,
  VGA_512x512_58Hz,
  VGA_640x200_70Hz,
  VGA_640x350_70Hz,
  VGA_640x350_70HzAlt1,
  VESA_640x350_85Hz,
  VGA_640x382_60Hz,
  VGA_640x384_60Hz,
  VGA_640x400_70Hz,
  VGA_640x480_60Hz,
  VGA_640x480_60HzAlt1,
  VGA_640x480_60HzD,
  VGA_640x480_73Hz,
  VESA_640x480_75Hz,
  VESA_720x400_85Hz,
  PAL_720x576_50Hz,
  VESA_768x576_60Hz,
  SVGA_800x600_56Hz,
  SVGA_800x600_60Hz,
};



int currentResolution = 0/*8*/;  // VGA_640x350_70HzAlt1
int moveX = 0;
int moveY = 0;
int shrinkX = 0;
int shrinkY = 0;



void printHelp()
{
  Serial.printf("Modeline studio\n");
  Serial.printf("Chip Revision: %d   Chip Frequency: %d MHz\n\n", ESP.getChipRevision(), ESP.getCpuFreqMHz());
  Serial.printf("%s\n", VGAController.getResolutionTimings()->label);
  Serial.printf("\nScreen move:\n");
  Serial.printf("  w = Move Up   z = Move Down   a = Move Left   s = Move Right\n");
  Serial.printf("Screen shrink:\n");
  Serial.printf("  n = Dec Horiz N = Inc Horiz   m = Dec Vert    M = Inc Vert\n");
  Serial.printf("Resolutions:\n");
  Serial.printf("  + = Next resolution  - = Prev resolution   x = Restore modeline\n");
  Serial.printf("Modeline change:\n");
  Serial.printf("  ! = Insert modline. Example: !\"640x350@70Hz\" 25.175 640 656 752 800 350 366 462 510 -HSync -VSync\n");
  /*
  Serial.printf("  e = Decrease horiz. Front Porch   E = Increase horiz. Front Porch\n");
  Serial.printf("  r = Decrease horiz. Sync Pulse    R = Increase horiz. Sync Pulse\n");
  Serial.printf("  t = Decrease horiz. Bach Porch    T = Increase horiz. Back Porch\n");
  Serial.printf("  y = Decrease vert. Front Porch    Y = Increase vert. Front Porch\n");
  Serial.printf("  u = Decrease vert. Sync Pulse     U = Increase vert. Sync Pulse\n");
  Serial.printf("  i = Decrease vert. Bach Porch     I = Increase vert. Back Porch\n");
  */
  Serial.printf("  o = Decrease frequency by 5KHz    O = Increase frequency by 5KHz\n");
  Serial.printf("  . = Change horiz. signals order\n");
  Serial.printf("Various:\n");
  Serial.printf("  h = Print This help\n\n");
}


void printInfo()
{
  auto t = VGAController.getResolutionTimings();
  Serial.printf("Modeline \"%s\" %2.8g %d %d %d %d %d %d %d %d %cHSync %cVSync %s %s\n",
                t->label,
                t->frequency / 1000000.0,
                t->HVisibleArea,
                t->HVisibleArea + t->HFrontPorch,
                t->HVisibleArea + t->HFrontPorch + t->HSyncPulse,
                t->HVisibleArea + t->HFrontPorch + t->HSyncPulse + t->HBackPorch,
                t->VVisibleArea,
                t->VVisibleArea + t->VFrontPorch,
                t->VVisibleArea + t->VFrontPorch + t->VSyncPulse,
                t->VVisibleArea + t->VFrontPorch + t->VSyncPulse + t->VBackPorch,
                t->HSyncLogic, t->VSyncLogic,
                t->scanCount == 2 ? "DoubleScan" : "",
                t->HStartingBlock == ScreenBlock::FrontPorch ? "FrontPorchBegins" :
                (t->HStartingBlock == ScreenBlock::Sync ? "SyncBegins" :
                (t->HStartingBlock == ScreenBlock::BackPorch ? "BackPorchBegins" : "VisibleBegins")));

  //Serial.printf("VFront = %d, VSync = %d, VBack = %d\n", t->VFrontPorch, t->VSyncPulse, t->VBackPorch);

  if (moveX || moveY)
    Serial.printf("moveScreen(%d, %d)\n", moveX, moveY);
  if (shrinkX || shrinkY)
    Serial.printf("shrinkScreen(%d, %d)\n", shrinkX, shrinkY);
  Serial.printf("Screen size   : %d x %d\n", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  Serial.printf("Viewport size : %d x %d\n", Canvas.getWidth(), Canvas.getHeight());
  Serial.printf("Free memory (total, min, largest): %d, %d, %d\n\n", heap_caps_get_free_size(MALLOC_CAP_DMA),
                                                                   heap_caps_get_minimum_free_size(MALLOC_CAP_DMA),
                                                                   heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}


void updateScreen()
{
  Canvas.setPenColor(Color::BrightRed);
  Canvas.setBrushColor(Color::BrightBlue);
  Canvas.clear();
  Canvas.fillRectangle(0, 0, Canvas.getWidth() - 1, Canvas.getHeight() - 1);
  Canvas.drawRectangle(0, 0, Canvas.getWidth() - 1, Canvas.getHeight() - 1);

  Canvas.setPenColor(Color::Black);
  Canvas.setBrushColor(Color::BrightYellow);
  Canvas.selectFont(Canvas.getPresetFontInfo(40, 14));
  Canvas.setGlyphOptions(GlyphOptions().FillBackground(true).DoubleWidth(1));
  Canvas.drawText(40, 20, VGAController.getResolutionTimings()->label);

  Canvas.setGlyphOptions(GlyphOptions());
  Canvas.setPenColor(Color::BrightWhite);
  Canvas.setBrushColor(Color::BrightBlue);
  Canvas.drawTextFmt(40, 40, "Screen Size   : %d x %d", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  Canvas.drawTextFmt(40, 60, "Viewport Size : %d x %d", Canvas.getWidth(), Canvas.getHeight());
  Canvas.drawText(40, 80,    "Commands (More on UART):");
  Canvas.drawText(40, 100,   "  w = Move Up    z = Move Down");
  Canvas.drawText(40, 120,   "  a = Move Left  s = Move Right");
  Canvas.drawText(40, 140,   "  + = Next Resolution");
  Canvas.setPenColor(Color::BrightGreen);
  Canvas.drawRectangle(35, 15, 295, 155);
}



void setup()
{
  Serial.begin(115200);

  // avoid garbage into the UART
  delay(500);

  // 8 colors
  //VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5);
  // 64 colors
  VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);

  VGAController.setResolution(PresetResolutions[currentResolution]);
  updateScreen();
  printHelp();
  printInfo();
}


void loop()
{
  fabgl::Timings t;

  if (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        printHelp();
        break;
      case 'w':
        --moveY;
        VGAController.moveScreen(0, -1);
        printInfo();
        break;
      case 'z':
        ++moveY;
        VGAController.moveScreen(0, +1);
        printInfo();
        break;
      case 'a':
        --moveX;
        VGAController.moveScreen(-1, 0);
        printInfo();
        break;
      case 's':
        ++moveX;
        VGAController.moveScreen(+1, 0);
        printInfo();
        break;
      case 'x':
        moveX = moveY = shrinkX = shrinkY = 0;
        VGAController.setResolution(PresetResolutions[currentResolution]);
        updateScreen();
        printInfo();
        break;
      case '+':
      case '-':
        moveX = moveY = shrinkX = shrinkY = 0;
        currentResolution = (c == '+' ? currentResolution + 1 : currentResolution - 1);
        if (currentResolution == sizeof(PresetResolutions) / sizeof(const char *))
          currentResolution = 0;
        if (currentResolution < 0)
          currentResolution = sizeof(PresetResolutions) / sizeof(const char *) - 1;
        VGAController.setResolution(PresetResolutions[currentResolution]);
        updateScreen();
        printInfo();
        break;
      case '!':
        moveX = moveY = shrinkX = shrinkY = 0;
        VGAController.setResolution( Serial.readStringUntil('\n').c_str() );
        updateScreen();
        printInfo();
        break;
      /*
      case 'e':
      case 'E':
        t = *VGAController.getResolutionTimings();
        t.HFrontPorch = (c == 'e' ? t.HFrontPorch - 1 : t.HFrontPorch + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 'r':
      case 'R':
        t = *VGAController.getResolutionTimings();
        t.HSyncPulse = (c == 'r' ? t.HSyncPulse - 1 : t.HSyncPulse + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 't':
      case 'T':
        t = *VGAController.getResolutionTimings();
        t.HBackPorch = (c == 't' ? t.HBackPorch - 1 : t.HBackPorch + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 'y':
      case 'Y':
        t = *VGAController.getResolutionTimings();
        t.VFrontPorch = (c == 'y' ? t.VFrontPorch - 1 : t.VFrontPorch + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 'u':
      case 'U':
        t = *VGAController.getResolutionTimings();
        t.VSyncPulse = (c == 'u' ? t.VSyncPulse - 1 : t.VSyncPulse + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 'i':
      case 'I':
        t = *VGAController.getResolutionTimings();
        t.VBackPorch = (c == 'i' ? t.VBackPorch - 1 : t.VBackPorch + 1);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      */
      case 'o':
      case 'O':
        t = *VGAController.getResolutionTimings();
        t.frequency = (c == 'o' ? t.frequency - 5000 : t.frequency + 5000);
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case '.':
        t = *VGAController.getResolutionTimings();
        switch (t.HStartingBlock) {
          case ScreenBlock::FrontPorch:
            t.HStartingBlock = ScreenBlock::Sync;
            break;
          case ScreenBlock::Sync:
            t.HStartingBlock = ScreenBlock::BackPorch;
            break;
          case ScreenBlock::BackPorch:
            t.HStartingBlock = ScreenBlock::VisibleArea;
            break;
          case ScreenBlock::VisibleArea:
            t.HStartingBlock = ScreenBlock::FrontPorch;
            break;
        }
        VGAController.setResolution(t);
        updateScreen();
        printInfo();
        break;
      case 'n':
        ++shrinkX;
        VGAController.shrinkScreen(+1, 0);
        updateScreen();
        printInfo();
        break;
      case 'N':
        --shrinkX;
        VGAController.shrinkScreen(-1, 0);
        updateScreen();
        printInfo();
        break;
      case 'm':
        ++shrinkY;
        VGAController.shrinkScreen(0, +1);
        updateScreen();
        printInfo();
        break;
      case 'M':
        --shrinkY;
        VGAController.shrinkScreen(0, -1);
        updateScreen();
        printInfo();
        break;
    }
  }
}
