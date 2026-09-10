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
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_LEFT);

    egg_catcher_model_set_side(&model, true);
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_RIGHT);
    egg_catcher_model_set_side(&model, false);
    assert(egg_catcher_model_basket_lane(&model) == EGG_LANE_LEFT);
}

static void test_catch_scores(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 11);
    egg_catcher_model_start(&model);
    egg_catcher_egg_t *egg = first_egg(&model);
    assert(egg != NULL);
    model.basket_right = egg->lane == EGG_LANE_RIGHT;
    egg->step = EGG_CATCHER_LANE_STEPS - 1;

    egg_catcher_event_t event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_CAUGHT) == 0);
    assert(egg->active && egg->falling);
    int fall_steps = egg_catcher_model_fall_steps(egg);
    for (int fall = 1; fall < fall_steps - 1; fall++) {
        event = egg_catcher_model_advance(
            &model, egg_catcher_model_move_interval_ms(&model));
    }
    assert((event & EGG_EVENT_CAUGHT) != 0);
    assert(egg->active && egg->caught);
    assert(model.score == 1);
    assert(model.misses == 0);

    /* Moving away after contact must not turn an already caught egg into a miss. */
    model.basket_right = egg->lane != EGG_LANE_RIGHT;
    model.spawn_elapsed_ms = 0;
    event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert(egg->active && egg->caught);
    assert(egg->step == fall_steps - 1);
    assert((event & EGG_EVENT_MISSED) == 0);

    event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert(!egg->active);
    assert((event & EGG_EVENT_MISSED) == 0);
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
        egg_catcher_lane_t missed_lane = egg->lane;
        bool missed_upper_track = egg->upper_track;
        model.basket_right = egg->lane != EGG_LANE_RIGHT;
        egg->step = EGG_CATCHER_LANE_STEPS - 1;
        egg_catcher_event_t event = egg_catcher_model_advance(
            &model, egg_catcher_model_move_interval_ms(&model));
        assert((event & EGG_EVENT_MISSED) == 0);
        assert(egg->active);
        assert(egg->falling);

        int fall_steps = egg_catcher_model_fall_steps(egg);
        for (int fall = 1; fall < fall_steps; fall++) {
            event = egg_catcher_model_advance(
                &model, egg_catcher_model_move_interval_ms(&model));
        }
        assert((event & EGG_EVENT_MISSED) != 0);
        assert(model.last_missed_lane == missed_lane);
        assert(model.last_missed_upper_track == missed_upper_track);
    }
    assert(model.misses == EGG_CATCHER_MAX_MISSES);
    assert(model.state == EGG_GAME_OVER);
}

static void test_missed_egg_stays_visible_while_falling(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 44);
    egg_catcher_model_start(&model);
    egg_catcher_egg_t *egg = first_egg(&model);
    assert(egg != NULL);
    model.basket_right = egg->lane != EGG_LANE_RIGHT;
    egg->step = EGG_CATCHER_LANE_STEPS - 1;

    egg_catcher_event_t event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_MISSED) == 0);
    assert(egg->active && egg->falling && egg->step == 0);

    int fall_steps = egg_catcher_model_fall_steps(egg);
    for (int step = 1; step < fall_steps - 1; step++) {
        event = egg_catcher_model_advance(
            &model, egg_catcher_model_move_interval_ms(&model));
        assert((event & EGG_EVENT_MISSED) == 0);
        assert(egg->active && egg->falling && egg->step == step);
    }

    model.spawn_elapsed_ms = 0;
    event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_MISSED) != 0);
    assert(!egg->active);
}

