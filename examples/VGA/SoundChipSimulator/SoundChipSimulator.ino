#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define PLAY_SOUND_PRIORITY 3

#include "fabgl.h"


fabgl::VGA16Controller VGAController;
fabgl::Canvas        canvas(&VGAController);
fabgl::PS2Controller PS2Controller;
SoundGenerator       soundGenerator;
fabgl::Terminal      Terminal;

enum wavetype { WAVE_SQUARE, WAVE_SINE, WAVE_TRIANGLE, WAVE_SAW, WAVE_NOISE };
enum modfreqmode { MODFREQ_NONE, MODFREQ_TO_END, MODFREQ_TO_RELEASE, MODFREQ_TO_SUSTAIN  };

struct playsounddata
{
  long attack; // time in millis (for now)
  long decay; // time in millis (for now)
  int sustain; // 0-127 range (over volume)
  long release; // time in millis (for now)

  wavetype wave; // square, sine, triangle, saw, noise
  int volume;
  int durationms;
  int freq_start;
  int freq_end;
  modfreqmode modfreq;

};

SquareWaveformGenerator swg;

void setup() {

  Serial.begin(115200);
  PS2Controller.begin(PS2Preset::KeyboardPort0); //, KbdMode::GenerateVirtualKeys)

  VGAController.begin();
  VGAController.setResolution(VGA_640x400_70Hz);
  VGAController.moveScreen(5, 0);

  soundGenerator.setVolume(126);
  soundGenerator.play(true);
  soundGenerator.attach( &swg);

  Terminal.begin(&VGAController);
  Terminal.connectLocally();
  Terminal.clear();
  Terminal.enableCursor(true);

  Terminal.write( "SoundChip simulator by Carles Oriol 2020\r\n\r\n");


}

void iPlaySound( void *pvParameters )
{
  playsounddata psd = *(playsounddata *)pvParameters;

  WaveformGenerator *pwave;
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
  delay(psd.durationms);
}


