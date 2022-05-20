************************************************
*** ttf2header.py

Converts a TTF font to FabGL header to be included and used as FabGL font.

Usage:
  python3 ttf2header.py filename size [-s stroke] [-o output_filename] [-r firstindex lastindex]

Examples:

 - create file Arial24.h, size 24, indexes 0 to 255
     python3 ttf2header.py Arial.ttf 24

 - create file Arial44.h, size 44, indexes 0 to 255, not filled with line width 1
     python3 ttf2header.py Arial.ttf 44 -s 1

 - create file myfont.h, size 44, indexes 32 to 127
     python3 ttf2header.py Arial.ttf 44 -o "myfont.h" -r 32 127

 - create file Arial44.h, size 44, indexes 32 to 127
     python3 ttf2header.py Arial.ttf 44 -r 32 127



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


