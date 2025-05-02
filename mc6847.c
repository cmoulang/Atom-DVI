/*

Convert Motorola 6847 VDU memory content to pico dvi frame buffer

Copyright 2025 Chris Moulang

This file is part of Atom-DVI

Atom-DVI is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Atom-DVI is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Atom-DVI. If not, see <https://www.gnu.org/licenses/>.

*/

#include "mc6847.h"

#include <stdbool.h>
#include <stdint.h>

#include "atom_if.h"
#include "atom_sid.h"
#include "colours.h"
#include "fonts.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include "platform.h"
#include "teletext.h"
#include "videomode.h"

// defines the number of buffers in the line buffer pool
#define LINE_BUFFER_POOL_COUNT 2
pixel_t line_buffer_pool[LINE_BUFFER_POOL_COUNT][MODE_H_ACTIVE_PIXELS];

static queue_t line_request_queue;

#define FB_ADDR 0x8000
#define VID_MEM_SIZE 0x2000

#define NO_COLOURS (9 + INCLUDE_BRIGHT_ORANGE)
pixel_t colour_palette_atom[NO_COLOURS] = {AT_GREEN,
                                           AT_YELLOW,
                                           AT_BLUE,
                                           AT_RED,
                                           AT_WHITE,
                                           AT_CYAN,
                                           AT_MAGENTA,
                                           AT_ORANGE,
                                           AT_BLACK
#if INCLUDE_BRIGHT_ORANGE
                                           ,
                                           BRIGHT_ORANGE
#endif
};

#define IDX_GREEN 0
#define IDX_YELLOW 1
#define IDX_BLUE 2
#define IDX_RED 3
#define IDX_WHITE 4
#define IDX_CYAN 5
#define IDX_MAGENTA 6
#define IDX_ORANGE 7
#define IDX_BLACK 8

#define MAX_COLOUR (8 + INCLUDE_BRIGHT_ORANGE)

#if INCLUDE_BRIGHT_ORANGE
#define IDX_BRIGHT_ORANGE 9
#endif

#define DEF_INK AT_GREEN
#define DEF_PAPER AT_BLACK

#if INCLUDE_BRIGHT_ORANGE
#define DEF_INK_ALT BRIGHT_ORANGE
#else
#define DEF_INK_ALT AT_ORANGE
#endif

pixel_t colour_palette_vga80[8] = {AT_BLACK, AT_BLUE,    AT_GREEN,  AT_CYAN,
                                   AT_RED,   AT_MAGENTA, AT_YELLOW, AT_WHITE};

pixel_t colour_palette_improved[4] = {
    AT_BLACK,
    AT_YELLOW,
    AT_GREEN,
    AT_MAGENTA,
};

pixel_t colour_palette_artifact1[4] = {
    AT_BLACK,
    AT_BLUE,
    AT_ORANGE,
    AT_WHITE,
};

pixel_t colour_palette_artifact2[4] = {
    AT_BLACK,
    AT_ORANGE,
    AT_BLUE,
    AT_WHITE,
};

// Masks to extract pixel colours from SG4 and SG6 bytes
#define SG4_COL_MASK 0x70
#define SG6_COL_MASK 0xC0

#define SG4_COL_SHIFT 4
#define SG6_COL_SHIFT 6

// Masks to extact bit paterns from SG4 and SG6
#define SG4_PAT_MASK 0x0F
#define SG6_PAT_MASK 0x3F

// Bytes / char array for text / semigraphics modes
// SG mode                     4  8  12 24  6
const unsigned int sg_bytes_row[5] = {1, 4, 6, 12, 1};

#define TEXT_INDEX 0
#define SG4_INDEX 0
#define SG8_INDEX 1
#define SG12_INDEX 2
#define SG24_INDEX 3
#define SG6_INDEX 4

pixel_t* colour_palette = colour_palette_atom;

//                             0  1a   1   2a    2   3a    3   4a    4
//                             0  1    2    3    4    5    6    7    8
const unsigned int width_lookup[9] = {32,  64,  128, 128, 128,
                                      128, 128, 128, 256};
const unsigned int height_lookup[9] = {16, 64, 64, 64, 96, 96, 192, 192, 192};

// Treat artifacted pmode 4 as pmode 3 with a different palette
static inline bool is_colour(unsigned int mode) { return !(mode & 0b10); };

