#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define PLAY_SOUND_PRIORITY (configMAX_PRIORITIES - 1)

enum wavetype { WAVE_SQUARE, WAVE_SINE, WAVE_TRIANGLE, WAVE_SAW, WAVE_NOISE };
enum modfreqmode { MODFREQ_NONE, MODFREQ_TO_END, MODFREQ_TO_RELEASE, MODFREQ_TO_SUSTAIN  };

struct playsounddata
{
  long attack; // time in millis
  long decay; // time in millis 
  int sustain; // 0-127 range (over sound volume)
  long release; // time in millis 

  wavetype wave; // square, sine, triangle, saw, noise
  int volume;
  int durationms;
  int freq_start;
  int freq_end;
  modfreqmode modfreq;

};

void iPlaySound( void *pvParameters )
{
  playsounddata psd = *(playsounddata *)pvParameters;

  WaveformGenerator *pwave = nullptr;
  if ( psd.wave == WAVE_SQUARE)   pwave = new SquareWaveformGenerator();
  if ( psd.wave == WAVE_SINE)     pwave = new SineWaveformGenerator();
  if ( psd.wave == WAVE_TRIANGLE) pwave = new TriangleWaveformGenerator();
  if ( psd.wave == WAVE_SAW)      pwave = new SawtoothWaveformGenerator();
  if ( psd.wave == WAVE_NOISE)    pwave = new NoiseWaveformGenerator();

  int sustainVolume = psd.sustain * psd.volume / 127;

  soundGenerator.attach( pwave);
  pwave->setVolume( ((psd.attack == 0) ? ( (psd.decay != 0) ? psd.volume : sustainVolume) : 0) );
  pwave->setFrequency( psd.freq_start );
  pwave->enable(true );

  long startTime = millis();

  while ( millis() - startTime < psd.durationms)
  {
    long current = millis() - startTime;

    if ( current < psd.attack ) // ATTACK VOLUME
      pwave->setVolume( (psd.volume * current) / psd.attack );
    else if ( current > psd.attack && current < (psd.attack + psd.decay)) // DECAY VOLUME
      pwave->setVolume( map( current - psd.attack,  0, psd.decay,  psd.volume, sustainVolume ) );
    else if ( current > psd.durationms - psd.release) // RELEASE VOLUME
      pwave->setVolume( map( current - (psd.durationms - psd.release),  0, psd.release,  sustainVolume, 0 ) );
    else
      pwave->setVolume( sustainVolume );


    if ( psd.modfreq != MODFREQ_NONE)
    {
      int maxtime = psd.durationms;
      if      (psd.modfreq == MODFREQ_TO_RELEASE)
        maxtime = (psd.durationms - psd.release); // until release
      else if (psd.modfreq == MODFREQ_TO_SUSTAIN)
        maxtime = (psd.attack + psd.decay); // until sustain

      int f = ((current > maxtime) ? psd.freq_end : (map(current, 0, maxtime, psd.freq_start, psd.freq_end)));
      pwave->setFrequency ( f ) ;
    }
    vTaskDelay(1);
  }

  soundGenerator.detach( pwave);
  pwave->enable( false );
  delete pwave;

  vTaskDelete( NULL );
}

void playSound( playsounddata ps )
{
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore( iPlaySound,  "iPlaySound",
                           4096,  // This stack size can be checked & adjusted by reading the Stack Highwater
                           &ps, // sound as parameters
                           PLAY_SOUND_PRIORITY,  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                           NULL,
                           ARDUINO_RUNNING_CORE);
}


void  syncPlaySound( playsounddata psd)
{
  playSound( psd);
   vTaskDelay(psd.durationms/portTICK_PERIOD_MS);
}


void playSoundPic()
{
  playSound ( {2, 1, 127, 3, WAVE_SQUARE, 127, 12, 977, 0, MODFREQ_NONE });
}

void playSoundTuc()
{
  playSound ( {5, 0, 127, 39, WAVE_TRIANGLE, 127, 44, 352, 275, MODFREQ_TO_END} );
}

void playSoundPong()
{
  playSound ( {0, 1, 127, 8, WAVE_SQUARE, 127, 21, 392, 0, MODFREQ_NONE} );
}
