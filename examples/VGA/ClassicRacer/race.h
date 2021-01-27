

#define MAXSPEED 450
#define MAXSPEED_FREQUENCY 150

#define TIME_AFTER_CRASH 2000

#define RACE_TIMEOUT (60L*1000L*5)

int RACECARS = 200;

#define NOCRASH_BONUS (RACECARS*3)

struct Race : public Scene
{
  Race()
    : Scene(8, 20, VGAController.getViewPortWidth(), VGAController.getViewPortHeight())
  {
  }

  Bitmap cariconl = Bitmap(8, 8, bitmap_cariconleft_data, PixelFormat::Mask, RGB888(255, 255, 0));
  Bitmap cariconr = Bitmap(8, 8, bitmap_cariconright_data, PixelFormat::Mask, RGB888(255, 255, 0));
  Bitmap carbitmap = Bitmap(24, 21, carbitmap_data, PixelFormat::Mask, RGB888(0, 0, 255));
  Bitmap carbitmap_prota = Bitmap(24, 21, carbitmap_data, PixelFormat::Mask, RGB888(255, 255, 0));
  Bitmap carbitmap_dreta = Bitmap(24, 21, carbitmap_data_dreta, PixelFormat::Mask, RGB888(255, 255, 0));
  Bitmap carbitmap_esquerra = Bitmap(24, 21, carbitmap_data_esquerra, PixelFormat::Mask, RGB888(255, 255, 0));
  Bitmap carbitmap_crash = Bitmap(24, 21, carbitmap_data_crash, PixelFormat::Mask, RGB888(255, 191, 0));
  Bitmap carbitmap_crash2 = Bitmap(24, 21, carbitmap_data_crash2, PixelFormat::Mask, RGB888(255, 64, 0));

  Bitmap carbitmap_anim[3] = { Bitmap(24, 21, carbitmap_data_anim0, PixelFormat::Mask, RGB888(255, 255, 0)),
                             Bitmap(24, 21, carbitmap_data_anim1, PixelFormat::Mask, RGB888(255, 255, 0)),
                             Bitmap(24, 21, carbitmap_data_anim2, PixelFormat::Mask, RGB888(255, 255, 0)) };

  Bitmap carbitmap_banim[3] = { Bitmap(24, 21, carbitmap_data_anim0, PixelFormat::Mask, RGB888(0, 0, 255)),
                             Bitmap(24, 21, carbitmap_data_anim1, PixelFormat::Mask, RGB888(0, 0, 255)),
                             Bitmap(24, 21, carbitmap_data_anim2, PixelFormat::Mask, RGB888(0, 0, 255)) };

  struct Player
  {    
    int player;
    Sprite *sprite;

    int xspeed;
    int yspeed;

    int lastCar;
    int lastCarSprite;

    int score, points;

    bool crashed;
    long crashTime;
    int cars;
    int dcrashx;

    int minx;
    int maxx;

    int level ;
    GameController *controller;

    int wheel_sound_mult = 1;

    SawtoothWaveformGenerator carEngineSound;
    SawtoothWaveformGenerator carAdvanceSound;
    NoiseWaveformGenerator carCrashSound;
    SawtoothWaveformGenerator carWheelSound;
    
    Player()
    {
      xspeed = 0; yspeed = 60; lastCar = 0; lastCarSprite = 0;
      score = 0; points = 0; crashed = false; crashTime = 0;
      cars = 0; level = 1;

      soundGenerator.attach(&carEngineSound);
      carEngineSound.setFrequency(30);
      carEngineSound.setVolume(75);
      carEngineSound.enable(true);

      soundGenerator.attach(&carAdvanceSound);
      carAdvanceSound.setFrequency(0);
      carAdvanceSound.setVolume(40);
      carAdvanceSound.enable(false);

      soundGenerator.attach(&carCrashSound);
      carCrashSound.setVolume(0);
      carCrashSound.enable(false);

      soundGenerator.attach(&carWheelSound);
      carWheelSound.setFrequency(4800);
      carWheelSound.setVolume(0);
      carWheelSound.enable(true);
    }

