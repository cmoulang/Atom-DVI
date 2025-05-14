/*

User interface

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

#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "atom_if.h"
#include "capture.h"
#include "licence.h"
#include "mc6847.h"
#include "pico/util/queue.h"

#define DOUBLE_CLICK_TIME 500

static absolute_time_t double_click_timeout;
static queue_t ui_q;

void ui_init() {
    queue_init(&ui_q, sizeof(ui_event_t), UI_Q_LENGTH);
    double_click_timeout = nil_time;
}

bool ui_post_event(ui_event_enum type, char key) {
    ui_event_t event;
    event.type = type;
    event.key = key;
    return queue_try_add(&ui_q, &event);
}

// #define LOWER_RIGHT_CORNER 0x79
// #define UPPER_RIGHT_CORNER 0x62
// #define UPPER_LEFT_CORNER 0x7A
// #define LOWER_LEFT_CORNER 0x60
// #define VERTICAL_LINE 0x66
// #define HORIZONTAL_LINE 0x64
#define LOWER_RIGHT_CORNER '+'
#define UPPER_RIGHT_CORNER '+'
#define UPPER_LEFT_CORNER '+'
#define LOWER_LEFT_CORNER '+'
#define VERTICAL_LINE ':'
#define HORIZONTAL_LINE '-'
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 40

/// @brief output a graphics char at the specified location
/// @param row
/// @param col
/// @param c
static void putgr(int row, int col, char c) {
    eb_set(0x8000 + row * 80 + col, c);
}

static void draw_box(int x, int y, int width, int height) {
    putgr(y, x, UPPER_LEFT_CORNER);
    putgr(y, x + width, UPPER_RIGHT_CORNER);
    putgr(y + height, x, LOWER_LEFT_CORNER);
    putgr(y + height, x + width, LOWER_RIGHT_CORNER);
    for (int i = 1; i < width; i++) {
        putgr(y, x + i, HORIZONTAL_LINE);
        putgr(y + height, x + i, HORIZONTAL_LINE);
    }
    for (int i = 1; i < height; i++) {
        putgr(y + i, x, VERTICAL_LINE);
        putgr(y + i, x + width, VERTICAL_LINE);
    }
}

void mui_left_justified_msgbox(char* str, int height, int width) {
    assert(str);
    assert(height > 0);
    assert(width > 0);
    str = strdup(str);
    int left_margin = (SCREEN_WIDTH - width) / 2;
    int top_margin = (SCREEN_HEIGHT - height) / 2;
    draw_box(left_margin, top_margin, width, height);
    int x = left_margin + 1;
    int y = top_margin + 1;

    mc6847_moveto(x, y);
    char* rest = str;
    char* token;
    while ((token = strtok_r(rest, " ", &rest))) {
        int len = strlen(token);
        if (x + len > left_margin + width) {
            x = left_margin + 1;
            y++;
        }
        mc6847_moveto(x, y);
        while (*token) {
            if (*token == '\n') {
                x = left_margin + 1;
                y++;
                mc6847_moveto(x, y);
            } else {
                x += 1;
                mc6847_outc(*token);
            }
            token++;
        }
        x += 1;
    }
    free(str);
}

static void display_licence() {
    mc6847_vga_mode();
    mc6847_print("\f");
    mui_left_justified_msgbox(gpl3_notice, 21, 65);

    ui_event_t event;
    while (1) {
        queue_remove_blocking(&ui_q, &event);
        if (event.type == BREAK_KEY_DOWN) {
            mc6847_print("\f");
        } else if (event.type == BREAK_KEY_UP) {
            return;
        } else if (event.type == KEY_PRESS) {
            printf("key press %x\n", event.key);
        }
    }
}

void ui_run() {
    ui_event_t event;
    while (queue_try_remove(&ui_q, &event)) {
        if (event.type == BREAK_KEY_UP) {
            if (get_absolute_time() < double_click_timeout) {
                display_licence();
                double_click_timeout = nil_time;
                return;
            } else {
                double_click_timeout = make_timeout_time_ms(DOUBLE_CLICK_TIME);
            }
        } else if (event.type == KEY_PRESS) {
            printf("key press %x\n", event.key);
        } else if (event.type == CAPTURE_KEY_DOWN) {
            capture();
        }
    }
    capture_task();
}
