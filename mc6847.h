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

#include "videomode.h"
#include <stdint.h>
#include <stdbool.h>


/// @brief initialise
void mc6847_init(bool vdu_ram_enabled, bool emulate_reset);

/// @brief run the emulation (does not return)
void mc6847_run();

/// @brief reset the mc6847 mode
void mc6847_reset();

/// @brief external vsync signal
void mc6847_vsync();

/// @brief get a display line of pixels
/// @param line_num the line number
/// @return pointer to a buffer containg the pixels
pixel_t* mc6847_get_line_buffer(int line_num);

void draw_line(int line_num, int mode, int atom_fb, int border_colour,
        unsigned char* p);