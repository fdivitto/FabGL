# vdp-gl

This is a fork of FabGL 1.0.8, the version used by agon-vdp. The goal is to customize FabGL for agon-vdp's needs.

=========


FabGL
### **ESP32** Display Controller (VGA, Color NTSC/PAL Composite, I2C and SPI displays), PS/2 Mouse and Keyboard Controller, Graphics Library, Sound Engine, Graphical User Interface (GUI), Game/Emulation Engine and ANSI/VT Terminal

**[Please look here for full API documentation](http://www.fabglib.org)**

**[See also my youtube channel where you can find demos and tutorials](https://www.youtube.com/user/fdivitto/videos)**


You can support development by purchasing my own [development board](https://www.tindie.com/products/24612/) and [Serial Terminal](https://www.tindie.com/products/21107/).
You may also support me donating hardware (boards, lab instruments, etc...). [**Credits**](https://github.com/fdivitto/FabGL/wiki/Credits)

...or just...[<img src="http://www.fabglib.org/images/kofi3.png?v=3">](https://ko-fi.com/W7W2C59SA)

=================================================================================

License terms:

Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>

Copyright (c) 2019-2022 Fabrizio Di Vittorio.

All rights reserved.

This library and related software is available under GPL v3.

Please contact fdivitto2013@gmail.com if you need a commercial license.

**Please don't remove copyright and/or original author from FabGL examples (ie from screens, dialogs, etc..), even from derived works which use examples as base.**

=================================================================================


FabGL is mainly a Graphics Library for ESP32. It implements several display drivers (VGA output, PAL/NTSC Color Composite, I2C and SPI displays).
FabGL can also get input from a PS/2 Keyboard and a Mouse. FabGL implements also: an Audio Engine (DAC and Sigma-Delta), a Graphical User Interface (GUI), a Game Engine and an ANSI/VT Terminal.

This library works with ESP32 revision 1 or upper. See [**Compatible Boards**][Boards].

VGA output requires a external digital to analog converter (DAC): it can be done by three 270 Ohm resistors to have 8 colors, or by 6 resistors to have 64 colors.
Composite output doesn't require external components (maybe a 5Mhz low pass filter).

There are several fixed and variable width fonts embedded.

Unlimited number of sprites are supported. However big sprites and a large amount of them reduces the frame rate and could generate flickering.

When there is enough memory (on low resolutions like 320x200), it is possible to allocate two screen buffers, so to implement double buffering.
In this case primitives are always drawn on the back buffer.

Except for double buffering or when explicitly disabled, all drawings are performed on vertical retracing, so no flickering is visible.
If the queue of primitives to draw is not processed before the vertical retracing ends, then it is interrupted and continued at next retracing.

There is a graphical user interface (GUI) with overlapping windows and mouse handling and a lot of widgets (buttons, editboxes, checkboxes, comboboxes, listboxes, etc..).

Finally, there is a sound engine, with multiple channels mixed to a mono output. Each channel can generate sine waveforms, square, etc... or custom sampled data.



### Installation Tutorial - fabgl development board (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/F2f0_9_TJmM/hqdefault.jpg)](https://www.youtube.com/watch?v=F2f0_9_TJmM "")

### Installation Tutorial - TTGO VGA32 (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/8OTaPQlSTas/hqdefault.jpg)](https://www.youtube.com/watch?v=8OTaPQlSTas "")

### IBM PC Emulator (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/3I1U2nEoxIQ/hqdefault.jpg)](https://www.youtube.com/watch?v=3I1U2nEoxIQ "")

### Space Invaders Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/LL8J7tjxeXA/hqdefault.jpg)](https://www.youtube.com/watch?v=LL8J7tjxeXA "")

### Dev board connected to TFT ILI9341 240x320, TFT ST7789 240x240 and OLED SSD1306 128x64 (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/OCsEqyJ7wu4/hqdefault.jpg)](https://www.youtube.com/watch?v=OCsEqyJ7wu4 "")

### Serial Terminal escape sequences for graphics and sound (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/1TjPOSc_RaI/hqdefault.jpg)](https://www.youtube.com/watch?v=1TjPOSc_RaI "")

### Serial Terminal for MBC2 Z80 Board (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/Ww_pH_ZOLqU/hqdefault.jpg)](https://www.youtube.com/watch?v=Ww_pH_ZOLqU "")

### Graphical User Interface - GUI (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/84ytGdiOih0/hqdefault.jpg)](https://www.youtube.com/watch?v=84ytGdiOih0 "")

### Sound Engine (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/RQtKFgU7OYI/hqdefault.jpg)](https://www.youtube.com/watch?v=RQtKFgU7OYI "")

### Altair 8800 emulator - CP/M text games (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/y0opVifEyS8/hqdefault.jpg)](https://www.youtube.com/watch?v=y0opVifEyS8 "")

### Commodore VIC20 emulator (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/ZW427HVWYys/hqdefault.jpg)](https://www.youtube.com/watch?v=ZW427HVWYys "")

### Simple Terminal Out Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/AmXN0SIRqqU/hqdefault.jpg)](https://www.youtube.com/watch?v=AmXN0SIRqqU "")

### Network Terminal Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/n5c27-y5tm4/hqdefault.jpg)](https://www.youtube.com/watch?v=n5c27-y5tm4 "")

### Modeline Studio Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/Urp0rPukjzE/hqdefault.jpg)](https://www.youtube.com/watch?v=Urp0rPukjzE "")

### Loopback Terminal Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/hQhU5hgWdcU/hqdefault.jpg)](https://www.youtube.com/watch?v=hQhU5hgWdcU "")

### Double Buffering Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/TRQcIiWQCJw/hqdefault.jpg)](https://www.youtube.com/watch?v=TRQcIiWQCJw "")

### Collision Detection Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/q3OPSq4HhDE/hqdefault.jpg)](https://www.youtube.com/watch?v=q3OPSq4HhDE "")

### Multitasking CP/M Plus Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/3UevsxMQZ5w/hqdefault.jpg)](https://www.youtube.com/watch?v=3UevsxMQZ5w "")




[Donations]: https://github.com/fdivitto/FabGL/wiki/Donations
[Boards]: https://github.com/fdivitto/FabGL/wiki/Boards
