#include "egg_catcher.h"

#include "bsp_battery.h"
#include "egg_catcher_model.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"

#define GAME_SCREEN_W 240
#define GAME_SCREEN_H 320
#define WOLF_COLOR_W  90
#define WOLF_COLOR_H  90
#define WOLF_X        75
#define WOLF_Y        226
#define GAME_TIMER_PERIOD_MS 30
#define EGG_SPRITE_SIZE 16
#define BREAK_SPRITE_W 28
#define BREAK_SPRITE_H 20

#define LCD_BG_COLOR 0xB8C6A3
#define LCD_INK_COLOR 0x17251D
#define LCD_RED_COLOR 0xD74A34
#define LCD_HI_COLOR 0xE8EEDC

enum {
    PAL_BG = 0,
    PAL_INK,
    PAL_RED,
    PAL_HI,
};

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} game_input_t;

typedef struct {
    int16_t x;
    int16_t y;
} game_point_t;

static const char *TAG = "egg_game";

static const game_point_t TRACK_START[EGG_CATCHER_LANE_COUNT][2] = {
    { { 35, 190 }, { 35, 143 } },
    { { 205, 190 }, { 205, 143 } },
};
static const game_point_t TRACK_END[EGG_CATCHER_LANE_COUNT][2] = {
    { { 78, 240 }, { 78, 198 } },
    { { 162, 240 }, { 162, 198 } },
};
static const int8_t FALL_X[EGG_CATCHER_FALL_STEPS] = { 2, 6, 10 };
static const int8_t FALL_Y[2][EGG_CATCHER_FALL_STEPS] = {
    { 10, 27, 47 },
    { 18, 48, 82 },
};

static const uint32_t WOLF_COLORS[15] = {
    0xEFDAA5, 0xD3C5A1, 0xA5A397, 0x737C79, 0x4E5959,
    0x141B1D, 0x33271C, 0x542C15, 0x7B3F1A, 0xA9571F,
    0xD68730, 0xEFB246, 0xBE2419, 0xF54326, 0xF7E8C2,
};

extern const uint8_t egg_game_background_start[]
    asm("_binary_egg_game_background_rgb565_start");
extern const uint8_t egg_game_wolf_start[]
    asm("_binary_egg_game_wolf_i4_start");

static const lv_image_dsc_t BACKGROUND_IMAGE = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .w = GAME_SCREEN_W,
        .h = GAME_SCREEN_H,
        .stride = GAME_SCREEN_W * 2,
    },
    .data_size = GAME_SCREEN_W * GAME_SCREEN_H * 2,
    .data = egg_game_background_start,
};

static const uint16_t EGG_OUTER_MASKS[4][EGG_SPRITE_SIZE] = {
    { 0x0080, 0x01C0, 0x07F0, 0x0FF8, 0x0FF8, 0x1FFC, 0x1FFC, 0x1FFC,
      0x3FFE, 0x3FFE, 0x3FFE, 0x1FFC, 0x1FFC, 0x0FF8, 0x07F0, 0x0080 },
    { 0x0000, 0x0380, 0x3FE0, 0x1FF8, 0x1FFC, 0x3FFE, 0x3FFE, 0x3FFF,
      0x1FFF, 0x3FFE, 0x1FFE, 0x0FFC, 0x0FFC, 0x07F4, 0x01E0, 0x0000 },
    { 0x0000, 0x00E0, 0x07F8, 0x1FFC, 0x3FFE, 0x3FFE, 0x7FFE, 0xFFFF,
      0x7FFE, 0x3FFE, 0x3FFE, 0x1FFC, 0x07F8, 0x00E0, 0x0000, 0x0000 },
    { 0x0180, 0x07E0, 0x0FF8, 0x1FF8, 0x1FFC, 0x3FFE, 0x3FFE, 0x7FFE,
      0x7FFE, 0x7FFC, 0x3FFC, 0x3FF8, 0x3FE0, 0x0740, 0x0000, 0x0000 },
};