unsigned int get_width(unsigned int mode) {
    unsigned int m = (mode + 1) / 2;
    return width_lookup[m];
};

unsigned int get_height(unsigned int mode) {
    unsigned int m = (mode + 1) / 2;
    return height_lookup[m];
};

unsigned int bytes_per_row(unsigned int mode) {
    unsigned int retval;
    if (!(mode & 1)) {
        retval = 32;
    } else if (is_colour(mode)) {
        retval = get_width(mode) / 4;
    } else {
        retval = get_width(mode) / 8;
    }
    return retval;
};

const unsigned int lines_per_row_lookup[] = {6, 6, 6, 4, 4, 2, 2, 2};

unsigned int lines_per_row(unsigned int mode) {
    unsigned int retval;
    if (!(mode & 1)) {
        retval = 24;
    } else {
        retval = lines_per_row_lookup[mode/2];
    }
    return retval;
}


#define COL80_OFF 0x00
#define COL80_ON 0x80
#define COL80_ATTR 0x08

const uint chars_per_row = 32;

const uint max_width = 256 * XSCALE;
const uint max_height = 192 * YSCALE;

const uint vertical_offset = (MODE_V_ACTIVE_LINES - max_height) / 2;
const uint horizontal_offset = (MODE_H_ACTIVE_PIXELS - max_width) / 2;

void reset_vga80() {
    eb_set(COL80_BASE, COL80_OFF);
    eb_set(COL80_FG, 0xB2);
    eb_set(COL80_BG, 0x00);
    eb_set(COL80_STAT, 0x12);
}

static pixel2_t vga80_lut[128 * 4];

void initialize_vga80() {
    // Reset the VGA80 hardware
    reset_vga80();
    // Use an LUT to allow two pixels to be calculated at once, taking account
    // of the attribute byte for colours
    //
    // Bit  8  7  6  5  4  3  2  1   0
    //      --bgc--  x  --fgc--  p1 p0
    //
    for (int i = 0; i < 128 * 4; i++) {
        vga80_lut[i] = ((i & 1) ? colour_palette_vga80[(i >> 2) & 7]
                                : colour_palette_vga80[(i >> 6) & 7])
                       << 8;
        vga80_lut[i] |= ((i & 2) ? colour_palette_vga80[(i >> 2) & 7]
                                 : colour_palette_vga80[(i >> 6) & 7]);
    }
}

static inline int _calc_fb_base() { return FB_ADDR; }

static inline uint8_t* add_border(uint8_t* buffer, const uint8_t color,
                                  const int width) {
    const uint x = color | (color << 8) | (color << 16) | (color << 24);
    uint* b = (uint*)buffer;
    for (int i = 0; i < width / 4; i++) {
        *b++ = x;
    }
    return (uint8_t*)b;
}

volatile uint8_t artifact = 0;

static inline bool alt_colour() {
#if (PLATFORM == PLATFORM_ATOM)
    return !!(eb_get(PIA_ADDR + 2) & 0x8);
#elif (PLATFORM == PLATFORM_DRAGON)
    return (eb_get(PIA_ADDR) & 0x08);
#endif
}

int get_mode() {
#if (PLATFORM == PLATFORM_ATOM)
    return (eb_get(PIA_ADDR) & 0xf0) >> 4;
#elif (PLATFORM == PLATFORM_DRAGON)
    return ((eb_get(PIA_ADDR) & 0x80) >> 7) | ((eb_get(PIA_ADDR) & 0x70) >> 3);
#endif
}

static inline void write_pixel(pixel_t** pp, pixel_t c) {
    uint16_t* q = (uint16_t*)*pp;
    q[0] = (c << 8) + c;
    *pp += 2;
}

static inline void write_pixel2(pixel_t** pp, pixel_t c) {
    uint32_t* q = (uint32_t*)*pp;
    uint32_t x = (c << 8) + c;
    x = (x << 16) + x;
    q[0] = x;
    *pp += 4;
}

static inline void write_pixel4(pixel_t** pp, pixel_t c) {
    write_pixel2(pp, c);
    write_pixel2(pp, c);
}

static inline void write_pixel8(pixel_t** pp, pixel_t c) {
    write_pixel4(pp, c);
    write_pixel4(pp, c);
}

