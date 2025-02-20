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

#include "videomode.h"

#define TELETEXT_LINES 500
#define TELETEXT_ROW_HEIGHT 20
#define TELETEXT_COLUMNS 40
#define TELETEXT_H_PIXELS (TELETEXT_COLUMNS * 12)

pixel_t* do_teletext(unsigned int line_num, pixel_t* p, bool is_debug);
void teletext_init(void);
