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


#include <stdlib.h>
#include <stdint.h>

#include "fabgl.h"


#define DEBUG6561 0


/////////////////////////////////////////////////////////////////////////////////////////////
// VIC (6561 - Video Interface Chip)


class Machine;


class MOS6561 {

public:

  static const int CharWidth          = 8;

  // assume VGA_256x384_60Hz
  static const int VGAWidth           = 256;
  static const int VGAHeight          = 384;

  // PAL specific params
  static const int PHI2               = 4433618;  // VIC frequency (CPU frequency is PHI2/4)
  static const int FrameWidth         = 284;   // includes horizontal blanking (must be a multiple of 4)
  static const int FrameHeight        = 312;   // includes vertical blanking
  static const int HorizontalBlanking = 63;
  static const int VerticalBlanking   = 28;
  static const int ScreenWidth        = FrameWidth - HorizontalBlanking;  // ScreenWidth  = 233
  static const int ScreenHeight       = FrameHeight - VerticalBlanking;   // ScreenHeight = 284
  static const int ScreenOffsetX      = (((VGAWidth - ScreenWidth) / 2) & 0xffc) + 4; // must be 32 bit aligned
  static const int ScreenOffsetY      = (VGAHeight - ScreenHeight) / 2;
  static const int CyclesPerFrame     = FrameWidth * FrameHeight / 4;
  static const int MaxTextColumns     = 32;


  MOS6561(Machine * machine, fabgl::VGAController * displayController);

  void reset();

  void writeReg(int reg, int value);
  int readReg(int reg);

  void tick(int cycles);

  Machine * machine() { return m_machine; }

  void enableAudio(bool value) { m_soundGen.play(value); }


private:

  void drawNextPixels();

  Machine *               m_machine;
  fabgl::VGAController *  m_displayController;

  int                     m_charHeight;
  int                     m_colCount;
  int                     m_rowCount;
  int                     m_scanX;
  int                     m_scanY;
  int                     m_topPos;
  int                     m_leftPos;   // where text starts
  int                     m_rightPos;  // where right border starts
  int                     m_charRow;
  int                     m_inCharRow; // row inside character tab
  int                     m_Y;
  int                     m_charAreaHeight;
  int                     m_foregroundColorCode;
  int                     m_charColumn;
  bool                    m_loadChar;
  bool                    m_isVBorder;
  uint8_t                 m_charData;
  uint8_t                 m_auxColor;
  uint8_t                 m_charInvertMask;  // 0x00 = no invert, 0xff = invert
  uint32_t                m_borderColor4;    // 4 times border color
  uint32_t                m_loNibble;
  uint32_t                m_hiNibble;
  uint32_t *              m_destScanline;
  uint8_t const *         m_videoLine;
  uint8_t const *         m_colorLine;
  uint16_t                m_videoMatrixAddr; // in the CPU address space
  uint16_t                m_charTableAddr;   // in the CPU address space
  uint8_t                 m_regs[16];
  uint8_t                 m_mcolors[4];
  uint8_t                 m_hcolors[2];

  SquareWaveformGenerator m_sqGen1;
  SquareWaveformGenerator m_sqGen2;
  SquareWaveformGenerator m_sqGen3;
  VICNoiseGenerator       m_noGen;
  SoundGenerator          m_soundGen;
};



