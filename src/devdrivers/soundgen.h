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


/**
 * @file
 *
 * @brief This file contains all classes related to FabGL Sound System
 */




#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"

#if __has_include("esp32/rom/lldesc.h")
  #include "esp32/rom/lldesc.h"
#else
  #include "rom/lldesc.h"
#endif
#include "soc/i2s_struct.h"
#include "soc/sens_struct.h"

#include "fabglconf.h"
#include "fabutils.h"



namespace fabgl {


#define FABGL_SOUNDGEN_DEFAULT_SAMPLE_RATE 16384


// DAC mode specific:
// setting 64 at 16KHz, generates 16000/64=250 interrupts per second
#define FABGL_SOUNDGEN_SAMPLE_BUFFER_SIZE 64



/** @brief Base abstract class for waveform generators. A waveform generator can be seen as an audio channel that will be mixed by SoundGenerator. */
class WaveformGenerator {
public:
  WaveformGenerator() : next(nullptr), m_sampleRate(0), m_volume(100), m_enabled(false), m_duration(-1), m_autoDestroy(false), m_autoDetach(false) { }

  virtual ~WaveformGenerator() { }

  /**
   * @brief Sets output frequency
   *
   * @param value Frequency in Hertz
   */
  virtual void setFrequency(int value) = 0;

  /**
   * @brief Sets number of samples to play
   *
   * @param value Number of samples to play. -1 = infinite.
   */
  void setDuration(uint32_t value) { m_duration = value; }

  /**
   * @brief Returns number of remaining samples to play
   *
   * @return Number of remaining samples to play. -1 = infinite.
   */
  uint32_t duration() { return m_duration; }

  /**
   * @brief Sets autodetach mode
   *
   * @param value if true: this object needs to be detached from the sound generator when there are no more samples to play.
   */
  void setAutoDetach(bool value) { m_autoDetach = value; }

  bool autoDetach() { return m_autoDetach; }

  /**
   * @brief Sets autodestroy mode
   *
   * @param value if true: this object needs to be destroyed by the sound generat or when there are no more samples to play. This will set also setAutoDetach(true).
   */
  void setAutoDestroy(bool value) { m_autoDestroy = value; m_autoDetach |= value; }

  bool autoDestroy() { return m_autoDestroy; }

  /**
   * @brief Gets next sample
   *
   * @return Sample value as signed 8 bit (-128..127 range)
   */
  virtual int getSample() = 0;

  /**
   * @brief Sets volume of this generator
   *
   * @param value Volume value. Minimum is 0, maximum is 127.
   */
  void setVolume(int value) { m_volume = value; }

  /**
   * @brief Determines current volume
   *
   * @return Current volume of this generator (0 = minimum, 127 = maximum)
   */
  int volume() { return m_volume; }

  /**
   * @brief Determines whether this generator is enabled or disabled
   *
   * @return True if this generator is enabled
   */
  bool enabled() { return m_enabled; }

  /**
   * @brief Enables or disabled this generator
   *
   * A generator is disabled for default and must be enabled in order to play sound
   *
   * @param value True to enable the generator, False to disable
   */
  void enable(bool value) { m_enabled = value; }

  /**
   * @brief Sets the sample rate
   *
   * Default sample rate is 160000 Hertz.
   *
   * @param value Sample rate in Hertz
   */
  void setSampleRate(int value) { m_sampleRate = value; }

  /**
   * @brief Determines the sample rate
   *
   * @return Current sample rate in Hertz
   */
  uint16_t sampleRate() { return m_sampleRate; }

  WaveformGenerator * next;

protected:

  void decDuration() { --m_duration; if (m_duration == 0) m_enabled = false; }

private:
  uint16_t m_sampleRate;
  int8_t   m_volume;
  int8_t   m_enabled;   // 0 = disabled, 1 = enabled
  uint32_t m_duration;  // number of samples to play (-1 = infinite)
  bool     m_autoDestroy; // if true: this object needs to be destroyed by the sound generator when there are no more samples to play
  bool     m_autoDetach;  // if true: this object needs to be autodetached from the sound generator when there are no more samples to play
};


/** @brief Sine waveform generator */
class SineWaveformGenerator : public WaveformGenerator {
public:
  SineWaveformGenerator();

  void setFrequency(int value);

  int getSample();

private:
  uint32_t m_phaseInc;
  uint32_t m_phaseAcc;
  uint16_t m_frequency;
  int16_t  m_lastSample;
};


/** @brief Square waveform generator */
class SquareWaveformGenerator : public WaveformGenerator {
public:
  SquareWaveformGenerator();

  void setFrequency(int value);

