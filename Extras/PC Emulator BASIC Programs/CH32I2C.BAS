
REM QBASIC program to control CH32V003 I/O expander inside PC Emulator

DECLARE FUNCTION ch32Available ()
DECLARE FUNCTION HexByte$ (i)
DECLARE FUNCTION INT$ (i, l)
DECLARE SUB i2cInit (clock)
DECLARE SUB i2cSlave (slave)
DECLARE FUNCTION i2cReadReg (reg)
DECLARE SUB i2cWriteReg (reg, value)

CLS

REM check for I/O available
IF ch32Available = 0 THEN
  PRINT "CH32V003 expander not available on this board!"
  END
END IF

PRINT "Configure I2C clock to 300 KHz"
i2cInit (300)

PRINT "Configure I2C slave Nunchuk  "
CALL i2cSlave(&H52)
CALL i2cWriteReg(&HF0, &H55)

PRINT "Press any key to stop"

LOCATE 10, 1
PRINT "Move the joystick..."
WHILE LEN(INKEY$) = 0
  LOCATE 11, 1
  PRINT "X = "; INT$(i2cReadReg(0), 3)
  LOCATE 11, 10
  PRINT "Y = "; INT$(i2cReadReg(1), 3)
WEND

REM CLS

LOCATE 2, 1
PRINT "Configure I2C slave RTC      "
CALL i2cSlave(&H68)

WHILE LEN(INKEY$) = 0
  LOCATE 10, 1
  CALL i2cWriteReg(&HE, &H3C)
  PRINT "Time          "; HexByte$(i2cReadReg(2)); ":"; HexByte$(i2cReadReg(1)); ":"; HexByte$(i2cReadReg(0));
  LOCATE 11, 1
  PRINT "Temperature   "; INT$(i2cReadReg(17), 0); "."; INT$(i2cReadReg(18) / 256 * 100, 0); CHR$(248); "C    "
WEND

FUNCTION HexByte$ (i)
  HexByte$ = RIGHT$("00" + HEX$(i), 2)
END FUNCTION

FUNCTION INT$ (i, l)
  R$ = LTRIM$(STR$(i))
  IF l <> 0 THEN
    R$ = RIGHT$(STRING$(l, " ") + R$, l)
  END IF
  INT$ = R$
END FUNCTION

FUNCTION ch32Available
  ch32Available = (INP(&HF0) AND 1)
END FUNCTION

SUB i2cInit (clock)
  REM LSB
  OUT &HF4, (clock MOD 256)
  REM MSB
  OUT &HF5, (clock \ 256)
END SUB

SUB i2cSlave (slave)
  OUT &HF6, slave
END SUB

FUNCTION i2cReadReg (reg)
  OUT &HF7, reg
  i2cReadReg = INP(&HF8)
END FUNCTION

SUB i2cWriteReg (reg, value)
  OUT &HF7, reg
  OUT &HF8, value
END SUB

