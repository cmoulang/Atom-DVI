/*

Convert Motorola 6847 VDU memory content to pico dvi frame buffer

Copyright 2025 Chris Moulang

This file is part of AtomHDMI

AtomHDMI is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

AtomHDMI is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
AtomHDMI. If not, see <https://www.gnu.org/licenses/>.

*/

#include "mc6847.h"

#include <stdbool.h>
#include <stdint.h>

#include "atom_if.h"
#include "hardware/sync.h"
#include "fonts.h"
#include "platform.h"

#define FB_ADDR 0x8000
#define VID_MEM_SIZE 0x2000

// Option for two levels of orange: normal for semi-graphics and bright for text
#ifndef INCLUDE_BRIGHT_ORANGE
#define INCLUDE_BRIGHT_ORANGE 0
#endif

#define AT_RED_1 0b01100000
#define AT_RED_2 0b10100000
#define AT_RED 0b11100000

#define AT_GREEN_1 0b00001100
#define AT_GREEN_2 0b00010100
#define AT_GREEN 0b00011100

#define AT_BLUE_1 0b00000001
#define AT_BLUE_2 0b00000010
#define AT_BLUE 0b00000011

#define AT_BLACK 0
#define AT_YELLOW (AT_RED | AT_GREEN)
#define AT_YELLOW_1 (AT_RED_1 | AT_GREEN_1)
#define AT_WHITE (AT_RED | AT_GREEN | AT_BLUE)
#define AT_WHITE_1 (AT_RED_1 | AT_GREEN_1 | AT_BLUE_1)
#define AT_WHITE_2 (AT_WHITE_1 << 1)
#define COLOUR AT_YELLOW
#define AT_CYAN (AT_GREEN | AT_BLUE)
#if INCLUDE_BRIGHT_ORANGE
#define AT_ORANGE (AT_RED | AT_GREEN_1)
#define BRIGHT_ORANGE (AT_RED | AT_GREEN_2)
#else
#define AT_ORANGE (AT_RED | AT_GREEN_2)
#endif
#define AT_MAGENTA (AT_RED | AT_BLUE)

#define NO_COLOURS (9 + INCLUDE_BRIGHT_ORANGE)
uint16_t colour_palette_atom[NO_COLOURS] = {AT_GREEN,
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

uint16_t colour_palette_vga80[8] = {AT_BLACK, AT_BLUE,    AT_GREEN,  AT_CYAN,
                                    AT_RED,   AT_MAGENTA, AT_YELLOW, AT_WHITE};

uint16_t colour_palette_improved[4] = {
    AT_BLACK,
    AT_YELLOW,
    AT_GREEN,
    AT_MAGENTA,
};

uint16_t colour_palette_artifact1[4] = {
    AT_BLACK,
    AT_BLUE,
    AT_ORANGE,
    AT_WHITE,
};

uint16_t colour_palette_artifact2[4] = {
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

uint16_t* colour_palette = colour_palette_atom;

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
    return is_colour(mode) ? get_width(mode) * 2 / 8 : get_width(mode) / 8;
};

#define COL80_OFF 0x00
#define COL80_ON 0x80
#define COL80_ATTR 0x08

const uint chars_per_row = 32;

const uint vga_width = 640;
const uint vga_height = 480;

const uint max_width = 512;
const uint max_height = 384;

const uint vertical_offset = (vga_height - max_height) / 2;
const uint horizontal_offset = (vga_width - max_width) / 2;

void __no_inline_not_in_flash_func(event_handler)() {
    static uint8_t lastpia = 0;
    dma_hw->ints1 = 1u << eb_get_event_chan();
    int address = eb_get_event();
    while (address > 0) {
        uint16_t x = eb_6502_addr(address);
        address = eb_get_event();
    }
}

void measure_freqs(void) {
    uint f_pll_sys =
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb =
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_hstx = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_HSTX);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    printf("pll_usb  = %dkHz\n", f_pll_usb);
    printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\n", f_clk_sys);
    printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb  = %dkHz\n", f_clk_usb);
    printf("clk_adc  = %dkHz\n", f_clk_adc);
    printf("clk_hstx  = %dkHz\n", f_clk_hstx);
    // Can't measure clk_ref / xosc as it is the ref
}

struct mc6847_context {
    uint8_t* pico_fb;
    unsigned int atom_fb;
    int mode;
    uint16_t border_colour;
};

typedef struct mc6847_context mc6847_context_t;

static mc6847_context_t _context = {0};

