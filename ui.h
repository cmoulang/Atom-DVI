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
#pragma once

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define UI_Q_LENGTH 32

typedef enum {
    KEY_PRESS,
    BREAK,
    DOUBLE_BREAK,
} ui_event_enum;

struct ui_event {
    ui_event_enum type;
    char key;
};

typedef struct ui_event ui_event_t;

#ifdef __cplusplus
extern "C" {
#endif

bool ui_post_event(ui_event_enum type, char key);
bool ui_get_event(ui_event_t *type);
void ui_init();
void ui_run();

#ifdef __cplusplus
}
#endif
