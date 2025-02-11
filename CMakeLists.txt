cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)
project(dvi_out_hstx_encoder_project)

pico_sdk_init()

add_executable(dvi_out_hstx_encoder
        dvi_out_hstx_encoder.c
        atom_if.c
        mc6847.c
        )

target_compile_definitions(dvi_out_hstx_encoder PRIVATE
        # See videomode.h for MODE values
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
        pico_sync
        )

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dvi_out_hstx_encoder)

pico_enable_stdio_uart(dvi_out_hstx_encoder 1)
pico_set_linker_script(dvi_out_hstx_encoder ${CMAKE_CURRENT_LIST_DIR}/memmap_default.ld)

