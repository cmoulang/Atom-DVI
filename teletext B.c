/*

saa5050 teletext character generator emulation

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

#include "teletext.h"

#include <stdbool.h>
#include <stdint.h>

#include "atom_if.h"
#include "colours.h"

#define FONT_CHARS 0x60
#define FONT_HEIGHT 12
#define FONT2_HEIGHT 20

extern uint8_t fontdata_saa5050_b[];

typedef unsigned int ff_bitmap_t;
ff_bitmap_t fast_font[FONT_CHARS * FONT2_HEIGHT * 3] = {0};
ff_bitmap_t ff_space[] = {0, 0, 0};

static inline ff_bitmap_t* ffont_char(ff_bitmap_t ff_font[], int ch,
                                      int sub_row) {
    int index = (ch * FONT2_HEIGHT + sub_row) * 3;
    return &ff_font[index];
}

static inline void write_pixel(pixel_t** pp, pixel_t pix) {
    **pp = pix;
    *pp += 1;
}

static inline int teletext_line_no(int line_num) {
    if (MODE_V_ACTIVE_LINES >= TELETEXT_LINES) {
        return line_num - (MODE_V_ACTIVE_LINES - TELETEXT_LINES) / 2;
    } else {
        return line_num * MODE_V_ACTIVE_LINES / TELETEXT_LINES;
    }
}

/// @brief lookup table for solid graphics
static ff_bitmap_t solid_lut[] = {
    0x00000000, 0x00000000, 0x00000000, 0x01010101, 0x00000101, 0x00000000,
    0x00000000, 0x01010000, 0x01010101, 0x01010101, 0x01010101, 0x01010101};

/// @brief lookup table for separated graphics
static ff_bitmap_t separated_lut[] = {
    0x00000000, 0x00000000, 0x00000000, 0x01010100, 0x00000001, 0x00000000,
    0x00000000, 0x01000000, 0x00010101, 0x01010100, 0x01000001, 0x00010101};

// the 20 lines in the 2x3 graphics are laid out as follows
//   0  1  2  3  4  5                         14 15 16 17 18 19
//                     6  7  8  9 10 11 12 13
static inline ff_bitmap_t* lookup_graphic(uint8_t c, int sub_row,
                                          bool separated, bool second_double) {
    if (second_double) {
        return ff_space;
    }
    c = c - 0x20;
    ff_bitmap_t* lut;

    if (separated) {
        const static int mask = 0b10000110000001100001;
        if ((1 << sub_row) & mask) {
            return ff_space;
        };
        lut = separated_lut;
    } else {
        lut = solid_lut;
    }

    if (sub_row < 6) {
        int index = c & 0b11;
        return &lut[index * 3];
    } else if (sub_row > 13) {
        int index = ((c >> 4) & 1) + ((c >> 5) & 2);
        return &lut[index * 3];
    } else {  // c >= 7 && c <= 13
        int index = (c >> 2) & 0b11;
        return &lut[index * 3];
    }
}

static inline ff_bitmap_t* lookup_character(uint8_t ch, const int sub_row,
                                            bool double_height,
                                            bool second_double) {
    ch = ch - 0x20;
    if (second_double) {
        if (double_height) {
            return ffont_char(fast_font, ch, sub_row / 2 + 10);  // + (10 - 19)
        } else {
            return ff_space;
        }
    } else {
        if (double_height) {
            return ffont_char(fast_font, ch, sub_row / 2);  // + (0..9)
        } else {
            return ffont_char(fast_font, ch, sub_row);  // + (0..19)
        }
    }
}

static inline pixel_t* out12_pixels(pixel_t* p, unsigned char fg_colour,
                                    unsigned char bg_colour, ff_bitmap_t* fbm) {
    unsigned int* q = (unsigned int*)p;

    q[0] = fbm[0] * fg_colour + (fbm[0] ^ 0x01010101) * bg_colour;
    q[1] = fbm[1] * fg_colour + (fbm[1] ^ 0x01010101) * bg_colour;
    q[2] = fbm[2] * fg_colour + (fbm[2] ^ 0x01010101) * bg_colour;

    return p + 12;
}

pixel_t* do_teletext(unsigned int line_num, pixel_t* p, unsigned char flags) {
    static int next_double = -1;
    static int frame_count = 0;
    static bool flash_now = false;
    static const pixel_t colours[] = {AT_BLACK, AT_RED,     AT_GREEN, AT_YELLOW,
                                      AT_BLUE,  AT_MAGENTA, AT_CYAN,  AT_WHITE};

    pixel_t fg_colour = AT_WHITE;
    pixel_t bg_colour = AT_BLACK;

    bool is_debug = flags & 0x01;
    bool reveal = flags & 0x02;
    bool graphics = false;
    bool flash = false;
    bool conceal = false;
    bool double_height = false;
    // bool box = false;
    bool separated_graphics = false;
    bool hold_graphics = false;
    ff_bitmap_t* last_graph_bitmap = ff_space;

    // Screen is 25 rows x 40 columns
    int relative_line_num = teletext_line_no(line_num);
    if (relative_line_num < 0 || relative_line_num >= TELETEXT_LINES) {
        int* q = (int*)p;
        for (int i = 0; i < MODE_H_ACTIVE_PIXELS / 4; i++) {
            q[i] = AT_BLACK;
        }
        return p + MODE_H_ACTIVE_PIXELS;
    }

    p += (MODE_H_ACTIVE_PIXELS - TELETEXT_H_PIXELS) / 2;

    const int row = (relative_line_num) / TELETEXT_ROW_HEIGHT;  // character row
    const int sub_row =
        relative_line_num -
        (TELETEXT_ROW_HEIGHT * row);  // row within current character (0..19)

    // start of frame initialisation
    if (relative_line_num == 0) {
        next_double = -1;
        // toggle the flash flag
        frame_count = (frame_count + 1) & 63;
        flash_now = frame_count < 21;
    };

    int ch_index = TELETEXT_PAGE_BUFFER + row * TELETEXT_COLUMNS;

    for (int col = 0; col < TELETEXT_COLUMNS; col++) {
        int ch = eb_get(ch_index + col) & 0x7F;
        bool non_printing = (ch < 0x20);

        // pre-render
        if (non_printing) {
            switch (ch) {
                case NORMAL_HEIGHT:
                    if (double_height) {
                        last_graph_bitmap = ff_space;
                        double_height = false;
                    }
                    break;
                case DOUBLE_HEIGHT:
                    if (next_double != row) {
                        next_double = row + 1;
                    }
                    if (!double_height) {
                        last_graph_bitmap = ff_space;
                        double_height = true;
                    }
                    break;

                case BLACK_BACKGROUND:
                    bg_colour = AT_BLACK;
                    break;

                case NEW_BACKGROUND:
                    bg_colour = fg_colour;
                    break;

                case HOLD_GRAPHICS:
                    hold_graphics = true;
                    break;
            }
        }

        ff_bitmap_t* bitmap = ff_space;
        if (non_printing) {
            if (is_debug) {
                int temp = ch & 0xF;
                if (temp <= 9) {
                    temp = temp + '0';
                } else {
                    temp = temp + 'a' - 10;
                }
                bitmap = lookup_character(temp, sub_row, false, false);
            } else {
                // non-printing char
                if (hold_graphics) {
                    bitmap = last_graph_bitmap;
                } else {
                    bitmap = ff_space;
                }
            }
        } else {
            if (graphics && (ch & (0x1 << 5))) {
                bitmap = lookup_graphic(ch, sub_row, separated_graphics,
                                        next_double == row);
                last_graph_bitmap = bitmap;
            } else {
                bitmap = lookup_character(ch, sub_row, double_height,
                                          next_double == row);
            }
        }

        if (non_printing && is_debug) {
            pixel_t f, b;
            if (ch & 0x10) {
                f = AT_ORANGE;
                b = AT_WHITE;
            } else {
                f = AT_ORANGE;
                b = AT_BLACK;
            }
            p = out12_pixels(p, f, b, bitmap);
        } else {
            if ((conceal && !reveal) || (flash && flash_now)) {
                // handle conceal and flash
                bitmap = ff_space;
            }
            p = out12_pixels(p, fg_colour, bg_colour, bitmap);
        }

        // post-render
        if (non_printing) {
            switch (ch) {
                case ALPHA_RED ... ALPHA_WHITE:
                    fg_colour = colours[ch];
                    if (graphics) {
                        last_graph_bitmap = ff_space;
                        graphics = false;
                    }
                    conceal = false;
                    break;
                case FLASH:
                    flash = true;
                    break;
                case STEADY:
                    flash = false;
                    break;
                // case END_BOX:
                //     box = false;
                //     break;
                // case START_BOX:
                //     box = true;
                //     break;
                case GRAPHICS_RED ... GRAPHICS_WHITE:
                    fg_colour = colours[ch & 7];
                    if (!graphics) {
                        last_graph_bitmap = ff_space;
                        graphics = true;
                    }
                    conceal = false;
                    break;
                case CONCEAL_DISPLAY:
                    conceal = true;
                    break;
                case CONTIGUOUS_GRAPHICS:
                    separated_graphics = false;
                    break;
                case SEPARATED_GRAPHICS:
                    separated_graphics = true;
                    break;
                case RELEASE_GRAPHICS:
                    hold_graphics = false;
                    break;
            }
        }
    }
    return p;
}

/// @brief Convert a single saa5050 character to its rounded version.
/// implements the saa5050 character rounding algorithm.
///
/// A B C --\ 1 2
/// D E F --/ 3 4
///
/// 1 = B | (A & E & !B & !D)
/// 2 = B | (C & E & !B & !F)
/// 3 = E | (!A & !E & B & D)
/// 4 = E | (!C & !E & B & F)
///
/// @param dest_ptr new font
/// @param src_ptr old font
/// @return
uint16_t* convert_char(uint16_t* dest_ptr, uint8_t* src_ptr) {
    for (int i = 1; i < 11; i += 1) {
        uint8_t src1 = src_ptr[i];
        uint8_t src2 = src_ptr[i + 1];
        uint16_t dst1 = 0;
        uint16_t dst2 = 0;

        for (int j = 0; j < 6; j++) {
            bool A = src1 & 0x80;
            bool B = src1 & 0x40;
            bool C = src1 & 0x20;

            bool D = src2 & 0x80;
            bool E = src2 & 0x40;
            bool F = src2 & 0x20;

            bool r1 = B | (A & E & !B & !D);
            bool r2 = B | (C & E & !B & !F);
            bool r3 = E | (!A & !E & B & D);
            bool r4 = E | (!C & !E & B & F);

            dst1 = dst1 | (r1 << 1) | r2;
            dst2 = dst2 | (r3 << 1) | r4;

            src1 = src1 << 1;
            src2 = src2 << 1;

            dst1 = dst1 << 2;
            dst2 = dst2 << 2;
        }

        *dest_ptr++ = dst1;
        *dest_ptr++ = dst2;
    }
    return dest_ptr;
}

/// @brief convert a 4 pixel bitmap to coefficients
/// @param bitmap
/// @return
static inline unsigned int bitmap_to_uint_old(unsigned int bitmap) {
    static unsigned int lut[16] = {
        0x00000000, 0x01000000, 0x00010000, 0x01010000,
        0x00000100, 0x01000100, 0x00010100, 0x01010100, 
        0x00000001, 0x01000001, 0x00010001, 0x01010001,
        0x00000101, 0x01000101, 0x00010101, 0x01010101,
    };

    return lut[0xF & bitmap];
}

/// @brief reverse and expand a 4-bit bitmap.
/// WXYZ -> 0x0Z0Y0X0W
/// eg. 0b1011 -> 0x01010001
/// @param bitmap the bitmap - only first 4 bits are used
/// @return the expanded bitmap
unsigned int bitmap_to_uint(unsigned int bitmap) {
    const int mask = 0x1010101;
    int x = bitmap & 0xF;
    x += (x << 9);
    x += (x << 18);
    return (x >> 3) & mask;
}

/// @brief convert a 12 pixel bitmap to coefficients
/// @param fb
/// @param bitmap
/// @return
static inline pixel_t* bitmap_to_fbitmap(ff_bitmap_t* fb,
                                         unsigned short bitmap) {
    fb[0] = bitmap_to_uint(bitmap >> 12);
    fb[1] = bitmap_to_uint(bitmap >> 8);
    fb[2] = bitmap_to_uint(bitmap >> 4);
}

/// @brief debug print 16 pixels
/// @param p
void print_pixels(uint16_t p) {
    for (uint16_t m = 0x8000; m > 0; m = m >> 1) {
        if (m & p) {
            putchar('#');
        } else {
            putchar(' ');
        }
    }
}

void init_font() {
    // convert 12x8 font to 20x16 and store in fast_font format
    for (int ch = 0; ch < 0x60; ch += 1) {
        uint16_t dest[FONT2_HEIGHT];
        uint8_t* src = fontdata_saa5050_b + ch * 12;
        convert_char(&dest[0], src);

        for (int sub_row = 0; sub_row < 20; sub_row += 1) {
            bitmap_to_fbitmap(ffont_char(fast_font, ch, sub_row),
                              dest[sub_row]);
            ff_bitmap_t* p = ffont_char(fast_font, ch, sub_row);
        }
    }
}

void teletext_init(void) {
    printf("teletext init x\n");
    init_font();
    printf("teletext init - complete\n");
}