static const uint16_t EGG_INNER_MASKS[4][EGG_SPRITE_SIZE] = {
    { 0x0000, 0x0000, 0x0000, 0x0080, 0x03E0, 0x03E0, 0x07F0, 0x07F0,
      0x0FF8, 0x0FF8, 0x07F0, 0x07F0, 0x03E0, 0x0080, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x0100, 0x0FD0, 0x07F8, 0x0FF0, 0x07F8,
      0x07F8, 0x07FC, 0x07F8, 0x03F8, 0x00A0, 0x0000, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0000, 0x00C0, 0x03F0, 0x0FF8, 0x0FF8, 0x1FFC,
      0x0FF8, 0x0FF8, 0x03F0, 0x00C0, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0x0000, 0x0000, 0x0040, 0x05F0, 0x0FF0, 0x07F8, 0x0FF0, 0x0FF8,
      0x1FF0, 0x0FF0, 0x0FE0, 0x0200, 0x0000, 0x0000, 0x0000, 0x0000 },
};

LV_DRAW_BUF_DEFINE_STATIC(wolf_buf, WOLF_COLOR_W, WOLF_COLOR_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(egg0_buf, EGG_SPRITE_SIZE, EGG_SPRITE_SIZE,
                          LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(egg1_buf, EGG_SPRITE_SIZE, EGG_SPRITE_SIZE,
                          LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(break_buf, BREAK_SPRITE_W, BREAK_SPRITE_H,
                          LV_COLOR_FORMAT_I4);

static egg_catcher_model_t s_model;
static lv_obj_t *s_screen;
static lv_obj_t *s_background;
static lv_obj_t *s_score;
static lv_obj_t *s_lives[EGG_CATCHER_MAX_MISSES];
static lv_obj_t *s_egg_objects[EGG_CATCHER_MAX_EGGS];
static lv_obj_t *s_wolf;
static lv_obj_t *s_message;
static lv_obj_t *s_feedback;
static lv_obj_t *s_break;
static lv_obj_t *s_battery;
static lv_timer_t *s_timer;
static QueueHandle_t s_input_queue;
static uint64_t s_last_tick_ms;
static uint64_t s_feedback_until_ms;
static uint64_t s_break_until_ms;
static uint64_t s_battery_due_ms;
static uint64_t s_last_press_ms[3];
static bool s_press_seen[3];
static bool s_battery_available;
static bool s_wolf_drawn;
static bool s_rendered_basket_right;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void sprite_px(lv_obj_t *canvas, int width, int height,
                      int x, int y, uint8_t color)
{
    if (!canvas || x < 0 || x >= width || y < 0 || y >= height) return;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    uint8_t *data = lv_draw_buf_goto_xy(draw_buf, x, y);
    if (!data) return;
    uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((color & 0x0FU) << shift));
}

static void sprite_line(lv_obj_t *canvas, int width, int height,
                        int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        sprite_px(canvas, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void sprite_filled_circle(lv_obj_t *canvas, int width, int height,
                                 int cx, int cy, int radius, uint8_t color)
{
    int limit = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= limit) {
                sprite_px(canvas, width, height, cx + x, cy + y, color);
            }
        }
    }
}

static void clear_sprite(lv_obj_t *canvas, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) sprite_px(canvas, width, height, x, y, PAL_BG);
    }
}

static void draw_egg_frame(lv_obj_t *canvas, uint8_t frame)
{
    frame &= 3U;
    clear_sprite(canvas, EGG_SPRITE_SIZE, EGG_SPRITE_SIZE);
    for (int y = 0; y < EGG_SPRITE_SIZE; y++) {
        for (int x = 0; x < EGG_SPRITE_SIZE; x++) {
            uint16_t bit = (uint16_t)(0x8000U >> x);
            if (EGG_OUTER_MASKS[frame][y] & bit) {
                sprite_px(canvas, EGG_SPRITE_SIZE, EGG_SPRITE_SIZE,
                          x, y, PAL_INK);
            }
            if (EGG_INNER_MASKS[frame][y] & bit) {
                sprite_px(canvas, EGG_SPRITE_SIZE, EGG_SPRITE_SIZE,
                          x, y, PAL_HI);
            }
        }
    }
    lv_obj_invalidate(canvas);
}

