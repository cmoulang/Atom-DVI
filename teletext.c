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

extern uint8_t fontdata_saa5050[];
static uint16_t font[FONT_CHARS * FONT2_HEIGHT];

static inline void write_pixel(pixel_t** pp, pixel_t c) {
    **pp = c;
    *pp += 1;
}

static inline int teletext_line_no(int line_num) {
    if (MODE_V_ACTIVE_LINES >= TELETEXT_LINES) {
        return line_num - (MODE_V_ACTIVE_LINES - TELETEXT_LINES) / 2;
    } else {
        return line_num * MODE_V_ACTIVE_LINES / TELETEXT_LINES;
    }
}

// the 20 lines in the 2x3 graphics are laid out as follows
// 0 1 2 3 4 5 6 - 7 8 9 10 11 12 13 - 14 15 16 17 18 19
static inline uint16_t lookup_graphic(uint8_t c, int sub_row, bool separated) {
    uint16_t left, right;
    if (separated) {
        left = 0b0011110000000000;
        right = 0b0000000011110000;
        if (sub_row == 5 || sub_row == 6 || sub_row == 12 || sub_row == 13 ||
            sub_row == 18 || sub_row == 19) {
            return 0;
        }
    } else {
        left = 0b1111110000000000;
        right = 0b0000001111110000;
    }
    uint16_t retval = 0;
    if (sub_row < 7) {
        if (c & 0x01) {
            retval = left;
        };
        if (c & 0x02) {
            retval |= right;
        };
    } else if (sub_row > 13) {
        if (c & 0x10) {
            retval = left;
        };
        if (c & 0x40) {
            retval |= right;
        };
    } else {  // c >= 7 && c <= 13
        if (c & 0x04) {
            retval = left;
        };
        if (c & 0x08) {
            retval |= right;
        };
    }
    return retval;
}

static inline uint16_t lookup_character(uint8_t ch, const int sub_row,
                                        bool double_height,
                                        bool second_double) {
    ch = ch & 0x7F;
    if ((ch & 0b1100000) == 0b1000000) {
        ch = ch ^ 0b1000000;
    } else if ((ch & 0b1100000) == 0b1100000) {
        ch = ch ^ 0b100000;
    }

    const uint16_t* fontdata;

    if (double_height) {
        if (second_double) {
            fontdata = font + sub_row / 2 + 10;  // + (10 - 19)
        } else {
            fontdata = font + sub_row / 2;  // + (0..9)
        }
    } else {
        fontdata = font + sub_row;  // font + (0..19)
    }

    return fontdata[ch * FONT2_HEIGHT];
}

