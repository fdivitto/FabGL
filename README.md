# FabGL
### VGA Controller, PS/2 Keyboard Controller, Graphics Library, Game Engine and ANSI/VT Terminal for the **ESP32**

**[Please look here for full API documentation](http://www.fabglib.org)**

This library works well with ESP32 revision 1 or upper.

VGA output requires a digital to analog converter (DAC): it can be done by three 270 Ohm resistors to have 8 colors, or by 6 resistors to have 64 colors.

Three fonts are embedded to best represents 80x25 or 132x25 text screen, at 640x350 resolution. 
However other fonts and resolutions can be used.

Sprites can have up to 64 colors (RGB, 2 bits per channel + transparency).
A sprite has associated one o more bitmaps, even of different size. Bitmaps (frames) can be selected in sequence to create animations.
Unlimited number of sprites are supported. However big sprites and a large amount of them reduces the frame rate and could generate flickering.

When there is enough memory (on low resolutions like 320x200), it is possible to allocate two screen buffers, so to implement double buffering.
In this case drawing primitives always draw on the back buffer.

Except for double buffering or when explicitly disabled, all drawings are performed on vertical retracing, so no flickering is visible.
If the queue of primitives to draw is not processed before the vertical retracing ends, then it is interrupted and continued at next retracing.

### Space Invaders Example (click for video):

[![Everything Is AWESOME](https://img.youtube.com/vi/LL8J7tjxeXA/hqdefault.jpg)](https://www.youtube.com/watch?v=LL8J7tjxeXA "")

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

