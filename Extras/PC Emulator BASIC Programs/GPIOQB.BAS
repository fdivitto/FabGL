REM QBASIC program to control MCP23S17 I/O expander inside PC Emulator

DECLARE SUB checkGPIO (gpio!)
CLS

REM check for I/O available
IF INP(&HE0) AND 1 = 0 THEN
  PRINT "Extended I/O not available on this board!"
  END
END IF

REM Setup Port A and B as Outputs
OUT &HE1, &HFF
OUT &HE2, &HFF

PRINT "on/off Port A and B"
FOR i = 1 TO 4
  OUT &HE5, 255
  OUT &HE6, 255
  FOR d = 1 TO 200: NEXT
  OUT &HE5, 0
  OUT &HE6, 0
  FOR d = 1 TO 200: NEXT
NEXT

PRINT "on/off single GPIO A0 to B2"
FOR i = 1 TO 4
  FOR gpio = 0 TO 10
    OUT &HE7, gpio
    OUT &HE9, 0
    OUT &HE7, gpio + 1
    OUT &HE9, 1
    FOR d = 1 TO 50: NEXT
  NEXT
  FOR gpio = 10 TO 0 STEP -1
    OUT &HE7, gpio
    OUT &HE9, 0
    OUT &HE7, gpio - 1
    OUT &HE9, 1
    FOR d = 1 TO 50: NEXT
  NEXT
NEXT

PRINT "Check for B3 to B7 as Inputs..."
PRINT "Press any key to stop"
OUT &HE2, &H7: REM setup GPIO B3 to B7 as inputs (bits 3..7 = 0)
WHILE LEN(INKEY$) = 0
  FOR gpio = 11 TO 15
    checkGPIO (gpio)
  NEXT
WEND

SUB checkGPIO (gpio)
  OUT &HE7, gpio
  IF INP(&HE9) = 1 THEN
    PRINT "pressed "; gpio; "...";
    WHILE INP(&HE9): WEND
    PRINT "released"
  END IF
END SUB