pixel_t* do_teletext(unsigned int line_num, pixel_t* p, bool is_debug) {
    static int next_double = -1;
    static int frame_count = 0;
    static bool flash_now = false;
    static const pixel_t colours[] = {AT_BLACK, AT_RED,     AT_GREEN, AT_YELLOW,
                                      AT_BLUE,  AT_MAGENTA, AT_CYAN,  AT_WHITE};

    pixel_t fg_colour = AT_WHITE;
    pixel_t bg_colour = AT_BLACK;

    bool graphics = false;
    bool flash_on = false;
    bool double_height = false;
    bool box = false;
    bool separated_graphics = false;
    bool hold_graphics = false;
    int last_graph = 0;

    // Screen is 25 rows x 40 columns
    int relative_line_num = teletext_line_no(line_num);
    if (relative_line_num < 0 || relative_line_num >= TELETEXT_LINES) {
        for (int i = 0; i < MODE_H_ACTIVE_PIXELS; i++) {
            *p++ = AT_BLACK;
        }
        return p;
    }

    p += (MODE_H_ACTIVE_PIXELS - TELETEXT_H_PIXELS) / 2;

    const int row = (relative_line_num) / TELETEXT_ROW_HEIGHT;  // character row
    const int sub_row =
        (relative_line_num) %
        TELETEXT_ROW_HEIGHT;  // row within current character (0..19)

    // start of frame initialisation
    if (relative_line_num == 0) {
        next_double = -1;
        // toggle the flash flag
        frame_count = (frame_count + 1) % 48;
        flash_now = frame_count < 16;
    };

    int ch_index = 0x8400 + row * TELETEXT_COLUMNS;

    for (int col = 0; col < TELETEXT_COLUMNS; col++) {
        int ch = eb_get(ch_index++) & 0x7F;

        // pre-render
        switch (ch) {
            case 12:  // normal height
                if (double_height) {
                    last_graph = 0;
                    double_height = false;
                }
                break;

            case 28:  // Black background
                bg_colour = AT_BLACK;
                break;

            case 29:  // New background colour
                bg_colour = fg_colour;
                break;

            case 30:  // Hold graphics
                hold_graphics = true;
                break;

            default:
                break;
        }

        bool non_printing = ((ch & 0x7F) < 0x20);
        uint16_t pix;
        if (non_printing) {
            if (is_debug) {
                int temp = ch & 0xF;
                if (temp <= 9) {
                    temp = temp + '0';
                } else {
                    temp = temp + 'a' - 10;
                }
                pix = lookup_character(temp, sub_row, false, false);
            } else {
                // non-printing char
                if (hold_graphics) {
                    pix = last_graph;
                } else {
                    pix = 0;
                }
            }
        } else {
            if (graphics) {
                if (ch >= 0x40 && ch < 0x60) {
                    pix = lookup_character(ch, sub_row, double_height,
                                           next_double == row);
                } else {
                    pix = lookup_graphic(ch, sub_row, separated_graphics);
                    last_graph = pix;
                }
            } else {
                pix = lookup_character(ch, sub_row, double_height,
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
            if (sub_row == 0 || sub_row == 19) {
                pix = 0xFFFF;
            }
            pix = pix | 0x8010;
            for (uint16_t mask = 0x8000; mask >= 0x10; mask = mask >> 1) {
                uint8_t c = (pix & mask) ? f : b;
                write_pixel(&p, c);
            }
        } else {
            if (flash_on && flash_now) {
                // handle flashing
                pix = 0;
            }

            for (uint16_t mask = 0x8000; mask >= 0x10; mask = mask >> 1) {
                uint8_t c = (pix & mask) ? fg_colour : bg_colour;
                write_pixel(&p, c);
            }
        }

        // post-render
        switch (ch) {
            case 1 ... 7:  // Alpha colour
                fg_colour = colours[ch & 7];
                if (graphics) {
                    last_graph = 0;
                    graphics = false;
                }
                break;
            case 8:  // flash
                flash_on = true;
                break;
            case 9:  // steady
                flash_on = false;
                break;
            case 10:  // end_box
                box = false;
                break;
            case 11:  // start_box
                box = true;
                break;
            case 12:  // normal height
                if (double_height) {
                    last_graph = 0;
                    double_height = false;
                }
                break;
            case 13:  // double height
                if (next_double != row) {
                    next_double = row + 1;
                }
                if (!double_height) {
                    last_graph = 0;
                    double_height = true;
                }
                break;
            case 17 ... 23:  // graphics colour
                fg_colour = colours[ch & 7];
                if (!graphics) {
                    last_graph = 0;
                    graphics = true;
                }
                break;
            case 24:  // Conceal display
                break;
            case 25:  // Contiguous graphics
                separated_graphics = false;
                break;
            case 26:  // Separated graphics
                separated_graphics = true;
                break;
            case 28:  // Black background
                bg_colour = AT_BLACK;
                break;
            case 29:  // New background colour
                bg_colour = fg_colour;
                break;
            case 30:  // Hold graphics
                hold_graphics = true;
                break;
            case 31:  // Release graphics
                hold_graphics = false;
                break;

            default:
                break;
        }
    }
    return p;
}

static inline uint16_t double_pixels(uint8_t src) {
    uint16_t dst = 0;
    if (src & 1) dst |= 3;
    if (src & 2) dst |= 3 << 2;
    if (src & 4) dst |= 3 << 4;
    if (src & 8) dst |= 3 << 6;
    if (src & 0x10) dst |= 3 << 8;
    if (src & 0x20) dst |= 3 << 10;
    if (src & 0x40) dst |= 3 << 12;
    if (src & 0x80) dst |= 3 << 14;
    return dst;
}

/// @brief Convert a single saa5050 font character to its rounded version.
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

void init_font() {
    uint16_t* dest_ptr = font;
    // convert 12x8 font to 20x16
    for (int ch = 0; ch < 0x60; ch += 1) {
        uint8_t* src = fontdata_saa5050 + ch * 12;
        dest_ptr = convert_char(dest_ptr, src);
    }
}

void teletext_init(void) {
    init_font();
    // for (int ch = 0; ch < 0x60; ch += 1) {
    //     printf("%x\n", ch);
    //     for (int r = 0; r < 20; r++) {
    //         putchar('|');
    //         uint16_t pix = font[ch * FONT2_HEIGHT + r];
    //         for (uint16_t mask = 0x8000; mask > 0; mask = mask >> 1) {
    //             putchar(pix & mask ? 'X' : '.');
    //         }
    //         putchar('|');
    //         putchar('\n');
    //     }
    // }
}
