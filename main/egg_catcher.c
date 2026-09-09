#include "egg_catcher.h"

#include "bsp_battery.h"
#include "egg_catcher_model.h"
#include "egg_sprites.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"

#define GAME_CANVAS_W 216
#define GAME_CANVAS_H 236
#define WOLF_LEFT_X   64
#define WOLF_RIGHT_X  96
#define WOLF_Y        143
#define GAME_TIMER_PERIOD_MS 30

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

static const game_point_t LANE_START[EGG_CATCHER_LANE_COUNT] = {
    { 24, 70 }, { 192, 70 },
};
static const game_point_t LANE_END[EGG_CATCHER_LANE_COUNT] = {
    { 78, 140 }, { 138, 140 },
};

LV_DRAW_BUF_DEFINE_STATIC(game_buf, GAME_CANVAS_W, GAME_CANVAS_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(wolf_buf, EGG_WOLF_SPRITE_WIDTH, EGG_WOLF_SPRITE_HEIGHT,
                          LV_COLOR_FORMAT_I4);

static egg_catcher_model_t s_model;
static lv_obj_t *s_screen;
static lv_obj_t *s_canvas;
static lv_obj_t *s_score;
static lv_obj_t *s_lives[EGG_CATCHER_MAX_MISSES];
static lv_obj_t *s_egg_objects[EGG_CATCHER_MAX_EGGS];
static lv_obj_t *s_wolf;
static lv_obj_t *s_message;
static lv_obj_t *s_feedback;
static lv_obj_t *s_battery;
static lv_timer_t *s_timer;
static QueueHandle_t s_input_queue;
static uint64_t s_last_tick_ms;
static uint64_t s_feedback_until_ms;
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

static void canvas_px(int x, int y, uint8_t color)
{
    if (!s_canvas || x < 0 || x >= GAME_CANVAS_W || y < 0 || y >= GAME_CANVAS_H) return;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(s_canvas);
    uint8_t *data = lv_draw_buf_goto_xy(draw_buf, x, y);
    if (!data) return;
    uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((color & 0x0FU) << shift));
}

static void canvas_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        canvas_px(x0, y0, color);
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

static void canvas_thick_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    canvas_line(x0, y0, x1, y1, color);
    canvas_line(x0 + 1, y0, x1 + 1, y1, color);
}

static bool mask_pixel(const uint8_t *mask, int width, int x, int y)
{
    int stride = (width + 7) / 8;
    return (mask[y * stride + x / 8] & (0x80U >> (x & 7))) != 0;
}

static void draw_ramp(egg_catcher_lane_t lane)
{
    game_point_t start = LANE_START[lane];
    game_point_t end = LANE_END[lane];
    int direction = end.x > start.x ? 1 : -1;
    canvas_thick_line(start.x, start.y + 8, end.x, end.y + 8, PAL_RED);
    canvas_thick_line(start.x, start.y + 13, end.x, end.y + 13, PAL_RED);
    for (int i = 0; i < 4; i++) {
        int x = start.x + (end.x - start.x) * i / 3;
        int y = start.y + 10 + (end.y - start.y) * i / 3;
        canvas_line(x, y - 3, x + direction * 5, y + 7, PAL_RED);
    }
}

static void draw_chicken(bool facing_right, int x, int y)
{
    static const game_point_t red_pixels[] = {
        { 0, 7 }, { 1, 7 }, { 2, 7 },
        { 7, 1 }, { 8, 0 }, { 9, 1 }, { 11, 1 }, { 5, 9 },
    };

    for (int sy = 0; sy < EGG_CHICKEN_SPRITE_HEIGHT; sy++) {
        for (int sx = 0; sx < EGG_CHICKEN_SPRITE_WIDTH; sx++) {
            if (!mask_pixel(egg_chicken_mask, EGG_CHICKEN_SPRITE_WIDTH, sx, sy)) continue;
            int dx = facing_right ? EGG_CHICKEN_SPRITE_WIDTH - 1 - sx : sx;
            canvas_px(x + dx, y + sy, PAL_INK);
        }
    }
    for (size_t i = 0; i < sizeof(red_pixels) / sizeof(red_pixels[0]); i++) {
        int dx = facing_right ? EGG_CHICKEN_SPRITE_WIDTH - 1 - red_pixels[i].x
                              : red_pixels[i].x;
        canvas_px(x + dx, y + red_pixels[i].y, PAL_RED);
    }
}

