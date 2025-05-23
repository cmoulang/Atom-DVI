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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "videomode.h"

/// @brief initialise
void mc6847_init(bool vdu_ram_enabled, bool emulate_reset);

/// @brief run the emulation (does not return)
void mc6847_run();

/// @brief reset the mc6847 mode
void mc6847_reset();

/// @brief set into 80 column vga mode
void mc6847_vga_mode();

/// @brief print an acsii string to the VDU 
/// @param str 
void mc6847_print(const char* str);

void mc6847_outc(char c);

/// @brief move cursor to column x, row y
/// @param x 
/// @param y 
void mc6847_moveto(int x, int y);

/// @brief get a display line of pixels
/// @param line_num the line number
/// @return pointer to a buffer containg the pixels
pixel_t* mc6847_get_line_buffer(int line_num);

void draw_line(int line_num, int mode, int atom_fb, unsigned char* p);

#define PIN_VSYNC 20