    void noSound()
    {
      carEngineSound.enable(false);
      carAdvanceSound.enable(false);
      carCrashSound.enable(false);
      carWheelSound.enable(false);
    }

    void stop(void)
    {
      soundGenerator.detach(&carEngineSound);
      soundGenerator.detach(&carAdvanceSound);
      soundGenerator.detach(&carCrashSound);
      soundGenerator.detach(&carWheelSound);
    }

    void init( int num_, Sprite *sprite_, int minx_, int maxx_)
    {
      sprite = sprite_; minx = minx_; maxx = maxx_; player = num_;

      if ( controller->mode == MODE_NONE ) {
        sprite->visible = false;
        yspeed = 120 + random(-50, 50);
      }
    }

    void checkX( )
    {
      if ( sprite->x < minx)
      {
        sprite->x = minx;
        xspeed = 0;
        carWheelSound.setVolume( 0);
      }

      if ( sprite->x > maxx)
      {
        sprite->x = maxx;
        xspeed = 0;
        carWheelSound.setVolume( 0);
      }
    }

    void Collision( int x)
    {
      crashed = true;
      cars++;
      sprite->setFrame( 3 );
      crashTime = millis();
      xspeed = 0;
      carAdvanceSound.enable( false);
      carEngineSound.enable( false);
      carCrashSound.enable(true);

      carCrashSound.setVolume(75);

      points -= 250;
      if ( points < 0) points = 0;

      showSpeed();
      dcrashx = ((sprite->x - x) * (yspeed / 80)) / 2 + random(-10, 10);
    }

    void crashedTimeout()
    {
      static int compta = 0;

      compta ++;

      if ( crashTime + TIME_AFTER_CRASH < millis() )
      {
        crashed = false;
        sprite->setFrame( 1 );
        yspeed = 60;
        lastCarSprite = 0;
        lastCar = score;
        carEngineSound.enable( true );
        carCrashSound.enable(false);
        showSpeed();
      }
      else
      {
        carCrashSound.setVolume( 75 - (millis() - crashTime) / 25);
        sprite->setFrame( 3 + (millis() / 100 % 2));

        //if( compta % 4 == 0)
        {
          sprite->x += dcrashx;
          if ( sprite->x < minx)
          {
            sprite->x = minx;
            dcrashx *= -1;
          }
          if ( sprite->x > maxx)
          {
            sprite->x = maxx;
            dcrashx *= -1;
          }

          dcrashx = (dcrashx * 999) / 1000;
        }
      }
      drawCrashedCars();
    }

    void accelerateAndMove( )
    {
      controller->update();

      if ((controller->isButtonA() || controller->isUp()) && (yspeed  < MAXSPEED))
        yspeed += 2;

      if (controller->isDown() )
        yspeed = (yspeed * 95) / 100;

      if (controller->mode != MODE_RELATIVEPOS )
      {

        if ( controller->isRight() )
        {
          if ( xspeed < 0)  xspeed = 0;   // change direction stop acceleration
          if ( xspeed == 0) xspeed = 200; // minimum speed
          else if ( xspeed < 500) xspeed += 25; // increase acceleration if maxiumum acceleration not reached
        }
        else if ( controller->isLeft() )
        {
          if ( xspeed > 0)  xspeed = 0;     // change direction stop acceleration
          if ( xspeed == 0) xspeed = -200;  // minimum speed
          else if ( xspeed > -500) xspeed -= 25; // increase acceleration if maxiumum acceleration not reached
        }
        else
          xspeed = 0;   // keys released stop acceleration
      }

      if (controller->mode == MODE_RELATIVEPOS && !crashed)
      {
        int dx = controller->getDX();
        xspeed = 0;
        sprite->x += dx;
        checkX();
        xspeed += dx;
        wheel_sound_mult = 20;
      }
    }

