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
project(atom_dvi_project)

pico_sdk_init()


add_custom_command(
    OUTPUT ${CMAKE_CURRENT_LIST_DIR}/asm/test.h
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/asm/make.bat
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/asm/test.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/asm
    COMMENT "Generating test.h from test.s"
    VERBATIM
)

add_custom_target(generate_test_h
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/asm/test.h
    COMMENT "Generating test.h"
)

add_executable(atom_dvi
        main.c
        bench.c
        dvi_out_hstx_encoder_mod.c
        atom_if.c
        mc6847.c
        teletext.c
        atom_sid.cc
        asm.c
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
add_dependencies(atom_dvi generate_test_h)


target_compile_definitions(atom_dvi PRIVATE
        # See videomode.h for MODE values
        # TELETEXT=1
        # MODE=MODE_720x576_60
        MODE=MODE_640x480_60_FAST
        # MODE=MODE_640x480_60
        RESET=0
        VDU_RAM=0
)

pico_set_binary_type(atom_dvi copy_to_ram)


pico_generate_pio_header(atom_dvi ${CMAKE_CURRENT_LIST_DIR}/sm.pio)

target_include_directories(atom_dvi PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/images
        ${CMAKE_CURRENT_LIST_DIR}/asm
        )

# pull in common dependencies
target_link_libraries(atom_dvi
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_dma
        hardware_pwm
        pico_sync
        )

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(atom_dvi)

pico_enable_stdio_uart(atom_dvi 1)
pico_set_linker_script(atom_dvi ${CMAKE_CURRENT_LIST_DIR}/memmap_copy_to_ram.ld)
