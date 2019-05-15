Converts an image (png, jpeg, ....) to FabGL Bitmap structure
Usage:
  python img2bitmap filename [-t x y] [-s width height] [-d]

  -t = pixel where to take transparent color
  -s = resize to specified values
  -d = enable dithering

Example:
  python img2bitmap.py test.png -s 64 64 >out.c