static void wolf_px(int x, int y, uint8_t color)
{
    if (!s_wolf || x < 0 || x >= EGG_WOLF_SPRITE_WIDTH ||
        y < 0 || y >= EGG_WOLF_SPRITE_HEIGHT) return;
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(s_wolf);
    uint8_t *data = lv_draw_buf_goto_xy(draw_buf, x, y);
    if (!data) return;
    uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((color & 0x0FU) << shift));
}

static void draw_wolf(bool basket_right)
{
    for (int y = 0; y < EGG_WOLF_SPRITE_HEIGHT; y++) {
        for (int x = 0; x < EGG_WOLF_SPRITE_WIDTH; x++) {
            wolf_px(x, y, PAL_BG);
        }
    }
    for (int y = 0; y < EGG_WOLF_SPRITE_HEIGHT; y++) {
        for (int x = 0; x < EGG_WOLF_SPRITE_WIDTH; x++) {
            if (!mask_pixel(egg_wolf_mask, EGG_WOLF_SPRITE_WIDTH, x, y)) continue;
            int dx = basket_right ? EGG_WOLF_SPRITE_WIDTH - 1 - x : x;
            wolf_px(dx, y, PAL_INK);
        }
    }
    lv_obj_set_pos(s_wolf,
                   12 + (basket_right ? WOLF_RIGHT_X : WOLF_LEFT_X),
                   54 + WOLF_Y);
    lv_obj_invalidate(s_wolf);
    s_rendered_basket_right = basket_right;
    s_wolf_drawn = true;
}

