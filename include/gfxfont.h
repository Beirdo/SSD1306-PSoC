// Font structures for newer Adafruit_GFX (1.1 and later).
// Example fonts are included in 'Fonts' directory.
// To use a font in your Arduino sketch, #include the corresponding .h
// file and pass address of GFXfont struct to setFont().  Pass NULL to
// revert to 'classic' fixed-space bitmap font.

#ifndef _GFXFONT_H_
#define _GFXFONT_H_

#include "project.h"

typedef struct { // Data stored PER GLYPH
	uint16 bitmapOffset;     // Pointer into GFXfont->bitmap
	uint8  width, height;    // Bitmap dimensions in pixels
	uint8  xAdvance;         // Distance to advance cursor (x axis)
	int8   xOffset, yOffset; // Dist from cursor pos to UL corner
} GFXglyph;

typedef struct { // Data stored for FONT AS A WHOLE:
	uint8  *bitmap;      // Glyph bitmaps, concatenated
	GFXglyph *glyph;       // Glyph array
	uint8   first, last; // ASCII extents
	uint8   yAdvance;    // Newline distance (y axis)
} GFXfont;

#endif // _GFXFONT_H_
