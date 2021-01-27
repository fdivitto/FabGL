#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define PLAY_SOUND_PRIORITY (configMAX_PRIORITIES - 1)

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

#define NOTESTEP 1.0594630943593

SquareWaveformGenerator swg;

void setup() {

  Serial.begin(115200);
  //PS2Controller.begin(PS2Preset::KeyboardPort0); //, KbdMode::GenerateVirtualKeys)

  VGAController.begin();
  VGAController.setResolution(VGA_640x400_70Hz);
  VGAController.moveScreen(5, 0);

  soundGenerator.setVolume(126);
  soundGenerator.play(true);
  soundGenerator.attach( &swg);

  Terminal.begin(&VGAController);
  //Terminal.connectLocally();
  Terminal.clear();
  Terminal.enableCursor(true);

  Terminal.write( "SoundChip simulator by Carles Oriol 2020\r\n\r\n");

}

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
    vTaskDelay(5);
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
                           3,  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                           NULL,
                           1);
}


void  syncPlaySound( playsounddata psd)
{
  playSound( psd);
  delay(psd.durationms);
}


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

// 0,5 = cortxea, 1 = negra 2 = blanca...
char const * noteToDelayEx(char const * note, int * delayMS)
{
  *delayMS = 0;
  while (*note && *note == ' ')
    ++note;
  if (*note == 0)
    return note;
  double val = atof(note);
  if (val > 0)
    *delayMS = 250 * val;

  while ((*note && isdigit(*note)) || *note == '.')
    ++note;
    
  return note;
}


void play_song1(bool overnote= false)
{
  const char * music = "A4 4 A4 4 A#4 4 C5 4 C5 4 A#4 4 A4 4 G4 4 F4 4 F4 4 G4 4 A4 4 A4 2 G4 16 G4 2 P 8 "
                       "A4 4 A4 4 A#4 4 C5 4 C5 4 A#4 4 A4 4 G4 4 F4 4 F4 4 G4 4 A4 4 G4 2 F4 16 F4 2 P 8";
  const char * m = music;

  while (*m ) 
  {
    int freq, delms;
    m = noteToFreq(m, &freq);
    m = noteToDelay(m, &delms);
    playSound (  {250, 189, 120, 256, WAVE_TRIANGLE, 127, delms, freq, 0, MODFREQ_NONE} );     
    if (overnote)
      playSound (  {500, 189, 120, 256, WAVE_SINE, 127, (delms*3)/2, freq/2, 0, MODFREQ_NONE} );     
    delay(delms); 
  }
}



void play_song2()
{
   const char * track1 =  "D5 1 D5 1 B4 1 D5 1 E5 1 D5 1 B4 2 B4 1 A4 3 B4 1 A4 3 D5 1 D5 1 B4 1 D5 1 E5 1 D5 1 B4 2 A4 2 B4 1 A4 1 G4 4"
                        "D5 1 D5 1 B4 1 D5 1 E5 1 D5 1 B4 2 B4 1 A4 3 B4 1 A4 3 D5 1 D5 1 B4 1 D5 1 E5 1 D5 1 B4 2 A4 2 B4 1 A4 1 G4 4 "
                        "G4 1.5 G4 0.5 B4 1 D5 1 G5 4 E5 1.5 E5 0.5 G5 1 E5 1 D5 4 D5 1 D5 1 B4 1 D5 1 E5 1 D5 1 B4 2 A4 1 C5 1 B4 1 A4 1 G4 4";

   const char * track2 =  "B4 1 B4 1 G4 1 B4 1 C5 1 B4 1 G4 2 D4 1 D4 3 D4 1 D4 3 B4 1 B4 1 G4 1 B4 1 C5 1 B4 1 G4 2 D4 2 D4 1 D4 1 G4 4 "
                          "B4 1 B5 1 G4 1 B5 1 C5 1 B5 1 G4 2 D4 1 D4 3 D4 1 D4 3 B4 1 B5 1 G4 1 B5 1 C5 1 B5 1 G4 2 D4 2 D4 1 D4 1 G4 4 "
                          "G3 2 G4 2 G3 2 G4 2 E4 2 E5 2 D3 2 D4 2 D4 2 D5 2 C5 1 B4 1 G4 2 D4 2 D4 1 D4 1 G4 4 "
                          ;

  const char * track3 =  
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 4 "
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 "                     
  "A4 0.5 A4 0.5 A4 0.5 A4 0.5 A4 1 A4 1 A4 4 "
  "A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 "                     
  "A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 "                     
  "A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 "                     
  "A4 1 A4 0.5 A4 0.5 A4 1 A4 0.5 A4 0.5 A4 4";
   
  const char * m1 = track1;  
  const char * m2 = track2;
  const char * m3 = track3;
  long track1_untilms = millis();  
  long track2_untilms = track1_untilms;  
  long track3_untilms = track1_untilms;  

  int previousfreq = -1;
  int previousfreq2 = -1;
  
  while (*m1) 
  {
    long now = millis();
    
    if(now >= track1_untilms)    
    {
      int freq, delms;
      m1 = noteToFreq(m1, &freq);
      m1 = noteToDelayEx(m1, &delms);
      track1_untilms += delms;      
      if ( freq != 0)
        playSound (  {75, 0, 127, 256, WAVE_SINE, 127, delms, previousfreq==-1?freq:previousfreq, freq, MODFREQ_TO_SUSTAIN} );      
      previousfreq = freq;   
    }

    if(now >= track2_untilms && *m2)    
    {
      int freq, delms;
      m2 = noteToFreq(m2, &freq);
      m2 = noteToDelayEx(m2, &delms);
      track2_untilms += delms;      
      if( freq != 0)        
        playSound (  {10, 10, 100, 512, WAVE_SQUARE, 60, delms, previousfreq2==-1?freq:previousfreq2, freq, MODFREQ_TO_SUSTAIN} );   
        previousfreq2 = freq;           
    }

    if(now >= track3_untilms && *m3)    
    {
      int freq, delms;
      m3 = noteToFreq(m3, &freq);
      m3 = noteToDelayEx(m3, &delms);      
      track3_untilms += delms;      
      if( freq != 0)
        playSound (  {0, 25, 120, 512, WAVE_NOISE, 127, delms/12, freq, 0, MODFREQ_NONE} );      
    }    
     delay(2);
  }
  while(millis() >= track1_untilms)  vTaskDelay(1);
}

void loop() {
  play_song1(); 
  delay(2000); 
  play_song1(true ); 
  delay(2000); 
  play_song2();  
  delay(2000);    
  Terminal.println( "That's all folks..." );
}