void mc6847_init(char* pico_fb) {
    measure_freqs();

    _context.pico_fb = pico_fb;
    _context.atom_fb = FB_ADDR;
    _context.mode = 0;

    memset((char*)&_eb_memory[0], 0, sizeof _eb_memory);
    eb_set_perm(EB_ADDRESS_LOW, EB_PERM_NONE, EB_ADDRESS_HIGH - EB_ADDRESS_LOW);
    eb_set_perm(FB_ADDR, EB_PERM_WRITE_ONLY, VID_MEM_SIZE);
    eb_set_perm_byte(PIA_ADDR, EB_PERM_WRITE_ONLY);
    eb_set_perm_byte(PIA_ADDR + 2, EB_PERM_WRITE_ONLY);

    eb_init(pio1);
    eb_set_exclusive_handler(event_handler);
}

static inline int _calc_fb_base() { return FB_ADDR; }

static inline uint8_t* add_border(uint8_t* buffer, uint8_t color, int width) {
    memset(buffer, color, width);
    return buffer + width;
}

volatile uint8_t artifact = 0;

static inline bool alt_colour() { return false; }

int get_mode() {
#if (PLATFORM == PLATFORM_ATOM)
    return (eb_get(PIA_ADDR) & 0xf0) >> 4;
#elif (PLATFORM == PLATFORM_DRAGON)
    return ((eb_get(PIA_ADDR) & 0x80) >> 7) | ((eb_get(PIA_ADDR) & 0x70) >> 3);
#endif
}

static inline uint8_t* do_graphics(uint8_t* p, mc6847_context_t* context,
                                   int _relative_line_num) {
    const int height = get_height(context->mode);
    const int graphics_line_num = (_relative_line_num / 2) * height / 192;
    const uint vdu_address =
        context->atom_fb + bytes_per_row(context->mode) * graphics_line_num;
    size_t bp = vdu_address;

    const uint pixel_count = get_width(context->mode);

    uint16_t* palette = colour_palette;
    if (alt_colour()) {
        palette += 4;
    }
    uint16_t* art_palette =
        (1 == artifact) ? colour_palette_artifact1 : colour_palette_artifact2;

    if (is_colour(context->mode)) {
        uint32_t word = 0;
        for (uint pixel = 0; pixel < pixel_count; pixel++) {
            if ((pixel % 16) == 0) {
                word = eb_get32(bp);
                bp += 4;
            }
            uint x = (word >> 30) & 0b11;
            uint16_t colour = palette[x];
            if (pixel_count == 128) {
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
            } else if (pixel_count == 64) {
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
                *p++ = colour;
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
                    for (uint32_t mask = 0x80000000; mask > 0;) {
                        uint16_t colour = (b & mask) ? fg : 0;
                        *p++ = colour;
                        *p++ = colour;
                        mask = mask >> 1;

                        colour = (b & mask) ? fg : 0;
                        *p++ = colour;
                        *p++ = colour;
                        mask = mask >> 1;

                        colour = (b & mask) ? fg : 0;
                        *p++ = colour;
                        *p++ = colour;
                        mask = mask >> 1;

                        colour = (b & mask) ? fg : 0;
                        *p++ = colour;
                        *p++ = colour;
                        mask = mask >> 1;
                    }
                } else {
                    uint32_t word = b;
                    for (uint apixel = 0; apixel < 16; apixel++) {
                        uint acol = (word >> 30) & 0b11;
                        uint16_t colour = art_palette[acol];

                        *p++ = colour;
                        *p++ = colour;
                        *p++ = colour;
                        *p++ = colour;

                        word = word << 2;
                    }
                }
            }

            else if (pixel_count == 128) {
                for (uint32_t mask = 0x80000000; mask > 0; mask = mask >> 1) {
                    uint16_t colour = (b & mask) ? palette[0] : 0;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                }
            } else if (pixel_count == 64) {
                for (uint32_t mask = 0x80000000; mask > 0; mask = mask >> 1) {
                    uint16_t colour = (b & mask) ? palette[0] : 0;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                    *p++ = colour;
                }
            }
        }
    }
    return p;
}

#define INV_MASK    0x80
#define AS_MASK     0x40
#define INTEXT_MASK AS_MASK

#define GetIntExt(ch)   (ch & INTEXT_MASK) ? true : false

volatile uint8_t fontno = DEFAULT_FONT;
volatile uint16_t paper = DEF_PAPER;
volatile uint16_t ink = DEF_INK;
volatile uint16_t ink_alt = DEF_INK_ALT;
volatile bool support_lower = false;
volatile uint8_t max_lower = LOWER_END;


// Always 0 on Atom
#define GetSAMSG()      0