  /**
   * @brief Sets square wave duty cycle
   *
   * @param dutyCycle Duty cycle in 0..255 range. 255 = 100%
   */
  void setDutyCycle(int dutyCycle);

  int getSample();

private:
  uint32_t m_phaseInc;
  uint32_t m_phaseAcc;
  uint16_t m_frequency;
  int8_t   m_lastSample;
  uint8_t  m_dutyCycle;
};


/** @brief Triangle waveform generator */
class TriangleWaveformGenerator : public WaveformGenerator {
public:
  TriangleWaveformGenerator();

  void setFrequency(int value);

  int getSample();

private:
  uint32_t m_phaseInc;
  uint32_t m_phaseAcc;
  uint16_t m_frequency;
  int8_t   m_lastSample;
};


/** @brief Sawtooth waveform generator */
class SawtoothWaveformGenerator : public WaveformGenerator {
public:
  SawtoothWaveformGenerator();

  void setFrequency(int value);

  int getSample();

private:
  uint32_t m_phaseInc;
  uint32_t m_phaseAcc;
  uint16_t m_frequency;
  int8_t   m_lastSample;
};


/** @brief Noise generator */
class NoiseWaveformGenerator : public WaveformGenerator {
public:
  NoiseWaveformGenerator();

  void setFrequency(int value);

  int getSample();

private:
  uint16_t m_noise;
};


/////////////////////////////////////////////////////////////////////////////////////////////
// "tries" to emulate VIC6561 noise generator
// derived from a reverse enginnered VHDL code: http://www.denial.shamani.dk/bb/viewtopic.php?t=8733&start=210

/**
 * @brief Emulates VIC6561 (VIC20) noise generator
 *
 * Inspired from a reverse enginnered VHDL code: http://www.denial.shamani.dk/bb/viewtopic.php?t=8733&start=210
 */
class VICNoiseGenerator : public WaveformGenerator {
public:
  VICNoiseGenerator();

  void setFrequency(int value);
  uint16_t frequency() { return m_frequency; }

  int getSample();

private:
  static const uint16_t LFSRINIT = 0x0202;
  static const int      CLK      = 4433618;

  uint16_t m_frequency;
  uint16_t m_counter;
  uint16_t m_LFSR;
  uint16_t m_outSR;
};



/**
 * @brief Samples generator
 *
 * Sample data should be sampled at the same samplerate of the sound generator.
 * Only 8 bit (signed - not compressed) depth is supported.
 */
class SamplesGenerator : public WaveformGenerator {
public:
  SamplesGenerator(int8_t const * data, int length);

  void setFrequency(int value);

  int getSample();

private:
  int8_t const * m_data;
  int            m_length;
  int            m_index;
};


/** \ingroup Enumerations
 * @brief
 */
enum class SoundGenMethod {
  DAC,             /**< Use DAC. Available on gpio 25 or 26. Very low resources occupation. */
  SigmaDelta,      /**< Use Sigma-Delta. Available on almost all GPIO pins. Requires a lot of CPU power for high sample rates. */
  Auto,            /**< Use DAC when video output is VGA, use SigmaDelta when video output is Composite. */
};


/**
 * @brief SoundGenerator handles audio output
 *
 * SoundGenerator generates audio samples using DAC or Sigma-Delta modulation. Use constructor to specify GPIO and generation method.
 * Applications attach waveform generators (like SineWaveformGenerator, SquareWaveformGenerator, etc...) and call SoundGenerator.play() to start audio generation.
 *
 * Here is a list of supported sound generators:
 * - SineWaveformGenerator
 * - SquareWaveformGenerator
 * - TriangleWaveformGenerator
 * - SawtoothWaveformGenerator
 * - NoiseWaveformGenerator
 * - VICNoiseGenerator
 * - SamplesGenerator
 */
class SoundGenerator {

public:

  /**
   * @brief Creates an instance of the sound generator. Only one instance is allowed
   *
   * @param sampleRate Sample rate in Hertz.
   * @param gpio GPIO to use. DAC mode can be set on 25 or 26. Value GPIO_AUTO will set GPIO 25 when genMethod is DAC and GPIO 23 when genMethod is SigmaDelta.
   * @param genMethod Sound generation method. Can be DAC, sigma-delta or automatic. If automatic then DAC is selected when VGA is used, otherwise SigmaDelta is selected when Composite is used.
   *
   * Example:
   *
   *     // creates sound generator with automatic values (depends by selected video output, VGA or CVBS)
   *     SoundGenerator soundGenerator;
   *
   *     // creates sound generator using 16Khz sample rate, GPIO 25 in DAC mode
   *     SoundGenerator soundGenerator(16000, GPIO_NUM_25, SoundGenMethod::DAC);
   *
   *     // creates sound generator using 16Khz sample rate, GPIO 23 in sigma-delta mode
   *     SoundGenerator soundGenerator(16000, GPIO_NUM_23, SoundGenMethod::SigmaDelta);
   */
  SoundGenerator(int sampleRate = FABGL_SOUNDGEN_DEFAULT_SAMPLE_RATE, gpio_num_t gpio = GPIO_AUTO, SoundGenMethod genMethod = SoundGenMethod::Auto);

