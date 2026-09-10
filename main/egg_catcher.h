#pragma once

#include <stdbool.h>

#include "bsp_button.h"

void egg_catcher_enter(bool buttons_available, bool battery_available,
                       bool audio_available);
void egg_catcher_exit(void);
void egg_catcher_key(bsp_btn_t btn, bsp_btn_ev_t event);
