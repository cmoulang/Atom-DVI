/*

Define constants for different video modes

Copyright 2021-2025 Chris Moulang

This file is part of Atom-DVI

Atom-DVI is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Atom-DVI is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
AtomHDMI. If not, see <https://www.gnu.org/licenses/>.

*/


#pragma once

#define MODE_800x600_60 1
#define MODE_720x576_60 2
#define MODE_640x480_60 3

// MODE is defined in CMakeLists.txt

#if (MODE == MODE_800x600_60)
#define MODE_H_SYNC_POLARITY 1
#define MODE_H_FRONT_PORCH 40
#define MODE_H_SYNC_WIDTH 128
#define MODE_H_BACK_PORCH 88
#define MODE_H_ACTIVE_PIXELS 800

#define MODE_V_SYNC_POLARITY 1
#define MODE_V_FRONT_PORCH 1
#define MODE_V_SYNC_WIDTH 4
#define MODE_V_BACK_PORCH 23
#define MODE_V_ACTIVE_LINES 600

#define XSCALE 2
#define YSCALE 2

#define REQUIRED_SYS_CLK_KHZ 200000
#define HSTX_CLK_KHZ 200000

#elif (MODE == MODE_720x576_60)
#define MODE_H_SYNC_POLARITY 1
#define MODE_H_FRONT_PORCH 48
#define MODE_H_SYNC_WIDTH 32
#define MODE_H_BACK_PORCH 80
#define MODE_H_ACTIVE_PIXELS 720

#define MODE_V_SYNC_POLARITY 0
#define MODE_V_FRONT_PORCH 3
#define MODE_V_SYNC_WIDTH 7
#define MODE_V_BACK_PORCH 7
#define MODE_V_ACTIVE_LINES 576

#define XSCALE 2
#define YSCALE 2

#define REQUIRED_SYS_CLK_KHZ 156000
#define HSTX_CLK_KHZ 156000

#elif (MODE == MODE_640x480_60)
#define MODE_H_SYNC_POLARITY 0
#define MODE_H_FRONT_PORCH 16
#define MODE_H_SYNC_WIDTH 96
#define MODE_H_BACK_PORCH 48
#define MODE_H_ACTIVE_PIXELS 640

#define MODE_V_SYNC_POLARITY 0
#define MODE_V_FRONT_PORCH 10
#define MODE_V_SYNC_WIDTH 2
#define MODE_V_BACK_PORCH 33
#define MODE_V_ACTIVE_LINES 480

#define XSCALE 2
#define YSCALE 2

#define REQUIRED_SYS_CLK_KHZ 126000
#define HSTX_CLK_KHZ 126000

#else
#error "Bad MODE"

#endif
