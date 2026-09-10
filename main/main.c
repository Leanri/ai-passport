#include "bsp_i2c.h"
#include "bsp_audio.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "egg_catcher.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"

static const char *TAG = "main";

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} app_input_t;

typedef enum {
    APP_SCREEN_GAME,
    APP_SCREEN_TITLE,
} app_screen_t;

static QueueHandle_t s_input_queue;
static lv_obj_t *s_title_screen;
static lv_obj_t *s_title_mascot;
static app_screen_t s_screen = APP_SCREEN_GAME;
static bool s_buttons_available;
static bool s_battery_available;
static bool s_audio_available;
static uint64_t s_title_opened_ms;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void title_enter(void)
{
    s_title_screen = ui_pixel_screen_create("EGG CATCHER");
    lv_obj_t *panel = ui_pixel_panel_create(s_title_screen, 18, 72, 204, 145,
                                             UI_PAPER);
    lv_obj_t *label = ui_pixel_label(panel,
                                    "PRESS OK TO PLAY\n\nUP / DOWN: MOVE\nHOLD OK: EXIT GAME",
                                    &lv_font_montserrat_14, UI_INK);
    lv_obj_set_width(label, 174);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    s_title_mascot = ui_pixel_mascot_create(s_title_screen, 101, 238);
    lv_screen_load(s_title_screen);
    s_title_opened_ms = now_ms();
}

static void game_enter(void)
{
    s_screen = APP_SCREEN_GAME;
    egg_catcher_enter(s_buttons_available, s_battery_available,
                      s_audio_available);
}

static void leave_game(void)
{
    s_screen = APP_SCREEN_TITLE;
    egg_catcher_exit();
    title_enter();
}

static void handle_input(app_input_t input)
{
    if (s_screen == APP_SCREEN_GAME) {
        if (input.button == BSP_BTN_OK) {
            if (input.event == BSP_BTN_LONG) leave_game();
            return;
        }
        egg_catcher_key(input.button, input.event);
        return;
    }

    if (input.button == BSP_BTN_OK && input.event == BSP_BTN_CLICK &&
        now_ms() - s_title_opened_ms > 500U) {
        ui_pixel_mascot_jump(s_title_mascot);
        lv_obj_delete(s_title_screen);
        s_title_screen = NULL;
        s_title_mascot = NULL;
        game_enter();
    }
}

static void input_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    app_input_t input;
    while (s_input_queue && xQueueReceive(s_input_queue, &input, 0) == pdTRUE) {
        handle_input(input);
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_input_queue) return;
    app_input_t input = { .button = btn, .event = ev };
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: key=%d event=%d", btn, ev);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Egg Catcher (reset_reason=%d)", esp_reset_reason());
    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    s_input_queue = xQueueCreate(12, sizeof(app_input_t));
    s_buttons_available = s_input_queue && bsp_button_init(on_key, NULL) == ESP_OK;
    s_audio_available = bsp_audio_init() == ESP_OK;
    s_battery_available = bsp_battery_init() == ESP_OK;
    if (bsp_lvgl_lock(1000)) {
        game_enter();
        (void)lv_timer_create(input_timer_cb, 20, NULL);
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d audio=%d",
             s_buttons_available, s_battery_available, s_audio_available);
}
