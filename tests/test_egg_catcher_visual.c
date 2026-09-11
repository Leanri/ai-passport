#include <assert.h>
#include <stdio.h>

#include "egg_catcher_visual.h"

int main(void)
{
    egg_catch_visual_t visual = egg_catcher_visual_at(0);
    assert(visual.phase == EGG_CATCH_VISUAL_FRONT);
    assert(visual.sink_pixels == 0);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS - 1U);
    assert(visual.phase == EGG_CATCH_VISUAL_FRONT);
    assert(visual.sink_pixels == 0);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS);
    assert(visual.phase == EGG_CATCH_VISUAL_SINK);
    assert(visual.sink_pixels == 0);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS + 1U);
    assert(visual.phase == EGG_CATCH_VISUAL_SINK);
    assert(visual.sink_pixels == 1);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS + 60U);
    assert(visual.phase == EGG_CATCH_VISUAL_SINK);
    assert(visual.sink_pixels == EGG_CATCH_SINK_PX);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS +
                                   EGG_CATCH_SINK_MS - 1U);
    assert(visual.phase == EGG_CATCH_VISUAL_SINK);
    assert(visual.sink_pixels == EGG_CATCH_SINK_PX);

    visual = egg_catcher_visual_at(EGG_CATCH_VISIBLE_MS +
                                   EGG_CATCH_SINK_MS);
    assert(visual.phase == EGG_CATCH_VISUAL_HIDDEN);
    assert(visual.sink_pixels == 0);

    puts("egg_catcher_visual: all tests passed");
    return 0;
}
