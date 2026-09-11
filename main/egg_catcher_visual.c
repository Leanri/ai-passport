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
        /* Reach the four-pixel depth before the last 30 ms display tick, then
         * hold there briefly so the physical LCD shows the final sunk frame. */
        uint32_t pixels = (sink_elapsed * EGG_CATCH_SINK_PX + 59U) / 60U;
        if (pixels > EGG_CATCH_SINK_PX) pixels = EGG_CATCH_SINK_PX;
        visual.sink_pixels = (int8_t)pixels;
    }
    return visual;
}
