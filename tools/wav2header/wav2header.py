#!/usr/bin/env python


# example:
#    python wav2header.py test.wav 16000 >out.h
#
# Generates C header representing PCM signed 8 bit
#
#
# prerequisites:
#   pip install scipy
#
# tips:
#   * to generate a compatible wav from MacOS say command:
#        say -o test.wav --data-format=LEF32@16000 "Hello, this is FAB G L sampled speech demostration"

import sys
import math
import numpy as np
from scipy.io import wavfile
from scipy import interpolate


filename = sys.argv[1]
new_samplerate = int(sys.argv[2])

old_samplerate, old_audio = wavfile.read(filename)

duration = float(old_audio.shape[0]) / old_samplerate
time_old  = np.linspace(0, duration, old_audio.shape[0])
time_new  = np.linspace(0, duration, int(float(old_audio.shape[0]) * new_samplerate / old_samplerate))
interpolator = interpolate.interp1d(time_old, old_audio.T)
new_audio = interpolator(time_new).T

print "const int8_t waveform[] = {"
c = 0
for v in new_audio:
  if old_audio.dtype == np.uint8: # source is 0..255
    print "{:4},".format(int(v - 128)),
  elif old_audio.dtype == np.float32 or old_audio.dtype == np.float64:  # source is -1..+1
    print "{:4},".format(int(v * 127)),
  elif old_audio.dtype == np.int16:  # source is -32768..32767
    print "{:4},".format(int(v / 257)),
  elif old_audio.dtype == np.int32:  # source is -2147483648..2147483647
    print "{:4},".format(int(v / 16909320)),
  if (c + 1) % 16 == 0:
    print
  c += 1
print "\n};"