static void draw_shell_half(bool right)
{
    static const uint8_t min_x[] = { 7, 5, 4, 3, 2, 2, 2, 3, 4, 6, 8 };
    static const uint8_t max_x[] = { 9, 10, 10, 9, 11, 9, 11, 9, 11, 12, 12 };
    for (int row = 0; row < 11; row++) {
        int y = row + 4;
        for (int x = min_x[row]; x <= max_x[row]; x++) {
            int draw_x = right ? BREAK_SPRITE_W - 1 - x : x;
            sprite_px(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H,
                      draw_x, y, PAL_HI);
        }
    }

    static const game_point_t edge[] = {
        { 9, 4 }, { 6, 4 }, { 4, 6 }, { 2, 9 },
        { 3, 12 }, { 6, 14 }, { 12, 14 },
    };
    static const game_point_t crack[] = {
        { 9, 4 }, { 11, 6 }, { 8, 7 }, { 11, 9 },
        { 8, 10 }, { 12, 12 }, { 12, 14 },
    };
    for (size_t i = 1; i < sizeof(edge) / sizeof(edge[0]); i++) {
        int x0 = right ? BREAK_SPRITE_W - 1 - edge[i - 1].x : edge[i - 1].x;
        int x1 = right ? BREAK_SPRITE_W - 1 - edge[i].x : edge[i].x;
        sprite_line(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H,
                    x0, edge[i - 1].y, x1, edge[i].y, PAL_INK);
    }
    for (size_t i = 1; i < sizeof(crack) / sizeof(crack[0]); i++) {
        int x0 = right ? BREAK_SPRITE_W - 1 - crack[i - 1].x : crack[i - 1].x;
        int x1 = right ? BREAK_SPRITE_W - 1 - crack[i].x : crack[i].x;
        sprite_line(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H,
                    x0, crack[i - 1].y, x1, crack[i].y, PAL_INK);
    }
}

static void show_broken_egg(egg_catcher_lane_t lane, bool upper_track,
                            uint64_t time_ms)
{
    clear_sprite(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H);
    sprite_line(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H, 4, 17, 24, 17, PAL_INK);
    sprite_line(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H, 7, 15, 3, 18, PAL_INK);
    sprite_line(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H, 21, 15, 25, 18, PAL_INK);
    sprite_filled_circle(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H,
                         14, 16, 5, PAL_INK);
    sprite_filled_circle(s_break, BREAK_SPRITE_W, BREAK_SPRITE_H,
                         14, 16, 3, PAL_RED);
    draw_shell_half(false);
    draw_shell_half(true);

    int direction = lane == EGG_LANE_LEFT ? 1 : -1;
    int track = upper_track ? 1 : 0;
    int impact_x = TRACK_END[lane][track].x +
                   direction * FALL_X[EGG_CATCHER_FALL_STEPS - 1];
    int impact_y = TRACK_END[lane][track].y +
                   FALL_Y[track][EGG_CATCHER_FALL_STEPS - 1];
    lv_obj_set_pos(s_break, impact_x - BREAK_SPRITE_W / 2, impact_y - 7);
    lv_obj_remove_flag(s_break, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_break);
    s_break_until_ms = time_ms + 900U;
}

static void draw_wolf(bool basket_right)
{
    clear_sprite(s_wolf, WOLF_COLOR_W, WOLF_COLOR_H);
    for (int y = 0; y < WOLF_COLOR_H; y++) {
        for (int x = 0; x < WOLF_COLOR_W; x++) {
            uint8_t packed = egg_game_wolf_start[y * (WOLF_COLOR_W / 2) + x / 2];
            uint8_t color = (x & 1) ? packed & 0x0FU : packed >> 4;
            if (color != PAL_BG) {
                int draw_x = basket_right ? WOLF_COLOR_W - 1 - x : x;
                sprite_px(s_wolf, WOLF_COLOR_W, WOLF_COLOR_H,
                          draw_x, y, color);
            }
        }
    }
    lv_obj_set_pos(s_wolf, WOLF_X, WOLF_Y);
    lv_obj_invalidate(s_wolf);
    s_rendered_basket_right = basket_right;
    s_wolf_drawn = true;
}