uint8_t *do_text(mc6847_context_t* context, unsigned int relative_line_num, uint8_t *p, bool is_debug)
{
    // Screen is 16 rows x 32 columns
    // Each char is 12 x 8 pixels
    // Note we divide ralative_line_number by 2 as we are double scanning each 6847 line to
    // 2 VGA lines.
    uint row = (relative_line_num / 2) / 12;              // char row
    uint sub_row = (relative_line_num / 2) % 12;          // scanline within current char row
    uint sgidx = is_debug ? TEXT_INDEX : GetSAMSG();      // index into semigraphics table
    uint rows_per_char = 12 / sg_bytes_row[sgidx];        // bytes per character space vertically
    uint8_t *fontdata = fonts[fontno].fontdata + sub_row; // Local fontdata pointer

    if (row < 16)
    {
        // Calc start address for this row
        uint vdu_address = ((chars_per_row * sg_bytes_row[sgidx]) * row) + (chars_per_row * (sub_row / rows_per_char));

        for (int col = 0; col < 32; col++)
        {
            // Get character data from RAM and extract inv,ag,int/ext
            uint ch = eb_get(context->atom_fb + vdu_address + col);
            bool inv = (ch & INV_MASK) ? true : false;
            bool as = (ch & AS_MASK) ? true : false;
            bool intext = GetIntExt(ch);

            uint16_t fg_colour;
            uint16_t bg_colour = paper;

            // Deal with text mode first as we can decide this purely on the setting of the
            // alpha/semi bit.
            if (!as)
            {
                uint8_t b = fontdata[(ch & 0x3f) * 12];

                fg_colour = alt_colour() ? ink_alt : ink;

                if (support_lower && ch >= LOWER_START && ch <= max_lower)
                {
                    b = fontdata[((ch & 0x3f) + 64) * 12];

                    if (LOWER_INVERT)
                    {
                        bg_colour = fg_colour;
                        fg_colour = paper;
                    }
                }
                else if (inv)
                {
                    bg_colour = fg_colour;
                    fg_colour = paper;
                }

                if (b == 0)
                {
                    for (int i=0; i<16; i++) {
                        *p++ = bg_colour;
                    }
                }
                else
                {
                    // The internal character generator is only 6 bits wide, however external
                    // character ROMS are 8 bits wide so we must handle them here
                    for (uint8_t mask = 0x80; mask > 0; mask = mask >> 1)
                    {
                        uint8_t c = (b & mask) ? fg_colour : bg_colour;
                        *p++ = c;
                        *p++ = c;
                    }
                }
            }
            else // Semigraphics
            {
                uint colour_index;

                if (as && intext)
                {
                    sgidx = SG6_INDEX; // SG6
                }

                colour_index = (SG6_INDEX == sgidx) ? (ch & SG6_COL_MASK) >> SG6_COL_SHIFT : (ch & SG4_COL_MASK) >> SG4_COL_SHIFT;

                if (alt_colour() && (SG6_INDEX == sgidx))
                {
                    colour_index += 4;
                }

                fg_colour = colour_palette_atom[colour_index];

                uint pix_row = (SG6_INDEX == sgidx) ? 2 - (sub_row / 4) : 1 - (sub_row / 6);

                uint16_t pix0 = ((ch >> (pix_row * 2)) & 0x1) ? fg_colour : bg_colour;
                uint16_t pix1 = ((ch >> (pix_row * 2)) & 0x2) ? fg_colour : bg_colour;
                for (int i=0; i<8; i++) {
                    *p++ = pix1;
                }
                for (int i=0; i<8; i++) {
                    *p++ = pix0;
                }
            }
        }
    }
    return p;
}

static inline void draw_line(int line_num, mc6847_context_t* context,
                             uint8_t* p) {
    int relative_line_num = line_num - vertical_offset;

    if (relative_line_num < 0 || relative_line_num >= max_height) {
        // Add top/bottom borders
        add_border(p, context->border_colour, vga_width);
    } else if (!(context->mode & 1))  // Alphanumeric or Semigraphics
    {
        p = add_border(p, context->border_colour, horizontal_offset);
        p = do_text(context, relative_line_num, p, false);
        add_border(p, context->border_colour, horizontal_offset);
    } else {
        p = add_border(p, context->border_colour, horizontal_offset);
        p = do_graphics(p, context, relative_line_num);
        add_border(p, context->border_colour, horizontal_offset);
    }
}

void mc6847_run() {
    absolute_time_t timeout = make_timeout_time_ms(0);
    absolute_time_t stats_timeout = make_timeout_time_ms(1000);
    int frame_count = 0;
    while (1) {
        if (get_absolute_time() >= stats_timeout) {
            printf("FPS: %d\n", frame_count);
            frame_count = 0;
            stats_timeout = make_timeout_time_ms(1000);
        }
        while (get_absolute_time() < timeout) {
            //__wfi();
        }
        timeout = make_timeout_time_ms(0);
        _context.mode = get_mode();
        _context.atom_fb = _calc_fb_base();
        _context.border_colour =  (_context.mode & 1) ? colour_palette[0] : 0;
        for (int r = 0; r < vga_height; r++) {
            draw_line(r, &_context, _context.pico_fb + r * vga_width);
        }
        frame_count++;
    }
}
