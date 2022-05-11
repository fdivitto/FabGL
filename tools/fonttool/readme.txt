************************************************
*** ttf2header.py

Converts a TTF font to FabGL header to be included and used as FabGL font.

Usage:
  python3 ttf2header.py filename size [stroke]

Examples:
  python3 ttf2header.py Arial.ttf 24
  python3 ttf2header.py Arial.ttf 24 1



************************************************
*** dewinfont.py

Converts a FON or FNT font to FabGL header to be included and used as FabGL font.

Usage:
  python3 dewinfont.py -h filename

Example:
  python3 dewinfont.py -h myfont.fon



************************************************
********** Using the generated header **********
************************************************

Put the generated header (ie Arial.h) in your sketch directory. Then include the header adding:


#define FABGL_FONT_INCLUDE_DEFINITION
#include "/Users/fdivitto/GoogleDrive_fdivitto/maker/FabGL/tools/fonttool/Arial.h"


...finally use it:

Canvas cv(&DisplayController);
cv.setPenColor(RGB888(255, 255, 255));
cv.selectFont(&fabgl::FONT_Arial);
cv.drawText(10, 10, "Hello World!");


