#define MENU_TIMEOUT 30000L

#define LEFT_POS      64
#define RIGHT_POS     228
#define MIDDLE_POS    104

#define LEFT 0
#define RIGHT 1


int playercontrol[2] = {0,0};


struct Menu : public Scene 
{
  Menu()
    : Scene(0, 20, VGAController.getViewPortWidth(), VGAController.getViewPortHeight())
  {
  }
  
  long menulastactivity = 0;     
  int exitValue = -1;
  char const *textControls[6] = { ".......", " MOUSE ", "JOYSTICK", " ARROWS ", "A S T F ", "Q A O P" };
  bool lockedControls[6] = { false, false, false, false,false,false };  
    
  void init()  
  {
    menulastactivity = millis();
    exitValue = -1;
    
    canvas.selectFont(&fabgl::FONT_8x8);
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));    
   
    canvas.setBrushColor(COLOR_GREEN);
    canvas.clear();

    Bitmap bitmap_classicracer = Bitmap(21*8, 16, bitmap_classicracer_data, PixelFormat::Mask, RGB888(255, 255, 255));   
    canvas.drawBitmap( 10*8, 2*8, &bitmap_classicracer );    
    canvas.setPenColor(COLOR_YELLOW);
    canvas.drawText(7*8, 5*8, "VIDEO COMPUTER SYSTEM");     
    canvas.drawText(5*8, 7*8, "GAME PROGRAM");     
    canvas.drawText(4*8, 19*8, "MOUSE JOYSTICK ARROWS ASTF QAOP");          
    canvas.setPenColor(RGB888(64, 64, 0));     
    canvas.drawText(16, 23*8, "A NEW FANWARE BY CARLES ORIOL - 2020");     
  
    canvas.waitCompletion();
  }

  void setControl( int direction, int control) // direction LEFT or RIGHT
  {
    if( !lockedControls[control-1] ) // debouncer
    {    
      if( direction == RIGHT) 
        if( playercontrol[LEFT] == control) { playercontrol[LEFT] = 0; playSoundPic(); }
        else { playercontrol[RIGHT] = control; playSoundPong(); }
      else
        if( playercontrol[RIGHT] == control) { playercontrol[RIGHT] = 0; playSoundPic(); }
        else { playercontrol[LEFT] = control; playSoundPong(); } 
  
        menulastactivity = millis();
     
      lockedControls[control-1] = true;
    }
  }

  void update( int updateCount )
  {    
    int t= (updateCount/3) % 8; t = (t>4?8-t:t); // Bouncer
    
    canvas.setBrushColor(COLOR_GREEN);
    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    canvas.setPenColor(COLOR_WHITE);
    canvas.drawText(MIDDLE_POS-16, 10*8, " SELECT CONTROLS ");          
    canvas.setPenColor(COLOR_YELLOW);
    canvas.drawText(LEFT_POS-3*8, 12*8, "LEFT PLAYER");          
    canvas.drawText(RIGHT_POS-5*8, 12*8, "RIGHT PLAYER");          

    canvas.setPenColor(COLOR_RED);
    canvas.fillRectangle(0, 14*8, 299, 15*8-1);
    canvas.drawText(LEFT_POS-1*8, 14*8, textControls[playercontrol[LEFT]]);
    canvas.drawText(RIGHT_POS-3*8, 14*8, textControls[playercontrol[RIGHT]]);

    canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
    
    if( playercontrol[0] != 0 || playercontrol[1] != 0)
    {
      canvas.setPenColor(COLOR_RED);
      canvas.drawText(MIDDLE_POS-32+t, 17*8, " ACCELERATE TO START ");              
    }
    else
    {
      canvas.setPenColor(COLOR_WHITE);
      canvas.drawText(MIDDLE_POS-28+t, 17*8, " MOVE LEFT OR RIGHT  ");          
    }
        
    canvas.waitCompletion();
 
    bool bExitB = false;

    for( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
    {
      GameController *controller = gameControllers[ncont];
      if( controller->mode == MODE_DIRECTIONAL)
      {
        if( controller->isRight() ) setControl( RIGHT, controller->id);
        else if( controller->isLeft() )  setControl( LEFT,  controller->id);
        else lockedControls[controller->id-1] = false;
      }
      else if( controller->mode == MODE_RELATIVEPOS)
      {
        controller->update();    
        int dx = controller->getDX();
        if( dx >  20) setControl( RIGHT, controller->id);      
        else if( dx < -20) setControl( LEFT, controller->id);      
        else lockedControls[cMouse.id-1] = false;           // release debouncer      
      }

      if( controller->isButtonB() ) bExitB = true;
    }

    if( gameControllers[playercontrol[LEFT]]->isButtonA() || gameControllers[playercontrol[LEFT]]->isUp() || 
        gameControllers[playercontrol[RIGHT]]->isButtonA() || gameControllers[playercontrol[RIGHT]]->isUp() )
      exitValue = 1; 
   
    if ( millis() > menulastactivity + MENU_TIMEOUT || bExitB )
      exitValue = 2; 
      
    if( exitValue != -1 ) {this->stop(); return; }
  }

  void collisionDetected(Sprite *spriteA, Sprite *spriteB, Point collisionPoint  ) {}
};
