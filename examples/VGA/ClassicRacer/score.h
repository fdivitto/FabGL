#define SCORE_TIMEOUT 35000L

#define HIGHSCORE_ITEMS 12

// redraw every millis
#define REDRAW_EVERY 5000

#define DELAYDRAW_MILLIS 5 

#define FORMAT_SPIFFS_IF_FAILED true

const char *scoreFileName = "/score.dat";

int highScore;
int lowestTopScore;
int fastest;

RGB888 scorecolors[12] = {  RGB888(255, 0, 0),RGB888(255, 255, 128),RGB888(255, 0, 255),
                            RGB888(0, 0, 255),RGB888(128, 255, 255),RGB888(255, 255,0 ),
                            RGB888(128, 0, 0),RGB888(0, 128, 0),RGB888(128, 0, 128),
                            RGB888(0, 0, 128),RGB888(0, 128, 128),RGB888(128, 128, 0),
                          };

struct ScoreCard
{
  char initials[4];
  int points;
  int timesec;
  int crashed;    
  int controler;    // 0 - mouse, 1 - Key arrows, 2 - Keys astf, 3 - Joystick
  int gameplayers;  // 1 o 2

  ScoreCard * operator =(ScoreCard *tscore )
  {
    memcpy(this, tscore, sizeof(ScoreCard));  
    return this;
  }
};

ScoreCard top[HIGHSCORE_ITEMS] = { 
   { "COM", 1220, 143, 2,  0, 1 },
   { "NCW", 1200, 136, 1,  3, 2 },
   { "NOC", 1100, 160, 4,  1, 2 },
   { "MOC", 1050, 164, 4,  2, 1 },
   { "JCP",  900, 300, 12, 4, 2 },
   { "CMD",  800, 350, 18, 5, 1 },                       
   { "GCW",  200, 250, 28, 4, 1 },                       
   { "ACG",  200, 250, 28, 3, 1 },                       
   { "PLA",  200, 250, 28, 4, 1 },                       
   { "YTH",  200, 250, 28, 1, 1 },                       
   { "EGA",  200, 250, 28, 2, 2 },                       
   { "MEE",  200, 250, 28, 5, 2 },                       
};

void saveScore()
{
  File file = SPIFFS.open(scoreFileName, "w");
  file.write ( (const uint8_t *)&top, sizeof(struct ScoreCard)*HIGHSCORE_ITEMS);   
  file.flush();  
  file.close();  
}

bool loadScore()
{ 
  File file = SPIFFS.open(scoreFileName, "r");
  if( !file) return false;          
  file.read ( (uint8_t *)&top, sizeof(struct ScoreCard)*HIGHSCORE_ITEMS);   
  file.close();
  return true;     
}

int getScorePosition(  int points )
{
  for (int compta = 0; compta < HIGHSCORE_ITEMS; compta++)
    if( top[compta].points < points )
      return compta;
  return -1;
}

int getScoreIndex( ScoreCard *psc )
{
  for (int compta = 0; compta < HIGHSCORE_ITEMS; compta++)
    if( &top[compta]== psc )
       return compta;
 return -1;
}

ScoreCard *allocScore( int points)
{
  int n = getScorePosition( points );
  if( n == -1 ) return NULL;
  
  for (int compta = HIGHSCORE_ITEMS-1; compta > n; compta--)
    top[compta] = top[compta-1];
  return (&top[n]);  
}

ScoreCard *addScore( const char *initials, int points, int timesec, int crashed, int controler, int gameplayers )
{
  ScoreCard *pscore= allocScore( points );
  if(pscore == NULL) return NULL;

  pscore->points = points;
  strcpy( pscore->initials, initials);
  pscore->timesec = timesec;
  pscore->crashed = crashed;
  pscore->controler = controler;
  pscore->gameplayers = MAX(gameplayers,1);
 
  return pscore;
}

struct Score : public Scene 
{   
  bool bExit = false;
  long scorelastactivity = 0;

