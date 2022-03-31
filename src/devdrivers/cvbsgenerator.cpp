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


#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/dac.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"
#include <soc/sens_reg.h>

#include "fabutils.h"
#include "devdrivers/cvbsgenerator.h"


#pragma GCC optimize ("O2")


namespace fabgl {


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CVBS Standards


// interlaced PAL-B (max 640x480)
static const struct CVBS_I_PAL_B : CVBSParams {
  CVBS_I_PAL_B() {
    desc                         = "I-PAL-B";
    
    //sampleRate_hz                = 17750000.0;  // = 1136/64*1000000
    //sampleRate_hz                = 4433618.75*4;  // = 1136/64*1000000
    sampleRate_hz                = 17500000.0; // 1120/64*1000000
    
    subcarrierFreq_hz            = 4433618.75;
    line_us                      = 64.0;
    hline_us                     = 32.0;
    hsync_us                     = 4.7;
    backPorch_us                 = 5.7;
    frontPorch_us                = 1.65;
    hblank_us                    = 1.0;
    burstCycles                  = 10.0;
    burstStart_us                = 0.9;
    fieldLines                   = 312.5;
    longPulse_us                 = 27.3;
    shortPulse_us                = 2.35;
    hsyncEdge_us                 = 0.3;
    vsyncEdge_us                 = 0.2;
    blankLines                   = 19;
    frameGroupCount              = 4;
    preEqualizingPulseCount      = 0;
    vsyncPulseCount              = 5;
    postEqualizingPulseCount     = 5;
    endFieldEqualizingPulseCount = 5;
    syncLevel                    = 0;
    blackLevel                   = 25;
    whiteLevel                   = 79;
    burstAmp                     = 12;
    defaultVisibleSamples        = 640;
    defaultVisibleLines          = 240;
    fieldStartingLine[0]         = 1;
    fieldStartingLine[1]         = 2;
    fields                       = 2;
    interlaceFactor              = 2;
  }

  double getComposite(bool oddLine, double phase, double red, double green, double blue, double * Y) const {
    *Y       = red * .299 + green * .587 + blue * .114;
    double U = 0.493 * (blue - *Y);
    double V = 0.877 * (red - *Y);
    return U * sin(phase) + (oddLine ? 1. : -1.) * V * cos(phase);
  }
  double getColorBurst(bool oddLine, double phase) const {
    // color burst is still composed by V and U signals, but U is permanently inverted (-sin...). This results in +135/-135 degrees swinging burst!
    return -sin(phase) + (oddLine ? 1. : -1.) * cos(phase);
  }
  // to support burst-blanking (Bruch blanking, Bruch sequence)... and to make my Tektronix VM700 happy!
  bool lineHasColorBurst(int frame, int frameLine) const {
    if (((frame == 1 || frame == 3) && (frameLine < 7 || (frameLine > 309 && frameLine < 319) || frameLine > 621)) ||
        ((frame == 2 || frame == 4) && (frameLine < 6 || (frameLine > 310 && frameLine < 320) || frameLine > 622)))
      return false; // no color burst
    return true;
  }
  
} CVBS_I_PAL_B;


// interlaced PAL-B wide (max 768x480)
static const struct CVBS_I_PAL_B_WIDE : CVBS_I_PAL_B {
  CVBS_I_PAL_B_WIDE() : CVBS_I_PAL_B() {
    desc                         = "I-PAL-B-WIDE";
    defaultVisibleSamples        = 768;
  }
} CVBS_I_PAL_B_WIDE;


// progressive PAL-B (max 640x240)
static const struct CVBS_P_PAL_B : CVBS_I_PAL_B {
  CVBS_P_PAL_B() : CVBS_I_PAL_B() {
    desc                         = "P-PAL-B";
    fieldStartingLine[0]         = 1;
    fieldStartingLine[1]         = 1;
    interlaceFactor              = 1;
  }
} CVBS_P_PAL_B;


// progressive PAL-B wide (max 768x240)
static const struct CVBS_P_PAL_B_WIDE : CVBS_P_PAL_B {
  CVBS_P_PAL_B_WIDE() : CVBS_P_PAL_B() {
    desc                         = "P-PAL-B-WIDE";
    defaultVisibleSamples        = 768;
  }
} CVBS_P_PAL_B_WIDE;


// interlaced NTSC-M (max 640x200)
static const struct CVBS_I_NTSC_M : CVBSParams {
  CVBS_I_NTSC_M() {
    desc                         = "I-NTSC-M";
    sampleRate_hz                = 14223774;    // =904/63.555564*1000000
    subcarrierFreq_hz            = 3579545.45;
    line_us                      = 63.555564;
    hline_us                     = 31.777782;
    hsync_us                     = 4.7;
    backPorch_us                 = 4.5;
    frontPorch_us                = 1.5;
    hblank_us                    = 1.5;
    burstCycles                  = 9.0;
    burstStart_us                = 0.6;
    fieldLines                   = 262.5;
    longPulse_us                 = 27.3;
    shortPulse_us                = 2.3;
    hsyncEdge_us                 = 0.3;
    vsyncEdge_us                 = 0.2;
    blankLines                   = 30;
    frameGroupCount              = 2;
    preEqualizingPulseCount      = 6;
    vsyncPulseCount              = 6;
    postEqualizingPulseCount     = 6;
    endFieldEqualizingPulseCount = 0;
    syncLevel                    = 0;
    blackLevel                   = 25;
    whiteLevel                   = 70;
    burstAmp                     = 15;
    defaultVisibleSamples        = 640;
    defaultVisibleLines          = 200;
    fieldStartingLine[0]         = 1;
    fieldStartingLine[1]         = 2;
    fields                       = 2;
    interlaceFactor              = 2;
  };
  
  double getComposite(bool oddLine, double phase, double red, double green, double blue, double * Y) const {
    *Y       = red * .299 + green * .587 + blue * .114;
    double Q =  .413 * (blue - *Y) + .478 * (red - *Y);
    double I = -.269 * (blue - *Y) + .736 * (red - *Y);
    return Q * sin(phase + TORAD(33.)) + I * cos(phase + TORAD(33.));
  }
  double getColorBurst(bool oddLine, double phase) const {
    // burst is 180˚ on subcarrier
    return sin(phase + TORAD(180.));
  }
  bool lineHasColorBurst(int frame, int frameLine) const {
    return true;
  }
  
} CVBS_I_NTSC_M;


// interlaced NTSC-M wide (max 768x200)
static const struct CVBS_I_NTSC_M_WIDE : CVBS_I_NTSC_M {
  CVBS_I_NTSC_M_WIDE() : CVBS_I_NTSC_M() {
    desc                         = "I-NTSC-M-WIDE";
    defaultVisibleSamples        = 768;
  };
} CVBS_I_NTSC_M_WIDE;


// progressive NTSC-M (max 640x200)
static const struct CVBS_P_NTSC_M : CVBS_I_NTSC_M {
  CVBS_P_NTSC_M() : CVBS_I_NTSC_M() {
    desc                         = "P-NTSC-M";
    fieldStartingLine[0]         = 1;
    fieldStartingLine[1]         = 1;
    interlaceFactor              = 1;
  };
} CVBS_P_NTSC_M;


// progressive NTSC-M wide (max 768x200)
static const struct CVBS_P_NTSC_M_WIDE : CVBS_P_NTSC_M {
  CVBS_P_NTSC_M_WIDE() : CVBS_P_NTSC_M() {
    desc                         = "P-NTSC-M-WIDE";
    defaultVisibleSamples        = 768;
  }
} CVBS_P_NTSC_M_WIDE;


// progressive NTSC-M extended (max 768x240)
static const struct CVBS_P_NTSC_M_EXT : CVBS_P_NTSC_M_WIDE {
  CVBS_P_NTSC_M_EXT() : CVBS_P_NTSC_M_WIDE() {
    desc                         = "P-NTSC-M-EXT";
    defaultVisibleLines          = 240;
    blankLines                   = 17;
  }
} CVBS_P_NTSC_M_EXT;



static CVBSParams const * CVBS_Standards[] = {
  &CVBS_I_PAL_B,
  &CVBS_P_PAL_B,
  &CVBS_I_PAL_B_WIDE,
  &CVBS_P_PAL_B_WIDE,
  &CVBS_I_NTSC_M,
  &CVBS_P_NTSC_M,
  &CVBS_I_NTSC_M_WIDE,
  &CVBS_P_NTSC_M_WIDE,
  &CVBS_P_NTSC_M_EXT,
};



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////



volatile int          CVBSGenerator::s_scanLine;
volatile bool         CVBSGenerator::s_VSync;
volatile int          CVBSGenerator::s_field;
volatile int          CVBSGenerator::s_frame;
volatile int          CVBSGenerator::s_frameLine;
volatile int          CVBSGenerator::s_activeLineIndex;
volatile scPhases_t * CVBSGenerator::s_subCarrierPhase;
volatile scPhases_t * CVBSGenerator::s_lineSampleToSubCarrierSample;
volatile int16_t      CVBSGenerator::s_firstVisibleSample;
volatile int16_t      CVBSGenerator::s_visibleSamplesCount;
volatile bool         CVBSGenerator::s_lineSwitch;


#if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
  volatile uint64_t s_cvbsctrlcycles = 0;
#endif


CVBSGenerator::CVBSGenerator() :
  m_DMAStarted(false),
  m_DMAChain(nullptr),
  m_lsyncBuf(nullptr),
  m_ssyncBuf(nullptr),
  m_lineBuf(nullptr),
  m_isr_handle(nullptr),
  m_drawScanlineCallback(nullptr),
  m_subCarrierPhases(),
  m_params(nullptr)
{
  s_lineSampleToSubCarrierSample = nullptr;
}


// gpio can be:
//    GPIO_NUM_25 : gpio 25 DAC1 connected to DMA, gpio 26 set using setConstDAC()
//    GPIO_NUM_26 : gpio 26 DAC2 connected to DMA, gpio 25 set using setConstDAC()
void CVBSGenerator::setVideoGPIO(gpio_num_t gpio)
{
  m_gpio = gpio;
}


// only I2S0 can control DAC channels
void CVBSGenerator::runDMA(lldesc_t volatile * dmaBuffers)
{
  if (!m_DMAStarted) {
    periph_module_enable(PERIPH_I2S0_MODULE);

    // Initialize I2S device
    I2S0.conf.tx_reset                     = 1;
    I2S0.conf.tx_reset                     = 0;

    // Reset DMA
    I2S0.lc_conf.in_rst                    = 1;
    I2S0.lc_conf.in_rst                    = 0;

    // Reset FIFO
    I2S0.conf.rx_fifo_reset                = 1;
    I2S0.conf.rx_fifo_reset                = 0;
    
    // false = use APLL, true use PLL_D2 clock
    bool usePLL = m_params->sampleRate_hz == 16000000 || m_params->sampleRate_hz == 13333333.3334;

    if (usePLL)
      I2S0.conf_chan.tx_chan_mod = (m_gpio == GPIO_NUM_25 ? 3 : 4);
    else
      I2S0.conf_chan.tx_chan_mod = (m_gpio == GPIO_NUM_25 ? 1 : 2);

    I2S0.fifo_conf.tx_fifo_mod_force_en    = 1;
    I2S0.fifo_conf.tx_fifo_mod             = 1;
    I2S0.fifo_conf.dscr_en                 = 1;

    I2S0.conf.tx_mono                      = 1;   // =0?
    I2S0.conf.tx_start                     = 0;
    I2S0.conf.tx_msb_right                 = 1;
    I2S0.conf.tx_right_first               = 1;
    I2S0.conf.tx_slave_mod                 = 0;
    I2S0.conf.tx_short_sync                = 0;
    I2S0.conf.tx_msb_shift                 = 0;
    
    I2S0.conf2.lcd_en                      = 1;
    I2S0.conf2.camera_en                   = 0;

    if (usePLL) {
      // valid just for 16MHz and 13.333Mhz
      I2S0.clkm_conf.clka_en               = 0;
      I2S0.clkm_conf.clkm_div_a            = m_params->sampleRate_hz == 16000000 ? 2 : 1;
      I2S0.clkm_conf.clkm_div_b            = 1;
      I2S0.clkm_conf.clkm_div_num          = 2;
      I2S0.sample_rate_conf.tx_bck_div_num = 2;
    } else {
      // valid for all other sample rates
      APLLParams p = { 0, 0, 0, 0 };
      double error, out_freq;
      uint8_t a = 1, b = 0;
      APLLCalcParams(m_params->sampleRate_hz * 2., &p, &a, &b, &out_freq, &error);
      I2S0.clkm_conf.val                   = 0;
      I2S0.clkm_conf.clkm_div_b            = b;
      I2S0.clkm_conf.clkm_div_a            = a;
      I2S0.clkm_conf.clkm_div_num          = 2;  // not less than 2
      I2S0.sample_rate_conf.tx_bck_div_num = 1;  // this makes I2S0O_BCK = I2S0_CLK
      rtc_clk_apll_enable(true, p.sdm0, p.sdm1, p.sdm2, p.o_div);
      I2S0.clkm_conf.clka_en               = 1;
    }

    I2S0.sample_rate_conf.tx_bits_mod      = 16;

    // prepares for first frame and field
    s_field = m_params->fields - 1;
    s_frame = m_params->frameGroupCount - 1;
    s_VSync = false;

    // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
    if (m_isr_handle == nullptr) {
      CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
      esp_intr_alloc_pinnedToCore(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, ISRHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
      I2S0.int_clr.val     = 0xFFFFFFFF;
      I2S0.int_ena.out_eof = 1;
    }
    
    I2S0.out_link.addr  = (uint32_t) &dmaBuffers[0];
    I2S0.out_link.start = 1;
    I2S0.conf.tx_start  = 1;

    dac_i2s_enable();
    if (usePLL) {
      // enable both DACs
      dac_output_enable(DAC_CHANNEL_1); // GPIO25: DAC1, right channel
      dac_output_enable(DAC_CHANNEL_2); // GPIO26: DAC2, left channel
    } else {
      // enable just used DAC
      dac_output_enable(m_gpio == GPIO_NUM_25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2);
    }

    m_DMAStarted = true;
  }
}


volatile lldesc_t * CVBSGenerator::setDMANode(int index, volatile uint16_t * buf, int len)
{
  m_DMAChain[index].eof          = 0;
  m_DMAChain[index].sosf         = 0;
  m_DMAChain[index].owner        = 1;
  m_DMAChain[index].qe.stqe_next = (lldesc_t *) (m_DMAChain + index + 1);
  m_DMAChain[index].offset       = 0;
  m_DMAChain[index].size         = len * sizeof(uint16_t);
  m_DMAChain[index].length       = len * sizeof(uint16_t);
  m_DMAChain[index].buf          = (uint8_t*) buf;
  return &m_DMAChain[index];
}


void CVBSGenerator::closeDMAChain(int index)
{
  m_DMAChain[index].qe.stqe_next = (lldesc_t *) m_DMAChain;
}


void CVBSGenerator::addExtraSamples(double us, double * aus, int * node)
{
  int extra_samples = imin((us - *aus) / m_sample_us, m_blackBufferLength) & ~1;
  if (extra_samples > 0) {
    setDMANode((*node)++, m_blackBuffer, extra_samples);
    *aus += extra_samples * m_sample_us;
    printf("added %d extra samples\n", extra_samples);
  }
}


// align samples, incrementing or decrementing value
static int bestAlignValue(int value)
{
  // 2 samples
  //return value & ~1;

  ///*
  // 4 samples (round up or down)
  int up   = (value + 3) & ~3;
  int down = (value & ~3);
  return abs(value - up) < abs(value - down) ? up : down;
  //*/
}


void CVBSGenerator::buildDMAChain()
{
  int lineSamplesCount  = round(m_params->line_us / m_sample_us);
  int hlineSamplesCount = round(m_params->hline_us / m_sample_us);

  //printf("lineSamplesCount  = %d\n", lineSamplesCount);
  //printf("hlineSamplesCount = %d\n\n", hlineSamplesCount);

  // make sizes aligned
  lineSamplesCount      = bestAlignValue(lineSamplesCount);
  hlineSamplesCount     = bestAlignValue(hlineSamplesCount);
    
  m_actualLine_us  = lineSamplesCount * m_sample_us;
  m_actualHLine_us = hlineSamplesCount * m_sample_us;

  //printf("adj lineSamplesCount  = %d\n", lineSamplesCount);
  //printf("adj hlineSamplesCount = %d\n", hlineSamplesCount);
  //printf("actual line duration       = %.6f us\n", m_actualLine_us);
  //printf("actual half line duration  = %.6f us\n", m_actualHLine_us);

  // setup long sync pulse buffer
  m_lsyncBuf = (volatile uint16_t *) heap_caps_malloc(hlineSamplesCount * sizeof(uint16_t), MALLOC_CAP_DMA);
  int lsyncStart = 0;
  int lsyncEnd   = m_params->longPulse_us / m_sample_us;
  int vedgeLen   = ceil(m_params->vsyncEdge_us / m_sample_us);
  for (int s = 0, fallCount = vedgeLen, riseCount = 0; s < hlineSamplesCount; ++s) {
    if (s < lsyncStart + vedgeLen)
      m_lsyncBuf[s ^ 1] = (m_params->syncLevel + m_params->blackLevel * --fallCount / vedgeLen) << 8;    // falling edge
    else if (s <= lsyncEnd - vedgeLen)
      m_lsyncBuf[s ^ 1] = m_params->syncLevel << 8;                                                      // sync
    else if (s < lsyncEnd)
      m_lsyncBuf[s ^ 1] = (m_params->syncLevel + m_params->blackLevel * ++riseCount / vedgeLen) << 8;    // rising edge
    else
      m_lsyncBuf[s ^ 1] = m_params->blackLevel << 8;                                                     // black level
  }
  
  // setup short sync pulse buffer and black buffer
  m_blackBuffer = nullptr;
  m_ssyncBuf    = (volatile uint16_t *) heap_caps_malloc(hlineSamplesCount * sizeof(uint16_t), MALLOC_CAP_DMA);
  int ssyncStart = 0;
  int ssyncEnd   = m_params->shortPulse_us / m_sample_us;
  for (int s = 0, fallCount = vedgeLen, riseCount = 0; s < hlineSamplesCount; ++s) {
    if (s < ssyncStart + vedgeLen)
      m_ssyncBuf[s ^ 1] = (m_params->syncLevel + m_params->blackLevel * --fallCount / vedgeLen) << 8;    // falling edge
    else if (s <= ssyncEnd - vedgeLen)
      m_ssyncBuf[s ^ 1] = m_params->syncLevel << 8;                                                      // sync
    else if (s < ssyncEnd)
      m_ssyncBuf[s ^ 1] = (m_params->syncLevel + m_params->blackLevel * ++riseCount / vedgeLen) << 8;    // rising edge
    else {
      m_ssyncBuf[s ^ 1] = m_params->blackLevel << 8;                                                     // black level
      if (!m_blackBuffer && !(s & 3)) {
        m_blackBuffer       = &m_ssyncBuf[s ^ 1];
        m_blackBufferLength = hlineSamplesCount - s;
      }
    }
  }

  // setup line buffer
  m_lineBuf = (volatile uint16_t * *) malloc(CVBS_ALLOCATED_LINES * sizeof(uint16_t*));
  int hsyncStart = 0;
  int hsyncEnd   = (m_params->hsync_us + m_params->hsyncEdge_us) / m_sample_us;
  int hedgeLen   = ceil(m_params->hsyncEdge_us / m_sample_us);
  for (int l = 0; l < CVBS_ALLOCATED_LINES; ++l) {
    m_lineBuf[l] = (volatile uint16_t *) heap_caps_malloc(lineSamplesCount * sizeof(uint16_t), MALLOC_CAP_DMA);
    for (int s = 0, fallCount = hedgeLen, riseCount = 0; s < lineSamplesCount; ++s) {
      if (s < hsyncStart + hedgeLen)
        m_lineBuf[l][s ^ 1] = (m_params->syncLevel + m_params->blackLevel * --fallCount / hedgeLen) << 8;    // falling edge
      else if (s <= hsyncEnd - hedgeLen)
        m_lineBuf[l][s ^ 1] = m_params->syncLevel << 8;                                                      // sync
      else if (s < hsyncEnd)
        m_lineBuf[l][s ^ 1] = (m_params->syncLevel + m_params->blackLevel * ++riseCount / hedgeLen) << 8;    // rising edge
      else
        m_lineBuf[l][s ^ 1] = m_params->blackLevel << 8;                                                     // back porch, active line, front porch
    }
  }
  
  // line sample (full line, from hsync to front porch) to m_colorBurstLUT[] item
  s_lineSampleToSubCarrierSample = (volatile scPhases_t *) heap_caps_malloc(lineSamplesCount * sizeof(scPhases_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  double K = m_params->subcarrierFreq_hz * CVBS_SUBCARRIERPHASES / m_params->sampleRate_hz;
  for (int s = 0; s < lineSamplesCount; ++s)
    s_lineSampleToSubCarrierSample[s] = round(fmod(K * s, CVBS_SUBCARRIERPHASES));
  
  // setup burst LUT
  for (int line = 0; line < 2; ++line) {
    for (int sample = 0; sample < CVBS_SUBCARRIERPHASES * 2; ++sample) {
      double phase = 2. * M_PI * sample / CVBS_SUBCARRIERPHASES;
      double burst = m_params->getColorBurst(line == 0, phase);
      m_colorBurstLUT[line][sample] = (uint16_t)(m_params->blackLevel + m_params->burstAmp * burst) << 8;
    }
  }
  
  // calculates nodes count
  int DMAChainLength = m_params->preEqualizingPulseCount +
                       m_params->vsyncPulseCount +
                       m_params->postEqualizingPulseCount +
                       m_params->endFieldEqualizingPulseCount +
                       ceil(m_params->fieldLines) * m_params->fields + 2;

  m_DMAChain = (volatile lldesc_t *) heap_caps_malloc(DMAChainLength * sizeof(lldesc_t), MALLOC_CAP_DMA);
  
  //printf("m_DMAChain size: %d bytes (%d items)\n", DMAChainLength * sizeof(lldesc_t), DMAChainLength);

  // associate DMA chain nodes to buffers

  #define SHOWDMADETAILS 0

  int node = 0;
  volatile lldesc_t * nodeptr = nullptr;
  
  // microseconds since start of frame sequence
  double us = m_params->hsyncEdge_us / 2.;
  
  // microseconds since start of frame sequence: actual value, sample size rounded
  double aus = us;

  bool lineSwitch = false;

  for (int frame = 1; frame <= m_params->frameGroupCount; ++frame) {

    #if SHOWDMADETAILS
    printf("frame = %d\n", frame);
    #endif

    // setup subcarrier phases buffer
    m_subCarrierPhases[frame - 1] = (volatile scPhases_t *) heap_caps_malloc(m_linesPerFrame * sizeof(scPhases_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

    double frameLine = 1.0;

    for (int field = 1; field <= m_params->fields; ++field) {

      #if SHOWDMADETAILS
      printf("  field = %d\n", field);
      #endif

      m_startingScanLine[field - 1] = m_params->fieldStartingLine[field - 1] - 1;

      double fieldLine        = 1.0;
      bool   startOfFieldISR  = false;
      bool   firstActiveLine  = true;
      int    activeLineIndex  = 0;
      
      while (fieldLine < m_params->fieldLines + 1.0) {
      
        #if SHOWDMADETAILS
        printf("    frameLine = %f  fieldLine = %f  ", frameLine, fieldLine);
        #endif
        
        double ipart;
        double subCarrierPhase = modf(m_params->subcarrierFreq_hz * aus / 1000000., &ipart); // 0.0 = 0˚ ... 1.0 = 360˚
        
        if (m_params->lineHasColorBurst(frame, frameLine)) {
          // stores subcarrier phase (in samples) for this line
          m_subCarrierPhases[frame - 1][(int)frameLine - 1] = subCarrierPhase * CVBS_SUBCARRIERPHASES;
        } else {
          // no burst for this line
          m_subCarrierPhases[frame - 1][(int)frameLine - 1] = CVBS_NOBURSTFLAG;
        }
        //printf("frame=%d frameLine=%d scPhase=%.1f\n", frame, (int)frameLine, subCarrierPhase * 360.);
        //printf("frame(0)=%d frameLine(0)=%d scPhase=%.1f scSample=%d\n", frame-1, (int)frameLine-1, subCarrierPhase * 360., m_subCarrierPhases[frame - 1][(int)frameLine - 1]);
        
        #if SHOWDMADETAILS
        printf("SCPhase = %.2f  ", subCarrierPhase * 360.);
        printf("BurstPhase = %.2f (%c)  ", fmod(subCarrierPhase * 2. * M_PI + (lineSwitch ? 1 : -1) * 3. * M_PI / 4., 2. * M_PI) / M_PI * 180., lineSwitch ? 'O' : 'E');
        #endif
        
        if (fieldLine < m_params->preEqualizingPulseCount * .5 + 1.0) {
        
          // pre-equalizing short pulse (half line)
          #if SHOWDMADETAILS
          printf("node = %d  - pre-equalizing pulse\n", node);
          #endif
          if (frame == 1) {
            //addExtraSamples(us, &aus, &node);
            nodeptr = setDMANode(node++, m_ssyncBuf, hlineSamplesCount);
          }
          frameLine += .5;
          fieldLine += .5;
          us        += m_params->hline_us;
          aus       += m_actualHLine_us;
        
        } else if (fieldLine < (m_params->preEqualizingPulseCount + m_params->vsyncPulseCount) * .5 + 1.0) {
        
          // vsync long pulse (half line)
          #if SHOWDMADETAILS
          printf("node = %d  - vsync pulse", node);
          #endif
          if (frame == 1) {
            //addExtraSamples(us, &aus, &node);
            nodeptr = setDMANode(node++, m_lsyncBuf, hlineSamplesCount);
            if (!startOfFieldISR) {
              // generate interrupt at the first vsync, this will start drawing first lines
              nodeptr->eof    = 1;
              nodeptr->sosf   = 1;  // internal flag to signal beginning of field
              startOfFieldISR = true;
              #if SHOWDMADETAILS
              printf("    EOF SOSF");
              #endif
            }
          }
          #if SHOWDMADETAILS
          printf("\n");
          #endif
          frameLine += .5;
          fieldLine += .5;
          us        += m_params->hline_us;
          aus       += m_actualHLine_us;
          
        } else if (fieldLine < (m_params->preEqualizingPulseCount + m_params->vsyncPulseCount + m_params->postEqualizingPulseCount) * .5 + 1.0) {
        
          // post-equalizing short pulse (half line)
          #if SHOWDMADETAILS
          printf("node = %d  - post-equalizing pulse\n", node);
          #endif
          if (frame == 1) {
            //addExtraSamples(us, &aus, &node);
            nodeptr = setDMANode(node++, m_ssyncBuf, hlineSamplesCount);
          }
          frameLine += .5;
          fieldLine += .5;
          us        += m_params->hline_us;
          aus       += m_actualHLine_us;
          
        } else if (fieldLine < m_params->fieldLines - m_params->endFieldEqualizingPulseCount * .5 + 1.0) {
        
          // active line
         
          #if SHOWDMADETAILS
          printf("node = %d  - active line, ", node);
          #endif
         
          if (firstActiveLine) {
            m_firstActiveFrameLine[field - 1] = (int)frameLine - 1;
            m_firstActiveFieldLineSwitch[frame - 1][field - 1] = lineSwitch;
            firstActiveLine = false;
            activeLineIndex = 0;
            #if SHOWDMADETAILS
            printf("FIRST ACTIVE, ");
            #endif
          } else
            ++activeLineIndex;
            
          if ((int)fieldLine == m_firstVisibleFieldLine) {
            m_firstVisibleFrameLine[field - 1] = (int)frameLine - 1;
            #if SHOWDMADETAILS
            printf("FIRST VISIBLE, ");
            #endif
          } else if ((int)fieldLine == m_lastVisibleFieldLine) {
            m_lastVisibleFrameLine[field - 1] = (int)frameLine - 1;
            #if SHOWDMADETAILS
            printf("LAST VISIBLE, ");
            #endif
          }
         
          double ipart;
          if (modf(frameLine, &ipart) == .5) {
            // ending half of line (half line)
            #if SHOWDMADETAILS
            printf("ending half");
            #endif
            if (frame == 1) {
              //addExtraSamples(us, &aus, &node);
              nodeptr = setDMANode(node++, m_lineBuf[activeLineIndex % CVBS_ALLOCATED_LINES] + hlineSamplesCount, hlineSamplesCount);
            }
            frameLine += .5;
            fieldLine += .5;
            us        += m_params->hline_us;
            aus       += m_actualHLine_us;
          } else if (fieldLine + 1.0 > m_params->fieldLines + 1.0 - m_params->endFieldEqualizingPulseCount * .5) {
            // beginning half of line (half line)
            #if SHOWDMADETAILS
            printf("beginning half");
            #endif
            if (frame == 1) {
              //addExtraSamples(us, &aus, &node);
              nodeptr = setDMANode(node++, m_lineBuf[activeLineIndex % CVBS_ALLOCATED_LINES], hlineSamplesCount);
            }
            frameLine += .5;
            fieldLine += .5;
            us        += m_params->hline_us;
            aus       += m_actualHLine_us;
          } else {
            // full line
            #if SHOWDMADETAILS
            printf("full");
            #endif
            if (frame == 1) {
              int l = activeLineIndex % CVBS_ALLOCATED_LINES;
              nodeptr = setDMANode(node++, m_lineBuf[l], lineSamplesCount);
            }
            frameLine += 1.0;
            fieldLine += 1.0;
            us        += m_params->line_us;
            aus       += m_actualLine_us;
          }
            
          // generate interrupt every half CVBS_ALLOCATED_LINES
          if (frame == 1 && (activeLineIndex % (CVBS_ALLOCATED_LINES / 2)) == 0) {
            nodeptr->eof = 1;
            #if SHOWDMADETAILS
            printf("    EOF");
            #endif
          }
          #if SHOWDMADETAILS
          printf("\n");
          #endif


        } else {
        
          // end-field equalizing short pulse (half line)
          #if SHOWDMADETAILS
          printf("node = %d  - end-field equalizing pulse\n", node);
          #endif
          if (frame == 1) {
            //addExtraSamples(us, &aus, &node);
            nodeptr = setDMANode(node++, m_ssyncBuf, hlineSamplesCount);
          }
          frameLine += .5;
          fieldLine += .5;
          us        += m_params->hline_us;
          aus       += m_actualHLine_us;
          
        }
        
        if (frameLine == (int)frameLine)
          lineSwitch = !lineSwitch;
        
      } // field-line loop
      
      //if (frame == 1)
      //  addExtraSamples(us, &aus, &node);
            
    } // field loop
    
  } // frame loop
  
  //printf("total us = %f\n", us);
    
  closeDMAChain(node - 1);
  
  //printf("DMAChainLength = %d node = %d\n", DMAChainLength, node);
}


void CVBSGenerator::buildDMAChain_subCarrierOnly()
{
  double fsamplesPerCycle = 1000000. / m_params->subcarrierFreq_hz / m_sample_us;
  int samplesPerCycle = 1000000. / m_params->subcarrierFreq_hz / m_sample_us;
  
  int cycles = 10;
  while ( (fsamplesPerCycle * cycles) - (int)(fsamplesPerCycle * cycles) > 0.5)
    cycles += 1;
    
  samplesPerCycle = fsamplesPerCycle;
  
  int count = (samplesPerCycle * cycles) & ~1;

  //printf("samplesPerCycle=%d m_sample_us=%f subcarrierFreq_hz=%f cycles=%d count=%d\n", samplesPerCycle, m_sample_us, m_params->subcarrierFreq_hz, cycles, count);

  m_lsyncBuf = (volatile uint16_t *) heap_caps_malloc(count * sizeof(uint16_t), MALLOC_CAP_DMA);
  
  auto sinLUT = new uint16_t[CVBS_SUBCARRIERPHASES * 2];
  
  for (int sample = 0; sample < CVBS_SUBCARRIERPHASES * 2; ++sample) {
    double phase = 2. * M_PI * sample / CVBS_SUBCARRIERPHASES;
    double value = sin(phase);
    sinLUT[sample] = (uint16_t)(m_params->blackLevel + m_params->burstAmp * value) << 8;
  }
  
  double K = m_params->subcarrierFreq_hz * CVBS_SUBCARRIERPHASES / m_params->sampleRate_hz;
  
  for (int sample = 0; sample < count; ++sample) {
    auto idx = (int)(K * sample) % CVBS_SUBCARRIERPHASES;
    m_lsyncBuf[sample ^ 1] = sinLUT[idx];
    //printf("%d = %d\n", sample, m_lsyncBuf[sample ^ 1] >> 8);
  }
  
  delete[] sinLUT;
  
  m_DMAChain = (volatile lldesc_t *) heap_caps_malloc(1 * sizeof(lldesc_t), MALLOC_CAP_DMA);
  setDMANode(0, m_lsyncBuf, count);
  closeDMAChain(0);
}


CVBSParams const * CVBSGenerator::getParamsFromDesc(char const * desc)
{
  for (auto std : CVBS_Standards)
    if (strcmp(std->desc, desc) == 0)
      return std;
  return nullptr;
}


void CVBSGenerator::setup(char const * desc)
{
  auto params = getParamsFromDesc(desc);
  setup(params ? params : CVBS_Standards[0]);
}

  
void CVBSGenerator::setup(CVBSParams const * params)
{
  m_params                  = params;
  
  m_sample_us               = 1000000. / m_params->sampleRate_hz;
  
  double activeLine_us      = m_params->line_us - m_params->hsync_us - m_params->backPorch_us - m_params->frontPorch_us;
  int maxVisibleSamples     = activeLine_us / m_sample_us;
  m_linesPerFrame           = m_params->fieldLines * m_params->fields;

  int usableFieldLines      = m_params->fieldLines - m_params->blankLines;
  m_visibleLines            = imin(usableFieldLines, m_params->defaultVisibleLines);
    
  // make sure m_visibleLines is divisible by CVBS_ALLOCATED_LINES
  m_visibleLines -= m_visibleLines % CVBS_ALLOCATED_LINES;
    
  m_firstVisibleFieldLine   = m_params->blankLines + ceil((usableFieldLines - m_visibleLines) / 2.0);
  m_lastVisibleFieldLine    = m_firstVisibleFieldLine + m_visibleLines - 1;
  
  //printf("m_visibleLines = %d m_firstVisibleFieldLine = %d m_lastVisibleFieldLine = %d\n", m_visibleLines, m_firstVisibleFieldLine, m_lastVisibleFieldLine);
  
  int blankSamples          = m_params->hblank_us / m_sample_us;
  int hsyncSamples          = m_params->hsync_us / m_sample_us;
  int backPorchSamples      = m_params->backPorch_us / m_sample_us;
  int usableVisibleSamples  = maxVisibleSamples - blankSamples;
  s_visibleSamplesCount     = imin(usableVisibleSamples, m_params->defaultVisibleSamples);
  s_firstVisibleSample      = (hsyncSamples + backPorchSamples + blankSamples + (usableVisibleSamples - s_visibleSamplesCount) / 2) & ~1; // aligned to 2
  
  double subcarrierCycle_us = 1000000. / m_params->subcarrierFreq_hz; // duration in microseconds of a subcarrier cycle
  
  m_firstColorBurstSample   = (m_params->hsync_us + m_params->hsyncEdge_us / 2. + m_params->burstStart_us) / m_sample_us;
  m_lastColorBurstSample    = m_firstColorBurstSample + (subcarrierCycle_us * m_params->burstCycles) / m_sample_us - 1;
}


void CVBSGenerator::run(bool subCarrierOnly)
{
  if (subCarrierOnly)
    buildDMAChain_subCarrierOnly();
  else
    buildDMAChain();
  runDMA(m_DMAChain);
}


void CVBSGenerator::stop()
{
  if (m_DMAStarted) {

    periph_module_disable(PERIPH_I2S0_MODULE);
    m_DMAStarted = false;

    if (m_isr_handle) {
      esp_intr_free(m_isr_handle);
      m_isr_handle = nullptr;
    }

    // cleanup DMA chain and buffers
    if (m_DMAChain) {
    
      heap_caps_free((void*)m_DMAChain);
      m_DMAChain = nullptr;
      
      if (m_ssyncBuf) {
        heap_caps_free((void*)m_ssyncBuf);
        m_ssyncBuf = nullptr;
      }
      
      if (m_lsyncBuf) {
        heap_caps_free((void*)m_lsyncBuf);
        m_lsyncBuf = nullptr;
      }
      
      if (m_lineBuf) {
        for (int i = 0; i < CVBS_ALLOCATED_LINES; ++i)
          heap_caps_free((void*)m_lineBuf[i]);
        free(m_lineBuf);
        m_lineBuf = nullptr;
      }
      
      for (int frame = 0; frame < m_params->frameGroupCount; ++frame)
        if (m_subCarrierPhases[frame]) {
          heap_caps_free((void*)m_subCarrierPhases[frame]);
          m_subCarrierPhases[frame] = nullptr;
        }
        
      if (s_lineSampleToSubCarrierSample) {
        heap_caps_free((void*)s_lineSampleToSubCarrierSample);
        s_lineSampleToSubCarrierSample = nullptr;
      }
      
    }

  }
}


void CVBSGenerator::setDrawScanlineCallback(CVBSDrawScanlineCallback drawScanlineCallback, void * arg)
{
  m_drawScanlineCallback = drawScanlineCallback;
  m_drawScanlineArg      = arg;
}


void IRAM_ATTR CVBSGenerator::ISRHandler(void * arg)
{
  #if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  if (I2S0.int_st.out_eof) {

    auto ctrl = (CVBSGenerator *) arg;
    auto desc = (volatile lldesc_t*) I2S0.out_eof_des_addr;
    
    // begin of field?
    if (desc->sosf) {
      s_field = (s_field + 1) % ctrl->m_params->fields;
      if (s_field == 0)
        s_frame = (s_frame + 1) % ctrl->m_params->frameGroupCount;  // first field
      s_frameLine        = ctrl->m_firstActiveFrameLine[s_field];
      s_subCarrierPhase  = &(ctrl->m_subCarrierPhases[s_frame][s_frameLine]);
      s_activeLineIndex  = 0;
      s_scanLine         = ctrl->m_startingScanLine[s_field];
      s_lineSwitch       = ctrl->m_firstActiveFieldLineSwitch[s_frame][s_field];
      s_VSync            = false;
    }

    auto drawScanlineCallback  = ctrl->m_drawScanlineCallback;
    auto drawScanlineArg       = ctrl->m_drawScanlineArg;
    auto lineBuf               = ctrl->m_lineBuf;
    auto firstVisibleFrameLine = ctrl->m_firstVisibleFrameLine[s_field];
    auto lastVisibleFrameLine  = ctrl->m_lastVisibleFrameLine[s_field];
    auto firstColorBurstSample = ctrl->m_firstColorBurstSample;
    auto lastColorBurstSample  = ctrl->m_lastColorBurstSample;
    auto interlaceFactor       = ctrl->m_params->interlaceFactor;
    
    for (int i = 0; i < CVBS_ALLOCATED_LINES / 2; ++i) {
    
      auto fullLineBuf = (uint16_t*)lineBuf[s_activeLineIndex % CVBS_ALLOCATED_LINES];

      if (*s_subCarrierPhase == CVBS_NOBURSTFLAG) {
        // no burst for this line
        uint16_t blk = ctrl->m_params->blackLevel << 8;
        for (int s = firstColorBurstSample; s <= lastColorBurstSample; ++s)
          fullLineBuf[s ^ 1] = blk;
      } else {
        // fill color burst
        auto colorBurstLUT = (uint16_t *) ctrl->m_colorBurstLUT[s_lineSwitch];
        auto sampleLUT     = CVBSGenerator::lineSampleToSubCarrierSample() + firstColorBurstSample;
        for (int s = firstColorBurstSample; s <= lastColorBurstSample; ++s)
          fullLineBuf[s ^ 1] = colorBurstLUT[*sampleLUT++ + *s_subCarrierPhase];
      }
      
      // fill active area
      if (s_frameLine >= firstVisibleFrameLine && s_frameLine <= lastVisibleFrameLine) {
        // visible lines
        drawScanlineCallback(drawScanlineArg, fullLineBuf, s_firstVisibleSample, s_scanLine);
        s_scanLine += interlaceFactor; // +2 if interlaced, +1 if progressive
      } else {
        // blank lines
        auto visibleBuf = fullLineBuf + s_firstVisibleSample;
        uint32_t blackFillX2 = (ctrl->m_params->blackLevel << 8) | (ctrl->m_params->blackLevel << (8 + 16));
        for (int col = 0; col < s_visibleSamplesCount; col += 2, visibleBuf += 2)
          *((uint32_t*)(visibleBuf)) = blackFillX2;
      }
      
      ++s_activeLineIndex;
      ++s_frameLine;
      ++s_subCarrierPhase;
      s_lineSwitch = !s_lineSwitch;
      
    }

    if (s_frameLine >= lastVisibleFrameLine)
      s_VSync = true;
  }

  #if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
  s_cvbsctrlcycles += getCycleCount() - s1;
  #endif

  I2S0.int_clr.val = I2S0.int_st.val;
}



} // namespace fabgl