    void showSpeed( )
    {
      int x = ((player == 0) ? 2 : 314) ;

      int y = 199 - map( (crashed ? 0 : yspeed), 0, MAXSPEED, 0, 199);
      canvas.setBrushColor(RGB888(0xFF, 0XFF, 0xFF));
      canvas.fillRectangle(x, 0, x + 3, y - 1);
      canvas.setBrushColor(RGB888(0xFF, 0X40, 0x40));
      canvas.fillRectangle(x, y, x + 3, 200);
    }

    void doScore()
    {
      if (sprite->visible)
        points += 5 + yspeed / 15;
      score++;
      carAdvanceSound.enable(false);

      if (score == 5 ) level = 2;
      if (score == 10 ) level = 3;
    }

    void update()
    {
      int freq = map((crashed ? 0 : yspeed),
                     0, MAXSPEED, 0, MAXSPEED_FREQUENCY);
      carEngineSound.setFrequency(MIN(freq, MAXSPEED_FREQUENCY));

      carWheelSound.setFrequency(random(2000 , 3000) + yspeed * 4);
      carWheelSound.setVolume( minInt(25, abs(xspeed) / 10)*wheel_sound_mult );
      if (wheel_sound_mult != 1 ) wheel_sound_mult = 1;
    }

    void drawCrashedCars()
    {
      canvas.setBrushColor(Color::Red);
      char buffer[16];
      canvas.setPenColor(RGB888(0xff, 0XFF, 0));
      if ( crashed && millis() % 200 > 100)
        canvas.setPenColor(Color::Red);
      sprintf( buffer, "%02d", cars );
      canvas.drawText(19 * 8, (player == 0 ? 22 : 23) * 8, buffer);
    }
  };

  Player players[2];
  Sprite sprites[8];
  uint8_t mapcars[200]; // max racecars = 200
  long lastSpriteTime = 0;
  long cary[6];
  long startRaceTime = 0;
  long currentTime = 0;
  int winner = 0;
  long winnerTime = 0L;

  int exitValue = -1;

  void setCarAdvanceSound()
  {
    // 166-205
    // frequencies 30-124
    // velocitats 0-500

    for (int ncompta = 2; ncompta < 8; ncompta++)
    {
      if ( sprites[ncompta].visible && sprites[ncompta].y >= 152  )
      {
        // 166-205-166 152 - 176 - 200
        int player = sprites[ncompta].x < 160 ? 0 : 1;
        players[player].carAdvanceSound.enable( !players[player].crashed );

        int freq = sprites[ncompta].y < 176 ? map(sprites[ncompta].y, 152, 176, 166, 255 ) : map(sprites[ncompta].y, 176, 200, 255, 100 );
        int volume = 70 - abs( sprites[player].x - sprites[ncompta].x) + players[player].yspeed / 5;

        players[player].carAdvanceSound.setFrequency(freq);
        players[player].carAdvanceSound.setVolume(volume);
      }
    }
  }

  void init()
  {

    for ( int ncompta = 0; ncompta < 8; ncompta++)
      sprites[ncompta].visible = false;

    for (int ncompta = 0; ncompta < RACECARS; ncompta++)
      mapcars[ncompta] = random(0, 99);

    for ( int ncompta = 0; ncompta < 2; ncompta++)
    {
      sprites[ncompta].addBitmap(&carbitmap_prota);
      sprites[ncompta].addBitmap(&carbitmap_dreta);
      sprites[ncompta].addBitmap(&carbitmap_esquerra);
      sprites[ncompta].addBitmap(&carbitmap_crash);
      sprites[ncompta].addBitmap(&carbitmap_crash2);
      sprites[ncompta].addBitmap(&carbitmap_anim[0]);
      sprites[ncompta].addBitmap(&carbitmap_anim[1]);
      sprites[ncompta].addBitmap(&carbitmap_anim[2]);
      sprites[ncompta].visible = true;
    }

    players[0].init(0, &sprites[0], 23, 122);
    players[1].init(1, &sprites[1], 175, 274);
    sprites[0].moveTo(24 + 48 - 24, 200 - 24);
    sprites[1].moveTo(176 + 48 + 24, 200 - 24);

    for ( int ncompta = 0; ncompta < 6; ncompta++)
    {
      sprites[2 + ncompta].addBitmap(&carbitmap);
      sprites[2 + ncompta].addBitmap(&carbitmap_banim[0]);
      sprites[2 + ncompta].addBitmap(&carbitmap_banim[1]);
      sprites[2 + ncompta].addBitmap(&carbitmap_banim[2]);
      sprites[2 + ncompta].moveTo(random(24, 123), 0);
      sprites[2 + ncompta].visible = false;
    }

    for ( int ncompta = 0; ncompta < 8; ncompta++)
      addSprite(&sprites[ncompta]);

    VGAController.setSprites(sprites, 8);


    drawBackground();
    canvas.waitCompletion();

    startRaceTime = lastSpriteTime = millis();

    soundGenerator.play(true);
    soundGenerator.setVolume(127);
  }

