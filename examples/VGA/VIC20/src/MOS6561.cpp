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


#include "MOS6561.h"
#include "machine.h"


#pragma GCC optimize ("O2")


/////////////////////////////////////////////////////////////////////////////////////////////
// VIC (6561 - Video Interface Chip)


static const RGB222 COLORS[16] = { {0, 0, 0},   // black
                                   {3, 3, 3},   // white
                                   {3, 0, 0},   // red
                                   {0, 2, 2},   // cyan
                                   {2, 0, 2},   // magenta
                                   {0, 2, 0},   // green
                                   {0, 0, 2},   // blue
                                   {2, 2, 0},   // yellow
                                   {2, 1, 0},   // orange
                                   {3, 2, 0},   // light orange
                                   {3, 2, 2},   // pink
                                   {0, 3, 3},   // light cyan
                                   {3, 0, 3},   // light magenta
                                   {0, 3, 0},   // light green
                                   {0, 0, 3},   // light blue
                                   {3, 3, 0} }; // light yellow

static uint8_t RAWCOLORS[16];


MOS6561::MOS6561(Machine * machine, fabgl::VGAController * displayController)
  : m_machine(machine),
    m_displayController(displayController)
{
  for (int i = 0; i < 16; ++i)
    RAWCOLORS[i] = m_displayController->createRawPixel(COLORS[i]);

  m_soundGen.attach(&m_sqGen1);
  m_soundGen.attach(&m_sqGen2);
  m_soundGen.attach(&m_sqGen3);
  m_soundGen.attach(&m_noGen);
  m_sqGen1.setVolume(60);
  m_sqGen2.setVolume(60);
  m_sqGen3.setVolume(60);
  m_noGen.setVolume(60);
  enableAudio(true);

  reset();
}


void MOS6561::reset()
{
  memset(m_regs, 0, 16);
  m_colCount        = 0;
  m_rowCount        = 23;
  m_charHeight      = 8;
  m_videoMatrixAddr = 0x0000;
  m_charTableAddr   = 0x0000;
  m_scanX           = 0;
  m_scanY           = 0;
  m_Y               = 0;
  m_charRow         = 0;
  m_charColumn      = 0;
  m_inCharRow       = 0;
  m_topPos          = 0;
  m_leftPos         = 0;
  m_isVBorder       = false;
  m_colorLine       = nullptr;
  m_videoLine       = nullptr;
  m_charInvertMask  = 0x00;
  m_auxColor        = RAWCOLORS[0];
  m_mcolors[3]      = m_auxColor;
  m_sqGen1.enable(false);
  m_sqGen2.enable(false);
  m_sqGen3.enable(false);
  m_noGen.enable(false);
}


// converts VIC char table address to CPU address
// this produces (m_charTableAddr+addr) with correct wrappings at 0x9C00 and 0x1C00
inline int CHARTABLE_VIC2CPU(int addr)
{
  return ((addr & 0x1fff) | (~((addr & 0x2000) << 2) & 0x8000));
}


void MOS6561::tick(int cycles)
{
  for (int c = 0; c < cycles; ++c) {

    m_scanX += 4;

    if (m_scanX == FrameWidth) {
      m_scanX = 0;
      ++m_scanY;

      if (m_scanY == FrameHeight) {
        // starting from invisible area (vertical blanking)
        m_scanY = 0;
        m_isVBorder = false;
        m_videoLine = nullptr;
      } else if (m_scanY >= VerticalBlanking) {
        // visible area, including vertical borders
        m_Y            = m_scanY - VerticalBlanking;
        m_destScanline = (uint32_t*) (m_displayController->getScanline(ScreenOffsetY + m_Y) + ScreenOffsetX);
        m_isVBorder    = (m_Y < m_topPos || m_Y >= m_topPos + m_charAreaHeight);
        if (!m_isVBorder) {
          // char area, including horizontal borders
          m_charColumn = m_leftPos < 0 ? -m_leftPos / 8 : 0;
          m_charRow    = (m_Y - m_topPos) / m_charHeight;
          m_inCharRow  = (m_Y - m_topPos) % m_charHeight;
          int vaddr    = m_videoMatrixAddr + m_charRow * m_colCount;
          m_videoLine  = m_machine->busReadVideoP(vaddr);
          m_colorLine  = m_machine->busReadColorP(0x9400 + (vaddr & 0x3ff));
          m_loadChar   = true;
        }
      }
    }

    if ((m_videoLine || m_isVBorder) && m_scanX >= HorizontalBlanking)
      drawNextPixels();

  }
}


