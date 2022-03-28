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

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#if __has_include("esp32/rom/lldesc.h")
  #include "esp32/rom/lldesc.h"
#else
  #include "rom/lldesc.h"
#endif
#include "soc/i2s_struct.h"
#include "soc/sens_struct.h"

#include "fabglconf.h"


namespace fabgl {


// Note about *frameGroupCount* and PAL:
//      It should be 4 (8 fields) to perform correct burst cycling (bruch sequence).
// Note about *frameGroupCount* and NTSC:
//      It should be 2 (4 fields) to perfom complete subcarrier cycle
// Note about *sampleRate_hz*:
//      Setting exactly 16000000 or 13333333.3334 will disable APLL, allowing second DAC channel to be usable
//      Other values makes second DAC not usable!
struct CVBSParams {
  char const *    desc;
  double          sampleRate_hz;                  // sample rate (see note above)
  double          subcarrierFreq_hz;
  double          line_us;                        // line duration
  double          hline_us;                       // half line duration (vsync and equalization pulse)
  double          hsync_us;                       // horizontal sync pulse duration
  double          backPorch_us;                   // back porch duration
  double          frontPorch_us;                  // front porch duration
  double          hblank_us;                      // horizontal blank after back porch to keep blank (adjusts horizontal position)
  double          burstCycles;                    // number of color burst cycles
  double          burstStart_us;                  // (breeze way) time from backPorch_us to color burst
  double          fieldLines;                     // number of lines in a field
  double          longPulse_us;                   // vertical sync, long pulse duration
  double          shortPulse_us;                  // vertical sync, short pulse duration (equalization pulse)
  double          hsyncEdge_us;                   // line sync falling and rising edges duration
  double          vsyncEdge_us;                   // short and long syncs falling and rising edge duration
  uint8_t         blankLines;                     // vertical blank after vertical sync to keep blank (adjusts vertical position)
  uint8_t         frameGroupCount;                // number of frames for each DMA chain. Controls how much fields are presented to getBurstPhase() function. See note above.
  int8_t          preEqualizingPulseCount;        // vertical sync, number of short pulses just before vsync (at the beginning of field)
  int8_t          vsyncPulseCount;                // vertical sync, number of long pulses (must be >0 to generate first ISR)
  int8_t          postEqualizingPulseCount;       // vertical sync, number of short pulses just after vsync
  int8_t          endFieldEqualizingPulseCount;   // verticao sync, number of short pulses at the end of field
  uint8_t         syncLevel;                      // DAC level of sync pulses
  uint8_t         blackLevel;                     // DAC level of black
  uint8_t         whiteLevel;                     // DAC level of white
  int8_t          burstAmp;                       // DAC amplitude of color burst
  uint16_t        defaultVisibleSamples;          // default horizontal visible samples
  uint16_t        defaultVisibleLines;            // default vertical visible lines (per field)
  uint8_t         fieldStartingLine[2];           // starting line of each field ([0]=first field, [1]=second field), in range 1..2
  uint8_t         fields;                         // number of fields (max 2)
  uint8_t         interlaceFactor;                // 1 = progressive, 2 = interlaced
  
  // params ranges:
  //   frame         : 1 ... frameGroupCount
  //   frameLine     : 1 ... fields * fieldLines
  // ret:
  //   false = line hasn't color burst
  virtual bool lineHasColorBurst(int frame, int frameLine) const = 0;
  
  // phase in radians, red 0...1, green 0...1, blue 0...1
  virtual double getComposite(bool oddLine, double phase, double red, double green, double blue, double * Y) const = 0;
  
  // phase in radians
  virtual double getColorBurst(bool oddLine, double phase) const = 0;
  
};


#define CVBS_ALLOCATED_LINES 4


// increasing this value will require more memory available
#define CVBS_SUBCARRIERPHASES 100

#if CVBS_SUBCARRIERPHASES > 128
  typedef uint16_t scPhases_t;
#else
  typedef uint8_t scPhases_t;
#endif

// value used in m_subCarrierPhases[] to indicate "no burst"
#define CVBS_NOBURSTFLAG (CVBS_SUBCARRIERPHASES * 2 - 1)


#if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
  extern volatile uint64_t s_cvbsctrlcycles;
#endif


typedef void (*CVBSDrawScanlineCallback)(void * arg, uint16_t * dest, int destSample, int scanLine);


class CVBSGenerator {

public:

  CVBSGenerator();

  // gpio can be:
  //    GPIO_NUM_25 : gpio 25 DAC connected to DMA, gpio 26 set using setConstDAC()
  //    GPIO_NUM_26 : gpio 26 DAC connected to DMA, gpio 25 set using setConstDAC()
  void setVideoGPIO(gpio_num_t gpio);
  
  void setDrawScanlineCallback(CVBSDrawScanlineCallback drawScanlineCallback, void * arg = nullptr);

  void setup(char const * desc);
  void setup(CVBSParams const * params);
  
  static CVBSParams const * getParamsFromDesc(char const * desc);

  void run(bool subCarrierOnly = false);

  void stop();
  