  void drawTime()
  {
    static long lastDrawTime = -999999;

    currentTime = millis();
    currentTime -= startRaceTime;

    if ( currentTime > lastDrawTime + 100)
    {
      long timeInSeconds = currentTime / 1000;

      int minuts = timeInSeconds / 60;
      int segons = timeInSeconds % 60;

      canvas.setBrushColor(Color::Red);
      canvas.fillRectangle(19 * 8, 6 * 8, 21 * 8 - 1, 20 * 8 - 1);

      canvas.setBrushColor(RGB888(0xff, 0xff, 0));
      canvas.drawBitmap( 19 * 8 + 2, 6 * 8, &numbers_bitmaps[(minuts / 10)]);
      canvas.drawBitmap( 19 * 8 + 2, 9 * 8, &numbers_bitmaps[(minuts % 10)]);
      canvas.drawBitmap( 19 * 8 + 2, 14 * 8, &numbers_bitmaps[(segons / 10)]);
      canvas.drawBitmap( 19 * 8 + 2, 17 * 8, &numbers_bitmaps[(segons % 10)]);

      if ( (currentTime / 500) % 2 == 0)
      {
        canvas.fillRectangle( 19 * 8 + 2, 13 * 8 - 4, 19 * 8 + 2 + FONT_SCALE, 13 * 8 - 4 + FONT_SCALE - 1);
        canvas.fillRectangle( 20 * 8 + 2, 13 * 8 - 4, 20 * 8 + 2 + FONT_SCALE, 13 * 8 - 4 + FONT_SCALE - 1);
      }

      lastDrawTime = currentTime;
    }
  }

  void drawBackground()
  {
    canvas.setBrushColor(RGB888(0, 0XFF, 0));
    canvas.clear();

    canvas.setBrushColor(Color::Red);
    canvas.fillRectangle(0, 0, 23, 200);
    canvas.fillRectangle(320 - 24, 0, 319, 200);
    canvas.fillRectangle(144, 0, 144 + 24 + 7, 200);
    canvas.setBrushColor(RGB888(0, 0XFF, 0));
    canvas.fillRectangle(8, 0, 15, 199);
    canvas.fillRectangle(304, 0, 311, 199);

    for (int ncompta = 0; ncompta < RACECARS; ncompta++)
    {
      canvas.setPixel(8 + 2 + (mapcars[ncompta] * 4) / 100,
                      200 - ((ncompta+1) * 200 / RACECARS), Color::Red );

      canvas.setPixel(306 + (mapcars[ncompta] * 4) / 100,
                      200 - ((ncompta+1) * 200 / RACECARS), Color::Red );
    }


    canvas.setBrushColor(Color::Red);
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.setPenColor(Color::Yellow);
    canvas.drawText(18 * 8, 0, "HIGH");

    canvas.drawText(18 * 8, 16, "TIME");

    char buffer[16];
    canvas.setPenColor(RGB888(0xff, 0XFF, 0));
    sprintf( buffer, "%04d", highScore );
    canvas.drawText(18 * 8, 8, buffer);

    canvas.setPenColor(RGB888(0xff, 0XFF, 0));
    sprintf( buffer, "%02d", fastest / 60 );
    canvas.drawText(18 * 8, 24, buffer);
    canvas.setPenColor(RGB888(0xff, 0X80, 0x00));
    sprintf( buffer, "%02d", fastest % 60 );
    canvas.drawText(20 * 8, 24, buffer);

    canvas.drawBitmap(18 * 8, 22 * 8, &cariconl);
    canvas.drawBitmap(21 * 8, 23 * 8, &cariconr);

    drawPoints();
    for (int player = 0; player < 2; player++)  players[player].showSpeed();

  }

