/**
 * A simple hello world program
 */

#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../fonts.h"
#define FONT_CHARS 0x60
#define FONT2_HEIGHT 20

static uint16_t font[FONT_CHARS * FONT2_HEIGHT];

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

/*

A B C --\ 1 2
D E F --/ 3 4

1 = B | (A & E & !B & !D)
2 = B | (C & E & !B & !F)
3 = E | (!A & !E & B & D)
4 = E | (!C & !E & B & F)

*/

static uint16_t* convert_char(uint16_t* dest_ptr, uint8_t* src_ptr) {
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

            dst1 = dst1 | (r1 ? 2 : 0);
            dst1 = dst1 | (r2 ? 1 : 0);

            dst2 = dst2 | (r3 ? 2 : 0);
            dst2 = dst2 | (r4 ? 1 : 0);

            src1 = src1 << 1;
            src2 = src2 << 1;

            dst1 = dst1 << 2;
            dst2 = dst2 << 2;
        }

        *dest_ptr++ = dst1;
        *dest_ptr++ = dst2;
    }
    return dest_ptr;

    for (int i = 1; i < 11; i++) {
        uint8_t src = src_ptr[i];
        uint16_t dst = double_pixels(src);
        *dest_ptr++ = dst;
        *dest_ptr++ = dst;
    }
    return dest_ptr;
}

void init_font() {
    uint16_t* dest_ptr = font;
    for (int ch = 0; ch < FONT_CHARS; ch += 1) {
        uint8_t* src = fontdata_saa5050_b + ch * 12;
        dest_ptr = convert_char(dest_ptr, src);
    }
}

void print_pixels(uint16_t p) {
    for (uint16_t m = 0x8000; m > 0; m = m >> 1) {
        if (m & p) {
            putchar('#');
        } else {
            putchar(' ');
        }
    }
}

void print_font() {
    for (int r = 0; r < 16; r++) {
        for (int col = 0; col < 6; col++) {
            int c = col * 16 + r;
            printf("// --- %2x ---   ", c);
        }
        putchar('\n');

        for (int i = 0; i < FONT2_HEIGHT; i++) {
            for (int col = 0; col < 6; col++) {
                int c = col * 16 + r;
                print_pixels(font[c * FONT2_HEIGHT + i]);
            }
            putchar('\n');

        }
        putchar('\n');
    }
}

int main(int argc, const char** argv) {
    init_font();
    print_font();
}