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

#pragma once

typedef unsigned char pixel_t;
typedef unsigned short pixel2_t;
static const int pixel_bits = 8;

void mc6847_init();
void mc6847_run();
char* getLine(int line_num);