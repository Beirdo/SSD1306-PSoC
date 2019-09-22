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
All text above, and the splash screen must be included in any redistribution
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

#ifndef _SSD1306_H_
#define _SSD1306_H_

#include "project.h"
#include "gfxfont.h"

#define BLACK 0
#define WHITE 1
#define INVERSE 2

#define SSD1306_I2C_ADDRESS   0x3C  // 011110+SA0 - 0x3C or 0x3D
// Address for 128x32 is 0x3C
// Address for 128x64 is 0x3D (default) or 0x3C (if SA0 is grounded)

/*=========================================================================
    SSD1306 Displays
    -----------------------------------------------------------------------
    The driver is used in multiple displays (128x64, 128x32, etc.).
    Select the appropriate display below to create an appropriately
    sized framebuffer, etc.

    SSD1306_128_64  128x64 pixel display

    SSD1306_128_32  128x32 pixel display

    SSD1306_96_16

    -----------------------------------------------------------------------*/
#define SSD1306_128_64
//   #define SSD1306_128_32
//   #define SSD1306_96_16
/*=========================================================================*/

#if defined SSD1306_128_64 && defined SSD1306_128_32
  #error "Only one SSD1306 display can be specified at once in SSD1306.h"
#endif
#if !defined SSD1306_128_64 && !defined SSD1306_128_32 && !defined SSD1306_96_16
  #error "At least one SSD1306 display must be specified in SSD1306.h"
#endif

#if defined SSD1306_128_64
  #define SSD1306_LCDWIDTH                  128
  #define SSD1306_LCDHEIGHT                 64
#endif
#if defined SSD1306_128_32
  #define SSD1306_LCDWIDTH                  128
  #define SSD1306_LCDHEIGHT                 32
#endif
#if defined SSD1306_96_16
  #define SSD1306_LCDWIDTH                  96
  #define SSD1306_LCDHEIGHT                 16
#endif

#define SSD1306_RAM_MIRROR_SIZE (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8)

#define SSD1306_PIXEL_ADDR(x, y) ((x) + ((y) >> 3) * SSD1306_LCDWIDTH)
#define SSD1306_PIXEL_MASK(y)	 (1 << ((y) & 0x07))

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA

#define SSD1306_SETVCOMDETECT 0xDB

#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9

#define SSD1306_SETMULTIPLEX 0xA8

#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10

#define SSD1306_SETSTARTLINE 0x40

#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22

#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8

#define SSD1306_SEGREMAP 0xA0

#define SSD1306_CHARGEPUMP 0x8D

#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

typedef enum {
    SET_BITS,
    CLEAR_BITS,
    TOGGLE_BITS,
} oper_t;

void SSD1306_initialize(void);
void SSD1306_setAddress(uint8 i2caddr);
void SSD1306_setVccstate(uint8 vccstate);
void SSD1306_reset(void);

void SSD1306_begin(void);

void SSD1306_clearDisplay(void);
void SSD1306_invertDisplay(uint8 i);
void SSD1306_display();

void SSD1306_startScrollRight(uint8 start, uint8 stop);
void SSD1306_startScrollLeft(uint8 start, uint8 stop);

void SSD1306_startScrollDiagRight(uint8 start, uint8 stop);
void SSD1306_startScrollDiagLeft(uint8 start, uint8 stop);
void SSD1306_stopScroll(void);

void SSD1306_dim(int dim);

void SSD1306_drawPixel(int16 x, int16 y, uint16 color);

void SSD1306_drawFastVLine(int16 x, int16 y, int16 h, uint16 color);
void SSD1306_drawFastHLine(int16 x, int16 y, int16 w, uint16 color);

extern const uint8 lcd_logo[SSD1306_RAM_MIRROR_SIZE];

// And now for the parts from the old base class.  Note: these have all been
// renamed to being SSD1306, but originally were from Adafruit_GFX class

void Adafruit_GFX_initialize(int16 w, int16 h); // Constructor

void Adafruit_GFX_drawLine(int16 x0, int16 y0, int16 x1, int16 y1, uint16 color);
void Adafruit_GFX_drawRect(int16 x, int16 y, int16 w, int16 h, uint16 color);
void Adafruit_GFX_fillRect(int16 x, int16 y, int16 w, int16 h, uint16 color);
void Adafruit_GFX_fillScreen(uint16 color);

void Adafruit_GFX_drawCircle(int16 x0, int16 y0, int16 r, uint16 color);
void Adafruit_GFX_drawCircleHelper(int16 x0, int16 y0, int16 r,
      uint8 cornername, uint16 color);

void Adafruit_GFX_fillCircle(int16 x0, int16 y0, int16 r, uint16 color);
void Adafruit_GFX_fillCircleHelper(int16 x0, int16 y0, int16 r, uint8 cornername,
      int16 delta, uint16 color);
void Adafruit_GFX_drawTriangle(int16 x0, int16 y0, int16 x1, int16 y1,
      int16 x2, int16 y2, uint16 color);
void Adafruit_GFX_fillTriangle(int16 x0, int16 y0, int16 x1, int16 y1,
      int16 x2, int16 y2, uint16 color);
void Adafruit_GFX_drawRoundRect(int16 x0, int16 y0, int16 w, int16 h,
      int16 radius, uint16 color);
void Adafruit_GFX_fillRoundRect(int16 x0, int16 y0, int16 w, int16 h,
      int16 radius, uint16 color);
void Adafruit_GFX_drawBitmap(int16 x, int16 y, uint8 *bitmap,
      int16 w, int16 h, uint16 color, uint16 bg);
void Adafruit_GFX_drawXBitmap(int16 x, int16 y, const uint8 *bitmap,
      int16 w, int16 h, uint16 color);
void Adafruit_GFX_drawChar(int16 x, int16 y, unsigned char c, uint16 color,
      uint16 bg, uint8 size);
void Adafruit_GFX_setCursor(int16 x, int16 y);
void Adafruit_GFX_setTextColor(uint16 c, uint16 bg);
void Adafruit_GFX_setTextSize(uint8 s);
void Adafruit_GFX_setTextWrap(int w);
void Adafruit_GFX_setRotation(uint8 r);
void Adafruit_GFX_cp437(int x);
void Adafruit_GFX_setFont(const GFXfont *f);
void Adafruit_GFX_getTextBounds(char *string, int16 x, int16 y,
      int16 *x1, int16 *y1, uint16 *w, uint16 *h);

size_t Adafruit_GFX_write(uint8);

int16 Adafruit_GFX_height(void);
int16 Adafruit_GFX_width(void);

uint8 Adafruit_GFX_getRotation(void);

// get current cursor position (get rotation safe maximum values, using: width() for x, height() for y)
int16 Adafruit_GFX_getCursorX(void);
int16 Adafruit_GFX_getCursorY(void);

#endif /* _SSD1306_H_ */
