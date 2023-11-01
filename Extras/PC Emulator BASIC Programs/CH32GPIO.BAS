
REM QBASIC program to control CH32V003 I/O expander inside PC Emulator

DECLARE FUNCTION HexByte$ (I)
DECLARE FUNCTION INT$ (I, L)
DECLARE FUNCTION ch32Available ()
DECLARE FUNCTION ch32VersionMinor ()
DECLARE FUNCTION ch32VersionMajor ()
DECLARE SUB gpioSelect (gpio)
DECLARE SUB gpioConfig (cfg)
DECLARE SUB gpioSet (level)
DECLARE FUNCTION gpioGet ()

CLS

REM check for I/O available
IF ch32Available = 0 THEN
  PRINT "CH32V003 expander not available on this board!"
  END
END IF

PRINT "CH32V003 expander firmware version "; INT$(ch32VersionMajor, 0); "."; INT$(ch32VersionMinor, 0); " found"

PRINT "Configure GPIO 6 as input pull-down"
gpioSelect (6)
gpioConfig (1)

PRINT "Configure GPIO 7 as output"
gpioSelect (7)
gpioConfig (0)

PRINT "Press any key to stop"

LOCATE 10, 1
WHILE LEN(INKEY$) = 0
  gpioSet (1)
  t = TIMER
  WHILE TIMER - t < 1: WEND
 
  gpioSet (0)
  t = TIMER
  WHILE TIMER - t < 1: WEND
WEND

FUNCTION HexByte$ (I)
  HexByte$ = RIGHT$("00" + HEX$(I), 2)
END FUNCTION

FUNCTION INT$ (I, L)
  R$ = LTRIM$(STR$(I))
  IF L <> 0 THEN
    R$ = RIGHT$(STRING$(L, " ") + R$, L)
  END IF
  INT$ = R$
END FUNCTION

FUNCTION ch32Available
  ch32Available = (INP(&HF0) AND 1)
END FUNCTION

FUNCTION ch32VersionMinor
  ch32VersionMinor = INP(&HFE)
END FUNCTION

FUNCTION ch32VersionMajor
  ch32VersionMajor = INP(&HFF)
END FUNCTION

SUB gpioSelect (gpio)
  OUT &HF1, gpio
END SUB

SUB gpioConfig (cfg)
  OUT &HF2, cfg
END SUB

FUNCTION gpioGet
  gpioGet = INP(&HF3)
END FUNCTION

SUB gpioSet (level)
  OUT &HF3, level
END SUB
