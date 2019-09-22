/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

/*
 * Reworked by Gavin Hurlbut <gjhurlbu@gmail.com> to use an attached SPI FRAM
 * for buffer storage, default to 128x64 display, I2C only
 *
 * changes (c) 2016 Gavin Hurlbut <gjhurlbu@gmail.com>
 * released under the BSD License
 *
 * Ported to C for the Cypress PSoC chipset using FreeRTOS
 * changes (c) 2019 Gavin Hurlbut <gjhurlbu@gmail.com>
 * released under an MIT License
 */

//#include <stdlib.h>

#include "project.h"
#include "SSD1306.h"
#include "i2cRegisters.h"

#ifndef _swap_int16
#define _swap_int16(a, b) { int16 t = a; a = b; b = t; }
#endif

#define draw_pixel(x, y) (cache_pixel(_draw_cache, (x), (y)))
#define cache_pixel(cache, x, y) ((cache)[SSD1306_PIXEL_ADDR((x), (y))])

static void _drawFastVLineInternal(int16 x, int16 y, int16 h, uint16 color);
static void _drawFastHLineInternal(int16 x, int16 y, int16 w, uint16 color);
static void _ssd1306_command(uint8 c);
static void _operCache(int16 x, int16 y, oper_t oper_, uint8 mask);


static uint8 _i2caddr;
static int8 _vccstate;
static uint8 _draw_cache[SSD1306_RAM_MIRROR_SIZE];
static uint8 _show_logo;
static int16 _WIDTH;	// Raw display, never changes
static int16 _HEIGHT;	// Raw display, never changes
static int16 _width;	// modified by current rotation
static int16 _height;	// modified by current rotation
static int16 _cursor_x;
static int16 _cursor_y;
static uint16 _textcolor;
static uint16 _textbgcolor;
static uint8 _textsize;
static uint8 _rotation;
static int _wrap;
static int _cp437;  // if set, use correct CP437 characterset (default off)
static GFXfont *gfxFont;

void SSD1306_initialize(void) {
  _WIDTH = SSD1306_LCDWIDTH;
  _HEIGHT = SSD1306_LCDHEIGHT;
  _width    = _WIDTH;
  _height   = _HEIGHT;
  _rotation  = 0;
  _cursor_y  = 0;
  _cursor_x  = 0;
  _textsize  = 1;
  _textcolor = 0xFFFF;
  _textbgcolor = 0xFFFF;
  _wrap      = 1;
  _cp437    = 0;
  _gfxFont   = NULL;
  _i2caddr = SSD1306_I2C_ADDRESS;
  _vccstate = SSD1306_SWITCHCAPVCC;
  SSD1306_reset();
}

void SSD1306_setAddress(uint8 i2caddr) {
  _i2caddr = i2caddr;
}

void SSD1306_reset(void) {
  SSD1306_clearDisplay();
  _show_logo = 1;
}

static void _operCache(int16 x, int16 y, oper_t oper_, uint8 mask)
{
    uint8 *addr = &draw_pixel(x, y);
    uint8_t data = *addr;

    switch (oper_) {
        case SET_BITS:
            data |= mask;
            break;
        case CLEAR_BITS:
            data &= ~mask;
            break;
        case TOGGLE_BITS:
            data ^= mask;
            break;
        default:
            break;
    }

    *addr = data;
}

void SSD1306_setVccstate(uint8 vccstate) {
  _vccstate = vccstate;
}

void SSD1306_begin(void) {
  // Init sequence
  _ssd1306_command(SSD1306_DISPLAYOFF);            // 0xAE
  _ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);    // 0xD5
  _ssd1306_command(0x80);                          // the suggested ratio 0x80

  _ssd1306_command(SSD1306_SETMULTIPLEX);          // 0xA8
  _ssd1306_command(SSD1306_LCDHEIGHT - 1);

  _ssd1306_command(SSD1306_SETDISPLAYOFFSET);      // 0xD3
  _ssd1306_command(0x0);                           // no offset
  _ssd1306_command(SSD1306_SETSTARTLINE | 0x0);    // line #0
  _ssd1306_command(SSD1306_CHARGEPUMP);            // 0x8D
  if (vccstate == SSD1306_EXTERNALVCC) {
    _ssd1306_command(0x10);
  } else {
    _ssd1306_command(0x14);
  }
  _ssd1306_command(SSD1306_MEMORYMODE);            // 0x20
  _ssd1306_command(0x00);                          // 0x0 act like ks0108
  _ssd1306_command(SSD1306_SEGREMAP | 0x1);
  _ssd1306_command(SSD1306_COMSCANDEC);