static inline pixel_t* do_graphics(pixel_t* p, int mode, int atom_fb,
                                   int border_colour, int _relative_line_num) {
    size_t bp = atom_fb;

    const uint pixel_count = get_width(mode);

    pixel_t* palette = colour_palette;
    if (alt_colour()) {
        palette += 4;
    }
    pixel_t* art_palette =
        (1 == artifact) ? colour_palette_artifact1 : colour_palette_artifact2;

    if (is_colour(mode)) {
        uint32_t word = 0;
        for (uint pixel = 0; pixel < pixel_count; pixel++) {
            if ((pixel % 16) == 0) {
                word = eb_get32(bp);
                bp += 4;
            }
            uint x = (word >> 30) & 0b11;
            uint16_t colour = palette[x];
            if (pixel_count == 128) {
                write_pixel2(&p, colour);
            } else if (pixel_count == 64) {
                write_pixel4(&p, colour);
            }
            word = word << 2;
        }
    } else {
        uint16_t fg = palette[0];
        for (uint i = 0; i < pixel_count / 32; i++) {
            const uint32_t b = eb_get32(bp);
            bp += 4;
            if (pixel_count == 256) {
                if (0 == artifact) {
                    write_pixel(&p, (b & 0x1 << 31) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 30) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 29) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 28) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 27) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 26) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 25) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 24) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 23) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 22) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 21) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 20) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 19) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 18) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 17) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 16) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 15) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 14) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 13) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 12) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 11) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 10) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 9) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 8) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 7) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 6) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 5) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 4) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 3) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 2) ? fg : 0);
                    write_pixel(&p, (b & 0x1 << 1) ? fg : 0);
                    write_pixel(&p, (b & 0x1) ? fg : 0);
                } else {
                    uint32_t word = b;
                    for (uint apixel = 0; apixel < 16; apixel++) {
                        uint acol = (word >> 30) & 0b11;
                        uint16_t colour = art_palette[acol];

                        write_pixel2(&p, colour);

                        word = word << 2;
                    }
                }
            } else if (pixel_count == 128) {
                for (uint32_t mask = 0x80000000; mask > 0; mask = mask >> 1) {
                    uint16_t colour = (b & mask) ? palette[0] : 0;
                    write_pixel2(&p, colour);
                }
            } else if (pixel_count == 64) {
                for (uint32_t mask = 0x80000000; mask > 0; mask = mask >> 1) {
                    uint16_t colour = (b & mask) ? palette[0] : 0;
                    write_pixel4(&p, colour);
                }
            }
        }
    }
    return p;
}

#define INV_MASK 0x80
#define AS_MASK 0x40
#define INTEXT_MASK AS_MASK

#define GetIntExt(ch) (ch & INTEXT_MASK) ? true : false

volatile uint8_t fontno = DEFAULT_FONT;
volatile uint16_t paper = DEF_PAPER;
volatile uint16_t ink = DEF_INK;
volatile uint16_t ink_alt = DEF_INK_ALT;
volatile bool support_lower = false;
volatile uint8_t max_lower = LOWER_END;

// Always 0 on Atom
#define GetSAMSG() 0

static inline void do_char(pixel_t* p, uint sub_row, char ch) {
    const pixel2_t fg_colour = AT_ORANGE | AT_ORANGE << pixel_bits;
    const pixel2_t bg_colour = AT_WHITE_1 | AT_WHITE_1 << pixel_bits;

    pixel2_t* q = (pixel2_t*)p;

    const uint8_t* fontdata =
        fonts[fontno].fontdata + sub_row;  // Local fontdata pointer

    uint8_t b = fontdata[(ch & 0x3f) * 12];

    if (ch >= LOWER_START && ch <= max_lower) {
        b = fontdata[((ch & 0x3f) + 64) * 12];
    }

    if (b == 0) {
        for (int i = 0; i < 8; i++) {
            *q++ = bg_colour;
        }
    } else {
        for (uint8_t mask = 0x80; mask > 0; mask = mask >> 1) {
            pixel2_t c = (b & mask) ? fg_colour : bg_colour;
            *q++ = c;
        }
    }
}

static inline void do_string(pixel_t* p, uint sub_row, char* str) {
    while (*str) {
        do_char(p, sub_row, *str);
        p += 16;
        str += 1;
    }
}

