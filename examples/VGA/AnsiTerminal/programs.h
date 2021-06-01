/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

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


static const char SPRITE_BAS[] =      "1 REM Sprites demo" NL
                                      "100 WIDTH 255" NL
                                      "110 GOSUB 10000: REM Define functions" NL
                                      "120 REM Cursor off" NL
                                      "130 PRINT FNCURSOROFF$;" NL
                                      "140 REM Number of sprites" NL
                                      "150 PRINT FNSPRITECOUNT$(2);" NL
                                      "160 REM Sprite 0" NL
                                      "170 PRINT FNSPRITEDEFRGB2$(0, 16, 14);" NL
                                      "180 GOSUB 20010: REM Sprite 0 data" NL
                                      "190 REM Sprite 1" NL
                                      "200 PRINT FNSPRITEDEFMONO$(1, 64, 64, 0, 255, 255);" NL
                                      "210 GOSUB 20200: REM Sprite 1 data" NL
                                      "220 REM move sprites" NL
                                      "230 X0 = 0: Y0 = 320" NL
                                      "240 O0 = 1" NL
                                      "260 FOR I = 0 TO 900" NL
                                      "270     REM Move sprites" NL
                                      "280     PRINT FNSPRITESET$(0, \"V\", 0, X0, Y0);" NL
                                      "290     PRINT FNSPRITESET$(1, \"V\", 0, 320 + SIN(I / 25) * 100, 200 + COS(I / 25) * 100);" NL
                                      "300     X0 = X0 + O0" NL
                                      "310     IF X0 > 400 THEN O0 = -1" NL
                                      "320     IF X0 < 200 THEN O0 = 1" NL
                                      "350     REM Draw starts and scroll down" NL
                                      "360     PRINT FNPEN$(255,255,255);" NL
                                      "370     IF (I MOD 2) = 0 THEN PRINT FNPIXEL$(640*RND, 0);FNSCROLL$(0, 1);" NL
                                      "410 NEXT I" NL
                                      "420 PRINT FNSPRITESET$(0, \"H\", 0, 0, 0);" NL
                                      "430 PRINT FNSPRITESET$(1, \"H\", 0, 0, 0);" NL
                                      "440 PRINT FNSPRITECOUNT$(0);" NL
                                      "450 PRINT FNCURSORON$;" NL
                                      "460 END" NL
                                      "10000 REM" NL
                                      "10010 REM ** DEFINE FUNCTIONS **" NL
                                      "10020 DEF FNSPRITECOUNT$(COUNT%) = CHR$(27)+\"_GSPRITECOUNT\"+STR$(COUNT%)+\"$\"" NL
                                      "10030 DEF FNSPRITEDEFMONO$(INDEX%, W%, H%, R%, G%, B%) = CHR$(27)+\"_GSPRITEDEF\"+STR$(INDEX%)+\";\"+STR$(W%)+\";\"+STR$(H%)+\";M;\"+STR$(R%)+\";\"+STR$(G%)+\";\"+STR$(B%)+\";\"" NL
                                      "10040 DEF FNSPRITEDEFRGB2$(INDEX%, W%, H%) = CHR$(27)+\"_GSPRITEDEF\"+STR$(INDEX%)+\";\"+STR$(W%)+\";\"+STR$(H%)+\";2;\"" NL
                                      "10050 DEF FNSPRITEDEFRGB8$(INDEX%, W%, H%) = CHR$(27)+\"_GSPRITEDEF\"+STR$(INDEX%)+\";\"+STR$(W%)+\";\"+STR$(H%)+\";8;\"" NL
                                      "10060 DEF FNSPRITESET$(INDEX%, VISIBLE$, FRAME%, X%, Y%) = CHR$(27)+\"_GSPRITESET\"+STR$(INDEX%)+\";\"+VISIBLE$+\";\"+STR$(FRAME%)+\";\"+STR$(X%)+\";\"+STR$(Y%)+\"$\"" NL
                                      "10070 DEF FNCURSORON$ = CHR$(27)+\"_E1$\"" NL
                                      "10080 DEF FNCURSOROFF$ = CHR$(27)+\"_E0$\"" NL
                                      "10090 DEF FNPEN$(R%, G%, B%) = CHR$(27)+\"_GPEN\"+STR$(R%)+\";\"+STR$(G%)+\";\"+STR$(B%)+\"$\"" NL
                                      "10100 DEF FNPIXEL$(X%, Y%) = CHR$(27)+\"_GPIXEL\"+STR$(X%)+\";\"+STR$(Y%)+\"$\"" NL
                                      "10110 DEF FNSCROLL$(X%, Y%) = CHR$(27)+\"_GSCROLL\"+STR$(X%)+\";\"+STR$(Y%)+\"$\"" NL
                                      "11000 RETURN" NL
                                      "20000 REM" NL
                                      "20010 REM ** Define 16x14, RGB222 sprite **" NL
                                      "20020 PRINT \"00000000000000eaea00000000000000\";" NL
                                      "20030 PRINT \"000000000000eaeaeaea000000000000\";" NL
                                      "20040 PRINT \"000000000000eaeaeaea000000000000\";" NL
                                      "20050 PRINT \"00e000000000eaeaeaea00000000e000\";" NL
                                      "20060 PRINT \"00e000000000eaeaeaea00000000e000\";" NL
                                      "20070 PRINT \"e0e000000000eaeaeaea00000000e0e0\";" NL
                                      "20080 PRINT \"e0e00000e000eaeaeaea00e00000e0e0\";" NL
                                      "20090 PRINT \"e0e00000e0eaeac2c2eaeae00000e0e0\";" NL
                                      "20100 PRINT \"e0e000e0e0eac2eaeac2eae0e000e0e0\";" NL
                                      "20110 PRINT \"e0e000eaeaeaeaeaeaeaeaeaea00e0e0\";" NL
                                      "20120 PRINT \"e0e0eaeaeaeac20000c2eaeaeaeae0e0\";" NL
                                      "20130 PRINT \"e0e0eaeaeac2c20000c2c2eaeaeae0e0\";" NL
                                      "20140 PRINT \"e0e0ea00c2c2c20000c2c2c200eae0e0\";" NL
                                      "20150 PRINT \"e00000000000c20000c20000000000e0$\";" NL
                                      "20160 RETURN" NL
                                      "20200 REM ** Define 64x64, monochrome sprite **" NL
                                      "20210 PRINT \"0000001f00000000000001fff0000000000007fffc00000000000ffffe000000\";" NL
                                      "20220 PRINT \"00003f001f80000000007c0007c000000000f80003e007c00001e00000f0fff8\";" NL
                                      "20230 PRINT \"0003c000007bfffc00038000003ffefe00070000001f800e00070000001e0007\";" NL
                                      "20240 PRINT \"000e0000000e0007000e0000000e0007001c0000000700030018000000030007\";" NL
                                      "20250 PRINT \"003800000003800700380000000380070030000000038006007000000003000e\";" NL
                                      "20260 PRINT \"007000000007000e006000000007001c00600000000e001c00e00000000e0038\";" NL
                                      "20270 PRINT \"00e00000001c003800e00000003c007000c00000003800f000c00000007000e0\";" NL
                                      "20280 PRINT \"00c0000000e001c000c0000001e003c000c0000003c0078000c0000007800700\";" NL
                                      "20290 PRINT \"00c000000f000f0000e000001e001e0000e000003c003c0000e0000078007800\";" NL
                                      "20300 PRINT \"00e00001f000f80000e00003e001f00000e0000fc003f00001f0001f0007f000\";" NL
                                      "20310 PRINT \"03f0007e000fb00003b001f8001e7000073807f0003c70000e1f7fc000786000\";" NL
                                      "20320 PRINT \"0e1fff0000f0e0001c0ff80001e0e0003c03c00007c0c000380000000f81c000\";" NL
                                      "20330 PRINT \"300000001f01c000700000007c03800070000000f8078000e0000003f0070000\";" NL
                                      "20340 PRINT \"e0000007c00e0000e000000f801e0000c000003f003c0000c00000fc00780000\";" NL
                                      "20350 PRINT \"c00003f800f00000c0000fe003e00000e0003fc00fc00000f001ffffff800000\";" NL
                                      "20360 PRINT \"7c3ff9fffe0000003fffe07ff80000001fff00078000000007f0000000000000$\";" NL
                                      "20370 RETURN" NL
                                      "\x1a";  // <<-- text file terminator


