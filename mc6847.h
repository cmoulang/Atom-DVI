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

#define FB_ADDR 0x8000
#define VID_MEM_SIZE 0x2000

void mc6847_init();

void mc6847_run();