#if defined SSD1306_128_32
  _ssd1306_command(SSD1306_SETCOMPINS);            // 0xDA
  _ssd1306_command(0x02);
  _ssd1306_command(SSD1306_SETCONTRAST);           // 0x81
  _ssd1306_command(0x8F);
#elif defined SSD1306_128_64
  _ssd1306_command(SSD1306_SETCOMPINS);            // 0xDA
  _ssd1306_command(0x12);
  _ssd1306_command(SSD1306_SETCONTRAST);           // 0x81
  if (vccstate == SSD1306_EXTERNALVCC) {
    _ssd1306_command(0x9F);
  } else {
    _ssd1306_command(0xCF);
  }
#elif defined SSD1306_96_16
  _ssd1306_command(SSD1306_SETCOMPINS);            // 0xDA
  _ssd1306_command(0x2);   //ada x12
  _ssd1306_command(SSD1306_SETCONTRAST);           // 0x81
  if (vccstate == SSD1306_EXTERNALVCC) {
    _ssd1306_command(0x10);
  } else {
    _ssd1306_command(0xAF);
#endif

  _ssd1306_command(SSD1306_SETPRECHARGE);          // 0xd9
  if (vccstate == SSD1306_EXTERNALVCC) {
    _ssd1306_command(0x22);
  } else {
    _ssd1306_command(0xF1);
  }
  _ssd1306_command(SSD1306_SETVCOMDETECT);         // 0xDB
  _ssd1306_command(0x40);
  _ssd1306_command(SSD1306_DISPLAYALLON_RESUME);   // 0xA4
  _ssd1306_command(SSD1306_NORMALDISPLAY);         // 0xA6

  _ssd1306_command(SSD1306_DEACTIVATE_SCROLL);

  _ssd1306_command(SSD1306_DISPLAYON);             //--turn on oled panel
}


void SSD1306_invertDisplay(uint8 i) {
  if (i) {
    _ssd1306_command(SSD1306_INVERTDISPLAY);
  } else {
    _ssd1306_command(SSD1306_NORMALDISPLAY);
  }
}

static void _ssd1306_command(uint8 c) {
  // I2C
  uint8 control = 0x00;   // Co = 0, D/C = 0
  i2c_register_write(_i2caddr, control, c);
}

