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




#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <string.h>
#include <ctype.h>
#include <math.h>

#include "driver/dac.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"
#include <soc/sens_reg.h>
#include "esp_log.h"
#include "driver/sigmadelta.h"


#include "soundgen.h"


#pragma GCC optimize ("O2")


namespace fabgl {



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SineWaveformGenerator


static const int8_t sinTable[257] = {
   0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
  49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
  90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
 117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
 127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
 117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
  90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
  49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
   0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
 -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
 -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
-117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
-127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
-117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
 -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
 -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
   0,
};


SineWaveformGenerator::SineWaveformGenerator()
 : m_phaseInc(0),
   m_phaseAcc(0),
   m_frequency(0),
   m_lastSample(0)
{
}


void SineWaveformGenerator::setFrequency(int value) {
  if (m_frequency != value) {
    m_frequency = value;
    m_phaseInc = (((uint32_t)m_frequency * 256) << 11) / sampleRate();
  }
}


int SineWaveformGenerator::getSample() {
  if (m_frequency == 0 || duration() == 0) {
    if (m_lastSample > 0)
      --m_lastSample;
    else if (m_lastSample < 0)
      ++m_lastSample;
    else
      m_phaseAcc = 0;
    return m_lastSample;
  }

  // get sample  (-128...+127)
  uint32_t index = m_phaseAcc >> 11;
  int sample = sinTable[index];

  // process volume
  sample = sample * volume() / 127;

  m_lastSample = sample;

  m_phaseAcc = (m_phaseAcc + m_phaseInc) & 0x7ffff;

  decDuration();

  return sample;
}


// SineWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SquareWaveformGenerator


SquareWaveformGenerator::SquareWaveformGenerator()
  : m_phaseInc(0),
    m_phaseAcc(0),
    m_frequency(0),
    m_lastSample(0),
    m_dutyCycle(127)
{
}


void SquareWaveformGenerator::setFrequency(int value) {
  if (m_frequency != value) {
    m_frequency = value;
    m_phaseInc = (((uint32_t)m_frequency * 256) << 11) / sampleRate();
  }
}


// dutyCycle: 0..255 (255=100%)
void SquareWaveformGenerator::setDutyCycle(int dutyCycle)
{
  m_dutyCycle = dutyCycle;
}


int SquareWaveformGenerator::getSample() {
  if (m_frequency == 0 || duration() == 0) {
    if (m_lastSample > 0)
      --m_lastSample;
    else if (m_lastSample < 0)
      ++m_lastSample;
    else
      m_phaseAcc = 0;
    return m_lastSample;
  }

  uint32_t index = m_phaseAcc >> 11;
  int sample = (index <= m_dutyCycle ? 127 : -127);

  // process volume
  sample = sample * volume() / 127;

  m_lastSample = sample;

  m_phaseAcc = (m_phaseAcc + m_phaseInc) & 0x7ffff;

  decDuration();

  return sample;
}

// SquareWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TriangleWaveformGenerator


TriangleWaveformGenerator::TriangleWaveformGenerator()
  : m_phaseInc(0),
    m_phaseAcc(0),
    m_frequency(0),
    m_lastSample(0)
{
}


void TriangleWaveformGenerator::setFrequency(int value) {
  if (m_frequency != value) {
    m_frequency = value;
    m_phaseInc = (((uint32_t)m_frequency * 256) << 11) / sampleRate();
  }
}


int TriangleWaveformGenerator::getSample() {
  if (m_frequency == 0 || duration() == 0) {
    if (m_lastSample > 0)
      --m_lastSample;
    else if (m_lastSample < 0)
      ++m_lastSample;
    else
      m_phaseAcc = 0;
    return m_lastSample;
  }

  uint32_t index = m_phaseAcc >> 11;
  int sample = (index & 0x80 ? -1 : 1) * ((index & 0x3F) * 2 - (index & 0x40 ? 0 : 127));

  // process volume
  sample = sample * volume() / 127;

  m_lastSample = sample;

  m_phaseAcc = (m_phaseAcc + m_phaseInc) & 0x7ffff;

  decDuration();

  return sample;
}

// TriangleWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SawtoothWaveformGenerator


SawtoothWaveformGenerator::SawtoothWaveformGenerator()
  : m_phaseInc(0),
    m_phaseAcc(0),
    m_frequency(0),
    m_lastSample(0)
{
}


void SawtoothWaveformGenerator::setFrequency(int value) {
  if (m_frequency != value) {
    m_frequency = value;
    m_phaseInc = (((uint32_t)m_frequency * 256) << 11) / sampleRate();
  }
}


int SawtoothWaveformGenerator::getSample() {
  if (m_frequency == 0 || duration() == 0) {
    if (m_lastSample > 0)
      --m_lastSample;
    else if (m_lastSample < 0)
      ++m_lastSample;
    else
      m_phaseAcc = 0;
    return m_lastSample;
  }

  uint32_t index = m_phaseAcc >> 11;
  int sample = index - 128;

  // process volume
  sample = sample * volume() / 127;

  m_lastSample = sample;

  m_phaseAcc = (m_phaseAcc + m_phaseInc) & 0x7ffff;

  decDuration();

  return sample;
}

// TriangleWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NoiseWaveformGenerator


NoiseWaveformGenerator::NoiseWaveformGenerator()
  : m_noise(0xFAB7)
{
}


void NoiseWaveformGenerator::setFrequency(int value)
{
}


int NoiseWaveformGenerator::getSample()
{
  if (duration() == 0) {
    return 0;
  }

  // noise generator based on Galois LFSR
  m_noise = (m_noise >> 1) ^ (-(m_noise & 1) & 0xB400u);
  int sample = 127 - (m_noise >> 8);

  // process volume
  sample = sample * volume() / 127;

  decDuration();

  return sample;
}


// NoiseWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////
// VICNoiseGenerator
// "tries" to emulate VIC6561 noise generator
// derived from a reverse engineering VHDL code: http://sleepingelephant.com/ipw-web/bulletin/bb/viewtopic.php?f=11&t=8733&fbclid=IwAR16x6OMMb670bA2ZtYcbB0Zat_X-oHNB0NxxYigPffXa8G_InSIoNAEiPU


VICNoiseGenerator::VICNoiseGenerator()
  : m_frequency(0),
    m_counter(0),
    m_LFSR(LFSRINIT),
    m_outSR(0)
{
}


void VICNoiseGenerator::setFrequency(int value)
{
  if (m_frequency != value) {
    m_frequency = value >= 127 ? 0 : value;
    m_LFSR      = LFSRINIT;
    m_counter   = 0;
    m_outSR     = 0;
  }
}


int VICNoiseGenerator::getSample()
{
  if (duration() == 0) {
    return 0;
  }

  const int reduc = CLK / 8 / sampleRate(); // resample to sampleRate() (ie 16000Hz)

  int sample = 0;

  for (int i = 0; i < reduc; ++i) {

    if (m_counter >= 127) {

      // reset counter
      m_counter = m_frequency;

      if (m_LFSR & 1)
        m_outSR = ((m_outSR << 1) | ~(m_outSR >> 7));

      m_LFSR <<= 1;
      int bit3  = (m_LFSR >> 3) & 1;
      int bit12 = (m_LFSR >> 12) & 1;
      int bit14 = (m_LFSR >> 14) & 1;
      int bit15 = (m_LFSR >> 15) & 1;
      m_LFSR |= (bit3 ^ bit12) ^ (bit14 ^ bit15);
    } else
      ++m_counter;

    sample += m_outSR & 1 ? 127 : -128;
  }

  // simple mean of all samples

  sample = sample / reduc;

  // process volume
  sample = sample * volume() / 127;

  decDuration();

  return sample;
}


// VICNoiseGenerator
/////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SamplesGenerator


SamplesGenerator::SamplesGenerator(int8_t const * data, int length)
  : m_data(data),
    m_length(length),
    m_index(0)
{
}


void SamplesGenerator::setFrequency(int value)
{
}


int SamplesGenerator::getSample() {

  if (duration() == 0) {
    return 0;
  }

  int sample = m_data[m_index++];

  if (m_index == m_length)
    m_index = 0;

  // process volume
  sample = sample * volume() / 127;

  decDuration();

  return sample;
}


// SamplesGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SoundGenerator


SoundGenerator::SoundGenerator(int sampleRate, gpio_num_t gpio, SoundGenMethod genMethod)
  : m_channels(nullptr),
    m_sampleBuffer{0},
    m_volume(100),
    m_sampleRate(sampleRate),
    m_play(false),
    m_gpio(gpio),
    m_isr_handle(nullptr),
    m_DMAChain(nullptr),
    m_genMethod(genMethod),
    m_initDone(false),
    m_timerHandle(nullptr)
{
}


SoundGenerator::~SoundGenerator()
{
  clear();
    
  if (m_isr_handle) {
    // cleanup DAC mode
    periph_module_disable(PERIPH_I2S0_MODULE);
    esp_intr_free(m_isr_handle);
    for (int i = 0; i < 2; ++i)
      heap_caps_free(m_sampleBuffer[i]);
    heap_caps_free((void*)m_DMAChain);
  }
  
  if (m_timerHandle) {
    // cleanup sigmadelta mode
    esp_timer_stop(m_timerHandle);
    esp_timer_delete(m_timerHandle);
    m_timerHandle = nullptr;
  }
  
  #ifdef FABGL_EMULATED
  SDL_CloseAudioDevice(m_device);
  #endif
    
}


void SoundGenerator::clear()
{
  play(false);
  m_channels = nullptr;
}


void SoundGenerator::setDMANode(int index, volatile uint16_t * buf, int len)
{
  m_DMAChain[index].eof          = 1; // always generate interrupt
  m_DMAChain[index].sosf         = 0;
  m_DMAChain[index].owner        = 1;
  m_DMAChain[index].qe.stqe_next = (lldesc_t *) (m_DMAChain + index + 1);
  m_DMAChain[index].offset       = 0;
  m_DMAChain[index].size         = len * sizeof(uint16_t);
  m_DMAChain[index].length       = len * sizeof(uint16_t);
  m_DMAChain[index].buf          = (uint8_t*) buf;
}


void SoundGenerator::dac_init()
{
  m_DMAChain = (volatile lldesc_t *) heap_caps_malloc(2 * sizeof(lldesc_t), MALLOC_CAP_DMA);
  
  for (int i = 0; i < 2; ++i) {
    m_sampleBuffer[i] = (uint16_t *) heap_caps_malloc(FABGL_SOUNDGEN_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    for (int j = 0; j < FABGL_SOUNDGEN_SAMPLE_BUFFER_SIZE; ++j)
      m_sampleBuffer[i][j] = 0x7f00;
    setDMANode(i, m_sampleBuffer[i], FABGL_SOUNDGEN_SAMPLE_BUFFER_SIZE);
  }
  m_DMAChain[1].sosf = 1;
  m_DMAChain[1].qe.stqe_next = (lldesc_t *) m_DMAChain; // closes DMA chain
    
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
  
  I2S0.conf_chan.tx_chan_mod = (m_gpio == GPIO_NUM_25 ? 3 : 4);

  I2S0.fifo_conf.tx_fifo_mod_force_en    = 1;
  I2S0.fifo_conf.tx_fifo_mod             = 1;
  I2S0.fifo_conf.dscr_en                 = 1;

  I2S0.conf.tx_mono                      = 1;
  I2S0.conf.tx_start                     = 0;
  I2S0.conf.tx_msb_right                 = 1;
  I2S0.conf.tx_right_first               = 1;
  I2S0.conf.tx_slave_mod                 = 0;
  I2S0.conf.tx_short_sync                = 0;
  I2S0.conf.tx_msb_shift                 = 0;
  
  I2S0.conf2.lcd_en                      = 1;
  I2S0.conf2.camera_en                   = 0;

  int a, b, num, m;
  m_sampleRate = calcI2STimingParams(m_sampleRate, &a, &b, &num, &m);
  I2S0.clkm_conf.clka_en                 = 0;
  I2S0.clkm_conf.clkm_div_a              = a;
  I2S0.clkm_conf.clkm_div_b              = b;
  I2S0.clkm_conf.clkm_div_num            = num;
  I2S0.sample_rate_conf.tx_bck_div_num   = m;

  I2S0.sample_rate_conf.tx_bits_mod      = 16;

  if (m_isr_handle == nullptr) {
    esp_intr_alloc_pinnedToCore(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1, ISRHandler, this, &m_isr_handle, CoreUsage::quietCore());
    I2S0.int_clr.val     = 0xFFFFFFFF;
    I2S0.int_ena.out_eof = 1;
  }
  
  I2S0.out_link.addr  = (uintptr_t) m_DMAChain;
  I2S0.out_link.start = 1;
  I2S0.conf.tx_start  = 1;

  dac_i2s_enable();
  dac_output_enable(m_gpio == GPIO_NUM_25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2);
}


void SoundGenerator::sigmadelta_init()
{
  sigmadelta_config_t sigmadelta_cfg;
  sigmadelta_cfg.channel             = SIGMADELTA_CHANNEL_0;
  sigmadelta_cfg.sigmadelta_prescale = 10;
  sigmadelta_cfg.sigmadelta_duty     = 0;
  sigmadelta_cfg.sigmadelta_gpio     = m_gpio;
  sigmadelta_config(&sigmadelta_cfg);

  esp_timer_create_args_t args = { };
  args.callback        = timerHandler;
  args.arg             = this;
  args.dispatch_method = ESP_TIMER_TASK;
  esp_timer_create(&args, &m_timerHandle);
}


#ifdef FABGL_EMULATED
void SoundGenerator::sdl_init()
{
  SDL_AudioSpec wantSpec, haveSpec;
  SDL_zero(wantSpec);
  wantSpec.freq     = m_sampleRate;
  wantSpec.format   = AUDIO_U8;
  wantSpec.channels = 1;
  wantSpec.samples  = 2048;
  wantSpec.callback = SDLAudioCallback;
  wantSpec.userdata = this;
  m_device = SDL_OpenAudioDevice(NULL, 0, &wantSpec, &haveSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  m_sampleRate = haveSpec.freq;
}
#endif


void SoundGenerator::init()
{
  if (!m_initDone) {
    // handle automatic paramters
    if (m_genMethod == SoundGenMethod::Auto)
      m_genMethod = CurrentVideoMode::get() == VideoMode::CVBS ? SoundGenMethod::SigmaDelta : SoundGenMethod::DAC;
    if (m_gpio == GPIO_AUTO)
      m_gpio = m_genMethod == SoundGenMethod::DAC ? GPIO_NUM_25 : GPIO_NUM_23;
    
    // actual init
    if (m_genMethod == SoundGenMethod::DAC)
      dac_init();
    else
      sigmadelta_init();
      
    #ifdef FABGL_EMULATED
    sdl_init();
    #endif
      
    m_initDone = true;
  }
}


bool SoundGenerator::play(bool value)
{
  if (value != m_play) {
    init();
    
    if (m_genMethod == SoundGenMethod::DAC) {
      I2S0.conf.tx_start = value;
    } else {
      if (value)
        esp_timer_start_periodic(m_timerHandle, 1000000 / m_sampleRate);
      else
        esp_timer_stop(m_timerHandle);
    }
    
    #ifdef FABGL_EMULATED
    SDL_PauseAudioDevice(m_device, !value);
    #endif
    
    m_play = value;
    return !value;
  } else
    return value;
}


SamplesGenerator * SoundGenerator::playSamples(int8_t const * data, int length, int volume, int durationMS)
{
  auto sgen = new SamplesGenerator(data, length);
  attach(sgen);
  sgen->setAutoDestroy(true);
  if (durationMS > -1)
    sgen->setDuration(durationMS > 0 ? (m_sampleRate / 1000 * durationMS) : length );
  sgen->setVolume(volume);
  sgen->enable(true);
  play(true);
  return sgen;
}


// does NOT take ownership of the waveform generator
void SoundGenerator::attach(WaveformGenerator * value)
{
  bool isPlaying = play(false);

  value->setSampleRate(m_sampleRate);

  value->next = m_channels;
  m_channels = value;

  play(isPlaying);
}


void SoundGenerator::detach(WaveformGenerator * value)
{
  if (!value)
    return;

  bool isPlaying = play(false);
  detachNoSuspend(value);
  play(isPlaying);
}


void SoundGenerator::detachNoSuspend(WaveformGenerator * value)
{
  for (WaveformGenerator * c = m_channels, * prev = nullptr; c; prev = c, c = c->next) {
    if (c == value) {
      if (prev)
        prev->next = c->next;
      else
        m_channels = c->next;
      if (value->autoDestroy())
        delete value;
      break;
    }
  }
}


int IRAM_ATTR SoundGenerator::getSample()
{
  int sample = 0, tvol = 0;
  for (auto g = m_channels; g; ) {
    if (g->enabled()) {
      sample += g->getSample();
      tvol += g->volume();
    } else if (g->duration() == 0 && g->autoDetach()) {
      auto curr = g;
      g = g->next;  // setup next item before detaching this one
      detachNoSuspend(curr);
      continue; // bypass "g = g->next;"
    }
    g = g->next;
  }

  int avol = tvol ? imin(127, 127 * 127 / tvol) : 127;
  sample = sample * avol / 127;
  sample = sample * volume() / 127;
  
  return sample;
}


// used by DAC generator
void IRAM_ATTR SoundGenerator::ISRHandler(void * arg)
{
  if (I2S0.int_st.out_eof) {

    auto soundGenerator = (SoundGenerator *) arg;
    auto desc = (volatile lldesc_t*) I2S0.out_eof_des_addr;
    
    auto buf = (uint16_t *) soundGenerator->m_sampleBuffer[desc->sosf];
    
    for (int i = 0; i < FABGL_SOUNDGEN_SAMPLE_BUFFER_SIZE; ++i)
      buf[i ^ 1] = (soundGenerator->getSample() + 127) << 8;
    
  }
  I2S0.int_clr.val = I2S0.int_st.val;
}


// used by sigma-delta generator
void SoundGenerator::timerHandler(void * args)
{
  auto soundGenerator = (SoundGenerator *) args;

  sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, soundGenerator->getSample());
}


#ifdef FABGL_EMULATED
void SoundGenerator::SDLAudioCallback(void * data, Uint8 * buffer, int length)
{
  auto soundGenerator = (SoundGenerator *) data;

  for (int i = 0; i < length; ++i)
    buffer[i] = soundGenerator->getSample() + 127;
}
#endif


// SoundGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



/*

// note (C,D,E,F,G,A,B) + [#,b] + octave (2..7) + space + tempo (99..1)
// pause (P) + space + tempo (99.1)
char const * noteToFreq(char const * note, int * freq)
{
  uint16_t NIDX2FREQ[][12] = { {   66,   70,   74,   78,   83,   88,   93,   98,  104,  110,  117,  124 }, // 2
                               {  131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247 }, // 3
                               {  262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494 }, // 4
                               {  523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988 }, // 5
                               { 1046, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976 }, // 6
                               { 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951 }, // 7
                             };
  uint8_t NNAME2NIDX[] = {9, 11, 0, 2, 4, 5, 7};  // A, B, C, D, E, F, G
  *freq = 0;
  while (*note && *note == ' ')
    ++note;
  if (*note == 0)
    return note;
  int noteIndex = (*note >= 'A' && *note <= 'G' ? NNAME2NIDX[*note - 'A'] : -2); // -2 = pause
  ++note;
  if (*note == '#') {
    ++noteIndex;
    ++note;
  } else if (*note == 'b') {
    --noteIndex;
    ++note;
  }
  int octave = *note - '0';
  ++note;
  if (noteIndex == -1) {
    noteIndex = 11;
    --octave;
  } else if (noteIndex == 12) {
    noteIndex = 0;
    ++octave;
  }
  if (noteIndex >= 0 && noteIndex <= 11 && octave >= 2 && octave <= 7)
    *freq = NIDX2FREQ[octave - 2][noteIndex];
  return note;
}


char const * noteToDelay(char const * note, int * delayMS)
{
  *delayMS = 0;
  while (*note && *note == ' ')
    ++note;
  if (*note == 0)
    return note;
  int val = atoi(note);
  if (val > 0)
    *delayMS = 1000 / val;
  return note + (val > 9 ? 2 : 1);
}



void play_task(void*arg)
{
  const char * music = "A4 4 A4 4 A#4 4 C5 4 C5 4 A#4 4 A4 4 G4 4 F4 4 F4 4 G4 4 A4 4 A4 2 G4 16 G4 2 P 8 "
                       "A4 4 A4 4 A#4 4 C5 4 C5 4 A#4 4 A4 4 G4 4 F4 4 F4 4 G4 4 A4 4 G4 2 F4 16 F4 2 P 8";
  while (true) {
    if (Play) {
      const char * m = music;
      while (*m && Play) {
        int freq, delms;
        m = noteToFreq(m, &freq);
        m = noteToDelay(m, &delms);
        frequency = freq;
        delay(delms);
        frequency = 0;
        delay(25);
      }
      Play = false;
    } else
      delay(100);
  }
}


*/








} // end of namespace