static void refresh_dynamic_objects(void)
{
    lv_label_set_text_fmt(s_score, "%04lu", (unsigned long)(s_model.score % 10000U));
    for (int i = 0; i < EGG_CATCHER_MAX_MISSES; i++) {
        lv_obj_set_style_bg_color(s_lives[i],
            lv_color_hex(i < s_model.misses ? LCD_RED_COLOR : LCD_HI_COLOR), 0);
    }

    if (!s_wolf_drawn || s_rendered_basket_right != s_model.basket_right) {
        draw_wolf(s_model.basket_right);
    }

    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        const egg_catcher_egg_t *egg = &s_model.eggs[i];
        lv_obj_t *object = s_egg_objects[i];
        if (!egg->active) {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int x;
        int y;
        uint8_t frame;
        int track = egg->upper_track ? 1 : 0;
        if (egg->falling) {
            uint8_t fall_step = egg->step < EGG_CATCHER_FALL_STEPS
                                    ? egg->step : EGG_CATCHER_FALL_STEPS - 1;
            int direction = egg->lane == EGG_LANE_LEFT ? 1 : -1;
            x = TRACK_END[egg->lane][track].x + direction * FALL_X[fall_step];
            y = TRACK_END[egg->lane][track].y + FALL_Y[track][fall_step];
            frame = (uint8_t)(EGG_CATCHER_LANE_STEPS + fall_step);
        } else {
            game_point_t start = TRACK_START[egg->lane][track];
            game_point_t end = TRACK_END[egg->lane][track];
            int divisor = EGG_CATCHER_LANE_STEPS - 1;
            x = start.x + (end.x - start.x) * egg->step / divisor;
            y = start.y + (end.y - start.y) * egg->step / divisor;
            frame = egg->step;
        }
        draw_egg_frame(object, frame);
        lv_obj_set_pos(object, x - EGG_SPRITE_SIZE / 2,
                       y - EGG_SPRITE_SIZE / 2);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_message(void)
{
    if (!s_message) return;
    if (s_model.state == EGG_GAME_READY) {
        lv_label_set_text(s_message, "UP: LEFT\nDOWN: RIGHT\n\nPRESS EITHER KEY");
        lv_obj_remove_flag(s_message, LV_OBJ_FLAG_HIDDEN);
    } else if (s_model.state == EGG_GAME_OVER) {
        lv_label_set_text_fmt(s_message, "GAME OVER\nSCORE %04lu\n\nPRESS UP OR DOWN",
                              (unsigned long)(s_model.score % 10000U));
        lv_obj_remove_flag(s_message, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_message, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_battery(void)
{
    if (!s_battery || !s_battery_available) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text_fmt(s_battery, "%d%%", soc);
    lv_obj_set_style_text_color(s_battery,
        lv_color_hex(soc < 20 ? UI_RED : 0xFFF7D2), 0);
    lv_obj_remove_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
}

static bool primary_event(bsp_btn_t button, bsp_btn_ev_t event, uint64_t time_ms)
{
    if ((unsigned)button >= 3U) return false;
    if (event == BSP_BTN_PRESS) {
        s_press_seen[button] = true;
        s_last_press_ms[button] = time_ms;
        return true;
    }
    return event == BSP_BTN_CLICK &&
           (!s_press_seen[button] || time_ms - s_last_press_ms[button] > 1500U);
}

static void handle_input(game_input_t input)
{
    uint64_t time_ms = now_ms();

    if (!primary_event(input.button, input.event, time_ms)) return;

    if (s_model.state == EGG_GAME_READY || s_model.state == EGG_GAME_OVER) {
        egg_catcher_model_start(&s_model);
        lv_obj_add_flag(s_break, LV_OBJ_FLAG_HIDDEN);
        s_break_until_ms = 0;
        s_last_tick_ms = time_ms;
    }
    if (s_model.state != EGG_GAME_PLAYING) return;

    if (input.button == BSP_BTN_UP) {
        egg_catcher_model_set_side(&s_model, false);
    } else if (input.button == BSP_BTN_DOWN) {
        egg_catcher_model_set_side(&s_model, true);
    } else if (input.button == BSP_BTN_OK) {
        egg_catcher_model_toggle_side(&s_model);
    }
    refresh_message();
    refresh_dynamic_objects();
}

static void show_feedback(const char *text, uint32_t color, uint64_t time_ms)
{
    lv_label_set_text(s_feedback, text);
    lv_obj_set_style_text_color(s_feedback, lv_color_hex(color), 0);
    lv_obj_remove_flag(s_feedback, LV_OBJ_FLAG_HIDDEN);
    s_feedback_until_ms = time_ms + 450U;
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    game_input_t input;
    while (s_input_queue && xQueueReceive(s_input_queue, &input, 0) == pdTRUE) {
        handle_input(input);
    }

    uint64_t time_ms = now_ms();
    uint64_t delta = time_ms - s_last_tick_ms;
    if (delta > 500U) delta = 500U;
    s_last_tick_ms = time_ms;

    egg_catcher_event_t event = egg_catcher_model_advance(&s_model, (uint32_t)delta);
    if (event != EGG_EVENT_NONE) refresh_dynamic_objects();
    if (event & EGG_EVENT_CAUGHT) show_feedback("CATCH!", UI_GRASS_DARK, time_ms);
    if (event & EGG_EVENT_MISSED) {
        show_feedback("MISS!", UI_RED, time_ms);
        show_broken_egg(s_model.last_missed_lane,
                        s_model.last_missed_upper_track, time_ms);
    }
    if (event & EGG_EVENT_GAME_OVER) refresh_message();

    if (s_feedback_until_ms && time_ms >= s_feedback_until_ms) {
        lv_obj_add_flag(s_feedback, LV_OBJ_FLAG_HIDDEN);
        s_feedback_until_ms = 0;
    }
    if (s_break_until_ms && time_ms >= s_break_until_ms) {
        lv_obj_add_flag(s_break, LV_OBJ_FLAG_HIDDEN);
        s_break_until_ms = 0;
    }
    if (s_battery_available && time_ms >= s_battery_due_ms) {
        update_battery();
        s_battery_due_ms = time_ms + 15000U;
    }
}

void egg_catcher_enter(bool buttons_available, bool battery_available)
{
    s_battery_available = battery_available;
    egg_catcher_model_init(&s_model, (uint32_t)esp_timer_get_time());
    s_input_queue = xQueueCreate(8, sizeof(game_input_t));
    if (!s_input_queue) ESP_LOGE(TAG, "input queue allocation failed");
    for (int i = 0; i < 3; i++) {
        s_last_press_ms[i] = 0;
        s_press_seen[i] = false;
    }
    s_wolf_drawn = false;

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x0A8DE8), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    s_background = lv_image_create(s_screen);
    lv_image_set_src(s_background, &BACKGROUND_IMAGE);
    lv_obj_set_pos(s_background, 0, 0);
    lv_obj_remove_flag(s_background, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *score_plate = lv_obj_create(s_screen);
    lv_obj_set_size(score_plate, 58, 25);
    lv_obj_set_pos(score_plate, 91, 3);
    lv_obj_set_style_radius(score_plate, 6, 0);
    lv_obj_set_style_bg_color(score_plate, lv_color_hex(0x0E5749), 0);
    lv_obj_set_style_bg_opa(score_plate, LV_OPA_90, 0);
    lv_obj_set_style_border_color(score_plate, lv_color_hex(0xF8DC5C), 0);
    lv_obj_set_style_border_width(score_plate, 2, 0);
    lv_obj_set_style_pad_all(score_plate, 0, 0);
    lv_obj_remove_flag(score_plate, LV_OBJ_FLAG_SCROLLABLE);

    s_score = ui_pixel_label(s_screen, "0000", &lv_font_montserrat_14, 0xFFF7D2);
    lv_obj_set_width(s_score, 52);
    lv_obj_set_style_text_align(s_score, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_score, 94, 6);

    for (int i = 0; i < EGG_CATCHER_MAX_MISSES; i++) {
        s_lives[i] = lv_obj_create(s_screen);
        lv_obj_set_size(s_lives[i], 9, 13);
        lv_obj_set_pos(s_lives[i], 10 + i * 14, 8);
        lv_obj_set_style_radius(s_lives[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(s_lives[i], lv_color_hex(0x17382A), 0);
        lv_obj_set_style_border_width(s_lives[i], 2, 0);
        lv_obj_set_style_pad_all(s_lives[i], 0, 0);
        lv_obj_remove_flag(s_lives[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    LV_DRAW_BUF_INIT_STATIC(wolf_buf);
    s_wolf = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_wolf, &wolf_buf);
    lv_canvas_set_palette(s_wolf, PAL_BG,
                          lv_color_to_32(lv_color_hex(LCD_BG_COLOR), LV_OPA_TRANSP));
    for (int i = 0; i < 15; i++) {
        lv_canvas_set_palette(s_wolf, i + 1,
                              lv_color_to_32(lv_color_hex(WOLF_COLORS[i]), LV_OPA_COVER));
    }
    lv_obj_remove_flag(s_wolf, LV_OBJ_FLAG_SCROLLABLE);

    LV_DRAW_BUF_INIT_STATIC(egg0_buf);
    LV_DRAW_BUF_INIT_STATIC(egg1_buf);
    lv_draw_buf_t *egg_buffers[EGG_CATCHER_MAX_EGGS] = { &egg0_buf, &egg1_buf };
    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        s_egg_objects[i] = lv_canvas_create(s_screen);
        lv_canvas_set_draw_buf(s_egg_objects[i], egg_buffers[i]);
        lv_canvas_set_palette(s_egg_objects[i], PAL_BG,
                              lv_color_to_32(lv_color_hex(LCD_BG_COLOR), LV_OPA_TRANSP));
        lv_canvas_set_palette(s_egg_objects[i], PAL_INK,
                              lv_color_to_32(lv_color_hex(LCD_INK_COLOR), LV_OPA_COVER));
        lv_canvas_set_palette(s_egg_objects[i], PAL_HI,
                              lv_color_to_32(lv_color_hex(LCD_HI_COLOR), LV_OPA_COVER));
        lv_obj_remove_flag(s_egg_objects[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_egg_objects[i], LV_OBJ_FLAG_HIDDEN);
    }

    LV_DRAW_BUF_INIT_STATIC(break_buf);
    s_break = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_break, &break_buf);
    lv_canvas_set_palette(s_break, PAL_BG,
                          lv_color_to_32(lv_color_hex(LCD_BG_COLOR), LV_OPA_TRANSP));
    lv_canvas_set_palette(s_break, PAL_INK,
                          lv_color_to_32(lv_color_hex(LCD_INK_COLOR), LV_OPA_COVER));
    lv_canvas_set_palette(s_break, PAL_RED,
                          lv_color_to_32(lv_color_hex(LCD_RED_COLOR), LV_OPA_COVER));
    lv_canvas_set_palette(s_break, PAL_HI,
                          lv_color_to_32(lv_color_hex(LCD_HI_COLOR), LV_OPA_COVER));
    lv_obj_remove_flag(s_break, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_break, LV_OBJ_FLAG_HIDDEN);

    s_battery = ui_pixel_label(s_screen, "", &lv_font_montserrat_14, 0xFFF7D2);
    lv_obj_set_width(s_battery, 50);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_opa(s_battery, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_battery, 184, 6);
    if (!battery_available) lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);

    s_feedback = ui_pixel_label(s_screen, "", &lv_font_montserrat_20, 0xFFF7D2);
    lv_obj_set_width(s_feedback, 100);
    lv_obj_set_style_text_align(s_feedback, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_feedback, 70, 32);
    lv_obj_add_flag(s_feedback, LV_OBJ_FLAG_HIDDEN);

    s_message = lv_label_create(s_screen);
    lv_obj_set_size(s_message, 190, 108);
    lv_obj_set_pos(s_message, 25, 96);
    lv_obj_set_style_text_font(s_message, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_message, lv_color_hex(LCD_INK_COLOR), 0);
    lv_obj_set_style_bg_color(s_message, lv_color_hex(LCD_HI_COLOR), 0);
    lv_obj_set_style_bg_opa(s_message, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_message, lv_color_hex(LCD_INK_COLOR), 0);
    lv_obj_set_style_border_width(s_message, 3, 0);
    lv_obj_set_style_pad_top(s_message, 10, 0);

    refresh_dynamic_objects();
    if (!buttons_available) {
        lv_label_set_text(s_message, "BUTTONS NOT FOUND\n\nCHECK THE BOARD");
    } else {
        refresh_message();
    }
    update_battery();
    lv_screen_load(s_screen);

    s_last_tick_ms = now_ms();
    s_battery_due_ms = s_last_tick_ms + 15000U;
    s_feedback_until_ms = 0;
    s_break_until_ms = 0;
    s_timer = lv_timer_create(timer_cb, GAME_TIMER_PERIOD_MS, NULL);

    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "ready: %u bytes LVGL free, %u%% fragmented",
             (unsigned)memory.free_size, memory.frag_pct);
}

void egg_catcher_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!s_input_queue) return;
    game_input_t input = { .button = button, .event = event };
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: key=%d event=%d", button, event);
    }
}
