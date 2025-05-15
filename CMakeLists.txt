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


set(TOP ${PICO_SDK_PATH}/lib/tinyusb)
message(STATUS "TOP is set to ${TOP}")


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

set(SOURCE_FILES
        asm.c
        atom_if.c
        atom_sid.cc
        bench.c
        capture.c
        dvi_out_hstx_encoder_mod.c
        main.c
        mc6847.c
        msc_app.c
        teletext.c
        ui.c
        ${TOP}/lib/fatfs/source/ff.c
        ${TOP}/lib/fatfs/source/ffsystem.c
        ${TOP}/lib/fatfs/source/ffunicode.c
)

set(INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/asm
        ${TOP}/lib/fatfs/source/
)

include(resid-0.16/Library.cmake)
include(lpng/Library.cmake)
include(zlib/Library.cmake)

add_executable(atom_dvi
        ${SOURCE_FILES}
)

add_dependencies(atom_dvi generate_test_h)

target_compile_definitions(atom_dvi PRIVATE
        # See videomode.h for MODE values
        # TELETEXT=1
        # MODE=MODE_720x576_60
        # MODE=MODE_640x480_60_FAST_DST
        MODE=MODE_640x480_60_FAST
        # MODE=MODE_640x480_60
        RESET=0
        VDU_RAM=1
        )

pico_set_binary_type(atom_dvi copy_to_ram)


pico_generate_pio_header(atom_dvi ${CMAKE_CURRENT_LIST_DIR}/sm.pio)

target_include_directories(atom_dvi PRIVATE
        ${INCLUDE_DIRS}
        )


# pull in common dependencies
target_link_libraries(atom_dvi
        tinyusb_host
        tinyusb_board
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
