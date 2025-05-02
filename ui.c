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

static queue_t ui_q;

void ui_init() {
    queue_init(&ui_q, sizeof(ui_event_t), UI_Q_LENGTH);
}

bool ui_post_event(ui_event_enum type, char key) {
    ui_event_t event;
    event.type = type;
    event.key = key;
    return queue_try_add(&ui_q, &event);
}

bool ui_get_event(ui_event_t *event) {
    return queue_try_remove(&ui_q, event);
}

void ui_run() {
    ui_event_t event;
    while (1) {
        if (ui_get_event(&event)) {
            switch (event.type) {
                case KEY_PRESS:
                    printf("key=%x\n", event.key);
                    break;

                case BREAK:
                    printf("break\n");
                    return;

                case DOUBLE_BREAK:
                    printf("double break\n");
                    break;

                default:
                    printf("unknown event\n");
                    break;
            }
        }
        __wfi;
    }
}
        




