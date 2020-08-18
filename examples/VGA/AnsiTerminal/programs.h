 /*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once



#define NL "\r\n"




static const char BLINK_BAS[] =       "1 PRINT \"Sets GPIO-12 as output pin and turns it On and Off\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL      // required to avoid PRINT to add New Line after reaching column 80!
                                      "110 REM Sets (_D) digital output (O) on GPIO-12" NL
                                      "120 PRINT CHR$(27);\"_DO12$\";" NL
                                      "130 WHILE 1" NL
                                      "140     REM Write (_W) logic High (H) to GPIO-12" NL
                                      "150     PRINT CHR$(27);\"_WH12$\";" NL
                                      "160     GOSUB 1000: REM DELAY" NL
                                      "165     REM" NL
                                      "170     REM Write (_W) logic Low (L) to GPIO-12" NL
                                      "180     PRINT CHR$(27);\"_WL12$\";" NL
                                      "190     GOSUB 1000: REM DELAY" NL
                                      "200 WEND" NL
                                      "1000 REM" NL
                                      "1010 REM DELAY SUBROUTINE" NL
                                      "1020 FOR I=0 TO 100: NEXT I" NL
                                      "1030 RETURN" NL
                                      "\x1a";  // <<-- text file terminator


static const char GPIOREAD_BAS[] =    "1 PRINT \"Sets GPIO-36 as digital input and continuously prints its value\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL
                                      "110 REM Sets (_D) digital input (I) on GPIO-36" NL
                                      "120 PRINT CHR$(27);\"_DI36$\";" NL
                                      "130 WHILE 1" NL
                                      "140     REM Read digital value (_R) from GPIO-36" NL
                                      "150     PRINT CHR$(27);\"_R36$\";" NL
                                      "160     RECV$ = INPUT$(2)" NL
                                      "170     V$ = RIGHT$(RECV$, 1)" NL
                                      "180     PRINT \"GPIO-0 = \"; V$; CHR$(13);" NL
                                      "190 WEND" NL
                                      "\x1a";  // <<-- text file terminator


static const char ADC_BAS[] =         "1 PRINT \"Sets GPIO-36 as analog input and draws its value\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL
                                      "110 REM Sets (_A) analog input, 12 bit resolution (12)," NL
                                      "120 REM with 11dB attenuation (3) on GPIO-36" NL
                                      "120 PRINT CHR$(27);\"_A12;3;36$\";" NL
                                      "130 WHILE 1" NL
                                      "140     REM Read (_G) analog value from GPIO-36" NL
                                      "150     PRINT CHR$(27);\"_C36$\";" NL
                                      "160     RECV$ = INPUT$(4)" NL
                                      "170     V = VAL(\"&H\" + RIGHT$(RECV$, 3))" NL
                                      "180     PRINT V TAB(8 + V / 53) \"*\"" NL
                                      "190 WEND" NL
                                      "\x1a";  // <<-- text file terminator


static const char ADCVOLTS_BAS[] =    "1 PRINT \"Sets GPIO-36 as analog input and prints its value in Volts\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL
                                      "110 REM Sets (_A) analog input, 12 bit resolution (12)," NL
                                      "120 REM with 11dB attenuation (3) on GPIO-36" NL
                                      "120 PRINT CHR$(27);\"_A12;3;36$\";" NL
                                      "130 WHILE 1" NL
                                      "140     REM Read (_G) analog value from GPIO-36" NL
                                      "150     PRINT CHR$(27);\"_C36$\";" NL
                                      "160     RECV$ = INPUT$(4)" NL
                                      "170     V = VAL(\"&H\" + RIGHT$(RECV$, 3))" NL
                                      "180     PRINT V / 4096 * 3.9; \"V          \"; CHR$(13);" NL
                                      "190 WEND" NL
                                      "\x1a";  // <<-- text file terminator

static const char SOUND_BAS[] =       "1 PRINT \"Generates some sounds\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL
                                      "110 FOR FREQ = 100 TO 2000 STEP 50" NL
                                      "120   GOSUB 1000" NL
                                      "130 NEXT FREQ" NL
                                      "140 FOR FREQ = 2000 TO 100 STEP -50" NL
                                      "150   GOSUB 1000" NL
                                      "160 NEXT FREQ" NL
                                      "170 WIDTH 80" NL
                                      "180 END" NL
                                      "1000 REM Generate a sound (_S), sine waveform (0)," NL
                                      "1010 REM frequency FREQ, duration 80ms, volume 100" NL
                                      "1020 PRINT CHR$(27)+\"_S0;\"+STR$(FREQ)+\";80;100$\";" NL
                                      "1030 REM delay 100ms" NL
                                      "1040 PRINT CHR$(27)+\"_Y100$\";" NL
                                      "1050 WHILE INKEY$ <> \"$\": WEND" NL
                                      "1060 RETURN" NL
                                      "\x1a";  // <<-- text file terminator


static const char MOUSE_BAS[] =       "1 PRINT \"Shows mouse positions and status\"" NL
                                      "2 REM" NL
                                      "100 WIDTH 255" NL
                                      "110 REM Enable mouse" NL
                                      "120 PRINT CHR$(27);\"_H1$\";" NL
                                      "130 WHILE 1" NL
                                      "140     REM Read mouse status" NL
                                      "150     PRINT CHR$(27);\"_M$\";" NL
                                      "160     S$ = INPUT$(12)" NL
                                      "170     PRINT S$;CHR$(13);" NL
                                      "180 WEND" NL
                                      "\x1a";  // <<-- text file terminator


static const char GRAPH_BAS[] =       "1 REM Draws some graphics" NL
                                      "100 WIDTH 255" NL
                                      "105 GOSUB 10000: REM Define functions" NL
                                      "106 REM Disable cursor and clear screen" NL
                                      "107 PRINT FNCURSOROFF$;FNCLR$;" NL
                                      "110 REM" NL
                                      "120 REM ** POINTS **" NL
                                      "130 FOR I = 0 TO 100" NL
                                      "140     REM Set random pen color" NL
                                      "150     PRINT FNPEN$(RND*255, RND*255, RND*255);" NL
                                      "160     REM Draw a pixel at random position" NL
                                      "170     PRINT FNPIXEL$(RND*640, RND*480);" NL
                                      "180 NEXT I" NL
                                      "190 FOR I = 0 TO 120: PRINT FNSCROLL$(4, 4);: NEXT I" NL
                                      "200 REM" NL
                                      "210 REM ** LINES **" NL
                                      "220 FOR I = 0 TO 100" NL
                                      "230     REM Set random pen color" NL
                                      "240     PRINT FNPEN$(RND*255, RND*255, RND*255);" NL
                                      "250     REM Draw a line" NL
                                      "260     PRINT FNDRAW$(RND*640, RND*480, RND*640, RND*480);" NL
                                      "270 NEXT I" NL
                                      "280 FOR I = 0 TO 120: PRINT FNSCROLL$(-4, 4);: NEXT I" NL
                                      "290 REM" NL
                                      "300 REM ** FILLED RECTANGLES **" NL
                                      "310 FOR I = 0 TO 50" NL
                                      "320     REM Set random brush color" NL
                                      "330     PRINT FNBRUSH$(RND*255, RND*255, RND*255);" NL
                                      "340     REM Set random pen color" NL
                                      "350     PRINT FNPEN$(RND*255, RND*255, RND*255);" NL
                                      "360     REM Fill and draw rectangle" NL
                                      "370     X1 = RND*640: Y1 = RND*480: X2 = RND*640: Y2 = RND*480" NL
                                      "380     PRINT FNFILLRECT$(X1, Y1, X2, Y2);" NL
                                      "390     PRINT FNRECT$(X1, Y1, X2, Y2);" NL
                                      "400 NEXT I" NL
                                      "410 FOR I = 0 TO 120: PRINT FNSCROLL$(4, -4);: NEXT I" NL
                                      "420 REM" NL
                                      "430 REM ** FILLED ELLIPSES **" NL
                                      "440 FOR I = 0 TO 50" NL
                                      "450     REM Set random brush color" NL
                                      "460     PRINT FNBRUSH$(RND*255, RND*255, RND*255);" NL
                                      "470     REM Set random pen color" NL
                                      "480     PRINT FNPEN$(RND*255, RND*255, RND*255);" NL
                                      "490     REM Fill and draw ellipse" NL
                                      "500     X = RND*640: Y = RND*480: W = RND*320: H = RND*240" NL
                                      "510     PRINT FNFILLELLIPSE$(X, Y, W, H);" NL
                                      "520     PRINT FNELLIPSE$(X, Y, W, H);" NL
                                      "530 NEXT I" NL
                                      "540 FOR I = 0 TO 120: PRINT FNSCROLL$(-4, -4);: NEXT I" NL
                                      "550 REM" NL
                                      "560 REM ** FILLED POLYGONS **" NL
                                      "570 FOR I = 0 TO 50" NL
                                      "580     REM Set random brush color" NL
                                      "590     PRINT FNBRUSH$(RND*255, RND*255, RND*255);" NL
                                      "600     REM Set random pen color" NL
                                      "610     PRINT FNPEN$(RND*255, RND*255, RND*255);" NL
                                      "620     REM Build coordinates" NL
                                      "630     C = 3 + INT(RND*4): REM Number of points" NL
                                      "640     P$ = \"\"" NL
                                      "650     FOR J = 1 TO C" NL
                                      "660         X = INT(RND*640): Y = INT(RND*480)" NL
                                      "670         P$ = P$ + STR$(X) + \";\" + STR$(Y)" NL
                                      "680         IF J < C THEN P$ = P$ + \";\"" NL
                                      "690     NEXT J" NL
                                      "700     REM Fill and draw path" NL
                                      "710     PRINT FNFILLPATH$(P$);" NL
                                      "720     PRINT FNPATH$(P$);" NL
                                      "730 NEXT I" NL
                                      "740 FOR I = 0 TO 120: PRINT FNSCROLL$(-4, 0);: NEXT I" NL
                                      "750 REM Clear screen, clear terminal and enable cursor" NL
                                      "760 PRINT FNCLR$;FNCLRTERM$;FNCURSORON$;" NL
                                      "765 WIDTH 80" NL
                                      "770 END" NL
                                      "10000 REM" NL
                                      "10010 REM ** DEFINE FUNCTIONS **" NL
                                      "10020 DEF FNPEN$(R%, G%, B%) = CHR$(27)+\"_GPEN\"+STR$(R%)+\";\"+STR$(G%)+\";\"+STR$(B%)+\"$\"" NL
                                      "10030 DEF FNBRUSH$(R%, G%, B%) = CHR$(27)+\"_GBRUSH\"+STR$(R%)+\";\"+STR$(G%)+\";\"+STR$(B%)+\"$\"" NL
                                      "10040 DEF FNPIXEL$(X%, Y%) = CHR$(27)+\"_GPIXEL\"+STR$(X%)+\";\"+STR$(Y%)+\"$\"" NL
                                      "10050 DEF FNDRAW$(X1%, Y1%, X2%, Y2%) = CHR$(27)+\"_GLINE\"+STR$(X1%)+\";\"+STR$(Y1%)+\";\"+STR$(X2%)+\";\"+STR$(Y2%)+\"$\"" NL
                                      "10060 DEF FNRECT$(X1%, Y1%, X2%, Y2%) = CHR$(27)+\"_GRECT\"+STR$(X1%)+\";\"+STR$(Y1%)+\";\"+STR$(X2%)+\";\"+STR$(Y2%)+\"$\"" NL
                                      "10070 DEF FNFILLRECT$(X1%, Y1%, X2%, Y2%) = CHR$(27)+\"_GFILLRECT\"+STR$(X1%)+\";\"+STR$(Y1%)+\";\"+STR$(X2%)+\";\"+STR$(Y2%)+\"$\"" NL
                                      "10080 DEF FNELLIPSE$(X%, Y%, W%, H%) = CHR$(27)+\"_GELLIPSE\"+STR$(X%)+\";\"+STR$(Y%)+\";\"+STR$(W%)+\";\"+STR$(H%)+\"$\"" NL
                                      "10090 DEF FNFILLELLIPSE$(X%, Y%, W%, H%) = CHR$(27)+\"_GFILLELLIPSE\"+STR$(X%)+\";\"+STR$(Y%)+\";\"+STR$(W%)+\";\"+STR$(H%)+\"$\"" NL
                                      "10110 DEF FNPATH$(POINTS$) = CHR$(27)+\"_GPATH\"+POINTS$+\"$\"" NL
                                      "10120 DEF FNFILLPATH$(POINTS$) = CHR$(27)+\"_GFILLPATH\"+POINTS$+\"$\"" NL
                                      "10130 DEF FNSCROLL$(X%, Y%) = CHR$(27)+\"_GSCROLL\"+STR$(X%)+\";\"+STR$(Y%)+\"$\"" NL
                                      "10140 DEF FNCLR$ = CHR$(27)+\"_GCLEAR$\"" NL
                                      "10150 DEF FNCURSORON$ = CHR$(27)+\"_E1$\"" NL
                                      "10160 DEF FNCURSOROFF$ = CHR$(27)+\"_E0$\"" NL
                                      "10170 DEF FNCLRTERM$ = CHR$(27)+\"_B$\"" NL
                                      "10180 RETURN" NL
                                      "\x1a";  // <<-- text file terminator


static const char * PROGRAMS_NAME[] =  { "BLINK.BAS",
                                         "GPIOREAD.BAS",
                                         "ADC.BAS",
                                         "ADCVOLTS.BAS",
                                         "SOUND.BAS",
                                         "MOUSE.BAS",
                                         "GRAPH.BAS",
                                      };

static const char * PROGRAMS[]    =    { BLINK_BAS,
                                         GPIOREAD_BAS,
                                         ADC_BAS,
                                         ADCVOLTS_BAS,
                                         SOUND_BAS,
                                         MOUSE_BAS,
                                         GRAPH_BAS,
                                       };


//                                      first row, second row
static const char * PROGRAMS_HELP[] = { "Sets GPIO-12 as Output pin and ", "turns it On (hi) and Off (low).",
                                        "Sets GPIO-36 as Input and ", "continuously prints its value.",
                                        "Sets GPIO-36 as analog input and ", "continously prints its value.",
                                        "Sets GPIO-36 as analog input and ", "continously prints its value in Volta.",
                                        "Generates some sounds", "",
                                        "Shows mouse positions and status", "",
                                        "Draws some graphics", "",
                                      };


constexpr int       PROGRAMS_COUNT = sizeof(PROGRAMS_NAME) / sizeof(char const *);


void sendString(char const * str)
{
  while (*str) {
    Terminal.send(*str++);
    delay(1);
  }
}


void installProgram(int progIndex)
{
  Terminal.printf("Saving %s...", PROGRAMS_NAME[progIndex]);
  Terminal.disableSerialPortRX(true);
  sendString("PIP ");
  sendString(PROGRAMS_NAME[progIndex]);
  sendString("=CON:\r\n");
  delay(3000);
  sendString(PROGRAMS[progIndex]);
  Terminal.disableSerialPortRX(false);
}
