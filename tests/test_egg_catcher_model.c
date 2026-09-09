#include <assert.h>
#include <stdio.h>

#include "egg_catcher_model.h"

static egg_catcher_egg_t *first_egg(egg_catcher_model_t *model)
{
    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        if (model->eggs[i].active) return &model->eggs[i];
    }
    return NULL;
}

static void test_initial_state_and_controls(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 1234);
    assert(model.state == EGG_GAME_READY);
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_LEFT_LOWER);

    egg_catcher_model_set_height(&model, true);
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_LEFT_UPPER);
    egg_catcher_model_toggle_side(&model);
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_RIGHT_UPPER);
}

static void test_catch_scores(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 11);
    egg_catcher_model_start(&model);
    egg_catcher_egg_t *egg = first_egg(&model);
    assert(egg != NULL);
    model.basket_right = egg->lane == EGG_LANE_RIGHT_UPPER ||
                         egg->lane == EGG_LANE_RIGHT_LOWER;
    model.basket_upper = egg->lane == EGG_LANE_LEFT_UPPER ||
                         egg->lane == EGG_LANE_RIGHT_UPPER;
    egg->step = EGG_CATCHER_LANE_STEPS - 1;

    egg_catcher_event_t event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_CAUGHT) != 0);
    assert(model.score == 1);
    assert(model.misses == 0);
}

static void test_three_misses_end_game(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 22);
    egg_catcher_model_start(&model);

    for (int miss = 0; miss < EGG_CATCHER_MAX_MISSES; miss++) {
        egg_catcher_egg_t *egg = first_egg(&model);
        if (!egg) {
            model.spawn_elapsed_ms = egg_catcher_model_spawn_interval_ms(&model);
            (void)egg_catcher_model_advance(&model, 0);
            egg = first_egg(&model);
        }
        assert(egg != NULL);
        model.basket_right = !(egg->lane == EGG_LANE_RIGHT_UPPER ||
                               egg->lane == EGG_LANE_RIGHT_LOWER);
        egg->step = EGG_CATCHER_LANE_STEPS - 1;
        egg_catcher_event_t event = egg_catcher_model_advance(
            &model, egg_catcher_model_move_interval_ms(&model));
        assert((event & EGG_EVENT_MISSED) != 0);
    }
    assert(model.misses == EGG_CATCHER_MAX_MISSES);
    assert(model.state == EGG_GAME_OVER);
}

static void test_pause_and_difficulty(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 33);
    egg_catcher_model_start(&model);
    egg_catcher_egg_t before = *first_egg(&model);

    assert(egg_catcher_model_toggle_pause(&model));
    assert(egg_catcher_model_advance(&model, 5000) == EGG_EVENT_NONE);
    assert(first_egg(&model)->step == before.step);
    assert(egg_catcher_model_toggle_pause(&model));

    model.score = 100;
    assert(egg_catcher_model_move_interval_ms(&model) == 168);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 520);
}

int main(void)
{
    test_initial_state_and_controls();
    test_catch_scores();
    test_three_misses_end_game();
    test_pause_and_difficulty();
    puts("egg_catcher_model: all tests passed");
    return 0;
}
