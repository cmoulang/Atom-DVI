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

#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "videomode.h"

enum  saa5050_ctrl {
    _NUL = 0,
    ALPHA_RED = 1,
    ALPHA_GREEN = 2,
    ALPHA_YELLOW = 3,
    ALPHA_BLUE = 4,
    ALPHA_MAGENTA = 5,
    ALPHA_CYAN = 6,
    ALPHA_WHITE = 7,
    FLASH = 8,
    STEADY = 9,
    END_BOX = 10,
    START_BOX = 11,
    NORMAL_HEIGHT = 12,
    DOUBLE_HEIGHT = 13,
    _S0 = 14,
    _S1 = 15,
    _DLE = 16,
    GRAPHICS_RED = 17,
    GRAPHICS_GREEN = 18,
    GRAPHICS_YELLOW = 19,
    GRAPHICS_BLUE = 20,
    GRAPHICS_MAGENTA = 21,
    GRAPHICS_CYAN = 22,
    GRAPHICS_WHITE = 23,
    CONCEAL_DISPLAY = 24,
    CONTIGUOUS_GRAPHICS = 25,
    SEPARATED_GRAPHICS = 26,
    _ESC = 27,
    BLACK_BACKGROUND = 28,
    NEW_BACKGROUND = 29,
    HOLD_GRAPHICS = 30,
    RELEASE_GRAPHICS = 31
} ;

#define TELETEXT_LINES 500
#define TELETEXT_ROW_HEIGHT 20
#define TELETEXT_COLUMNS 40
#define TELETEXT_ROWS 25
#define TELETEXT_H_PIXELS (TELETEXT_COLUMNS * 12)
#define TELETEXT_PAGE_BUFFER 0x8000
#define TELETEXT_CRTA 0x8400
#define TELETEXT_CRTB 0x8401
#define TELETEXT_REG_BASE (TELETEXT_PAGE_BUFFER + TELETEXT_COLUMNS * TELETEXT_ROWS)
#define TELETEXT_REG_FLAGS (TELETEXT_REG_BASE + 8)

/*
Register
Address	Function	ProgramUnit	Type	No of
Bits Used
Add Function                Unit        Type        Bits
00	Horizontal total	    char	    Write only	8
01	Horizontal displayed	"	        "	        8  
02	H Sync Position	        "	        "	        8
03	H Sync Width	        "	        "	        4
04	Vertical total	        char row	"	        7
05	V total adjust	        scan line	"	        5
06	Vertical displayed	    char row	"	        7
07	V sync position	        char row	"	        7
08	Interlace mode	        -	        "	        2
09	Max scan line address	scan line	"	        5
0A	Cursor start	        scan line	"	        5+2
0B	Cursor end	            scan line	"	        5
0C	Start address H	        -	        "	        6
0D	Start address L	        -	        "	        8
0E	Cursor H	            -	        Read/Write	6
0F	Cursor L	            -	        Read/Write	8
10	Light pen H	            -	        Read only	6
11	Light pen L	            -	        "	        8
*/

enum teletext_reg {
    TELETEXT_REG_H_TOTAL = 0,
    TELETEXT_REG_H_DISPLAYED,
    TELETEXT_REG_H_SYNC_POS,
    TELETEXT_REG_H_SYNC_WIDTH,
    TELETEXT_REG_V_TOTAL,
    TELETEXT_REG_V_TOTAL_ADJUST,
    TELETEXT_REG_V_DISPLAYED,
    TELETEXT_REG_V_SYNC_POS,
    TELETEXT_REG_INTERLACE,
    TELETEXT_REG_MAX_SCAN_LINE,
    TELETEXT_REG_CURSOR_START,
    TELETEXT_REG_CURSOR_END,
    TELETEXT_REG_START_ADDR_H,
    TELETEXT_REG_START_ADDR_L,
    TELETEXT_REG_CURSOR_H,
    TELETEXT_REG_CURSOR_L,
    TELETEXT_REG_LIGHT_PEN_H,
    TELETEXT_REG_LIGHT_PEN_L,
    TELETEXT_REG_DEBUG,
    TELETEXT_REG_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

pixel_t* do_teletext(pixel_t* p, size_t len, unsigned int line_num, unsigned char flags);
void teletext_init(void);
void teletext_reg_write(int reg, unsigned char val);
#ifdef __cplusplus
}
#endif
