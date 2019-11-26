/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
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


//#include "Arduino.h"    // REMOVE!

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <string.h>
#include <ctype.h>
#include <math.h>

#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"


#include "soundgen.h"


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
  if (m_frequency == 0) {
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
  if (m_frequency == 0) {
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
  if (m_frequency == 0) {
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
  if (m_frequency == 0) {
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


int NoiseWaveformGenerator::getSample() {
  // noise generator based on Galois LFSR
  m_noise = (m_noise >> 1) ^ (-(m_noise & 1) & 0xB400u);
  int sample = 127 - (m_noise >> 8);

  // process volume
  sample = sample * volume() / 127;

  return sample;
}


// NoiseWaveformGenerator
////////////////////////////////////////////////////////////////////////////////////////////////////////////



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
  int sample = m_data[m_index++];

  if (m_index == m_length)
    m_index = 0;

  // process volume
  sample = sample * volume() / 127;

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
    m_sampleRate(sampleRate)
{
  i2s_audio_init();
}


SoundGenerator::~SoundGenerator()
{
  clear();
  vTaskDelete(m_waveGenTaskHandle);
  free(m_sampleBuffer);
}


void SoundGenerator::clear()
{
  play(false);
  m_channels = nullptr;
}


void SoundGenerator::i2s_audio_init()
{
  i2s_config_t i2s_config;
  i2s_config.mode                 = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  i2s_config.sample_rate          = m_sampleRate;
  i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
  i2s_config.channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT;
  i2s_config.intr_alloc_flags     = 0;
  i2s_config.dma_buf_count        = 2;
  i2s_config.dma_buf_len          = I2S_SAMPLE_BUFFER_SIZE * sizeof(uint16_t);
  i2s_config.use_apll             = 0;
  i2s_config.tx_desc_auto_clear   = 0;
  i2s_config.intr_alloc_flags     = 0;
  // install and start i2s driver
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  // init DAC pad
  i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN); // GPIO25

  m_sampleBuffer = (uint16_t*) malloc(I2S_SAMPLE_BUFFER_SIZE * sizeof(uint16_t));
}


// the same of suspendPlay, but fill also output DMA with 127s, making output mute (and making "bumping" effect)
bool SoundGenerator::play(bool value)
{
  bool r = suspendPlay(value);
  if (!value)
    mutizeOutput();
  return r;
}


bool SoundGenerator::suspendPlay(bool value)
{
  bool isPlaying = playing();
  if (value) {
    // play
    if (!isPlaying) {
      if (m_waveGenTaskHandle)
        vTaskResume(m_waveGenTaskHandle);
      else
        xTaskCreate(waveGenTask, "", WAVEGENTASK_STACK_SIZE, this, 5, &m_waveGenTaskHandle);
    }
  } else {
    // stop
    if (isPlaying) {
      // request task to suspend itself when possible
      xTaskNotifyGive(m_waveGenTaskHandle);
      // wait for task switch to suspend state (TODO: is there a better way?)
      while (eTaskGetState(m_waveGenTaskHandle) != eSuspended)
        taskYIELD();
    }
  }
  return isPlaying;
}


bool SoundGenerator::playing()
{
  return m_waveGenTaskHandle && eTaskGetState(m_waveGenTaskHandle) != eSuspended;
}


// does NOT take ownership of the waveform generator
void SoundGenerator::attach(WaveformGenerator * value)
{
  bool isPlaying = suspendPlay(false);

  value->setSampleRate(m_sampleRate);

  value->next = m_channels;
  m_channels = value;

  suspendPlay(isPlaying);
}


void SoundGenerator::detach(WaveformGenerator * value)
{
  if (!value)
    return;

  bool isPlaying = suspendPlay(false);

  for (WaveformGenerator * c = m_channels, * prev = nullptr; c; prev = c, c = c->next) {
    if (c == value) {
      if (prev)
        prev->next = c->next;
      else
        m_channels = c->next;
      break;
    }
  }

  suspendPlay(isPlaying);
}


void SoundGenerator::waveGenTask(void * arg)
{
  SoundGenerator * soundGenerator = (SoundGenerator*) arg;

  i2s_set_clk(I2S_NUM_0, soundGenerator->m_sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  uint16_t * buf = soundGenerator->m_sampleBuffer;

  while (true) {

    int mainVolume = soundGenerator->volume();

    for (int i = 0; i < I2S_SAMPLE_BUFFER_SIZE; ++i) {
      int sample = 0, tvol = 0;
      for (auto g = soundGenerator->m_channels; g; g = g->next)
        if (g->enabled()) {
          sample += g->getSample();
          tvol += g->volume();
        }

      int avol = tvol ? imin(127, 127 * 127 / tvol) : 127;
      sample = sample * avol / 127;
      sample = sample * mainVolume / 127;

      buf[i + (i & 1 ? -1 : 1)] = (127 + sample) << 8;
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, buf, I2S_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), &bytes_written, portMAX_DELAY);

    // request to suspend?
    if (ulTaskNotifyTake(true, 0)) {
      vTaskSuspend(nullptr);
    }

  }
}


void SoundGenerator::mutizeOutput()
{
  for (int i = 0; i < I2S_SAMPLE_BUFFER_SIZE; ++i)
    m_sampleBuffer[i] = 127 << 8;
  size_t bytes_written;
  for (int i = 0; i < 4; ++i)
    i2s_write(I2S_NUM_0, m_sampleBuffer, I2S_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), &bytes_written, portMAX_DELAY);
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

