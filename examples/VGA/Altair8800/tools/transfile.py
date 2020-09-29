#!/usr/bin/env python

# Transfer files between remote machine and host, using XMODEM protocol
#
# Requires XMODEM library:
#   sudo pip install xmodem
# Requires serial library:
#   sudo pip install pyserial

import sys
import serial
from xmodem import XMODEM


def getc(size, timeout=1):
  return ser.read(size) or None

def putc(data, timeout=1):
  return ser.write(data)


if len(sys.argv) < 4:
  print "Transfer files between remote machine and host - uses XMODEM protocol"
  print "at 115200 baud rate."
  print ""
  print "Usage:"
  print "  python transfile filename (from/to) serialport"
  print ""
  print "Example, put MBASIC.COM to emulated machine:"
  print "  python transfile.py MBASIC.COM to /dev/cu.SLAB_USBtoUART"
  print "Example, get MBASIC.COM from emulated machine:"
  print "  python transfile.py MBASIC.COM from /dev/cu.SLAB_USBtoUART"

  sys.exit()

filename   = sys.argv[1]
cmd        = sys.argv[2].lower()     # from/to
serialPort = sys.argv[3]

ser = serial.Serial(serialPort, baudrate=115200, timeout=None, rtscts=False, dsrdtr=False)
ser.rts = False
modem = XMODEM(getc, putc)

if cmd == 'from':
  # copy from remote machine
  stream = open(filename, 'wb')
  modem.recv(stream)

if cmd == 'to':
  # copy to remote machine
  stream = open(filename, 'rb')
  modem.send(stream)