pixel_t* do_text(int mode, int atom_fb, int border_colour,
                                      unsigned int relative_line_num,
                                      pixel_t* p) {
    // Screen is 16 rows x 32 columns
    // Each char is 12 x 8 pixels
    const uint row = (relative_line_num) / 12;  // char row
    const uint sub_row =
        (relative_line_num) % 12;  // scanline within current char row
    uint sgidx = GetSAMSG();       // index into semigraphics table
    const uint rows_per_char =
        12 / sg_bytes_row[sgidx];  // bytes per character space vertically
    const uint8_t* fontdata =
        fonts[fontno].fontdata + sub_row;  // Local fontdata pointer

    if (row < 16) {
        for (int col = 0; col < 32; col++) {
            // Get character data from RAM and extract inv,ag,int/ext
            uint ch = eb_get(atom_fb + col);
            bool inv = (ch & INV_MASK) ? true : false;
            bool as = (ch & AS_MASK) ? true : false;
            bool intext = GetIntExt(ch);

            uint16_t fg_colour;
            uint16_t bg_colour = paper;

            // Deal with text mode first as we can decide this purely on the
            // setting of the alpha/semi bit.
            if (!as) {
                uint8_t b = fontdata[(ch & 0x3f) * 12];

                fg_colour = alt_colour() ? ink_alt : ink;

                if (support_lower && ch >= LOWER_START && ch <= max_lower) {
                    b = fontdata[((ch & 0x3f) + 64) * 12];

                    if (LOWER_INVERT) {
                        bg_colour = fg_colour;
                        fg_colour = paper;
                    }
                } else if (inv) {
                    bg_colour = fg_colour;
                    fg_colour = paper;
                }

                if (b == 0) {
                    write_pixel8(&p, bg_colour);
                } else {
                    // The internal character generator is only 6 bits wide,
                    // however external character ROMS are 8 bits wide so we
                    // must handle them here
                    write_pixel(&p, (b & 0x80) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x40) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x20) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x10) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x08) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x04) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x02) ? fg_colour : bg_colour);
                    write_pixel(&p, (b & 0x01) ? fg_colour : bg_colour);
                }
            } else  // Semigraphics
            {
                uint colour_index;

                if (as && intext) {
                    sgidx = SG6_INDEX;  // SG6
                }

                colour_index = (SG6_INDEX == sgidx)
                                   ? (ch & SG6_COL_MASK) >> SG6_COL_SHIFT
                                   : (ch & SG4_COL_MASK) >> SG4_COL_SHIFT;

                if (alt_colour() && (SG6_INDEX == sgidx)) {
                    colour_index += 4;
                }

                fg_colour = colour_palette_atom[colour_index];

                uint pix_row = (SG6_INDEX == sgidx) ? 2 - (sub_row / 4)
                                                    : 1 - (sub_row / 6);

                uint16_t pix0 =
                    ((ch >> (pix_row * 2)) & 0x1) ? fg_colour : bg_colour;
                uint16_t pix1 =
                    ((ch >> (pix_row * 2)) & 0x2) ? fg_colour : bg_colour;
                write_pixel4(&p, pix1);
                write_pixel4(&p, pix0);
            }
        }
    }
    return p;
}

