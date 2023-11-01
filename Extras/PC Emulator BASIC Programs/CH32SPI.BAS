
REM QBASIC program to control CH32V003 I/O expander inside PC Emulator

DECLARE FUNCTION HexByte$ (I)
DECLARE FUNCTION INT$ (I, L)
DECLARE FUNCTION ch32Available ()
DECLARE FUNCTION ch32VersionMinor ()
DECLARE FUNCTION ch32VersionMajor ()
DECLARE SUB spiClock (clock)
DECLARE SUB spiMode (mode)
DECLARE FUNCTION spiTransfer8 (I)
DECLARE FUNCTION spiTransfer16 (I)

CLS

REM check for I/O available
IF ch32Available = 0 THEN
  PRINT "CH32V003 expander not available on this board!"
  END
END IF

PRINT "CH32V003 expander firmware version "; INT$(ch32VersionMajor, 0); "."; INT$(ch32VersionMinor, 0); " found"

PRINT "Configure SPI mode 3"
spiMode (3)

PRINT "Configure SPI clock to 50 KHz"
spiClock (50)

PRINT "Press any key to stop"

LOCATE 10, 1
WHILE LEN(INKEY$) = 0
  LOCATE 10, 1
  IF spiTransfer16(&H8200) AND 1 THEN
    PRINT "X "; INT$(spiTransfer16(&H8300), 5)
  END IF

  LOCATE 10, 10
  IF spiTransfer16(&H8400) AND 1 THEN
    PRINT "Y "; INT$(spiTransfer16(&H8500), 5)
  END IF

  LOCATE 10, 20
  IF spiTransfer16(&H8600) AND 1 THEN
    PRINT "Z "; INT$(spiTransfer16(&H8700), 5)
  END IF

  PRINT "Temperature   "; INT$(spiTransfer16(&H8800) / 2 - 30, 0); CHR$(248); "C   "
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

SUB spiMode (mode)
  OUT &HF9, mode
END SUB

SUB spiClock (clock)
  OUT &HFA, 50
END SUB

FUNCTION spiTransfer8 (I)
   OUT &HFB, I
   T = TIMER
   WHILE TIMER - T < .1: WEND
   spiTransfer8 = INP(&HFB)
END FUNCTION

FUNCTION spiTransfer16 (I)
   OUT &HFC, (I MOD 256)
   OUT &HFD, (I \ 256)
   T = TIMER
   WHILE TIMER - T < .1: WEND
   spiTransfer16 = INP(&HFC) + INP(&HFD) * 256
END FUNCTION