  long lastDraw = 0;
  Bitmap bitmap_cariconright = Bitmap(8, 8, bitmap_car_to_right_data, PixelFormat::Mask, RGB888(0, 0, 255));
  Sprite sprites[1];
  int exitValue = 0;
  int editItem = -1; // item number to edit
  int editInitial = 0; // 0 to 3
  int currentStart = 0;

  bool checkExit()
  {
    if( editItem != -1 ) return false;
    for( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
    { 
      gameControllers[ncont]->update();
      if( gameControllers[ncont]->isButtonA() || gameControllers[ncont]->isButtonB() ||
          gameControllers[ncont]->isLeft() || gameControllers[ncont]->isRight()   ) 
      bExit = true;
    }
    return bExit;
  }
  
  Score()
    : Scene(1, 20, VGAController.getViewPortWidth(), VGAController.getViewPortHeight())
  {
  }

  void slowRefresh( int cariconx=-1, int caricony = -1) 
  {     
    if(cariconx != -1)  sprites[0].x = cariconx; 
    if(caricony != -1)  sprites[0].y = caricony;
    if( !bExit)
    {
      sprites[0].visible = true;
      VGAController.refreshSprites();     
      canvas.waitCompletion();     
      vTaskDelay(DELAYDRAW_MILLIS/portTICK_PERIOD_MS);      
    }
  }

  void slowDrawText(int x, int y, const char *text, int dx=8 )
  {
    slowRefresh( x, y );    
    while( *text )
    {
      drawChar( x, y, *text++);
      x += dx;
      slowRefresh( x+8, y );
      checkExit();
    }    
  }     

  void drawScores()
  {
    Bitmap bitmap_classicracer = Bitmap(21*8, 16, bitmap_classicracer_data, PixelFormat::Mask, RGB888(255, 255, 255));      
    const uint8_t *bitmap_controls_list[6] = {bitmap_mouse_data,
                                              bitmap_mouse_data, bitmap_joystick_data,
                                              bitmap_keybb_data, bitmap_keyba_data,  bitmap_keybo_data  };
      
    //int nselected = 1;
    
    canvas.setBrushColor(RGB888(0, 0xff, 0));
    canvas.clear();
    
    canvas.setBrushColor(RGB888(0, 0xff, 0));
    canvas.fillRectangle(0, 8, 319, 5*8 - 1);    
    canvas.drawBitmap( 10*8, 2*8, &bitmap_classicracer );  
    
    canvas.setBrushColor(RGB888(0, 0xff, 0));
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.setPenColor(RGB888(0, 0, 0));   
    canvas.drawText(14*8, 7*8, "HALL OF FAME");
    canvas.setPenColor(RGB888(0xff, 0xff, 192));   
    canvas.drawText(9*8, 10*8, "NAME SCORE TIME CARS GAME");
    slowRefresh();

    if( editItem != -1) currentStart = editItem;
 
    for (int ncompta = 0; ncompta < 6; ncompta++)
    {
      char buffer[16];
      int y = (12+ncompta*2) * 8;

      if( ncompta+currentStart < HIGHSCORE_ITEMS )
      {      
        ScoreCard *ptop = &top[ncompta+currentStart];        
        
        canvas.setPenColor(scorecolors[ncompta+currentStart]);
  
        sprintf( buffer, "%2d", ncompta+currentStart+1 );
        slowDrawText(6*8, y, buffer);
        slowDrawText(9*8, y, ptop->initials);
        sprintf( buffer, "%4d", ptop->points );
        slowDrawText(14*8, y, buffer);
        sprintf( buffer, "%01d:%02d", ptop->timesec/60, ptop->timesec%60 );
        slowDrawText(20*8, y, buffer);
        sprintf( buffer, "%2d", ptop->crashed );      
        slowDrawText(26*8, y, buffer);
  
        Bitmap bitmap_control = Bitmap(8, 8, bitmap_controls_list[ptop->controler], PixelFormat::Mask, scorecolors[ncompta+currentStart]);
        canvas.drawBitmap( 30*8, y, &bitmap_control );  
        slowRefresh(31*8);
  
        Bitmap bitmap_caricon = Bitmap(8, 8, bitmap_caricon_data, PixelFormat::Mask, scorecolors[ncompta+currentStart]);
        canvas.drawBitmap( 32*8, y, &bitmap_caricon );  
        slowRefresh(33*8);
        
        if( ptop->gameplayers == 2)
        {
          Bitmap bitmap_cariconr = Bitmap(8, 8, bitmap_cariconright_data, PixelFormat::Mask, scorecolors[ncompta+currentStart]);
          canvas.drawBitmap( 33*8, y, &bitmap_cariconr );        
          slowRefresh(34*8);
        }
      }
      
      slowRefresh(-1, 400);
    }

    if( editItem != -1)
    {
      canvas.setPenColor( RGB888(255,255,255) );
      canvas.drawRectangle( 9*8 - 4, 12*8 - 4, 12*8 + 3, 13*8+3); 
    }
  
    currentStart += 6;
    currentStart %= HIGHSCORE_ITEMS;
    
    slowRefresh( 400, 0);
    canvas.waitCompletion();
    lastDraw = millis();  
  }

  void init()
  {   
    if( editItem != -1 ) currentStart = 0;
    editInitial = 0;
    scorelastactivity = millis();
    sprites[0].addBitmap(&bitmap_cariconright);
    sprites[0].visible = false;
    sprites[0].y=1000; // hide it
    VGAController.setSprites(sprites, 1); 
    drawScores();      
  }

  void update( int updateCount )
  { 
    if( editItem == -1 )
      checkExit();
    else
    {    
      bool bleft = false;
      bool bright = false;
      bool bfire = false;     
      
      for( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
      { 
        gameControllers[ncont]->update();
        if( gameControllers[ncont]->isLeft() ) bleft = true;
        if( gameControllers[ncont]->isRight() ) bright = true;
        if( gameControllers[ncont]->isButtonA() ) bfire = true;
      }

      if( bleft || bright || bfire )
      {        
        if( bleft ) top[editItem].initials[editInitial]--;
        if( bright ) top[editItem].initials[editInitial]++;
        if( top[editItem].initials[editInitial] < 'A' ) top[editItem].initials[editInitial] = 'Z';
        if( top[editItem].initials[editInitial] > 'Z' ) top[editItem].initials[editInitial] = 'A';
        
        if( bfire)
        {
          editInitial++;          
          playSoundPic();
          waitNoButton(1000);           // 1s max
        }
        else
          playSoundPong();
             
        canvas.setPenColor(scorecolors[editItem]);
        canvas.setBrushColor(RGB888(0,255,0) );
        canvas.drawText(9*8, 12*8, top[editItem].initials);             
        vTaskDelay(50/portTICK_PERIOD_MS);
   
        if(editInitial == 3 )
        {
            saveScore();
            bExit = true;
            exitValue = 2;
            editItem = -1;
            VGAController.removeSprites();
            this->stop();                
            return;
        }
      }

        canvas.setPenColor( millis() % 250 == 0 ? RGB888(0,255,0) : scorecolors[editItem] );  
        canvas.setBrushColor(RGB888(0,255,0) );             
        canvas.drawChar((9+editInitial)*8, 12*8, top[editItem].initials[editInitial]);      
        canvas.waitCompletion(); 
    }

     if ( bExit || millis() > scorelastactivity + SCORE_TIMEOUT )
     {
          canvas.waitCompletion();  
          exitValue = 0;
          editItem = -1;
          VGAController.removeSprites();
          this->stop();                
          return;
     }
 
    if (millis() > lastDraw+REDRAW_EVERY && editItem == -1 )
      drawScores();

     canvas.waitCompletion();
     vTaskDelay(10/portTICK_PERIOD_MS);
  }
 
  void collisionDetected(Sprite *spriteA, Sprite *spriteB, Point collisionPoint  ) {}
};