uint8_t* do_text_vga80(uint relative_line_num, pixel_t* p) {
    // Screen is 80 columns by 40 rows
    // Each char is 12 x 8 pixels
    uint row = relative_line_num / 12;
    uint sub_row = relative_line_num % 12;

    uint8_t* fd = fonts[FONT_CGA_THIN].fontdata + sub_row;

    if (row < 40) {
        // Compute the start address of the current row in the Atom
        // framebuffer
        uint char_addr = GetVidMemBase() + 80 * row;

        // Read the VGA80 control registers
        uint vga80_ctrl1 = eb_get(COL80_FG);
        uint vga80_ctrl2 = eb_get(COL80_BG);

        // *p++ = COMPOSABLE_RAW_RUN;
        // *p++ = BLACK;    // Extra black pixel
        // *p++ = 642 - 3;  //
        // *p++ = BLACK;    // Extra black pixel

        // For efficiency, compute two pixels at a time using a lookup table
        // p is now on a word boundary due to the extra pixels above
        pixel2_t* q = (pixel2_t*)p;

        if (vga80_ctrl1 & 0x08) {
            // Attribute mode enabled, attributes follow the characters in
            // the frame buffer
            uint attr_addr = char_addr + 80 * 40;
            uint shift = (sub_row >> 1) & 0x06;  // 0, 2 or 4
            // Compute these outside of the for loop for efficiency
            uint smask0 = 0x10 >> shift;
            uint smask1 = 0x20 >> shift;
            uint ulmask = (sub_row == 10) ? 0xFF : 0x00;
            for (int col = 0; col < 80; col++) {
                uint ch = eb_get(char_addr++);
                uint attr = eb_get(attr_addr++);
                pixel2_t* vp = vga80_lut + ((attr & 0x77) << 2);
                if (attr & 0x80) {
                    // Semi Graphics
                    pixel2_t p1 = (ch & smask1) ? *(vp + 3) : *vp;
                    pixel2_t p0 = (ch & smask0) ? *(vp + 3) : *vp;
                    // Unroll the writing of the four pixel pairs
                    *q++ = p1;
                    *q++ = p1;
                    *q++ = p0;
                    *q++ = p0;
                } else {
#if (PLATFORM == PLATFORM_DRAGON)
                    ch ^= 0x60;
#endif
                    // Text
                    uint8_t b = fd[(ch & 0x7f) * 12];
                    if (ch >= 0x80) {
                        b = ~b;
                    }
                    // Underlined
                    if (attr & 0x08) {
                        b |= ulmask;
                    }
                    // Unroll the writing of the four pixel pairs
                    *q++ = *(vp + ((b >> 6) & 3));
                    *q++ = *(vp + ((b >> 4) & 3));
                    *q++ = *(vp + ((b >> 2) & 3));
                    *q++ = *(vp + ((b >> 0) & 3));
                }
            }
        } else {
            // Attribute mode disabled, use default colours from the VGA80
            // control registers:
            //   bits 2..0 of VGA80_CTRL1 (#BDE4) are the default foreground
            //   colour bits 2..0 of VGA80_CTRL2 (#BDE5) are the default
            //   background colour
            uint attr = ((vga80_ctrl2 & 7) << 4) | (vga80_ctrl1 & 7);
            pixel2_t* vp = vga80_lut + (attr << 2);
            for (int col = 0; col < 80; col++) {
                uint ch = eb_get(char_addr++);
                bool inv = (ch & INV_MASK) ? true : false;

#if (PLATFORM == PLATFORM_DRAGON)
                ch ^= 0x40;
#endif
                uint8_t b = fd[(ch & 0x7f) * 12];
                if (inv) {
                    b = ~b;
                }
                // Unroll the writing of the four pixel pairs
                *q++ = *(vp + ((b >> 6) & 3));
                *q++ = *(vp + ((b >> 4) & 3));
                *q++ = *(vp + ((b >> 2) & 3));
                *q++ = *(vp + ((b >> 0) & 3));
            }
        }
    }
    // The above loops add 80 x 4 = 320 32-bit words, which is 640 16-bit
    // words
    return p + 640;
}

void draw_line(int line_num, int mode, int atom_fb, uint8_t* p) {
    static int prev_mode = 0;
    static int border_colour = 0;
    static int mem_reg = 0;
    static int counter = 99;
    int relative_line_num = line_num - vertical_offset;

    if (relative_line_num == 0) {
        mem_reg = 0;
        counter = lines_per_row(mode);
    } 
    

    if (relative_line_num < 0 || relative_line_num >= max_height) {
        // Add top/bottom borders
        add_border(p, border_colour, MODE_H_ACTIVE_PIXELS);
    } else if (!(mode & 1))  // Alphanumeric or Semigraphics
    {
        border_colour = AT_BLACK;
        p = add_border(p, border_colour, horizontal_offset);
        p = do_text(mode, atom_fb + mem_reg, border_colour, relative_line_num / YSCALE, p);
        p = add_border(p, border_colour, horizontal_offset);
    } else {
        border_colour = colour_palette[0];
        p = add_border(p, border_colour, horizontal_offset);
        p = do_graphics(p, mode, atom_fb + mem_reg, border_colour,
                        relative_line_num * 2 / YSCALE);
        p = add_border(p, border_colour, horizontal_offset);
    }


    counter--;
    if (counter == 0 | prev_mode != mode) {
        counter = lines_per_row(mode);
        mem_reg += bytes_per_row(mode);
        prev_mode = mode;
    }
}

