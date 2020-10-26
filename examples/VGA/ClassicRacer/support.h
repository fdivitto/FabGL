#define waitForKey(V) while (!keyboard->isVKDown(V) ) delay(10);
#define waitForKeyRelease(V) while (keyboard->isVKDown(V) ) delay(10);

#define MIN(A,B) ((A<B)?(A):(B))
#define MAX(A,B) ((A>B)?(A):(B))
#define BETWEEN(A,V,B) MIN(MAX(V,A),B)
int maxInt( int a, int b ) { return MAX(a,b);}
int minInt( int a, int b ) { return MIN(a,b);}

void drawChar( int x, int y, char c)
{
  char b[2] = {0,0};
  b[1] = 0;
  b[0]=c;

  canvas.drawText( x, y, b );
}

void drawInt( int x, int y, int value, RGB888 const & color, char *prefix = NULL, char *sufix = NULL)
{
  canvas.selectFont(&fabgl::FONT_8x8);
     
  String towrite = "";
  if (prefix != NULL)
    towrite += prefix;

  towrite += value;
  
  if (sufix != NULL)
    towrite += sufix;

  canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
  canvas.setPenColor(color);
  canvas.drawText(x, y, towrite.c_str());
}

class BWScaleBitmap : public Bitmap
{
  public:
  void loadBWImageScale(int width_, int height_, int scale_, uint8_t const * data_, RGB888 foregroundColor_)  
  {
    Bitmap source = Bitmap(width_, height_, data_,  PixelFormat::Mask, RGB888(255, 255, 255));
    width = width_*scale_;    
    height = height_*scale_;
    format = PixelFormat::Mask;
    data = (uint8_t*) malloc((width + 7) * (height / 8)); // ?? form allocate code, is rotated?
    dataAllocated = true;
    foregroundColor = foregroundColor_;
    
    for( int y = 0; y < height; y++)
    for( int x = 0; x < width; x++)
      setPixel( x, y, source.getAlpha(x/scale_, y/scale_) );
  }
};



BWScaleBitmap numbers_bitmaps[10];  

void initNumbers( void )
{
    for ( int ncompta = 0; ncompta < 10; ncompta++)
      numbers_bitmaps[ncompta].loadBWImageScale( FONT_WIDTH, FONT_HEIGHT, FONT_SCALE, number_data_3x5[ncompta], RGB888(255, 255, 0));
}

void fillRect( int x, int y, int w, int h, fabgl::RGB888 const &overcolor)
{
  canvas.setBrushColor(  overcolor );
  
}

void checkered(int x, int y, int w, int h, int cw, int ch )
{
  
  for( int py = y; py < y+h; py+=ch)  
    for( int px = x; px < x+w; px+=cw)
    {      
      int dx= maxInt(px, x);
      int dy= maxInt(py, y);
      
      int dex = minInt(px+cw-1, x+w-1);      
      int dey = minInt(py+ch-1, y+h-1);

      if( dex > x && dey > y)
      {
        canvas.setBrushColor( (int)(((px/cw+py/ch) % 2) == 0) ? RGB888(255,255,255) : RGB888(0,0,0) );
        canvas.fillRectangle( dx, dy, dex, dey );
      }
    }
      
}

void waitButton(void)
{
  while(true)  
   for( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
      { 
        gameControllers[ncont]->update();
          if( gameControllers[ncont]->isButtonA() ) return;
      }
}

void waitNoButton(long timeout = 0)
{
  long start = millis();
  bool bExit = false;
  
  while(!bExit)   
  {    
    bExit = true;
    
    for( int ncont = 0; ncont < sizeof(gameControllers) / sizeof( GameController *); ncont++)
      { 
        gameControllers[ncont]->update();
        if( gameControllers[ncont]->isButtonA() ) bExit = false;
      }

    if( millis()-start > timeout ) bExit = true;
  }
}

void DEBUG( String str)
{
  Serial.println( str); delay(10);
}
