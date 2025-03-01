#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "mc6847.h"
#include "pico/stdlib.h"
#include "videomode.h"
#include "atom_if.h"
#include <stdlib.h>

void draw_line(int line_num, int mode, int atom_fb, int border_colour, uint8_t* p);

// Function to benchmark draw_line
void benchmark_draw_line_2(int mode, int atom_fb) {
    int border_colour = 42;  // Example colour

    pixel_t line_buffer[MODE_H_ACTIVE_PIXELS];

    const int num_iterations = MODE_V_ACTIVE_LINES * 100;
    uint64_t start_time, end_time;
    uint64_t total_time = 0;

    printf("Benchmarking draw_line...\n");

    for (int i = 0; i < num_iterations; i++) {
        start_time = time_us_64();
        draw_line(i % MODE_V_ACTIVE_LINES, mode, atom_fb, border_colour, 
                  line_buffer);
        end_time = time_us_64();
        total_time += (end_time - start_time);
    }

    printf("Total time for mode %d\n %d frames: %llu us\n", mode, num_iterations / MODE_V_ACTIVE_LINES,
           total_time);

    printf("Frames per second: %llu\n", (1000000ll * num_iterations / MODE_V_ACTIVE_LINES) / total_time);
}

pixel_t* do_teletext(unsigned int line_num, pixel_t* p, unsigned char flags);


void benchmark_teletext() {
    pixel_t line_buffer[MODE_H_ACTIVE_PIXELS];

    const int num_iterations = MODE_V_ACTIVE_LINES * 100;
    uint64_t start_time, end_time;
    uint64_t total_time = 0;

    printf("Benchmarking do_teletext...\n");

    for (int i = 0; i < num_iterations; i++) {
        start_time = time_us_64();
        do_teletext(i % MODE_V_ACTIVE_LINES, line_buffer, 0);
        end_time = time_us_64();
        total_time += (end_time - start_time);
    }

    printf("Total time for teletext \n %d frames: %llu us\n", num_iterations / MODE_V_ACTIVE_LINES,
           total_time);

    printf("Frames per second: %llu\n", (1000000ll * num_iterations / MODE_V_ACTIVE_LINES) / total_time);
}

void benchmark_draw_line() {
    int atom_fb = 0x8000;
    for (size_t i = 0; i < 0x400*6; i++)
    {
        eb_set(atom_fb+i, rand() %256) ;
    }
    for (size_t i = 0; i < 1000; i++)
    {
        eb_set(0x9800+i, i%256) ;
    }
    benchmark_teletext();
    for (int mode=0; mode<16; mode++) {
        benchmark_draw_line_2(mode, atom_fb);
    }
}