  void drawPoints( )
  {
    for (int player = 0; player < 2; player++)
    {
      int leftpos = player == 0 ? 7 * 8 : 26 * 8;
      int points = players[player].points;

      canvas.setBrushColor(RGB888(0, 0xff, 0));
      canvas.fillRectangle(leftpos, 2 * 8, leftpos + (FONT_WIDTH + 1) * 4 * FONT_SCALE, 2 * 8 + FONT_HEIGHT * FONT_SCALE - 1);
      canvas.drawBitmap(leftpos, 2 * 8, &numbers_bitmaps[(points / 1000) % 10]);
      canvas.drawBitmap(leftpos + (FONT_WIDTH + 1)*FONT_SCALE, 2 * 8, &numbers_bitmaps[(points / 100) % 10]);
      canvas.drawBitmap(leftpos + (FONT_WIDTH + 1) * 2 * FONT_SCALE, 2 * 8, &numbers_bitmaps[(points / 10) % 10]);
      canvas.drawBitmap(leftpos + (FONT_WIDTH + 1) * 3 * FONT_SCALE, 2 * 8, &numbers_bitmaps[points % 10]);

      players[player].drawCrashedCars();
    }
  }

  void doScore( int player)
  {
    players[player].doScore();
    drawPoints( );

    if ( players[player].score > 0  && players[player].score <= RACECARS)
    {
      int y = 200 - ((players[player].score) * 200) / RACECARS;
      int x = ((player == 0) ? 8 : 304) ;

      canvas.setPenColor(((player == 0) ? RGB888(255, 255, 0) : RGB888(255, 255, 0)) );
      canvas.setBrushColor(RGB888(0xff, 0xff, 0));
      canvas.fillRectangle( x + 1, y, x + 6, y + (200 / RACECARS) );

      //canvas.setPixel(x+1+(mapcars[players[player].score -1]*6)/100, y, Color::Red );
    }
  }

  int getFirstFreeSprite()
  {
    for (int compta = 2; compta < 8; compta++)
      if (sprites[compta].visible == false )
        return compta;
    return 0;
  }

  void exitRace( int value )
  {
    canvas.waitCompletion();
    exitValue = value;
    players[0].stop(); players[1].stop();
    VGAController.removeSprites();
    this->stop();
  }


  void slowDrawText(int x, int y, const char *text, int dx = 8 )
  {
    while ( *text )
    {
      canvas.drawChar(x, y, *text++ );      
      x += dx;
      vTaskDelay(20 / portTICK_PERIOD_MS);
      canvas.waitCompletion();
    }
  }

