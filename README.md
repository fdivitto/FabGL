# FabGL
### **ESP32** Display Controller (VGA, Color NTSC/PAL Composite, I2C and SPI displays), PS/2 Mouse and Keyboard Controller, Graphics Library, Sound Engine, Graphical User Interface (GUI), Game/Emulation Engine and ANSI/VT Terminal

**[Please look here for full API documentation](http://www.fabglib.org)**

**[See also my youtube channel where you can find demos and tutorials](https://www.youtube.com/user/fdivitto/videos)**


You can support development by purchasing my own [development board](https://www.tindie.com/products/24612/) and [Serial Terminal](https://www.tindie.com/products/26801/).
You may also support me donating hardware (boards, lab instruments, etc...).

======================================================

License terms:

Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>

Copyright (c) 2019-2022 Fabrizio Di Vittorio.

All rights reserved.

This library and related software is available under GPL v3.

Please contact fdivitto2013@gmail.com if you need a commercial license.

**Please don't remove copyright and/or original author from FabGL examples (ie from screens, dialogs, etc..), even from derived works which use examples as base.**

========================================

## How to install ##

### Set up the Arduino environment ###

1. Install and start Arduino IDE, get it from: https://www.arduino.cc/en/software

2. Add the esp32 board support by Espressif using the Board Manager (Tools > Board > Board Manager and search for "esp32"). Use the package called “esp32” by Esoressif Systems and use the version selector to load a version 2.x.x! IMPORTANT! At the time of writing this, the FabGL library is NOT compatible with version 3.x.x or newer of the package. From the drop-down menu select version 2.0.x, we used 2.0.11.

3. The Olimex fork of FabGL adds support for the ESP32-SBC-FabGL board and needs to be installed as a local library. You'll also need to first uninstall Fabrizio's FabGL library if it's already installed.
    - Go to the Olimex FabGLrepo at GitHub: https://github.com/OLIMEX/FabGL
    - Click "Download ZIP" from the green "Code" drop-down button, top-right. Save the ZIP file as "Olimex-FabGL.zip" so you don't get confused with Fabrizio's library and repo.
    - Unzip and copy the contents to a new folder in the "Documents\Arduino\libraries" folder (e.g. "C:\Users\username\Documents\Arduino\libraries\Olimex-FabGL").

4. Close Arduino IDE. The next time the IDE is started, the local Olimex FabGL library will be available for use.

### Compile and download examples to ESP32-SBC-FabGL ###

1. Connect the ESP32-SBC-FabGL board to your desktop PC using a USB cable. The USB-C port on the board serves as both power and data.

2. Start Arduino IDE.

3. Verify the Olimex FabGL library is loaded by:

    - Opening the Library Manager (Tools > Manage Libraries).
    - Typing "FabGL" in to the "filter" textbox.
    - Changing the "Type" to "Installed".
    - It should list "FabGL 1.0.9" (at the time of this writing) as one of the installed libraries.

5. Select a FabGL demo from (File > Examples > FabGL). The FabGL examples will be at the bottom of a lengthy list.

6. Configure the board.
    - Select the "ESP32 Dev Module" board (Tools > Board > esp32 > ESP32 Dev Module).
    - Set the board port to upload to (Tools > Port > COM#).
    - Set the partition scheme (Tools > Partition > Huge APP).
    - Disable PSRAM (Tools > PSRAM > Disabled).
    - Set the Upload Speed (Tools > Upload Speed > 921600).
    - If an upload error occurs, lower the transfer speed.

8. Edit the demo if needed. Most demos are well-commented on what has to be edited. Some demos require also preparing an SD card or else in specific manner.

9. Compile and upload to the board (Sketch > Upload).

    - If all goes well, the ESP32-SBC-FabGL board will reboot after the compilation and upload complete.
    - If the wireless parameters were not set, a prompt asking to configure the wireless connection will appear.
    - After that, the boot menu should show.

If you have compilation problems with most of the examples you probably installed latest version of espressif package for ESP32. The most important part is to use ESP32 package version older than 3.x.x ESP32 package (we used Espressif 2.0.11). Also do NOT use the “Arduino ESP32 Boards” package! Use the “esp32” package by espressif systems!

=======================================


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