// draw next 4 pixels
void MOS6561::drawNextPixels()
{
  // column to draw relative to frame buffer
  // (HorizontalBlanking & 3) to align2 to 32 bit
  int x = m_scanX - HorizontalBlanking - (4 - (HorizontalBlanking & 3));

  if (m_isVBorder || x < m_leftPos || x >= m_rightPos) {

    //// top/bottom/left/right borders

    *m_destScanline++ = m_borderColor4;

  } else {

    //// chars area

    // which nibble to draw?
    if (m_loadChar) {
      m_loadChar = false;

      // character pixels from char table
      int charIndex = m_videoLine[m_charColumn];
      m_charData = m_machine->busReadCharDefs(CHARTABLE_VIC2CPU(m_charTableAddr + charIndex * m_charHeight + m_inCharRow));

      // character foreground color from color RAM
      m_foregroundColorCode = m_colorLine[m_charColumn];

      if (m_foregroundColorCode & 0x8) {

        // Multicolor

        m_mcolors[2] = RAWCOLORS[m_foregroundColorCode & 7];

        int cv = m_charData;

        // pack high 4 pixels in raw 32 bit word
        m_hiNibble        =  (m_mcolors[(cv >> 6) & 3] << 16)
                           | (m_mcolors[(cv >> 6) & 3] << 24)
                           | (m_mcolors[(cv >> 4) & 3]      )
                           | (m_mcolors[(cv >> 4) & 3] << 8 );

        // pack low 4 pixels in raw 32 bit word
        m_loNibble        =  (m_mcolors[(cv >> 2) & 3] << 16)
                           | (m_mcolors[(cv >> 2) & 3] << 24)
                           | (m_mcolors[(cv     ) & 3]      )
                           | (m_mcolors[(cv     ) & 3] << 8 );

      } else {

        // HI-RES

        m_hcolors[1] = RAWCOLORS[m_foregroundColorCode & 7];

        // invert foreground and background colors?
        int cv = m_charData ^ m_charInvertMask;

        // pack high 4 pixels in raw 32 bit word
        m_hiNibble        =  (m_hcolors[(cv >> 7) & 1] << 16)
                           | (m_hcolors[(cv >> 6) & 1] << 24)
                           | (m_hcolors[(cv >> 5) & 1]      )
                           | (m_hcolors[(cv >> 4) & 1] << 8 );

        // pack low 4 pixels in raw 32 bit word
        m_loNibble        =  (m_hcolors[(cv >> 3) & 1] << 16)
                           | (m_hcolors[(cv >> 2) & 1] << 24)
                           | (m_hcolors[(cv >> 1) & 1]      )
                           | (m_hcolors[(cv)      & 1] << 8 );

      }

    }

    if (~(m_leftPos + x) & 0x4) {
      // draw high nibble (left part of the char)
      *m_destScanline++ = m_hiNibble;
    } else {
      // draw low nibble (right part of the char)
      *m_destScanline++ = m_loNibble;
      // advance to the next column
      ++m_charColumn;
      m_loadChar = true;
    }

  }
}


void MOS6561::writeReg(int reg, int value)
{
  if (m_regs[reg] != value) {
    m_regs[reg] = value;
    switch (reg) {
      case 0x0:
        m_leftPos  = ((m_regs[0] & 0x7f) - 7) * 4;
        m_rightPos = m_leftPos + m_colCount * CharWidth;
        break;
      case 0x1:
        m_topPos = (m_regs[1] - 14) * 2;
        break;
      case 0x2:
        m_videoMatrixAddr = ((m_regs[2] & 0x80) << 2) | ((m_regs[5] & 0x70) << 6) | ((~m_regs[5] & 0x80) << 8);
        m_colCount        = fabgl::imin(m_regs[2] & 0x7f, MaxTextColumns);
        m_rightPos        = m_leftPos + m_colCount * CharWidth;
        break;
      case 0x3:
        m_charHeight     = m_regs[3] & 1 ? 16 : 8;
        m_rowCount       = (m_regs[3] >> 1) & 0x3f;
        m_charAreaHeight = m_rowCount * m_charHeight;
        break;
      case 0x5:
        m_charTableAddr   = ((m_regs[5] & 0xf) << 10);
        m_videoMatrixAddr = ((m_regs[2] & 0x80) << 2) | ((m_regs[5] & 0x70) << 6) | ((~m_regs[5] & 0x80) << 8);
        break;
      case 0xe:
        m_auxColor        = RAWCOLORS[(m_regs[0xe] >> 4) & 0xf];
        m_mcolors[3]      = m_auxColor;
        m_soundGen.setVolume((m_regs[0xe] & 0xf) << 3);
        break;
      case 0xa:
        m_sqGen1.enable(value & 0x80);
        m_sqGen1.setFrequency(PHI2 / 64 / 16 / (128 - ((value + 1) & 0x7f)));
        break;
      case 0xb:
        m_sqGen2.enable(value & 0x80);
        m_sqGen2.setFrequency(PHI2 / 32 / 16 / (128 - ((value + 1) & 0x7f)));
        break;
      case 0xc:
        m_sqGen3.enable(value & 0x80);
        m_sqGen3.setFrequency(PHI2 / 16 / 16 / (128 - ((value + 1) & 0x7f)));
        break;
      case 0xd:
        m_noGen.enable(value & 0x80);
        m_noGen.setFrequency(value & 0x7f);
        break;
      case 0xf:
      {
        int backColorCode   = (m_regs[0xf] >> 4) & 0xf;
        m_charInvertMask    = (m_regs[0xf] & 0x8) == 0 ? 0xff : 0x00;
        uint8_t borderColor = RAWCOLORS[m_regs[0xf] & 7];
        m_borderColor4      = borderColor | (borderColor << 8) | (borderColor << 16) | (borderColor << 24);
        m_mcolors[1]        = borderColor;
        m_mcolors[0]        = m_hcolors[0] = RAWCOLORS[backColorCode];
        break;
      }
    }
  }
}


int MOS6561::readReg(int reg)
{
  switch (reg) {
    case 0x3:
      m_regs[0x3] = (m_regs[0x3] & 0x7f) | ((m_scanY & 1) << 7);
      break;
    case 0x4:
      m_regs[0x4] = (m_scanY >> 1) & 0xff;
      break;
  }
  #if DEBUG6561
  printf("VIC, read reg 0x%02x, val = 0x%02x\n", reg, m_regs[reg]);
  #endif
  return m_regs[reg];
}




