#include "FS.h"
#include "SPIFFS.h"
#include "fabgl.h"

fabgl::VGAController VGAController;
fabgl::Canvas        canvas(&VGAController);
fabgl::PS2Controller PS2Controller;
SoundGenerator       soundGenerator;
SquareWaveformGenerator swg;

#include "controllers.h"
#include "gameimages.h"

#define JOY_LEFT 13
#define JOY_RIGHT 2
#define JOY_DOWN 14
#define JOY_FIRE 12

GameController           cNone = GameController(0, MODE_NONE);
GameControllerMouse      cMouse;
GameControllerJoystick   cJoystick;
GameControllerKeys       cKeysArrows;
GameControllerKeys       cKeysASTF;
GameControllerKeys       cKeysQAOP;

GameController *gameControllers[] = { &cNone, &cMouse, &cJoystick, &cKeysArrows, &cKeysASTF, &cKeysQAOP };

#include "support.h"
#include "soundchip.h"
#include "score.h"
#include "race.h"
#include "menu.h"

void setup()
{
  Serial.begin(115200);
  Serial.println( "ClassicRacer by Carles Oriol 2020");

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  cMouse      = GameControllerMouse (1);
  cJoystick   = GameControllerJoystick (2, JOY_FIRE, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_FIRE );
  cKeysArrows = GameControllerKeys (3, fabgl::VK_UP, fabgl::VK_DOWN, fabgl::VK_LEFT, fabgl::VK_RIGHT, fabgl::VK_RSHIFT, fabgl::VK_ESCAPE);
  cKeysASTF   = GameControllerKeys (4, fabgl::VK_t, fabgl::VK_f, fabgl::VK_a, fabgl::VK_s, fabgl::VK_t, fabgl::VK_ESCAPE);
  cKeysQAOP   = GameControllerKeys (5, fabgl::VK_q, fabgl::VK_a, fabgl::VK_o, fabgl::VK_p, fabgl::VK_SPACE, fabgl::VK_ESCAPE );

  VGAController.begin();
  VGAController.setResolution(VGA_320x200_75Hz, 320, 200);
  VGAController.moveScreen(-6, 0);
  soundGenerator.setVolume(127);
  soundGenerator.play(true);
  soundGenerator.attach( &swg);

  initNumbers();  

  SPIFFS.begin(true);      
  loadScore();
  highScore = top[0].points;
  fastest = top[0].timesec; 
  lowestTopScore = top[HIGHSCORE_ITEMS-1].points;
}

void loop()
{
    static int exitv = 0;    
    static int editItem = -1;

    playSoundTuc();
    waitNoButton(250); // No button pressing between scene change
    
    if ( exitv == 2 )
    { 
      Score score;
      score.editItem = editItem;      
      score.start();      
      editItem = -1;
      exitv = score.exitValue;      
    }    
    else if ( exitv == 1 )
    {
      
      
      Race race;
      race.players[0].controller = gameControllers[playercontrol[0]];
      race.players[1].controller = gameControllers[playercontrol[1]];
      race.start();  
      exitv = 2; // or 4

      if( exitv == 2 )
      {        
        ScoreCard *psc = addScore(   "AAA", 
                        race.players[race.winner].points, 
                        race.winnerTime, 
                        race.players[race.winner].cars, 
                        race.players[race.winner].controller->id, 
                        (gameControllers[playercontrol[0]]->id!=0?1:0)+(gameControllers[playercontrol[1]]->id!=0?1:0));

        if( psc != NULL )
          {
            editItem = getScoreIndex( psc ); // to set initials
            saveScore();
            highScore = top[0].points;
            fastest = top[0].timesec;    
            lowestTopScore = top[HIGHSCORE_ITEMS-1].points;
          }
      }
    }
    else
    {
      Menu menu;
      menu.start();
      exitv = menu.exitValue;
    }    
}