static const char * PROGRAMS_NAME[] =  { "BLINK.BAS",
                                         "GPIOREAD.BAS",
                                         "ADC.BAS",
                                         "ADCVOLTS.BAS",
                                         "SOUND.BAS",
                                         "MOUSE.BAS",
                                         "GRAPH.BAS",
                                         "SPRITE.BAS",
                                      };

static const char * PROGRAMS[]    =    { BLINK_BAS,
                                         GPIOREAD_BAS,
                                         ADC_BAS,
                                         ADCVOLTS_BAS,
                                         SOUND_BAS,
                                         MOUSE_BAS,
                                         GRAPH_BAS,
                                         SPRITE_BAS,
                                       };


//                                      first row, second row
static const char * PROGRAMS_HELP[] = { "Sets GPIO-12 as Output pin and ", "turns it On (hi) and Off (low).",
                                        "Sets GPIO-36 as Input and ", "continuously prints its value.",
                                        "Sets GPIO-36 as analog input and ", "continously prints its value.",
                                        "Sets GPIO-36 as analog input and ", "continously prints its value in Volts.",
                                        "Generates some sounds", "",
                                        "Shows mouse positions and status", "",
                                        "Draws some graphics", "",
                                        "Shows how create and move sprites", "",
                                      };


constexpr int       PROGRAMS_COUNT = sizeof(PROGRAMS_NAME) / sizeof(char const *);


void sendString(char const * str)
{
  while (*str) {
    Terminal.send(*str++);
    delay(5);
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