static void test_difficulty(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 33);
    egg_catcher_model_start(&model);

    model.score = 14;
    assert(egg_catcher_model_move_interval_ms(&model) == 360);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 1000);

    model.score = 15;
    assert(egg_catcher_model_move_interval_ms(&model) == 330);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 930);

    model.score = 30;
    assert(egg_catcher_model_move_interval_ms(&model) == 300);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 860);

    model.score = 45;
    assert(egg_catcher_model_move_interval_ms(&model) == 270);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 790);

    model.score = 60;
    assert(egg_catcher_model_move_interval_ms(&model) == 240);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 720);

    model.score = 75;
    assert(egg_catcher_model_move_interval_ms(&model) == 210);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 650);

    model.score = 90;
    assert(egg_catcher_model_move_interval_ms(&model) == 180);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 580);

    model.score = EGG_CATCHER_WIN_SCORE;
    assert(egg_catcher_model_move_interval_ms(&model) == 180);
    assert(egg_catcher_model_spawn_interval_ms(&model) == 580);
}

static void test_upper_eggs_get_more_fall_frames(void)
{
    egg_catcher_egg_t egg = { 0 };
    assert(egg_catcher_model_fall_steps(&egg) == EGG_CATCHER_LOWER_FALL_STEPS);

    egg.upper_track = true;
    assert(egg_catcher_model_fall_steps(&egg) == EGG_CATCHER_UPPER_FALL_STEPS);
}

static void test_hundredth_catch_wins(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 66);
    egg_catcher_model_start(&model);
    egg_catcher_egg_t *egg = first_egg(&model);
    assert(egg != NULL);

    model.score = EGG_CATCHER_WIN_SCORE - 1U;
    model.basket_right = egg->lane == EGG_LANE_RIGHT;
    egg->upper_track = false;
    egg->step = EGG_CATCHER_LANE_STEPS - 1;

    egg_catcher_event_t event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_WON) == 0);

    event = egg_catcher_model_advance(
        &model, egg_catcher_model_move_interval_ms(&model));
    assert((event & EGG_EVENT_CAUGHT) != 0);
    assert((event & EGG_EVENT_WON) != 0);
    assert(model.score == EGG_CATCHER_WIN_SCORE);
    assert(model.state == EGG_GAME_WON);
    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        assert(!model.eggs[i].active);
    }

    assert(egg_catcher_model_advance(&model, 1000) == EGG_EVENT_NONE);
    egg_catcher_model_start(&model);
    assert(model.state == EGG_GAME_PLAYING);
    assert(model.score == 0);
}

static void test_long_running_session_stays_bounded(void)
{
    egg_catcher_model_t model;
    egg_catcher_model_init(&model, 55);
    egg_catcher_model_start(&model);

    int wins = 0;
    /* Simulate one hour at the UI timer cadence while catching every egg. */
    for (int tick = 0; tick < 120000; tick++) {
        egg_catcher_model_set_side(&model, false);
        for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
            if (model.eggs[i].active) model.eggs[i].lane = EGG_LANE_LEFT;
        }
        egg_catcher_event_t event = egg_catcher_model_advance(&model, 30);
        if (event & EGG_EVENT_WON) {
            assert(model.score == EGG_CATCHER_WIN_SCORE);
            wins++;
            egg_catcher_model_start(&model);
        }

        assert(model.state == EGG_GAME_PLAYING);
        assert(model.misses == 0);
        assert(model.move_elapsed_ms < egg_catcher_model_move_interval_ms(&model));
        assert(model.spawn_elapsed_ms < egg_catcher_model_spawn_interval_ms(&model));
        for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
            const egg_catcher_egg_t *egg = &model.eggs[i];
            if (!egg->active) continue;
            assert(egg->lane < EGG_CATCHER_LANE_COUNT);
            assert(egg->step < (egg->falling
                                    ? egg_catcher_model_fall_steps(egg)
                                    : EGG_CATCHER_LANE_STEPS));
        }
    }
    assert(wins > 0);
}

int main(void)
{
    test_initial_state_and_controls();
    test_catch_scores();
    test_three_misses_end_game();
    test_missed_egg_stays_visible_while_falling();
    test_difficulty();
    test_upper_eggs_get_more_fall_frames();
    test_hundredth_catch_wins();
    test_long_running_session_stays_bounded();
    puts("egg_catcher_model: all tests passed");
    return 0;
}
