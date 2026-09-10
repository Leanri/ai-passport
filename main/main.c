// FoloToy AI Passport main menu and page-level input dispatch.
//
// UP/DOWN select a menu item. OK enters it. While a page is open, holding OK
// always closes that page and rebuilds the main AI Passport menu.
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "demo.h"
#include "egg_catcher.h"
#include "esp_log.h"
#include "esp_sleep.h"
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

static QueueHandle_t s_input_queue;
static bool s_buttons_available;
static bool s_audio_available;
static bool s_battery_available;

static void game_enter(void)
{
    egg_catcher_enter(s_buttons_available, s_battery_available,
                      s_audio_available);
}

static void game_exit(void)
{
    egg_catcher_exit();
}

static void game_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    egg_catcher_key(button, event);
}

enum {
    DEMO_EGG_CATCHER,
    DEMO_DISPLAY,
    DEMO_BUTTON,
    DEMO_AUDIO,
    DEMO_BATTERY,
    DEMO_WIFI,
    DEMO_BLE,
    DEMO_LOW_POWER,
};

static const demo_entry_t DEMOS[] = {
    { "Egg Catcher", game_enter, game_exit, game_key },
    { "Display", demo_display_enter, demo_display_exit, demo_display_key },
    { "Button", demo_button_enter, demo_button_exit, demo_button_key },
    { "Audio", demo_audio_enter, demo_audio_exit, demo_audio_key },
    { "Battery", demo_battery_enter, demo_battery_exit, demo_battery_key },
    { "Wi-Fi", demo_wifi_enter, demo_wifi_exit, demo_wifi_key },
    { "BLE", demo_ble_enter, demo_ble_exit, demo_ble_key },
    { "Low Power", demo_low_power_enter, demo_low_power_exit, demo_low_power_key },
};
#define DEMO_COUNT (sizeof(DEMOS) / sizeof(DEMOS[0]))

static bool s_ok[DEMO_COUNT];
static lv_obj_t *s_menu_screen;
static lv_obj_t *s_cards[DEMO_COUNT];
static lv_obj_t *s_rows[DEMO_COUNT];
static lv_obj_t *s_mascot;
static int s_selected;
static int s_active = -1;
static uint64_t s_menu_opened_ms;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void menu_refresh(void)
{
    for (size_t i = 0; i < DEMO_COUNT; i++) {
        lv_label_set_text_fmt(s_rows[i], "%s%s", DEMOS[i].name,
                              s_ok[i] ? "" : "  [FAIL]");
        ui_pixel_set_selected(s_cards[i], (int)i == s_selected, s_ok[i]);
        lv_obj_set_style_text_color(
            s_rows[i], lv_color_hex(s_ok[i] ? UI_INK : 0x7A2020), 0);
    }
}

static void menu_build(void)
{
    s_menu_screen = ui_pixel_screen_create("FoloToy");

    for (size_t i = 0; i < DEMO_COUNT; i++) {
        int x = 11 + (int)(i % 2) * 112;
        int y = 52 + (int)(i / 2) * 47;
        s_cards[i] = ui_pixel_panel_create(s_menu_screen, x, y, 102, 40,
                                            UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }

    s_mascot = ui_pixel_mascot_create(s_menu_screen, 101, 242);
    menu_refresh();
    lv_screen_load(s_menu_screen);
    s_menu_opened_ms = now_ms();
}

static void enter_menu(void)
{
    s_active = -1;
    menu_build();
}

/* Runs from the LVGL timer, so page enter/exit and all object access stay on
 * the LVGL task instead of blocking the hardware-button callback. */
static void handle_input(app_input_t input)
{
    if (s_active >= 0) {
        if (input.button == BSP_BTN_OK && input.event == BSP_BTN_LONG) {
            DEMOS[s_active].exit();
            enter_menu();
        } else {
            DEMOS[s_active].key(input.button, input.event);
        }
        return;
    }

    if (input.event != BSP_BTN_CLICK) return;

    if (input.button == BSP_BTN_UP) {
        s_selected = (s_selected + DEMO_COUNT - 1) % DEMO_COUNT;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
    } else if (input.button == BSP_BTN_DOWN) {
        s_selected = (s_selected + 1) % DEMO_COUNT;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
    } else if (input.button == BSP_BTN_OK && s_ok[s_selected] &&
               now_ms() - s_menu_opened_ms > 500U) {
        s_active = s_selected;
        ui_pixel_mascot_jump(s_mascot);
        lv_obj_delete(s_menu_screen);
        s_menu_screen = NULL;
        s_mascot = NULL;
        DEMOS[s_active].enter();
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

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_queue) return;

    app_input_t input = { .button = button, .event = event };
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: key=%d event=%d", button, event);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting FoloToy AI Passport main menu");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Wakeup cause: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG,
                 "Display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC,
                 BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    s_input_queue = xQueueCreate(12, sizeof(app_input_t));
    s_buttons_available =
        s_input_queue && bsp_button_init(on_key, NULL) == ESP_OK;
    s_audio_available = bsp_audio_init() == ESP_OK;
    s_battery_available = bsp_battery_init() == ESP_OK;

    s_ok[DEMO_EGG_CATCHER] = s_buttons_available;
    s_ok[DEMO_DISPLAY] = true;
    s_ok[DEMO_BUTTON] = s_buttons_available;
    s_ok[DEMO_AUDIO] = s_audio_available;
    s_ok[DEMO_BATTERY] = s_battery_available;
    s_ok[DEMO_WIFI] = true;
    s_ok[DEMO_BLE] = true;
    s_ok[DEMO_LOW_POWER] = true;

    if (bsp_lvgl_lock(1000)) {
        enter_menu();
        (void)lv_timer_create(input_timer_cb, 20, NULL);
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d audio=%d",
             s_buttons_available, s_battery_available, s_audio_available);
}
