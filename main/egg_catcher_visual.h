#pragma once

#include <stdint.h>

#define EGG_CATCH_VISIBLE_MS 90U
#define EGG_CATCH_SINK_MS    90U
#define EGG_CATCH_SINK_PX    4

typedef enum {
    EGG_CATCH_VISUAL_FRONT,
    EGG_CATCH_VISUAL_SINK,
    EGG_CATCH_VISUAL_HIDDEN,
} egg_catch_visual_phase_t;

typedef struct {
    egg_catch_visual_phase_t phase;
    int8_t sink_pixels;
} egg_catch_visual_t;

egg_catch_visual_t egg_catcher_visual_at(uint32_t elapsed_ms);
