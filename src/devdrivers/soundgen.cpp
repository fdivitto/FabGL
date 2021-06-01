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




#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <string.h>
#include <ctype.h>
#include <math.h>

#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#if __has_include("hal/i2s_types.h")
  #include "hal/i2s_types.h"
#endif
#include "esp_log.h"


#include "soundgen.h"


#pragma GCC optimize ("O2")


namespace fabgl {


// maximum value is I2S_SAMPLE_BUFFER_SIZE
#define FABGL_SAMPLE_BUFFER_SIZE 32



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
  double fmul = (double)(m_phaseAcc & 0x7ff) / 2048.0;
  int sample = sinTable[index] + (sinTable[index + 1] - sinTable[index]) * fmul;

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


// NoiseWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SoundGenerator


SoundGenerator::SoundGenerator(int sampleRate)
  : m_waveGenTaskHandle(nullptr),
    m_channels(nullptr),
    m_sampleBuffer(nullptr),
    m_volume(100),
    m_sampleRate(sampleRate),
    m_play(false),
    m_state(SoundGeneratorState::Stop)
{
  m_mutex = xSemaphoreCreateMutex();
  i2s_audio_init();
}


SoundGenerator::~SoundGenerator()
{
  clear();
  vTaskDelete(m_waveGenTaskHandle);
  heap_caps_free(m_sampleBuffer);
  vSemaphoreDelete(m_mutex);
}


void SoundGenerator::clear()
{
  AutoSemaphore autoSemaphore(m_mutex);
  play(false);
  m_channels = nullptr;
}


void SoundGenerator::i2s_audio_init()
{
  i2s_config_t i2s_config;
  i2s_config.mode                 = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  i2s_config.sample_rate          = m_sampleRate;
  i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  #if FABGL_ESP_IDF_VERSION <= FABGL_ESP_IDF_VERSION_VAL(4, 1, 1)
    i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
  #else
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  #endif
  i2s_config.channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT;
  i2s_config.intr_alloc_flags     = 0;
  i2s_config.dma_buf_count        = 2;
  i2s_config.dma_buf_len          = FABGL_SAMPLE_BUFFER_SIZE * sizeof(uint16_t);
  i2s_config.use_apll             = 0;
  i2s_config.tx_desc_auto_clear   = 0;
  i2s_config.fixed_mclk           = 0;
  // install and start i2s driver
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  // init DAC pad
  i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN); // GPIO25

  m_sampleBuffer = (uint16_t*) heap_caps_malloc(FABGL_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}


// the same of forcePlay(), but fill also output DMA with 127s, making output mute (and making "bumping" effect)
bool SoundGenerator::play(bool value)
{
  AutoSemaphore autoSemaphore(m_mutex);
  m_play = value;
  if (actualPlaying() != value) {
    bool r = forcePlay(value);
    if (!value)
      mutizeOutput();
    return r;
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


bool SoundGenerator::forcePlay(bool value)
{
  bool isPlaying = actualPlaying();
  if (value) {
    // play
    if (!isPlaying) {
      if (!m_waveGenTaskHandle)
        xTaskCreate(waveGenTask, "", WAVEGENTASK_STACK_SIZE, this, 5, &m_waveGenTaskHandle);
      m_state = SoundGeneratorState::RequestToPlay;
      xTaskNotifyGive(m_waveGenTaskHandle);
    }
  } else {
    // stop
    if (isPlaying) {
      // request task to suspend itself when possible
      m_state = SoundGeneratorState::RequestToStop;
      // wait for task switch to suspend state (TODO: is there a better way?)
      while (m_state != SoundGeneratorState::Stop)
        vTaskDelay(1);
    }
  }
  return isPlaying;
}


bool SoundGenerator::actualPlaying()
{
  return m_waveGenTaskHandle && m_state == SoundGeneratorState::Playing;
}


// does NOT take ownership of the waveform generator
void SoundGenerator::attach(WaveformGenerator * value)
{
  AutoSemaphore autoSemaphore(m_mutex);

  bool isPlaying = forcePlay(false);

  value->setSampleRate(m_sampleRate);

  value->next = m_channels;
  m_channels = value;

  forcePlay(isPlaying || m_play);
}


void SoundGenerator::detach(WaveformGenerator * value)
{
  if (!value)
    return;

  AutoSemaphore autoSemaphore(m_mutex);

  bool isPlaying = forcePlay(false);
  detachNoSuspend(value);
  forcePlay(isPlaying);
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


void IRAM_ATTR SoundGenerator::waveGenTask(void * arg)
{
  SoundGenerator * soundGenerator = (SoundGenerator*) arg;

  i2s_set_clk(I2S_NUM_0, soundGenerator->m_sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  uint16_t * buf = soundGenerator->m_sampleBuffer;

  // number of mute (without channels to play) cycles
  int muteCyclesCount = 0;

  while (true) {

    // suspend?
    if (soundGenerator->m_state == SoundGeneratorState::RequestToStop || soundGenerator->m_state == SoundGeneratorState::Stop) {
      soundGenerator->m_state = SoundGeneratorState::Stop;
      while (soundGenerator->m_state == SoundGeneratorState::Stop)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // wait for "give"
    }

    // mutize output?
    if (soundGenerator->m_channels == nullptr && muteCyclesCount >= 8) {
      soundGenerator->m_state = SoundGeneratorState::Stop;
      while (soundGenerator->m_state == SoundGeneratorState::Stop)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // wait for "give"
    }

    soundGenerator->m_state = SoundGeneratorState::Playing;

    int mainVolume = soundGenerator->volume();

    for (int i = 0; i < FABGL_SAMPLE_BUFFER_SIZE; ++i) {
      int sample = 0, tvol = 0;
      for (auto g = soundGenerator->m_channels; g; ) {
        if (g->enabled()) {
          sample += g->getSample();
          tvol += g->volume();
        } else if (g->duration() == 0 && g->autoDetach()) {
          auto curr = g;
          g = g->next;  // setup next item before detaching this one
          soundGenerator->detachNoSuspend(curr);
          continue; // bypass "g = g->next;"
        }
        g = g->next;
      }

      int avol = tvol ? imin(127, 127 * 127 / tvol) : 127;
      sample = sample * avol / 127;
      sample = sample * mainVolume / 127;

      buf[i + (i & 1 ? -1 : 1)] = (127 + sample) << 8;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, buf, FABGL_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), &bytes_written, portMAX_DELAY);

    muteCyclesCount = soundGenerator->m_channels == nullptr ? muteCyclesCount + 1 : 0;
  }
}


void SoundGenerator::mutizeOutput()
{
  for (int i = 0; i < FABGL_SAMPLE_BUFFER_SIZE; ++i)
    m_sampleBuffer[i] = 127 << 8;
  size_t bytes_written;
  for (int i = 0; i < 4; ++i)
    i2s_write(I2S_NUM_0, m_sampleBuffer, FABGL_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), &bytes_written, portMAX_DELAY);
}


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

