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

#include <stdlib.h>

// This might be... fun
#include <Adafruit_GFX.h>
#include "SSD1306.h"

#define ssd1306_swap(a, b) { int16 t = a; a = b; b = t; }
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

void SSD1306_initialize(uint8 i2caddr) {
  // Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT);
  _i2caddr = i2caddr;
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

void SSD1306_begin(uint8 vccstate) {
  _vccstate = vccstate;

  // I2C Init
  Wire.begin();
#ifdef __SAM3X8E__
  // Force 400 KHz I2C, rawr! (Uses pins 20, 21 for SDA, SCL)
  TWI1->TWI_CWGR = 0;
  TWI1->TWI_CWGR = ((VARIANT_MCK / (2 * 400000)) - 4) * 0x101;
#endif

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

  Wire.setClock(400000);
  Wire.beginTransmission(_i2caddr);
  Wire.write(control);
  Wire.write(c);
  Wire.endTransmission();
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startscrollright(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x00);
  _ssd1306_command(0xFF);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startscrollleft(uint8 start, uint8 stop){
  _ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  _ssd1306_command(0x00);
  _ssd1306_command(start);
  _ssd1306_command(0x00);
  _ssd1306_command(stop);
  _ssd1306_command(0x00);
  _ssd1306_command(0xFF);
  _ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startscrolldiagright(uint8 start, uint8 stop){
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

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306_startscrolldiagleft(uint8 start, uint8 stop){
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

void SSD1306_stopscroll(void){
  _ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void SSD1306_dim(boolean dim) {
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
      // send a bunch of data in one transmission
      Wire.setClock(400000);
      Wire.beginTransmission(_i2caddr);
      Wire.write(0x40);
      for (uint8 i = 0; i < 16; i++) {
	uint8 data = cache_pixel(cache, x + i, y);
        Wire.write(data);
      }
      Wire.endTransmission();
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
    ssd1306_swap(x, y);
    x = WIDTH - x - 1;
    break;
  case 2:
    x = WIDTH - x - 1;
    y = HEIGHT - y - 1;
    break;
  case 3:
    ssd1306_swap(x, y);
    y = HEIGHT - y - 1;
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
  boolean bSwap = false;
  switch(rotation) {
    case 0:
      // 0 degree rotation, do nothing
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x
      bSwap = true;
      ssd1306_swap(x, y);
      x = WIDTH - x - 1;
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      x -= (w-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y  and adjust y for w (not to become h)
      bSwap = true;
      ssd1306_swap(x, y);
      y = HEIGHT - y - 1;
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
  if (y < 0 || y >= HEIGHT) {
    return;
  }

  // make sure we don't try to draw below 0
  if (x < 0) {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if ((x + w) > WIDTH) {
    w = (WIDTH - x);
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
  bool bSwap = false;
  switch(rotation) {
    case 0:
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x and adjust x for h (now to become w)
      bSwap = true;
      ssd1306_swap(x, y);
      x = WIDTH - x - 1;
      x -= (h-1);
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      y -= (h-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y
      bSwap = true;
      ssd1306_swap(x, y);
      y = HEIGHT - y - 1;
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
  if (x < 0 || x >= WIDTH) {
    return;
  }

  // make sure we don't try to draw below 0
  if (__y < 0) {
    // __y is negative, this will subtract enough from __h to account for __y being 0
    __h += __y;
    __y = 0;
  }

  // make sure we don't go past the height of the display
  if ((__y + __h) > HEIGHT) {
    __h = (HEIGHT - __y);
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