// startScrolLright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startScrollRight(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x00);
  _ssd1306_command(0xFF);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startScrollLeft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startScrollLeft(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x00);
  _ssd1306_command(0xFF);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startScrollDiagRight
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startScrollDiagRight(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  _ssd1306_command(0x00);
  _ssd1306_command(SSD1306_LCDHEIGHT);
  _ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x01);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startScrollDiagLeft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startScrollDiagLeft(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  _ssd1306_command(0x00);
  _ssd1306_command(SSD1306_LCDHEIGHT);
  _ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x01);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_stopScroll(void){
  _ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void SSD1306_dim(int dim) {
  uint8 contrast;

  if (dim) {
    contrast = 0; // Dimmed display
  } else {
    if (_vccstate == SSD1306_EXTERNALVCC) {
      contrast = 0x9F;
    } else {
      contrast = 0xCF;
    }
  }
  // the range of contrast to too small to be really useful
  // it is useful to dim the display
  _ssd1306_command(SSD1306_SETCONTRAST);
  _ssd1306_command(contrast);
}

void SSD1306_display(void) {
  _ssd1306_command(SSD1306_COLUMNADDR);
  _ssd1306_command(0);                  // Column start address (0 = reset)
  _ssd1306_command(SSD1306_LCDWIDTH-1); // Column end address (127 = reset)

  _ssd1306_command(SSD1306_PAGEADDR);
  _ssd1306_command(0);                  // Page start address (0 = reset)
  _ssd1306_command((SSD1306_LCDHEIGHT >> 3) - 1); // Page end address

  uint8 *cache = (_show_logo ? lcd_logo : _draw_cache);

  for (int16 y = 0; y < SSD1306_LCDHEIGHT; y += 8) {
    for (uint8 x = 0; x < SSD1306_BUFFER_SIZE; x += 16) {
      // Co = 0, D/C = 1
      i2c_register_write_buffer(_i2caddr, 0x40, &cache_pixel(cache, x, y), 16);
    }
  }

  if (_show_logo) {
    clearDisplay();
  }
}

// clear everything
void SSD1306_clearDisplay(void) {
  _show_logo = 0;
  memset(_draw_cache, 0, SSD1306_RAM_MIRROR_SIZE);
}

// the most basic function, set a single pixel
void SSD1306_drawPixel(int16 x, int16 y, uint16 color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
    return;

  // check rotation, move pixel around if necessary
  switch (getRotation()) {
  case 1:
    _swap_int16(x, y);
    x = _WIDTH - x - 1;
    break;
  case 2:
    x = _WIDTH - x - 1;
    y = _HEIGHT - y - 1;
    break;
  case 3:
    _swap_int16(x, y);
    y = _HEIGHT - y - 1;
    break;
  }

  // x is which column
  uint8 mask = (1 << (y & 0x07));
  
  switch (color)
  {
    case WHITE:   
      _operCache(x, y, SET_BITS, mask);  
      break;
    case BLACK:   
      _operCache(x, y, CLEAR_BITS, mask);  
      break;
    case INVERSE: 
      _operCache(x, y, TOGGLE_BITS, mask);  
      break;
    default:
      return;
  }
}


void SSD1306_drawFastHLine(int16 x, int16 y, int16 w, uint16 color) {
  int bSwap = 0;
  switch(_rotation) {
    case 0:
      // 0 degree rotation, do nothing
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x
      bSwap = 1;
      _swap_int16(x, y);
      x = _WIDTH - x - 1;
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = _WIDTH - x - 1;
      y = _HEIGHT - y - 1;
      x -= (w-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y  and adjust y for w (not to become h)
      bSwap = 1;
      _swap_int16(x, y);
      y = _HEIGHT - y - 1;
      y -= (w-1);
      break;
  }

  if(bSwap) {
    _drawFastVLineInternal(x, y, w, color);
  } else {
    _drawFastHLineInternal(x, y, w, color);
  }
}

static void _drawFastHLineInternal(int16 x, int16 y, int16 w, uint16 color) {
  // Do bounds/limit checks
  if (y < 0 || y >= _HEIGHT) {
    return;
  }

  // make sure we don't try to draw below 0
  if (x < 0) {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if ((x + w) > _WIDTH) {
    w = (_WIDTH - x);
  }

  // if our width is now negative, punt
  if (w <= 0) {
    return;
  }

  register uint8 mask = SSD1306_PIXEL_MASK(y);
  for (uint8 i = x; i < x + w; i++) {
    switch (color)
    {
      case WHITE:
        _operCache(i, y, SET_BITS, mask);
        break;
      case BLACK:
        _operCache(i, y, CLEAR_BITS, mask);
        break;
      case INVERSE:
        _operCache(i, y, TOGGLE_BITS, mask);
        break;
      default:
        return;
    }
  }
}

void SSD1306_drawFastVLine(int16 x, int16 y, int16 h, uint16 color) {
  int bSwap = 0;
  switch(_rotation) {
    case 0:
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x and adjust x for h (now to become w)
      bSwap = 1;
      _swap_int16(x, y);
      x = _WIDTH - x - 1;
      x -= (h-1);
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = _WIDTH - x - 1;
      y = _HEIGHT - y - 1;
      y -= (h-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y
      bSwap = 1;
      _swap_int16(x, y);
      y = _HEIGHT - y - 1;
      break;
  }

  if(bSwap) {
    _drawFastHLineInternal(x, y, h, color);
  } else {
    _drawFastVLineInternal(x, y, h, color);
  }
}


static void _drawFastVLineInternal(int16 x, int16 __y, int16 __h, uint16 color) {

  // do nothing if we're off the left or right side of the screen
  if (x < 0 || x >= _WIDTH) {
    return;
  }

  // make sure we don't try to draw below 0
  if (__y < 0) {
    // __y is negative, this will subtract enough from __h to account for __y being 0
    __h += __y;
    __y = 0;
  }

  // make sure we don't go past the height of the display
  if ((__y + __h) > _HEIGHT) {
    __h = (_HEIGHT - __y);
  }

  // if our height is now negative, punt
  if (__h <= 0) {
    return;
  }

  // this display doesn't need ints for coordinates, use local byte registers
  // for faster juggling
  register uint8 y = __y;
  register uint8 h = __h;

  // do the first partial byte, if necessary - this requires some masking
  register uint8 mod = (y & 0x07);
  register uint8 data;
  if (mod) {
    // mask off the high n bits we want to set
    mod = 8 - mod;

    // note - lookup table results in a nearly 10% performance improvement in
    // fill* functions
    // register uint8 mask = ~(0xFF >> (mod));
    static uint8 premask[8] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
    register uint8 mask = premask[mod];

    // adjust the mask if we're not going to reach the end of this byte
    if (h < mod) {
      mask &= (0xFF >> (mod-h));
    }

    switch (color)
    {
      case WHITE:
        _operCache(x, y, SET_BITS, mask);
        break;
      case BLACK:
        _operCache(x, y, CLEAR_BITS, mask);
        break;
      case INVERSE:
        _operCache(x, y, TOGGLE_BITS, mask);
        break;
      default:
        return;
    }

    // fast exit if we're done here!
    if (h < mod) {
      return;
    }

    h -= mod;
    y += mod;
  }

  // write solid bytes while we can - effectively doing 8 rows at a time
  if (h >= 8) {
    if (color == INVERSE)  {
      // separate copy of the code so we don't impact performance of the
      // black/white write version with an extra comparison per loop
      do {
        _operCache(x, y, TOGGLE_BITS, 0xFF);

        // adjust h & y (there's got to be a faster way for me to do this, but
        // this should still help a fair bit for now)
        h -= 8;
        y += 8;
      } while (h >= 8);
    } else {
      // store a local value to work with
      data = (color == WHITE) ? 0xFF : 0;

      do  {
	draw_pixel(x, y) = data;

        // adjust h & y (there's got to be a faster way for me to do this, but
        // this should still help a fair bit for now)
        h -= 8;
        y += 8;
      } while (h >= 8);
    }
  }

  // now do the final partial byte, if necessary
  if (h) {
    mod = h & 0x07;
    // this time we want to mask the low bits of the byte, vs the high bits we
    // did above
    // register uint8 mask = (1 << mod) - 1;
    // note - lookup table results in a nearly 10% performance improvement in
    // fill* functions
    static uint8 postmask[8] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F,
                                  0x7F};
    register uint8 mask = postmask[mod];

    switch (color)
    {
      case WHITE:
        _operCache(x, y, SET_BITS, mask);
        break;
      case BLACK:
        _operCache(x, y, CLEAR_BITS, mask);
        break;
      case INVERSE:
        _operCache(x, y, TOGGLE_BITS, mask);
        break;
    }
  }
}

// From Adafruit_GFX base class, ported to PSoC with FreeRTOS (and C)


// Draw a circle outline
void SSD1306_drawCircle(int16 x0, int16 y0, int16 r,
 uint16 color) {
  int16 f = 1 - r;
  int16 ddF_x = 1;
  int16 ddF_y = -2 * r;
  int16 x = 0;
  int16 y = r;

  drawPixel(x0  , y0+r, color);
  drawPixel(x0  , y0-r, color);
  drawPixel(x0+r, y0  , color);
  drawPixel(x0-r, y0  , color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void SSD1306_drawCircleHelper( int16 x0, int16 y0,
 int16 r, uint8 cornername, uint16 color) {
  int16 f     = 1 - r;
  int16 ddF_x = 1;
  int16 ddF_y = -2 * r;
  int16 x     = 0;
  int16 y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void SSD1306_fillCircle(int16 x0, int16 y0, int16 r,
 uint16 color) {
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void SSD1306_fillCircleHelper(int16 x0, int16 y0, int16 r,
 uint8 cornername, int16 delta, uint16 color) {

  int16 f     = 1 - r;
  int16 ddF_x = 1;
  int16 ddF_y = -2 * r;
  int16 x     = 0;
  int16 y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

// Bresenham's algorithm - thx wikpedia
void SSD1306_drawLine(int16 x0, int16 y0, int16 x1, int16 y1,
 uint16 color) {
  int16 steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16(x0, y0);
    _swap_int16(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16(x0, x1);
    _swap_int16(y0, y1);
  }

  int16 dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16 err = dx / 2;
  int16 ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

// Draw a rectangle
void SSD1306_drawRect(int16 x, int16 y, int16 w, int16 h,
 uint16 color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}

void SSD1306_fillRect(int16 x, int16 y, int16 w, int16 h,
 uint16 color) {
  // Update in subclasses if desired!
  for (int16 i=x; i<x+w; i++) {
    drawFastVLine(i, y, h, color);
  }
}

void SSD1306_fillScreen(uint16 color) {
  fillRect(0, 0, _width, _height, color);
}

// Draw a rounded rectangle
void SSD1306_drawRoundRect(int16 x, int16 y, int16 w,
 int16 h, int16 r, uint16 color) {
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x    , y+r  , h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// Fill a rounded rectangle
void SSD1306_fillRoundRect(int16 x, int16 y, int16 w,
 int16 h, int16 r, uint16 color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}

// Draw a triangle
void SSD1306_drawTriangle(int16 x0, int16 y0,
 int16 x1, int16 y1, int16 x2, int16 y2, uint16 color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

// Fill a triangle
void SSD1306_fillTriangle(int16 x0, int16 y0,
 int16 x1, int16 y1, int16 x2, int16 y2, uint16 color) {

  int16 a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    _swap_int16(y0, y1); _swap_int16(x0, x1);
  }
  if (y1 > y2) {
    _swap_int16(y2, y1); _swap_int16(x2, x1);
  }
  if (y0 > y1) {
    _swap_int16(y0, y1); _swap_int16(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int32
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) _swap_int16(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) _swap_int16(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }
}

// Draw a 1-bit image (bitmap) at the specified (x,y) position from the
// provided bitmap buffer using the specified foreground (for set bits)
// and background (for clear bits) colors.
// If foreground and background are the same, unset bits are transparent
void SSD1306_drawBitmap(int16 x, int16 y,
 const uint8 *bitmap, int16 w, int16 h, uint16 color, uint16 bg) {

  int16 i, j, byteWidth = (w + 7) / 8;
  uint8 byte;

  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(i & 7) byte <<= 1;
      else      byte   = bitmap[j * byteWidth + i / 8];
      if(byte & 0x80) drawPixel(x+i, y+j, color);
      else if(color != bg) drawPixel(x+i, y+j, bg);
    }
  }
}

//Draw XBitMap Files (*.xbm), exported from GIMP,
//Usage: Export from GIMP to *.xbm, rename *.xbm to *.c and open in editor.
//C Array can be directly used with this function
void SSD1306_drawXBitmap(int16 x, int16 y,
 const uint8 *bitmap, int16 w, int16 h, uint16 color) {

  int16 i, j, byteWidth = (w + 7) / 8;
  uint8 byte;

  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(i & 7) byte >>= 1;
      else      byte   = bitmap[j * byteWidth + i / 8];
      if(byte & 0x01) drawPixel(x+i, y+j, color);
    }
  }
}

size_t SSD1306_write(uint8 c) {
  if(!_gfxFont) { // 'Classic' built-in font

    if(c == '\n') {
      _cursor_y += _textsize*8;
      _cursor_x  = 0;
    } else if(c == '\r') {
      // skip em
    } else {
      if(_wrap && ((_cursor_x + _textsize * 6) >= _width)) { // Heading off edge?
        _cursor_x  = 0;            // Reset x to zero
        _cursor_y += _textsize * 8; // Advance y one line
      }
      drawChar(_cursor_x, _cursor_y, c, _textcolor, _textbgcolor, _textsize);
      _cursor_x += _textsize * 6;
    }

  } else { // Custom font

    if(c == '\n') {
      _cursor_x  = 0;
      _cursor_y += (int16)_textsize * _gfxFont->yAdvance;
    } else if(c != '\r') {
      uint8 first = _gfxFont->first;
      if((c >= first) && (c <= _gfxFont->last)) {
        uint8   c2    = c - _gfxFont->first;
        GFXglyph *glyph = &(_gfxFont->glyph[c2]);
        uint8   w     = glyph->width,
                h     = glyph->height;
        if((w > 0) && (h > 0)) { // Is there an associated bitmap?
          int16 xo = glyph->xOffset;
          if(_wrap && ((_cursor_x + _textsize * (xo + w)) >= _width)) {
            // Drawing character would go off right edge; wrap to new line
            _cursor_x  = 0;
            _cursor_y += (int16)_textsize * _gfxFont->yAdvance;
          }
          drawChar(_cursor_x, _cursor_y, c, _textcolor, _textbgcolor, _textsize);
        }
        _cursor_x += glyph->xAdvance * (int16)_textsize;
      }
    }

  return 1;
}

// Draw a character
void SSD1306_drawChar(int16 x, int16 y, unsigned char c,
 uint16 color, uint16 bg, uint8 size) {

  if(!_gfxFont) { // 'Classic' built-in font

    if((x >= _width)            || // Clip right
       (y >= _height)           || // Clip bottom
       ((x + 6 * size - 1) < 0) || // Clip left
       ((y + 8 * size - 1) < 0))   // Clip top
      return;

    if(!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior

    for(int8 i=0; i<6; i++ ) {
      uint8 line;
      if(i < 5) line = font[(c*5)+i];
      else      line = 0x0;
      for(int8 j=0; j<8; j++, line >>= 1) {
        if(line & 0x1) {
          if(size == 1) drawPixel(x+i, y+j, color);
          else          fillRect(x+(i*size), y+(j*size), size, size, color);
        } else if(bg != color) {
          if(size == 1) drawPixel(x+i, y+j, bg);
          else          fillRect(x+i*size, y+j*size, size, size, bg);
        }
      }
    }

  } else { // Custom font

    // Character is assumed previously filtered by write() to eliminate
    // newlines, returns, non-printable characters, etc.  Calling drawChar()
    // directly with 'bad' characters of font may cause mayhem!

    c -= _gfxFont->first;
    GFXglyph *glyph  = &(_gfxFont->glyph[c]);
    uint8  *bitmap = _gfxFont->bitmap;

    uint16 bo = glyph->bitmapOffset;
    uint8  w  = glyph->width,
           h  = glyph->height,
           xa = glyph->xAdvance;
    int8   xo = glyph->xOffset,
           yo = glyph->yOffset;
    uint8  xx, yy, bits, bit = 0;
    int16  xo16, yo16;

    if(size > 1) {
      xo16 = xo;
      yo16 = yo;
    }

    // Todo: Add character clipping here

    // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
    // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
    // has typically been used with the 'classic' font to overwrite old
    // screen contents with new data.  This ONLY works because the
    // characters are a uniform size; it's not a sensible thing to do with
    // proportionally-spaced fonts with glyphs of varying sizes (and that
    // may overlap).  To replace previously-drawn text when using a custom
    // font, use the getTextBounds() function to determine the smallest
    // rectangle encompassing a string, erase the area with fillRect(),
    // then draw new text.  This WILL infortunately 'blink' the text, but
    // is unavoidable.  Drawing 'background' pixels will NOT fix this,
    // only creates a new set of problems.  Have an idea to work around
    // this (a canvas object type for MCUs that can afford the RAM and
    // displays supporting setAddrWindow() and pushColors()), but haven't
    // implemented this yet.

    for(yy=0; yy<h; yy++) {
      for(xx=0; xx<w; xx++) {
        if(!(bit++ & 7)) {
          bits = bitmap[bo++];
        }
        if(bits & 0x80) {
          if(size == 1) {
            drawPixel(x+xo+xx, y+yo+yy, color);
          } else {
            fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size, size, size, color);
          }
        }
        bits <<= 1;
      }
    }

  } // End classic vs custom font
}

void SSD1306_setCursor(int16 x, int16 y) {
  _cursor_x = x;
  _cursor_y = y;
}

int16 SSD1306_getCursorX(void) const {
  return _cursor_x;
}

int16 SSD1306_getCursorY(void) const {
  return _cursor_y;
}

void SSD1306_setTextSize(uint8 s) {
  _textsize = (s > 0) ? s : 1;
}

void SSD1306_setTextColor(uint16 c, uint16 b) {
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  _textcolor   = c;
  _textbgcolor = b;
}

void SSD1306_setTextWrap(int w) {
  _wrap = w;
}

uint8 SSD1306_getRotation(void) const {
  return _rotation;
}

void SSD1306_setRotation(uint8 x) {
  _rotation = (x & 3);
  switch(_rotation) {
   case 0:
   case 2:
    _width  = _WIDTH;
    _height = _HEIGHT;
    break;
   case 1:
   case 3:
    _width  = _HEIGHT;
    _height = _WIDTH;
    break;
  }
}

// Enable (or disable) Code Page 437-compatible charset.
// There was an error in glcdfont.c for the longest time -- one character
// (#176, the 'light shade' block) was missing -- this threw off the index
// of every character that followed it.  But a TON of code has been written
// with the erroneous character indices.  By default, the library uses the
// original 'wrong' behavior and old sketches will still work.  Pass 'true'
// to this function to use correct CP437 character values in your code.
void SSD1306_cp437(int x) {
  _cp437 = x;
}

void SSD1306_setFont(const GFXfont *f) {
  if(f) {          // Font struct pointer passed in?
    if(!_gfxFont) { // And no current font struct?
      // Switching from classic to new font behavior.
      // Move cursor pos down 6 pixels so it's on baseline.
      _cursor_y += 6;
    }
  } else if(_gfxFont) { // NULL passed.  Current font struct defined?
    // Switching from new to classic font behavior.
    // Move cursor pos up 6 pixels so it's at top-left of char.
    _cursor_y -= 6;
  }
  _gfxFont = (GFXfont *)f;
}

// Pass string and a cursor position, returns UL corner and W,H.
void SSD1306_getTextBounds(char *str, int16 x, int16 y,
 int16 *x1, int16 *y1, uint16 *w, uint16 *h) {
  uint8 c; // Current character

  *x1 = x;
  *y1 = y;
  *w  = *h = 0;

  if(_gfxFont) {

    GFXglyph *glyph;
    uint8   first = _gfxFont->first,
            last  = _gfxFont->last,
            gw, gh, xa;
    int8    xo, yo;
    int16   minx = _width, miny = _height, maxx = -1, maxy = -1,
              gx1, gy1, gx2, gy2, ts = (int16)_textsize,
              ya = ts * _gfxFont->yAdvance;

    while((c = *str++)) {
      if(c != '\n') { // Not a newline
        if(c != '\r') { // Not a carriage return, is normal char
          if((c >= first) && (c <= last)) { // Char present in current font
            c    -= first;
            glyph = &(_gfxFont->glyph[c]);
            gw    = glyph->width;
            gh    = glyph->height;
            xa    = glyph->xAdvance;
            xo    = glyph->xOffset;
            yo    = &glyph->yOffset;
            if(_wrap && ((x + (((int16)xo + gw) * ts)) >= _width)) {
              // Line wrap
              x  = 0;  // Reset x to 0
              y += ya; // Advance y by 1 line
            }
            gx1 = x   + xo * ts;
            gy1 = y   + yo * ts;
            gx2 = gx1 + gw * ts - 1;
            gy2 = gy1 + gh * ts - 1;
            if(gx1 < minx) minx = gx1;
            if(gy1 < miny) miny = gy1;
            if(gx2 > maxx) maxx = gx2;
            if(gy2 > maxy) maxy = gy2;
            x += xa * ts;
          }
        } // Carriage return = do nothing
      } else { // Newline
        x  = 0;  // Reset x
        y += ya; // Advance y by 1 line
      }
    }
    // End of string
    *x1 = minx;
    *y1 = miny;
    if(maxx >= minx) *w  = maxx - minx + 1;
    if(maxy >= miny) *h  = maxy - miny + 1;

  } else { // Default font

    uint16 lineWidth = 0, maxWidth = 0; // Width of current, all lines

    while((c = *str++)) {
      if(c != '\n') { // Not a newline
        if(c != '\r') { // Not a carriage return, is normal char
          if(_wrap && ((x + _textsize * 6) >= _width)) {
            x  = 0;            // Reset x to 0
            y += _textsize * 8; // Advance y by 1 line
            if(lineWidth > maxWidth) maxWidth = lineWidth; // Save widest line
            lineWidth  = _textsize * 6; // First char on new line
          } else { // No line wrap, just keep incrementing X
            lineWidth += _textsize * 6; // Includes interchar x gap
          }
        } // Carriage return = do nothing
      } else { // Newline
        x  = 0;            // Reset x to 0
        y += _textsize * 8; // Advance y by 1 line
        if(lineWidth > maxWidth) maxWidth = lineWidth; // Save widest line
        lineWidth = 0;     // Reset lineWidth for new line
      }
    }
    // End of string
    if(lineWidth) y += _textsize * 8; // Add height of last (or only) line
    if(lineWidth > maxWidth) maxWidth = lineWidth; // Is the last or only line the widest?
    *w = maxWidth - 1;               // Don't include last interchar x gap
    *h = y - *y1;

  } // End classic vs custom font
}

// Return the size of the display (per current rotation)
int16 SSD1306_width(void) const {
  return _width;
}

int16 SSD1306_height(void) const {
  return _height;
}


