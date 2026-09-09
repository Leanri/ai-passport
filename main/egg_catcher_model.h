#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EGG_CATCHER_LANE_COUNT 4
#define EGG_CATCHER_MAX_EGGS   4
#define EGG_CATCHER_LANE_STEPS 7
#define EGG_CATCHER_MAX_MISSES 3

typedef enum {
    EGG_LANE_LEFT_UPPER = 0,
    EGG_LANE_RIGHT_UPPER,
    EGG_LANE_LEFT_LOWER,
    EGG_LANE_RIGHT_LOWER,
} egg_catcher_lane_t;

typedef enum {
    EGG_GAME_READY = 0,
    EGG_GAME_PLAYING,
    EGG_GAME_PAUSED,
    EGG_GAME_OVER,
} egg_catcher_state_t;

typedef enum {
    EGG_EVENT_NONE      = 0,
    EGG_EVENT_MOVED     = 1 << 0,
    EGG_EVENT_SPAWNED   = 1 << 1,
    EGG_EVENT_CAUGHT    = 1 << 2,
    EGG_EVENT_MISSED    = 1 << 3,
    EGG_EVENT_GAME_OVER = 1 << 4,
} egg_catcher_event_t;

typedef struct {
    bool active;
    egg_catcher_lane_t lane;
    uint8_t step;
} egg_catcher_egg_t;

typedef struct {
    egg_catcher_egg_t eggs[EGG_CATCHER_MAX_EGGS];
    egg_catcher_state_t state;
    uint32_t score;
    uint32_t rng;
    uint32_t move_elapsed_ms;
    uint32_t spawn_elapsed_ms;
    uint8_t misses;
    bool basket_right;
    bool basket_upper;
} egg_catcher_model_t;

void egg_catcher_model_init(egg_catcher_model_t *model, uint32_t seed);
void egg_catcher_model_start(egg_catcher_model_t *model);
void egg_catcher_model_set_height(egg_catcher_model_t *model, bool upper);
void egg_catcher_model_toggle_side(egg_catcher_model_t *model);
bool egg_catcher_model_toggle_pause(egg_catcher_model_t *model);
egg_catcher_lane_t egg_catcher_model_basket_lane(const egg_catcher_model_t *model);
egg_catcher_event_t egg_catcher_model_advance(egg_catcher_model_t *model,
                                               uint32_t elapsed_ms);
uint32_t egg_catcher_model_move_interval_ms(const egg_catcher_model_t *model);
uint32_t egg_catcher_model_spawn_interval_ms(const egg_catcher_model_t *model);