static void draw_static_scene(void)
{
    if (!s_canvas) return;
    lv_canvas_fill_bg(s_canvas, lv_color_hex(LCD_BG_COLOR), LV_OPA_COVER);
    canvas_line(52, 34, 164, 34, PAL_INK);
    for (int lane = 0; lane < EGG_CATCHER_LANE_COUNT; lane++) {
        draw_ramp((egg_catcher_lane_t)lane);
    }
    draw_chicken(true, 2, 44);
    draw_chicken(false, 186, 44);
    lv_obj_invalidate(s_canvas);
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
        game_point_t start = LANE_START[egg->lane];
        game_point_t end = LANE_END[egg->lane];
        int divisor = EGG_CATCHER_LANE_STEPS - 1;
        int x = start.x + (end.x - start.x) * egg->step / divisor;
        int y = start.y + (end.y - start.y) * egg->step / divisor;
        lv_obj_set_pos(object, 12 + x - 5, 54 + y - 7);
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
        lv_color_hex(soc < 20 ? UI_RED : UI_INK), 0);
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
    if (event & EGG_EVENT_MISSED) show_feedback("MISS!", UI_RED, time_ms);
    if (event & EGG_EVENT_GAME_OVER) refresh_message();

    if (s_feedback_until_ms && time_ms >= s_feedback_until_ms) {
        lv_obj_add_flag(s_feedback, LV_OBJ_FLAG_HIDDEN);
        s_feedback_until_ms = 0;
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

    s_screen = ui_pixel_screen_create("EGG GAME");
    (void)ui_pixel_panel_create(s_screen, 6, 48, 228, 248, LCD_BG_COLOR);

    LV_DRAW_BUF_INIT_STATIC(game_buf);
    s_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_canvas, &game_buf);
    lv_canvas_set_palette(s_canvas, PAL_BG,
                          lv_color_to_32(lv_color_hex(LCD_BG_COLOR), LV_OPA_COVER));
    lv_canvas_set_palette(s_canvas, PAL_INK,
                          lv_color_to_32(lv_color_hex(LCD_INK_COLOR), LV_OPA_COVER));
    lv_canvas_set_palette(s_canvas, PAL_RED,
                          lv_color_to_32(lv_color_hex(LCD_RED_COLOR), LV_OPA_COVER));
    lv_canvas_set_palette(s_canvas, PAL_HI,
                          lv_color_to_32(lv_color_hex(LCD_HI_COLOR), LV_OPA_COVER));
    lv_obj_set_pos(s_canvas, 12, 54);

    s_score = ui_pixel_label(s_screen, "0000", &lv_font_montserrat_20, LCD_INK_COLOR);
    lv_obj_set_width(s_score, 72);
    lv_obj_set_style_text_align(s_score, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_score, 84, 58);

    for (int i = 0; i < EGG_CATCHER_MAX_MISSES; i++) {
        s_lives[i] = lv_obj_create(s_screen);
        lv_obj_set_size(s_lives[i], 9, 13);
        lv_obj_set_pos(s_lives[i], 19 + i * 14, 61);
        lv_obj_set_style_radius(s_lives[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(s_lives[i], lv_color_hex(LCD_INK_COLOR), 0);
        lv_obj_set_style_border_width(s_lives[i], 2, 0);
        lv_obj_set_style_pad_all(s_lives[i], 0, 0);
        lv_obj_remove_flag(s_lives[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    LV_DRAW_BUF_INIT_STATIC(wolf_buf);
    s_wolf = lv_canvas_create(s_screen);
    lv_canvas_set_draw_buf(s_wolf, &wolf_buf);
    lv_canvas_set_palette(s_wolf, PAL_BG,
                          lv_color_to_32(lv_color_hex(LCD_BG_COLOR), LV_OPA_TRANSP));
    lv_canvas_set_palette(s_wolf, PAL_INK,
                          lv_color_to_32(lv_color_hex(LCD_INK_COLOR), LV_OPA_COVER));
    lv_obj_remove_flag(s_wolf, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < EGG_CATCHER_MAX_EGGS; i++) {
        s_egg_objects[i] = lv_obj_create(s_screen);
        lv_obj_set_size(s_egg_objects[i], 10, 14);
        lv_obj_set_style_radius(s_egg_objects[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_egg_objects[i], lv_color_hex(LCD_HI_COLOR), 0);
        lv_obj_set_style_border_color(s_egg_objects[i], lv_color_hex(LCD_INK_COLOR), 0);
        lv_obj_set_style_border_width(s_egg_objects[i], 2, 0);
        lv_obj_set_style_pad_all(s_egg_objects[i], 0, 0);
        lv_obj_remove_flag(s_egg_objects[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_egg_objects[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_battery = ui_pixel_label(s_screen, "", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_width(s_battery, 58);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_battery, 174, 29);
    if (!battery_available) lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);

    s_feedback = ui_pixel_label(s_screen, "", &lv_font_montserrat_20, UI_INK);
    lv_obj_set_width(s_feedback, 100);
    lv_obj_set_style_text_align(s_feedback, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_feedback, 70, 86);
    lv_obj_add_flag(s_feedback, LV_OBJ_FLAG_HIDDEN);

    s_message = lv_label_create(s_screen);
    lv_obj_set_size(s_message, 184, 92);
    lv_obj_set_pos(s_message, 28, 123);
    lv_obj_set_style_text_font(s_message, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_message, lv_color_hex(LCD_INK_COLOR), 0);
    lv_obj_set_style_bg_color(s_message, lv_color_hex(LCD_HI_COLOR), 0);
    lv_obj_set_style_bg_opa(s_message, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_message, lv_color_hex(LCD_INK_COLOR), 0);
    lv_obj_set_style_border_width(s_message, 3, 0);
    lv_obj_set_style_pad_top(s_message, 10, 0);

    draw_static_scene();
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