  void winAnimation( int player)
  {
    char buffer[32];

    checkered(players[player].minx + 1, 6 * 8, 120, 2 * 8, 8, 8);
    checkered(players[player].minx + 1, 18 * 8, 120, 2 * 8, 8, 8);

    canvas.setPenColor(RGB888(0xFF, 0XFF, 0xFF));
    canvas.setBrushColor(RGB888(0x00, 0XFF, 0x00));
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));

    slowDrawText( players[player].minx + 2 * 8 + 1, 9 * 8, "YOU WON" );

    sprintf( buffer, "SCORE:  %4d",      players[player].points );
    slowDrawText( players[player].minx + 2 * 8 + 1, 11 * 8, buffer );

    sprintf( buffer, "TIME : %02ld:%02ld", (currentTime / 1000) / 60,  (currentTime / 1000) % 60 );
    slowDrawText( players[player].minx + 2 * 8 + 1, 12 * 8, buffer );

    sprintf( buffer, "CARS :  %4d", players[player].cars  );
    slowDrawText( players[player].minx + 2 * 8 + 1, 13 * 8, buffer );

    sprintf( buffer, "BONUS:  %4d", (players[player].cars == 0) ? NOCRASH_BONUS : 0);
    slowDrawText( players[player].minx + 2 * 8 + 1, 14 * 8, buffer);

    if ( players[player].cars == 0) players[player].points += NOCRASH_BONUS;

    bool bExit = false;
    int ncompta = 0;
    long tstart = millis();

    while (!bExit )
    {
      ncompta++;
      
      for ( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
      {
        gameControllers[ncont]->update();
        if ( gameControllers[ncont]->isButtonA() ) bExit = true;
      }

      if ( highScore < players[player].points)
      {
        canvas.setPenColor(scorecolors[ncompta%12]);
        slowDrawText( players[player].minx + 2 * 8 + 1, 16 * 8, " TOP RECORD ");            
      }
      else if ( lowestTopScore < players[player].points)
      {
        canvas.setPenColor( (ncompta % 2) ? RGB888(0x0, 0XFF, 0x0) : RGB888(0xFF, 0XFF, 0x0));         
        slowDrawText( players[player].minx + 2 * 8 + 1, 16 * 8, "HALL OF FAME");
      }

      canvas.waitCompletion();
      vTaskDelay(10 / portTICK_PERIOD_MS);

      if( millis()-tstart > 10000) bExit = true; // 10 seconds max time
    }
  }

  void update(int updateCount)
  {
    long currentTime = millis();
    long ellapsed = currentTime - lastSpriteTime;

    if ( millis() - startRaceTime > RACE_TIMEOUT ) // brutal timeout
    {
      exitRace( 2 );
      return ;
    }

    static bool wasMaxPoints = false;

    int maxPoints = MAX(players[0].points, players[1].points);
    if ( maxPoints > highScore )
    {
      char buffer[16];

      canvas.setBrushColor(Color::Red);
      canvas.setPenColor(RGB888(0xff, 0XFF, 0x00));
      if ( millis() % 200 > 100)
        canvas.setPenColor( Color::Red);

      sprintf( buffer, "%04d", maxPoints );

      canvas.drawText(18 * 8, 8, buffer);
      wasMaxPoints = true;
    }
    else if ( wasMaxPoints )
    {
      wasMaxPoints = false; // points could decrese.

      char buffer[16];

      canvas.setBrushColor(Color::Red);
      canvas.setPenColor(RGB888(0xff, 0XFF, 0x00));
      sprintf( buffer, "%04d", highScore );

      canvas.drawText(18 * 8, 8, buffer);
    }

    drawTime();

    setCarAdvanceSound();

    int oldyspeed[2] = { -1, -1};
    int oldx[2] = { -1, -1};

    for ( int player = 0; player < 2; player++)
    {
      if ( players[player].score == RACECARS) // won
      {
        winner = player;
        winnerTime = (millis() - startRaceTime) / 1000;

        players[0].noSound();
        players[1].noSound();

        winAnimation( player);
        exitRace( player );

        return;
      }

      players[player].update();
      oldyspeed[player] = players[player].yspeed;     // keey old y speed
      oldx[player] = sprites[player].x;               // keep old x position

      if ( !players[player].crashed )
      {
        sprites[player].x += (players[player].xspeed * ellapsed) / 1000;
        players[player].checkX();

        int nlevelcardelay = 60; // 60
        if ( players[player].level == 2 ) nlevelcardelay = 100;
        if ( players[player].level == 1 ) nlevelcardelay = 130;

        if ( (players[player].lastCarSprite == 0 || sprites[players[player].lastCarSprite].y > nlevelcardelay) )
        {
          if (  players[player].lastCar < RACECARS )
          {
            int nsprite = getFirstFreeSprite();
            if (nsprite != 0)
            {
              int xpos = mapcars[players[player].lastCar] ;
              players[player].lastCar++;
              players[player].lastCarSprite = nsprite;

              sprites[nsprite].moveTo( ((player == 0) ? 23 : 175) + xpos , 0);
              sprites[nsprite].visible = true;
              cary[nsprite - 2] = -2400;

            }
          }

        }
      }
      else
        players[player].crashedTimeout();
    }

    for ( int compta = 0; compta < 6; compta++)
    {
      if ( sprites[2 + compta].visible )
      {
        long speedy = (sprites[2 + compta].x < 160) ? players[0].yspeed : players[1].yspeed;

        int player = (sprites[2 + compta].x > 150) ? 1 : 0;

        if ( !players[player].crashed )
        {
          cary[compta] += (speedy * ellapsed) / 10;
          sprites[2 + compta].y = cary[compta] / 100;

          if (sprites[2 + compta].y >= 200)
          {
            sprites[2 + compta].y = -24;
            cary[compta] = -2400;

            sprites[2 + compta].visible = false;
            if ( !players[player].crashed )
              doScore(player);
          }
        }
        else
        {
          cary[compta] -= (300 * ellapsed) / 10;
          sprites[2 + compta].y = cary[compta] / 100;

          if (sprites[2 + compta].y < -24)
            sprites[2 + compta].visible = false;

        }
      }

    }

    lastSpriteTime = currentTime;

    auto keyboard = PS2Controller.keyboard();

    if (keyboard->isKeyboardAvailable() && keyboard->isVKDown(fabgl::VK_ESCAPE))
    {
      exitRace(3);
      return;
    }

    for ( int player = 0; player < 2; player++)
      players[player].accelerateAndMove();


    static bool MkeyPressed = false;
    if (keyboard->isVKDown(fabgl::VK_F2) && !MkeyPressed)
    {
      soundGenerator.setVolume( soundGenerator.volume() == 0 ? 127 : 0 );
      MkeyPressed = true;
    }
    if ( keyboard->isVKDown(fabgl::VK_F2) == 0 && MkeyPressed == 1)
      MkeyPressed = false;

    if (keyboard->isVKDown(fabgl::VK_F1) )
    {
      waitForKeyRelease( fabgl::VK_F1 ); // debounce

      // paused
      int nvolume = soundGenerator.volume();
      soundGenerator.setVolume( 0 );

      waitForKey( fabgl::VK_F1 ); // wait key p to continue
      waitForKeyRelease( fabgl::VK_F1 ); // debounce

      soundGenerator.setVolume( nvolume );

      lastSpriteTime = millis(); // resync speed positions
    }


    for ( int player = 0; player < 2; player++)
    {
      if ( oldyspeed[player] != players[player].yspeed )
        players[player].showSpeed();

      if ( !players[player].crashed )
      {
        if ( oldx[player] < sprites[player].x ) sprites[player].setFrame(1);
        else if ( oldx[player] > sprites[player].x ) sprites[player].setFrame(2);
        else
        {
          int wheeldiv = (1 + (4 - players[player].yspeed / 50));
          if (wheeldiv < 1 ) wheeldiv = 1;
          sprites[player].setFrame(5 + 2 - ((updateCount / wheeldiv) % 3));
        }
      }

    }

    for ( int ncompta = 0; ncompta < 6; ncompta++)
      sprites[2 + ncompta].setFrame(2 - (updateCount / 3 + ncompta) % 3 + 1);


    for ( int ncompta = 0; ncompta < 8; ncompta++)
      updateSpriteAndDetectCollisions(&sprites[ncompta]);


    VGAController.refreshSprites();
    canvas.waitCompletion();

  }

  void collisionDetected(Sprite * spriteA, Sprite * spriteB, Point collisionPoint)
  {
    for ( int player = 0; player < 2; player++)
    {
      if ( spriteA == players[player].sprite)
      {
        players[player].Collision( spriteB->x);
        drawPoints();
        spriteB->visible = false;
        spriteB->y = 0;
      }
    }
  }

};
