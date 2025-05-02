/*

Interface to resid emulation

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
Atom-DVI. If not, see <https://www.gnu.org/licenses/>.

*/

#include "atom_sid.h"
#include "resid-0.16/sid.h"
#include "hardware/pwm.h"
#include <hardware/clocks.h>
#include <math.h>
#include <stdio.h>
#include "ui.h"
#include "teletext.h"

#define C64_CLOCK 1000000
#define AS_SAMPLE_RATE 50000
#define AS_TICK_US (C64_CLOCK / AS_SAMPLE_RATE)
#define AS_PWM_BITS 11
#define AS_PWM_WRAP (1 << AS_PWM_BITS)

int as_count = 0;

extern "C" void as_show_status()
{
    //    printf("%d %d\n", in_count, out_count);
    for (int i = 0; i < SID_LEN; i++)
    {
        printf("%02x ", eb_get(SID_BASE_ADDR + i));
        if (((i + 1) % 7) == 0)
        {
            puts("");
        }
    }
    puts("");
}

SID *sid16 = NULL;
queue_t as_q;

static void init_dac()
{
    gpio_init(AS_PIN);
    gpio_set_dir(AS_PIN, GPIO_OUT);
    gpio_set_function(AS_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AS_PIN);
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv(&c, 1);
    pwm_config_set_phase_correct(&c, false);
    pwm_config_set_wrap(&c, AS_PWM_WRAP);
    pwm_init(audio_pin_slice, &c, true);
    pwm_set_gpio_level(AS_PIN, 0);
    gpio_set_drive_strength(AS_PIN, GPIO_DRIVE_STRENGTH_12MA);
    pwm_set_enabled(audio_pin_slice, true);
}

extern "C" void __no_inline_not_in_flash_func(sid_event_handler)() {
    dma_hw->intr = 1u << eb_get_event_chan();
    int address = eb_get_event();
    while (address > 0) {
        int ad65 = eb_6502_addr(address);
        if (ad65 >= SID_BASE_ADDR && ad65 < (SID_BASE_ADDR + SID_WRITEABLE))
        {
            as_sid_write(address);
        } else if (ad65 == TELETEXT_CRTA || ad65 == TELETEXT_CRTB) {
            teletext_reg_write(ad65, eb_get(ad65));
        } 
        address = eb_get_event();
    }
}

extern "C" void as_init()
{
    int spinlock = spin_lock_claim_unused(true);
    queue_init_with_spinlock(&as_q, sizeof(as_element_t), AS_Q_LENGTH, spinlock);

    int rate = AS_SAMPLE_RATE;
    int interval = AS_TICK_US;
    puts("INIT SID CALLED - interrupt driven version " __DATE__ " " __TIME__);
    printf("Sample rate: %d/s\n", rate);
    printf("Sample interval: %dus\n", interval);

    sid16 = new SID();
    sid16->set_chip_model(MOS8580);
    // sid16->set_chip_model(MOS6581);
    sid16->reset();
    // bool ok = sid16->set_sampling_parameters(C64_CLOCK, SAMPLE_INTERPOLATE, AS_SAMPLE_RATE);
    bool ok = sid16->set_sampling_parameters(C64_CLOCK, SAMPLE_FAST, AS_SAMPLE_RATE);
    hard_assert(ok);
    sid16->enable_filter(true);
    sid16->enable_external_filter(true);

    sid16->input(0);
    for (int i = 0; i < SID_LEN; i++)
    {
        sid16->write(i, 0);
    }

    init_dac();

    eb_set_perm(SID_BASE_ADDR, EB_PERM_WRITE_ONLY, SID_WRITEABLE);
    eb_set_perm(SID_BASE_ADDR + SID_WRITEABLE, EB_PERM_READ_ONLY, 4);
    as_update_reg(0x19, 0xFF);
    as_update_reg(0x1A, 0xFF);
}

#ifdef DEBUG_SID_DATA
int debug_count = 0;
uint16_t debug_buf[500];
#endif

static bool __time_critical_func(as_timer_callback)(repeating_timer_t *)
{
    // Output current sample
    int sample = sid16->output(AS_PWM_BITS);
    sample = sample + (1 << (AS_PWM_BITS - 1));
    pwm_set_gpio_level(AS_PIN, sample);
    as_element_t el;
    while (queue_try_remove(&as_q, &el))
    {
        if (el.address == 0)
        {
            for (int i = 0; i < SID_LEN; i++)
            {
                sid16->write(i, 0);
            }
        }
        else
        {
            uint8_t data = el.data;
            uint8_t reg = eb_6502_addr(el.address) & 0x1F;

            sid16->write(reg, data);
        }
    }
    sid16->clock(AS_TICK_US);
    // Update the read-only SID regs
    as_update_reg(0x1B, sid16->read(0x1B));
    as_update_reg(0x1C, sid16->read(0x1C));    return true;
}

extern "C" void as_run()
{
    printf("as_run()\n");
    eb_set_exclusive_handler(sid_event_handler);

    uint32_t last_time = 0;
    for (;;)
    {
        uint32_t cur_time = time_us_32();
        while (cur_time == last_time || cur_time % AS_TICK_US)
        {
            cur_time = time_us_32();
        }
        as_timer_callback(NULL);
        ui_event_t uev;
        while (ui_get_event(&uev)) {

            if (uev.type == DOUBLE_BREAK) {
                printf("DOUBLE BREAK\n");
                ui_run();
            } else if (uev.type == BREAK) {
                printf("BREAK\n");
            } else if (uev.type == KEY_PRESS) {
                printf("KEY PRESS %x\n", uev.key);
            }
        }
    }
}

repeating_timer_t as_timer;

extern "C" void as_run_async()
{
    printf("as_run_async()\n");
    eb_set_exclusive_handler(sid_event_handler);

    bool ok = add_repeating_timer_us(-(int64_t)AS_TICK_US, as_timer_callback, NULL, &as_timer);
    hard_assert(ok);
}