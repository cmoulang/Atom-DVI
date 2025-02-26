#
#
# CMakeLists.txt
#
# Copyright 2021-2025 Chris Moulang
#
# This file is part of Atom-DVI
#
# Atom-DVI is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Atom-DVI is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# Atom-DVI. If not, see <https://www.gnu.org/licenses/>.
#


cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)
project(dvi_out_hstx_encoder_project)

pico_sdk_init()

add_executable(dvi_out_hstx_encoder
        main.c
        dvi_out_hstx_encoder.c
        atom_if.c
        mc6847.c
        teletext.c
        atom_sid.cc
        resid-0.16/envelope.cc
        resid-0.16/extfilt.cc
        resid-0.16/pot.cc
        resid-0.16/filter.cc
        resid-0.16/sid.cc
        resid-0.16/voice.cc
        resid-0.16/wave.cc
        resid-0.16/wave6581__ST.cc
        resid-0.16/wave6581_P_T.cc
        resid-0.16/wave6581_PS_.cc
        resid-0.16/wave6581_PST.cc
        resid-0.16/wave8580__ST.cc
        resid-0.16/wave8580_P_T.cc
        resid-0.16/wave8580_PS_.cc
        resid-0.16/wave8580_PST.cc      
        )

target_compile_definitions(dvi_out_hstx_encoder PRIVATE
        # See videomode.h for MODE values
        TELETEXT=1
        # MODE=MODE_720x576_60
        # MODE=MODE_640x480_60_FAST
        MODE=MODE_640x480_60
)

pico_generate_pio_header(dvi_out_hstx_encoder ${CMAKE_CURRENT_LIST_DIR}/sm.pio)

target_include_directories(dvi_out_hstx_encoder PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/images
        )

# pull in common dependencies
target_link_libraries(dvi_out_hstx_encoder
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_dma
        hardware_pwm
        pico_sync
        )

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dvi_out_hstx_encoder)

pico_enable_stdio_uart(dvi_out_hstx_encoder 1)
pico_set_linker_script(dvi_out_hstx_encoder ${CMAKE_CURRENT_LIST_DIR}/memmap_default.ld)

