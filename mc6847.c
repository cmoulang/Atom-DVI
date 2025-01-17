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

#include "atom_if.h"
#include "mc6847.h"

void __no_inline_not_in_flash_func(event_handler)() {
    static uint8_t lastpia = 0;
    dma_hw->ints1 = 1u << eb_get_event_chan();
    int address = eb_get_event();
    while (address > 0) {
        uint16_t x = eb_6502_addr(address);
        address = eb_get_event();
    }
}

void mc6847_init() {
    eb_set_perm(EB_ADDRESS_LOW, EB_PERM_NONE, EB_ADDRESS_HIGH - EB_ADDRESS_LOW);
    eb_set_perm(FB_ADDR, EB_PERM_WRITE_ONLY, VID_MEM_SIZE);
    eb_set_perm_byte(PIA_ADDR, EB_PERM_WRITE_ONLY);
    eb_set_perm_byte(PIA_ADDR + 2, EB_PERM_WRITE_ONLY);

    eb_init(pio1);
    eb_set_exclusive_handler(event_handler);
}

void mc6847_run() {
}