static inline void ascii_to_atom(char* str) {
    while (*str != 0) {
        *str = *str + 0x20;
        if (*str < 0x80) {
            *str = *str ^ 0x60;
        }
        str++;
    }
}


static int row = 0;
static int col = 0;

void mc6847_outc(char c) {
    if (c == '\n') {
        row += 1;
        if (row == 25) {
            row = 0;
        }
        col = 0;
        return;
    }

    if (c == '\f') {
        for (int i = FB_ADDR; i < FB_ADDR + VID_MEM_SIZE; i++) {
            eb_set(i, 32);
        }
        row = 0;
        col = 0;
        return;
    }

    if (eb_get(COL80_BASE) & COL80_ON) {
        if (c >= 0x60 && c < 0x80) {
            c = c - 0x20;
        } else if (c >= 0x40 && c < 0x60) {
            c = c - 0x40;
        }
    } else {
    c = c + 0x20;
    if (c < 0x80) {
        c = c ^ 0x60;
    }
}
    eb_set(0x8000 + row * 80 + col, c);
    col += 1;
    if (col == 80) {
        col = 0;
        row += 1;
        if (row == 25) {
            row = 0;
        }
    }
}

void mc6847_print(const char* str) {
    while (*str != 0) {
        mc6847_outc(*str);
        str++;
    }
}

/// @brief Called from the DMA IRQ routine.
/// Get the pixels for the given line number and add the line number to the
/// event queue.
/// @param line_num the line number
/// @return MODE_H_ACTIVE_PIXELS pixels
pixel_t* mc6847_get_line_buffer(const int line_num) {
    queue_try_add(&line_request_queue, &line_num);
    if (line_num >= 0) {
        int buf_index = line_num % LINE_BUFFER_POOL_COUNT;
        return line_buffer_pool[buf_index];
    } else {
        return NULL;
    }
}

void mc6847_reset() { reset_vga80(); }

void mc6847_vga_mode()
{
    reset_vga80();
    eb_set(COL80_BASE, COL80_ON);
}


void mc6847_init(bool vdu_ram_enabled, bool emulate_reset) {
    printf("mc6847_init\n");
    teletext_init();

    queue_init(&line_request_queue, sizeof(int), LINE_BUFFER_POOL_COUNT);

    if (vdu_ram_enabled) {
        eb_set_perm(FB_ADDR, EB_PERM_READ_WRITE, VID_MEM_SIZE);
    } else {
        eb_set_perm(FB_ADDR, EB_PERM_WRITE_ONLY, VID_MEM_SIZE);
    }

    eb_set_perm(0xF000, EB_PERM_WRITE_ONLY, 0x400);
    eb_set_perm_byte(PIA_ADDR, EB_PERM_WRITE_ONLY);
    eb_set_perm_byte(PIA_ADDR + 2, EB_PERM_WRITE_ONLY);
    eb_set_perm(COL80_BASE, EB_PERM_READ_WRITE, 16);
    if (emulate_reset) {
        mc6847_print("\fACORN ATOM");
    }
    initialize_vga80();

    eb_init(pio1);
}

#define VSYNC_ON (192 * 2 + vertical_offset)
#define VSYNC_OFF 0

// run the emulation - can be run from both cores simultaneously
void mc6847_run() {
    while (1) {
        int line_num;
        queue_remove_blocking(&line_request_queue, &line_num);

        if (line_num >= 0) {
            const int next =
                (line_num + LINE_BUFFER_POOL_COUNT - 1) % MODE_V_ACTIVE_LINES;
            int buf_index = next % LINE_BUFFER_POOL_COUNT;
            char* p = line_buffer_pool[buf_index];
            int mode = get_mode();
            int atom_fb = _calc_fb_base();
            if (eb_get(COL80_BASE) & COL80_ON) {
                do_text_vga80(next, p);
            } else {
#ifdef TELETEXT
                do_teletext(p, MODE_H_ACTIVE_PIXELS, next,
                            eb_get(TELETEXT_REG_FLAGS));
#else
                draw_line(next, mode, atom_fb, p);
#endif
            }
        }

        if (line_num == VSYNC_ON) {
            gpio_put(PIN_VSYNC, false);
        } else if (line_num == VSYNC_OFF) {
            gpio_put(PIN_VSYNC, true);
        }
    }
}
