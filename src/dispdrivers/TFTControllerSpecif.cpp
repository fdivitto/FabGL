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



#include "fabutils.h"
#include "TFTControllerSpecif.h"



namespace fabgl {


#pragma GCC optimize ("O2")



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// ST7789


#define ST7789_SWRST      0x01
#define ST7789_RDDCOLMOD  0x0C
#define ST7789_SLPOUT     0x11
#define ST7789_PTLON      0x12
#define ST7789_NORON      0x13
#define ST7789_INVOFF     0x20
#define ST7789_INVON      0x21
#define ST7789_DISPON     0x29
#define ST7789_PTLAR      0x30
#define ST7789_COLMOD     0x3A
#define ST7789_WRDISBV    0x51
#define ST7789_WRCTRLD    0x53
#define ST7789_WRCACE     0x55
#define ST7789_WRCABCMB   0x5E
#define ST7789_RAMCTRL    0xB0
#define ST7789_PORCTRL    0xB2
#define ST7789_GCTRL      0xB7
#define ST7789_VCOMS      0xBB
#define ST7789_LCMCTRL    0xC0
#define ST7789_VDVVRHEN   0xC2
#define ST7789_VRHS       0xC3
#define ST7789_VDVS       0xC4
#define ST7789_FRCTRL2    0xC6
#define ST7789_PWCTRL1    0xD0
#define ST7789_PVGAMCTRL  0xE0
#define ST7789_NVGAMCTRL  0xE1


void ST7789Controller::softReset()
{
  // software reset
  SPIBeginWrite();
  writeCommand(ST7789_SWRST);
  SPIEndWrite();
  vTaskDelay(150 / portTICK_PERIOD_MS);

  SPIBeginWrite();

  // Sleep Out
  writeCommand(ST7789_SLPOUT);
  vTaskDelay(120 / portTICK_PERIOD_MS);

  // Normal Display Mode On
  writeCommand(ST7789_NORON);

  setupOrientation();

  // 0x55 = 0 (101) 0 (101) => 65K of RGB interface, 16 bit/pixel
  writeCommand(ST7789_COLMOD);
  writeByte(0x55);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // Porch Setting
  writeCommand(ST7789_PORCTRL);
  writeByte(0x0c);
  writeByte(0x0c);
  writeByte(0x00);
  writeByte(0x33);
  writeByte(0x33);

  // Gate Control
  // VGL = -10.43V
  // VGH = 13.26V
  writeCommand(ST7789_GCTRL);
  writeByte(0x35);

  // VCOM Setting
  // 1.1V
  writeCommand(ST7789_VCOMS);
  writeByte(0x28);

  // LCM Control
  // XMH, XMX
  writeCommand(ST7789_LCMCTRL);
  writeByte(0x0C);

  // VDV and VRH Command Enable
  // CMDEN = 1, VDV and VRH register value comes from command write.
  writeCommand(ST7789_VDVVRHEN);
  writeByte(0x01);
  writeByte(0xFF);

  // VRH Set
  // VAP(GVDD) = 4.35+( vcom+vcom offset+vdv) V
  // VAN(GVCL) = -4.35+( vcom+vcom offset-vdv) V
  writeCommand(ST7789_VRHS);
  writeByte(0x10);

  // VDV Set
  // VDV  = 0V
  writeCommand(ST7789_VDVS);
  writeByte(0x20);

  // Frame Rate Control in Normal Mode
  // RTNA = 0xf (60Hz)
  // NLA  = 0 (dot inversion)
  writeCommand(ST7789_FRCTRL2);
  writeByte(0x0f);

  // Power Control 1
  // VDS  = 2.3V
  // AVCL = -4.8V
  // AVDD = 6.8v
  writeCommand(ST7789_PWCTRL1);
  writeByte(0xa4);
  writeByte(0xa1);

  // Positive Voltage Gamma Control
  writeCommand(ST7789_PVGAMCTRL);
  writeByte(0xd0);
  writeByte(0x00);
  writeByte(0x02);
  writeByte(0x07);
  writeByte(0x0a);
  writeByte(0x28);
  writeByte(0x32);
  writeByte(0x44);
  writeByte(0x42);
  writeByte(0x06);
  writeByte(0x0e);
  writeByte(0x12);
  writeByte(0x14);
  writeByte(0x17);

  // Negative Voltage Gamma Control
  writeCommand(ST7789_NVGAMCTRL);
  writeByte(0xd0);
  writeByte(0x00);
  writeByte(0x02);
  writeByte(0x07);
  writeByte(0x0a);
  writeByte(0x28);
  writeByte(0x31);
  writeByte(0x54);
  writeByte(0x47);
  writeByte(0x0e);
  writeByte(0x1c);
  writeByte(0x17);
  writeByte(0x1b);
  writeByte(0x1e);

  // Display Inversion On
  writeCommand(ST7789_INVON);

  // Display On
  writeCommand(ST7789_DISPON);

  SPIEndWrite();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// ILI9341


#define ILI9341_SWRESET           0x01
#define ILI9341_SLEEPOUT          0x11
#define ILI9341_NORON             0x13
#define ILI9341_GAMMASET          0x26
#define ILI9341_DISPON            0x29
#define ILI9341_PIXELFORMATSET    0x3A
#define ILI9341_FRAMERATECTRL1    0xB1
#define ILI9341_DISPLAYFUNCCTRL   0xB6
#define ILI9341_POWERCTR1         0xC0
#define ILI9341_POWERCTR2         0xC1
#define ILI9341_VCOMCTR1          0xC5
#define ILI9341_VCOMCTR2          0xC7
#define ILI9341_POWERCTRLA        0xCB
#define ILI9341_POWERCTRLB        0xCF
#define ILI9341_POSGAMMACORR      0xE0
#define ILI9341_NEGGAMMACORR      0xE1
#define ILI9341_DRIVERTIMINGCTRLA 0xE8
#define ILI9341_DRIVERTIMINGCTRLB 0xEA
#define ILI9341_POWERONSEQCTRL    0xED
#define ILI9341_DEVICECODE        0xEF
#define ILI9341_ENABLE3G          0xF2
#define ILI9341_PUMPRATIOCTRL     0xF7



void ILI9341Controller::softReset()
{
  m_reverseHorizontal = true;

  // software reset
  SPIBeginWrite();
  writeCommand(ILI9341_SWRESET);
  SPIEndWrite();
  vTaskDelay(150 / portTICK_PERIOD_MS);

  SPIBeginWrite();

  // unknown but required init sequence!
  writeCommand(ILI9341_DEVICECODE);
  writeByte(0x03);
  writeByte(0x80);
  writeByte(0x02);

  // Power control B
  writeCommand(ILI9341_POWERCTRLB);
  writeByte(0x00);
  writeByte(0XC1);
  writeByte(0X30);

  // Power on sequence control
  writeCommand(ILI9341_POWERONSEQCTRL);
  writeByte(0x64);
  writeByte(0x03);
  writeByte(0X12);
  writeByte(0X81);

  // Driver timing control A
  writeCommand(ILI9341_DRIVERTIMINGCTRLA);
  writeByte(0x85);
  writeByte(0x00);
  writeByte(0x78);

  // Power control A
  writeCommand(ILI9341_POWERCTRLA);
  writeByte(0x39);
  writeByte(0x2C);
  writeByte(0x00);
  writeByte(0x34);
  writeByte(0x02);

  // Pump ratio control
  writeCommand(ILI9341_PUMPRATIOCTRL);
  writeByte(0x20);

  // Driver timing control B
  writeCommand(ILI9341_DRIVERTIMINGCTRLB);
  writeByte(0x00);
  writeByte(0x00);

  // Power Control 1
  writeCommand(ILI9341_POWERCTR1);
  writeByte(0x23);

  // Power Control 2
  writeCommand(ILI9341_POWERCTR2);
  writeByte(0x10);

  // VCOM Control 1
  writeCommand(ILI9341_VCOMCTR1);
  writeByte(0x3e);
  writeByte(0x28);

  // VCOM Control 2
  writeCommand(ILI9341_VCOMCTR2);
  writeByte(0x86);

  setupOrientation();

  // COLMOD: Pixel Format Set
  writeCommand(ILI9341_PIXELFORMATSET);
  writeByte(0x55);

  // Frame Rate Control (In Normal Mode/Full Colors)
  writeCommand(ILI9341_FRAMERATECTRL1);
  writeByte(0x00);
  writeByte(0x13); // 0x18 79Hz, 0x1B 70Hz (default), 0x13 100Hz

  // Display Function Control
  writeCommand(ILI9341_DISPLAYFUNCCTRL);
  writeByte(0x08);
  writeByte(0x82);
  writeByte(0x27);

  // Enable 3G (gamma control)
  writeCommand(ILI9341_ENABLE3G);
  writeByte(0x00);  // bit 0: 0 => disable 3G

  // Gamma Set
  writeCommand(ILI9341_GAMMASET);
  writeByte(0x01);  // 1 = Gamma curve 1 (G2.2)

  // Positive Gamma Correction
  writeCommand(ILI9341_POSGAMMACORR);
  writeByte(0x0F);
  writeByte(0x31);
  writeByte(0x2B);
  writeByte(0x0C);
  writeByte(0x0E);
  writeByte(0x08);
  writeByte(0x4E);
  writeByte(0xF1);
  writeByte(0x37);
  writeByte(0x07);
  writeByte(0x10);
  writeByte(0x03);
  writeByte(0x0E);
  writeByte(0x09);
  writeByte(0x00);

  // Negative Gamma Correction
  writeCommand(ILI9341_NEGGAMMACORR);
  writeByte(0x00);
  writeByte(0x0E);
  writeByte(0x14);
  writeByte(0x03);
  writeByte(0x11);
  writeByte(0x07);
  writeByte(0x31);
  writeByte(0xC1);
  writeByte(0x48);
  writeByte(0x08);
  writeByte(0x0F);
  writeByte(0x0C);
  writeByte(0x31);
  writeByte(0x36);
  writeByte(0x0F);

  // Sleep Out
  writeCommand(ILI9341_SLEEPOUT);

  // Normal Display Mode On
  writeCommand(ILI9341_NORON);

  SPIEndWrite();

  vTaskDelay(120 / portTICK_PERIOD_MS);

  SPIBeginWrite();

  // Display ON
  writeCommand(ILI9341_DISPON);
  
  SPIEndWrite();
}


} // end of namespace