  // usable just when sampleRate is 16Mhz or 13.333Mhz!
  void setConstDAC(uint8_t value) {
    //WRITE_PERI_REG(I2S_CONF_SIGLE_DATA_REG(0), value << 24);
    I2S0.conf_single_data = value << 24;
  }
  
  static bool VSync()                                         { return s_VSync; }
  static int field()                                          { return s_field; }
  static int frame()                                          { return s_frame; }
  static int frameLine()                                      { return s_frameLine; }
  static int subCarrierPhase()                                { return *s_subCarrierPhase; }
  static int pictureLine()                                    { return s_scanLine; }
  static bool lineSwitch()                                    { return s_lineSwitch; }
  static scPhases_t * lineSampleToSubCarrierSample()          { return (scPhases_t*) s_lineSampleToSubCarrierSample; }
  static int firstVisibleSample()                             { return s_firstVisibleSample; }     // first visible sample in a line
  
  int visibleLines()                                          { return m_visibleLines; }           // visible lines in a field
  int visibleSamples()                                        { return s_visibleSamplesCount; }    // visible samples in a line
    
  CVBSParams const * params()                                 { return m_params; }


private:

  void runDMA(lldesc_t volatile * dmaBuffers);
  volatile lldesc_t * setDMANode(int index, volatile uint16_t * buf, int len);
  void closeDMAChain(int index);
  void buildDMAChain();
  void buildDMAChain_subCarrierOnly();
  void addExtraSamples(double us, double * aus, int * node);
  static void ISRHandler(void * arg);


  // live counters
  static volatile int           s_scanLine;
  static volatile bool          s_VSync;
  static volatile int           s_field;                        // current field: 0 = first field, 1 = second field
  static volatile int           s_frame;                        // current frame: 0 to m_params->frameGroupCount - 1
  static volatile int           s_frameLine;                    // current frame line: 0 to m_params->fieldLines * 2 - 1. Integer part only (ie line 200.5 will be 200). This is not equivalent to image "scanline" in case of interlaced fields.
  static volatile int           s_activeLineIndex;              // current active line index: 0... active line index (reset for each field)
  static volatile scPhases_t *  s_subCarrierPhase;              // ptr to current subcarrier phase: sample index in m_colorBurstLUT[] LUT, 0..CVBS_SUBCARRIERPHASES*2
  static volatile bool          s_lineSwitch;                   // line switch

  gpio_num_t                    m_gpio;
  bool                          m_DMAStarted;
  lldesc_t volatile *           m_DMAChain;
  
  // signals buffers
  volatile uint16_t *           m_lsyncBuf;                     // vertical blank, long pulse buffer
  volatile uint16_t *           m_ssyncBuf;                     // vertical blank, short pulse buffer (equalizing pulse)
  volatile uint16_t * *         m_lineBuf;                      // hsync + back porch + line + front porch
  
  // not allocated buffers
  volatile uint16_t *           m_blackBuffer;                  // derived from ending black part of m_ssyncBuf
  int                           m_blackBufferLength;            // number of available samples in m_blackBuffer
  
  
  intr_handle_t                 m_isr_handle;
  static volatile int16_t       s_visibleSamplesCount;          // visible samples in a line
  static volatile int16_t       s_firstVisibleSample;           // first visible sample (0...)
  CVBSDrawScanlineCallback      m_drawScanlineCallback;
  void *                        m_drawScanlineArg;
  volatile int16_t              m_visibleLines;                 // visible lines in a field
  int16_t                       m_firstVisibleFieldLine;        // 1...
  int16_t                       m_lastVisibleFieldLine;         // 1...
  volatile int16_t              m_firstActiveFrameLine[2];      // first active frame line for specified field (0..) in range 0..
  volatile int16_t              m_firstVisibleFrameLine[2];     // first visible frame line for specified field (0..) in range 0..
  volatile int16_t              m_lastVisibleFrameLine[2];      // last visible frame line for specified field (0..) in range 0..
  volatile int16_t              m_startingScanLine[2];          // starting scanline for each field (0..) in range 0..
  volatile scPhases_t *         m_subCarrierPhases[4];          // subcarrier phase for [frame][frameLine], in samples from start of hsync
  volatile uint16_t             m_colorBurstLUT[2][CVBS_SUBCARRIERPHASES * 2];
  volatile uint16_t             m_firstColorBurstSample;        // sample where color burst starts (starting from hsync)
  volatile uint16_t             m_lastColorBurstSample;         // sample where color burst ends
  volatile int                  m_linesPerFrame;                // number of lines in a frame
  static volatile scPhases_t *  s_lineSampleToSubCarrierSample; // converts a line sample (full line, from hsync to front porch) to m_colorBurstLUT[] item
  double                        m_actualLine_us;                // actual value of m_params->line_us, after samples alignment
  double                        m_actualHLine_us;               // actual value of m_params->hline_us, after samples alignment
  double                        m_sample_us;                    // duration of a sample
  bool                          m_firstActiveFieldLineSwitch[4][2];  // line switch state for first active line at frame (0..) and field (0..)
  
  CVBSParams const *            m_params;                       // decides the CVBS standard (PAL, NTSC...)
  
};



} // namespace fabgl
