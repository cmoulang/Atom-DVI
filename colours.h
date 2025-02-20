/*

Define some colours

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
