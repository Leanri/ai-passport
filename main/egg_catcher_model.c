#include "egg_catcher_model.h"

#include <string.h>

static uint32_t random_next(egg_catcher_model_t *model)
{
    uint32_t value = model->rng;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    model->rng = value ? value : 0x6D2B79F5U;
    return model->rng;
}

egg_catcher_lane_t egg_catcher_model_basket_lane(const egg_catcher_model_t *model)
{
    return model->basket_right ? EGG_LANE_RIGHT : EGG_LANE_LEFT;
}

static uint8_t speed_level_at(egg_catcher_difficulty_t difficulty,
                              uint32_t score)
{
    static const uint8_t kids_thresholds[] = { 15U, 30U, 45U, 60U, 75U, 90U };
    static const uint8_t adult_thresholds[] = { 15U, 30U, 45U, 60U, 70U, 85U };
    const uint8_t *thresholds = difficulty == EGG_DIFFICULTY_ADULTS
                                    ? adult_thresholds : kids_thresholds;
    uint8_t level = 0;

    while (level < sizeof(kids_thresholds) && score >= thresholds[level]) {
        level++;
    }
    return level;
}

uint8_t egg_catcher_model_speed_level(const egg_catcher_model_t *model)
{
    return speed_level_at(model->difficulty, model->score);
}

bool egg_catcher_model_speed_increased(const egg_catcher_model_t *model)
{
    if (model->score == 0U) return false;
    return speed_level_at(model->difficulty, model->score) >
           speed_level_at(model->difficulty, model->score - 1U);
}

uint32_t egg_catcher_model_move_interval_ms(const egg_catcher_model_t *model)
{
    static const uint16_t kids_interval_ms[] = {
        420U, 370U, 330U, 300U, 280U, 260U, 240U,
    };
    static const uint16_t adult_interval_ms[] = {
        360U, 300U, 260U, 230U, 205U, 165U, 145U,
    };
    const uint16_t *intervals = model->difficulty == EGG_DIFFICULTY_ADULTS
                                    ? adult_interval_ms : kids_interval_ms;
    return intervals[egg_catcher_model_speed_level(model)];
}

uint32_t egg_catcher_model_spawn_interval_ms(const egg_catcher_model_t *model)
{
    static const uint16_t kids_interval_ms[] = {
        1200U, 1080U, 970U, 880U, 800U, 730U, 670U,
    };
    static const uint16_t adult_interval_ms[] = {
        1000U, 850U, 740U, 650U, 560U, 440U, 380U,
    };
    const uint16_t *intervals = model->difficulty == EGG_DIFFICULTY_ADULTS
                                    ? adult_interval_ms : kids_interval_ms;
    return intervals[egg_catcher_model_speed_level(model)];
}

uint8_t egg_catcher_model_fall_steps(const egg_catcher_egg_t *egg)
{
    return egg->upper_track ? EGG_CATCHER_UPPER_FALL_STEPS
                            : EGG_CATCHER_LOWER_FALL_STEPS;
}

static bool spawn_egg(egg_catcher_model_t *model)
{
    int free_slot = -1;
    bool lane_busy[EGG_CATCHER_LANE_COUNT] = { false };

    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        if (model->eggs[i].active) {
            lane_busy[model->eggs[i].lane] = true;
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) return false;

    uint8_t start = (uint8_t)(random_next(model) % EGG_CATCHER_LANE_COUNT);
    for (uint8_t offset = 0; offset < EGG_CATCHER_LANE_COUNT; offset++) {
        uint8_t lane = (uint8_t)((start + offset) % EGG_CATCHER_LANE_COUNT);
        if (lane_busy[lane]) continue;
        model->eggs[free_slot].active = true;
        model->eggs[free_slot].falling = false;
        model->eggs[free_slot].caught = false;
        model->eggs[free_slot].upper_track = (random_next(model) & 1U) != 0;
        model->eggs[free_slot].lane = (egg_catcher_lane_t)lane;
        model->eggs[free_slot].step = 0;
        return true;
    }
    return false;
}

void egg_catcher_model_init(egg_catcher_model_t *model, uint32_t seed)
{
    memset(model, 0, sizeof(*model));
    model->rng = seed ? seed : 0xC001D00DU;
    model->state = EGG_GAME_READY;
    model->difficulty = EGG_DIFFICULTY_KIDS;
    model->basket_right = false;
}

