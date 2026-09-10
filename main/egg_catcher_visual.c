#include "egg_catcher_visual.h"

egg_catch_visual_t egg_catcher_visual_at(uint32_t elapsed_ms)
{
    egg_catch_visual_t visual = {
        .phase = EGG_CATCH_VISUAL_FRONT,
        .sink_pixels = 0,
    };

    if (elapsed_ms < EGG_CATCH_VISIBLE_MS) return visual;

    uint32_t sink_elapsed = elapsed_ms - EGG_CATCH_VISIBLE_MS;
    if (sink_elapsed >= EGG_CATCH_SINK_MS) {
        visual.phase = EGG_CATCH_VISUAL_HIDDEN;
        return visual;
    }

    visual.phase = EGG_CATCH_VISUAL_SINK;
    if (sink_elapsed > 0U) {
        visual.sink_pixels = (int8_t)((sink_elapsed * EGG_CATCH_SINK_PX +
                                       EGG_CATCH_SINK_MS - 1U) /
                                      EGG_CATCH_SINK_MS);
    }
    return visual;
}
