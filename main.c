/*

Main program.

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "atom_if.h"
#include "atom_sid.h"
#include "mc6847.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "videomode.h"

void hstx_main(void);
#define DMACH_PING 0
#define DMACH_PONG 1

#define PIN_NRST 22
#define PIN_VSYNC 20

static semaphore_t core1_initted;

/// @brief emit diagnostic tones from the pwm-audio pin
/// @param args list of frquencies to emit termiated with -1
void beep(int args, ...) {
    va_list ap;

    gpio_init(AS_PIN);
    gpio_set_dir(AS_PIN, true);

    va_start(ap, args);
    int pitch = args;
    while (pitch > 0) {
        int delay_us = 500000 / pitch;
        for (int i = 0; i < 50; i++) {
            sleep_us(delay_us);
            gpio_put(AS_PIN, 0);
            sleep_us(delay_us);
            gpio_put(AS_PIN, 1);
        }
        pitch = va_arg(ap, int);
    }
    va_end(ap);
}

/// @brief print some clock diagnostics
void measure_freqs(void) {
    uint f_pll_sys =
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb =
        frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_hstx = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_HSTX);

    printf("pll_sys  = %dkHz\n", f_pll_sys);
    // printf("pll_usb  = %dkHz\n", f_pll_usb);
    // printf("rosc     = %dkHz\n", f_rosc);
    printf("clk_sys  = %dkHz\n", f_clk_sys);
    // printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb  = %dkHz\n", f_clk_usb);
    // printf("clk_adc  = %dkHz\n", f_clk_adc);
    printf("clk_hstx  = %dkHz\n", f_clk_hstx);
    // Can't measure clk_ref / xosc as it is the ref
}

void __no_inline_not_in_flash_func(gpio_callback)(uint gpio, uint32_t events) {
    // gpio_acknowledge_irq(gpio, events);
    if (gpio == PIN_NRST) {
        mc6847_reset();
        as_reset();
    } else if (gpio == PIN_VSYNC) {
        mc6847_vsync();
    }
}

/// @brief
void core1_func() {
    // run sid on this core
    as_init();
    mc6847_init();
    benchmark_draw_line();

    // setup interrupt handler for NRST and VSYNC
    gpio_set_irq_callback(gpio_callback);
    gpio_set_irq_enabled(PIN_VSYNC, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(PIN_NRST, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    sem_release(&core1_initted);

#ifdef TELETEXT                
    mc6847_run();
#else
    as_run();
#endif
    // as_run_async();
    // mc6847_run();


    while (1) {
        __wfi();
    }
}

int main(void) {
    beep(250, 500, -1);

    // Set custom clock speeds
    if (SYS_CLK_KHZ != REQUIRED_SYS_CLK_KHZ) {
        set_sys_clock_khz(REQUIRED_SYS_CLK_KHZ, true);
    }

    if (SYS_CLK_KHZ != HSTX_CLK_KHZ) {
        bool clock_ok = clock_configure(
            clk_hstx, 0, CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
            REQUIRED_SYS_CLK_KHZ, HSTX_CLK_KHZ);
        hard_assert(clock_ok);
    }

    stdio_uart_init();
    printf("Atom DVI v0.0.3-beta\n");
    measure_freqs();

    // initialise the shadow memory
    for (int i = 0; i < EB_BUFFER_LENGTH; i++) {
        _eb_memory[i] = 0;
    }

    // Reserve DMA channels for hstx use
    dma_claim_mask((1u << DMACH_PING) | (1u << DMACH_PONG));

    // init GPIO
    gpio_init(PIN_VSYNC);
    gpio_set_dir(PIN_VSYNC, false);
    gpio_init(PIN_NRST);
    gpio_put(PIN_NRST, false);

    // create a semaphore to be posted when initialisation is complete
    sem_init(&core1_initted, 0, 1);

    multicore_launch_core1(core1_func);
    sem_acquire_blocking(&core1_initted);

    hstx_main();

    // toggle the 6502 reset pin
    gpio_set_dir(PIN_NRST, true);
    busy_wait_ms(10);
    gpio_set_dir(PIN_NRST, false);


    mc6847_run();
    while (1) {
        __wfi();
    }
}