void loop() {

    Terminal.println( "shot" );
      syncPlaySound ( {0, 0, 127, 64, WAVE_NOISE, 127, 64, 120, 0, MODFREQ_NONE} );
      delay(2000);
    
    Terminal.println( "boom" );
      syncPlaySound ( {5, 128, 120, 256, WAVE_NOISE, 127, 512, 1, 1, MODFREQ_NONE} );
      delay(2000);
    
    Terminal.println( "pong" );
      syncPlaySound ( {0, 1, 127, 8, WAVE_SQUARE, 127, 21, 392, 0, MODFREQ_NONE} );
      delay(2000);
  
    Terminal.println( "pic" );
      syncPlaySound ( {2, 1, 127, 3, WAVE_SQUARE, 127, 12, 977, 0, MODFREQ_NONE} );
      delay(2000);
  
    Terminal.println( "tuc" );
      syncPlaySound ( {5, 0, 127, 39, WAVE_TRIANGLE, 127, 44, 352, 275, MODFREQ_TO_END} );
      delay(2000);
  
    Terminal.println( "piu" );
      syncPlaySound (  {5, 0, 127, 39, WAVE_TRIANGLE, 127, 154, 1200, 440, MODFREQ_TO_END} );
      delay(2000);
  
    Terminal.println( "fiu fiu whistle" );
      syncPlaySound ({100, 0, 127, 20, WAVE_SQUARE, 127, 356, 220, 3000, MODFREQ_TO_END} );    
      delay(64);
      syncPlaySound ({100, 0, 127, 20, WAVE_SQUARE, 100, 256, 220, 3000, MODFREQ_TO_END});
      syncPlaySound ({32, 32, 100, 512 - 64, WAVE_SQUARE, 100, 512, 4400, 220, MODFREQ_TO_END});    
      delay(2000);

    Terminal.println( "fiu fiu serpentina" );
      syncPlaySound ({100, 0, 127, 20, WAVE_SAW, 127, 356, 220, 3000, MODFREQ_TO_END} );
      delay(64);
      syncPlaySound ({100, 0, 127, 20, WAVE_SAW, 100, 256, 220, 3000, MODFREQ_TO_END});
      syncPlaySound ({32, 32, 100, 512 - 64, WAVE_SAW, 100, 512, 4400, 220, MODFREQ_TO_END});
      delay(2000);
  
    Terminal.println( "motor" );
      syncPlaySound ( {100, 0, 127, 20, WAVE_SAW, 100, 2048, 0, 120, MODFREQ_TO_END});    
      syncPlaySound ( {100, 0, 127, 20, WAVE_SAW, 127, 2048, 100, 220, MODFREQ_TO_END});    
      syncPlaySound ( {100, 0, 127, 20, WAVE_SAW, 127, 2048, 180, 250, MODFREQ_TO_END});    
      syncPlaySound ( {100, 0, 127, 20, WAVE_SAW, 127, 2048, 220, 280, MODFREQ_TO_END});    
      delay(2000);
  
    Terminal.println( "escala" );
      int escala[] = { 262,  294,  330,  349,  392,  440,  494, 523   };
      for ( int ncompta = 0; ncompta < 8; ncompta++)    
        syncPlaySound ( {10, 5, 100, 10, WAVE_SINE, 127, 1000, (int)escala[ncompta], 0, MODFREQ_NONE} );          
      delay(2000);
  
    Terminal.println( "escala_inc" );
      for (int freqmode = MODFREQ_TO_END; freqmode <= MODFREQ_TO_SUSTAIN; freqmode++)
        for ( int ncompta = 0; ncompta < 8; ncompta++)
          syncPlaySound ( {50, 50, 100, 250, WAVE_SINE, 127, 1000, escala[ncompta], (escala[ncompta] * 2), (modfreqmode)freqmode} );              
      delay(2000);

    Terminal.println( "escalax" );    
      for ( int nc = 0; nc < 4; nc++)
      {
        for ( int ncompta = 0; ncompta < 8; ncompta++)      
          syncPlaySound ( {2, 2, 50, 10, WAVE_SINE, 64, 64, (int)escala[ncompta], 0, MODFREQ_NONE} );        
        for ( int ncompta = 0; ncompta < 8; ncompta++)
          syncPlaySound ( {2, 2, 50, 10, WAVE_TRIANGLE, 127, 64, (int)escala[ncompta], 0, MODFREQ_NONE} );      
      }
  
      delay(2000);
    
    Terminal.println( "chord" );
      syncPlaySound (  {10, 30, 90, 50, WAVE_SINE, 127, 1000, 262, 0, MODFREQ_NONE} );    
      syncPlaySound (  {10, 30, 90, 50, WAVE_SINE, 127, 1000, 330, 0, MODFREQ_NONE} );    
      syncPlaySound (  {10, 30, 90, 50, WAVE_SINE, 127, 1000, 392, 0, MODFREQ_NONE} );    
      syncPlaySound (  {10, 30, 90, 50, WAVE_SINE, 127, 1000, 523, 0, MODFREQ_NONE} );    
  
      delay ( 2000 );
  
      playSound (  {10, 30, 90, 50, WAVE_SINE, 127, 4000, 262, 0, MODFREQ_NONE} );
      delay(1000);
      playSound (  {10, 30, 90, 50, WAVE_SINE, 127, 3000, 330, 0, MODFREQ_NONE} );
      delay(1000);
      playSound (  {10, 30, 90, 50, WAVE_SINE, 127, 2000, 392, 0, MODFREQ_NONE} );
      delay(1000);
      playSound (  {10, 30, 90, 50, WAVE_SINE, 127, 1000, 523, 0, MODFREQ_NONE} );
      delay(1000);
  
      delay ( 2000 );
  
      playSound (  {1, 30, 90, 50, WAVE_SINE, 127, 2000, 262, 0, MODFREQ_NONE} );
      delay(25);
      playSound (  {1, 30, 90, 50, WAVE_SINE, 127, 1975, 330, 0, MODFREQ_NONE} );
      delay(25);
      playSound (  {1, 30, 90, 50, WAVE_SINE, 127, 1950, 392, 0, MODFREQ_NONE} );
      delay(25);
      playSound (  {1, 30, 90, 50, WAVE_SINE, 127, 1925, 523, 0, MODFREQ_NONE} );
      delay(2000);
  
      delay(2000);
  
      playSound (  {100, 30, 90, 50, WAVE_SINE, 127, 2000, 262, 0, MODFREQ_NONE} );
      playSound (  {100, 30, 90, 50, WAVE_SINE, 127, 2000, 311, 0, MODFREQ_NONE} );
      playSound (  {100, 30, 90, 50, WAVE_SINE, 127, 2000, 392, 0, MODFREQ_NONE} );
  
      for ( int compta = 0; compta < 5; compta++)
      {
        syncPlaySound (  {100, 30, 90, 50, WAVE_SINE, 127, 200, 494, 0, MODFREQ_NONE} );      
        syncPlaySound (  {100, 30, 90, 50, WAVE_SINE, 127, 200, 523, 0, MODFREQ_NONE} );      
      }
      
      delay ( 2000 );

    Terminal.println( "crossed" );
      playSound ({100, 0, 127, 100, WAVE_SINE, 127, 2000, 100, 4000, MODFREQ_TO_END} );
      playSound ({100, 0, 127, 100, WAVE_SINE, 127, 2000, 4000, 100, MODFREQ_TO_END} );
      delay(2000);
      delay(2000);
  
      for( int ncompta = 0; ncompta < 5; ncompta++)
      {
        playSound ({100, 0, 127, 100, WAVE_SQUARE, 127, 2000, 100, 4000, MODFREQ_TO_END} );    
        delay(150);
      }
      delay(2000);
  
  
      for( int ncompta = 0; ncompta < 5; ncompta++)
      {
        playSound ({100, 0, 127, 100, WAVE_SQUARE, 127, 2000, 100, 4000, MODFREQ_TO_END} );
        playSound ({100, 0, 127, 100, WAVE_SQUARE, 127, 2000, 4000, 100, MODFREQ_TO_END} );
        delay(100);
      }
      delay(2000);
  
      delay(2000);
    

    Terminal.println( "Random frequencies mixed" );
      for( int ncompta = 0; ncompta < 30; ncompta++)
      {
        playSound ({100, 0, 127, 100, WAVE_SQUARE, 127, 2000, random(20,2000), random(20,2000), MODFREQ_TO_END} );    
        delay(250);
      }
      delay(2000);
  
    Terminal.println( "That's all folks..." );
}