  ~SoundGenerator();

  /**
   * @brief Stops playing and removes all attached waveform generators
   */
  void clear();

  /**
   * @brief Starts or stops playing
   *
   * @param value True = starts playing, False = stops playing
   *
   * @return Returns previous playing state
   *
   * Example:
   *
   *     soundGenerator.play(true);
   */
  bool play(bool value);

  /**
   * @brief Plays the specified samples
   *
   * Starts immediately to play the specified samples. It is not required to call play().
   * This method returns without wait the end of sound.
   *
   * @param data Samples to play.
   * @param length Number of samples to play.
   * @param volume Volume value. Minimum is 0, maximum is 127.
   * @param durationMS Duration in milliseconds. 0 = untile end of samples, -1 = infinite loop.
   *
   * @return Pointer to SamplesGenerator object. Lifetime of this object is limited to the duration.
   */
  SamplesGenerator * playSamples(int8_t const * data, int length, int volume = 100, int durationMS = 0);

  /**
   * @brief Plays the specified waveform
   *
   * Starts immediately to play the specified waveform. It is not required to call play().
   * This method returns without wait the end of sound.
   *
   * @param waveform Waveform to play.
   * @param frequency Frequency in Hertz.
   * @param durationMS Duration in milliseconds.
   * @param volume Volume value. Minimum is 0, maximum is 127.
   *
   * Example:
   *
   *     // plays a sinewave at 500Hz for 200 milliseconds
   *     soundGen.playSound(SineWaveformGenerator(), 500, 200);
   *
   *     // plays a C Major chord for 1 second
   *     soundGen.playSound(SineWaveformGenerator(), 262, 1000);  // C
   *     soundGen.playSound(SineWaveformGenerator(), 330, 1000);  // E
   *     soundGen.playSound(SineWaveformGenerator(), 392, 1000);  // G
   */
  template <typename T>
  void playSound(T const & waveform, int frequency, int durationMS, int volume = 100) {
    auto wf = new T(waveform);
    attach(wf);
    wf->setFrequency(frequency);
    wf->setVolume(volume);
    wf->setAutoDestroy(true);
    wf->setDuration(m_sampleRate / 1000 * durationMS);
    wf->enable(true);
    play(true);
  }

  /**
   * @brief Determines whether sound generator is playing
   *
   * @return True when playing, False otherwise
   */
  bool playing() { return m_play; }

  WaveformGenerator * channels() { return m_channels; }

  /**
   * @brief Attaches a waveform generator
   *
   * @param value Pointer of the waveform generator to attach
   *
   * Example:
   *
   *     SineWaveformGenerator sine;
   *     soundGenerator.attach(&sine);
   *     sine.enable(true);
   *     sine.setFrequency(500);  // 500 Hz
   */
  void attach(WaveformGenerator * value);

  /**
   * @brief Detaches a waveform generator
   *
   * @param value Pointer of the waveform generator to detach
   */
  void detach(WaveformGenerator * value);

  /**
   * @brief Sets the overall volume
   *
   * @param value Volume value. Minimum is 0, maximum is 127.
   */
  void setVolume(int value) { m_volume = value; }

  /**
   * @brief Determines current overall volume
   *
   * @return Current overall volume (0 = minimum, 127 = maximum)
   */
  int volume() { return m_volume; }


private:

  static void ISRHandler(void * arg);     // used in DAC mode
  static void timerHandler(void * args);  // used in sigma-delta mode

  void dac_init();
  void sigmadelta_init();
  void detachNoSuspend(WaveformGenerator * value);
  void setDMANode(int index, volatile uint16_t * buf, int len);
  void init();


  WaveformGenerator * m_channels;

  uint16_t *          m_sampleBuffer[2];

  int8_t              m_volume;

  uint16_t            m_sampleRate;

  bool                m_play;
  
  gpio_num_t          m_gpio;
  
  intr_handle_t       m_isr_handle;
  
  lldesc_t volatile * m_DMAChain;
  
  SoundGenMethod      m_genMethod;
  
  bool                m_initDone;
  
  esp_timer_handle_t  m_timerHandle;

};




} // end of namespace