void egg_catcher_model_set_difficulty(egg_catcher_model_t *model,
                                      egg_catcher_difficulty_t difficulty)
{
    model->difficulty = difficulty == EGG_DIFFICULTY_ADULTS
                            ? EGG_DIFFICULTY_ADULTS : EGG_DIFFICULTY_KIDS;
}

void egg_catcher_model_start(egg_catcher_model_t *model)
{
    uint32_t seed = model->rng ? model->rng : 0xC001D00DU;
    egg_catcher_difficulty_t difficulty = model->difficulty;
    memset(model, 0, sizeof(*model));
    model->rng = seed;
    model->state = EGG_GAME_PLAYING;
    model->difficulty = difficulty;
    model->basket_right = false;
    (void)spawn_egg(model);
}

void egg_catcher_model_set_side(egg_catcher_model_t *model, bool right)
{
    model->basket_right = right;
}

void egg_catcher_model_toggle_side(egg_catcher_model_t *model)
{
    model->basket_right = !model->basket_right;
}

static egg_catcher_event_t move_eggs(egg_catcher_model_t *model)
{
    egg_catcher_event_t event = EGG_EVENT_NONE;
    egg_catcher_lane_t basket = egg_catcher_model_basket_lane(model);

    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        egg_catcher_egg_t *egg = &model->eggs[i];
        if (!egg->active) continue;

        if (egg->falling) {
            uint8_t fall_steps = egg_catcher_model_fall_steps(egg);
            uint8_t basket_step = fall_steps - 2U;

            if (egg->caught) {
                /* The contact frame was already shown at the basket opening.
                 * Consume the egg now instead of drawing another fall step
                 * below a basket whose transparent outline may expose it. */
                egg->active = false;
                event |= EGG_EVENT_MOVED;
                continue;
            }

            egg->step++;
            event |= EGG_EVENT_MOVED;

            if (egg->step >= basket_step && egg->lane == basket) {
                /* Latch at the opening, before the egg sinks behind the rim. */
                egg->caught = true;
                model->score++;
                event |= EGG_EVENT_CAUGHT;
                if (model->score >= EGG_CATCHER_WIN_SCORE) {
                    for (int j = 0; j < EGG_CATCHER_MAX_EGGS; j++) {
                        model->eggs[j].active = false;
                    }
                    model->state = EGG_GAME_WON;
                    return event | EGG_EVENT_WON;
                }
            } else if (egg->step >= fall_steps - 1U) {
                egg->active = false;
                model->last_missed_lane = egg->lane;
                model->last_missed_upper_track = egg->upper_track;
                model->misses++;
                event |= EGG_EVENT_MISSED;
                if (model->misses >= EGG_CATCHER_MAX_MISSES) {
                    model->state = EGG_GAME_OVER;
                    event |= EGG_EVENT_GAME_OVER;
                }
            }
            continue;
        }

        egg->step++;
        event |= EGG_EVENT_MOVED;
        if (egg->step < EGG_CATCHER_LANE_STEPS) continue;

        egg->falling = true;
        egg->step = 0;
    }
    return event;
}
egg_catcher_event_t egg_catcher_model_advance(egg_catcher_model_t *model,
                                               uint32_t elapsed_ms)
{
    if (model->state != EGG_GAME_PLAYING) return EGG_EVENT_NONE;

    egg_catcher_event_t event = EGG_EVENT_NONE;
    model->move_elapsed_ms += elapsed_ms;
    model->spawn_elapsed_ms += elapsed_ms;

    uint32_t move_interval = egg_catcher_model_move_interval_ms(model);
    while (model->move_elapsed_ms >= move_interval &&
           model->state == EGG_GAME_PLAYING) {
        model->move_elapsed_ms -= move_interval;
        event |= move_eggs(model);
        move_interval = egg_catcher_model_move_interval_ms(model);
    }

    uint32_t spawn_interval = egg_catcher_model_spawn_interval_ms(model);
    while (model->spawn_elapsed_ms >= spawn_interval &&
           model->state == EGG_GAME_PLAYING) {
        model->spawn_elapsed_ms -= spawn_interval;
        if (spawn_egg(model)) event |= EGG_EVENT_SPAWNED;
        spawn_interval = egg_catcher_model_spawn_interval_ms(model);
    }
    return event;
}
