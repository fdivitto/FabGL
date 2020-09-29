#!/usr/bin/env python

# Transfer disk images between remote machine and host

import sys
import serial
from time import sleep


if len(sys.argv) < 4:
  print "Transfer disk images between remote machine and host at 115200 bauds"
  print ""
  print "Usage:"
  print "  python transdisk filename (from/to) serialport"
  print ""
  print "Example, put mydisk.dsk to emulated machine:"
  print "  python transdisk.py mydisk.dsk to /dev/cu.SLAB_USBtoUART"
  print "Example, get MBASIC.COM from emulated machine:"
  print "  python transdisk.py mydisk.dsk from /dev/cu.SLAB_USBtoUART"

  sys.exit()

filename   = sys.argv[1]
cmd        = sys.argv[2].lower()     # from/to
serialPort = sys.argv[3]

ser = serial.Serial(serialPort, baudrate=115200, timeout=None)
ser.rts = False

SECTOR_SIZE = 137
TRACK_SIZE = 32
TRACKS_COUNT = 77

if cmd == 'from':
  # copy from remote machine
  print "Please activate disk image send from remote machine..."
  stream = open(filename, 'wb')
  for i in range(0, SECTOR_SIZE * TRACK_SIZE * TRACKS_COUNT):
    stream.write(ser.read())
    if (i % 10) == 0:
      sys.stdout.write("Received " + str(i) + " bytes\r")
      sys.stdout.flush()

if cmd == 'to':
  # copy to remote machine
  stream = open(filename, 'rb')
  for i in range(0, SECTOR_SIZE * TRACK_SIZE * TRACKS_COUNT):
    ser.write(stream.read(1))
    if ((i+1) % SECTOR_SIZE) == 0:
      sys.stdout.write("Sent " + str(i) + " bytes\r")
      sys.stdout.flush()
      # wait ACK
      r = ord(ser.read()[0])
      if r != 0x06:
        print "Error, expected ACK!"
        sys.exit()

print

