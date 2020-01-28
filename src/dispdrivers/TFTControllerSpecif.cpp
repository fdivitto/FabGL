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



#include "fabutils.h"
#include "TFTControllerSpecif.h"



namespace fabgl {



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





} // end of namespace
