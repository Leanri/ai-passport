// Standalone Egg Catcher launcher and input dispatch.
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "egg_catcher.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";

static bool s_buttons_available;
static bool s_audio_available;
static bool s_battery_available;

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    /* PRESS is the only event the game needs. Forward it directly to the
     * game's non-blocking queue so CLICK/DOUBLE/LONG events cannot delay or
     * crowd out a direction change. */
    if (event == BSP_BTN_PRESS) egg_catcher_key(button, event);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting standalone Egg Catcher");
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

    s_buttons_available = bsp_button_init(on_key, NULL) == ESP_OK;
    s_audio_available = bsp_audio_init() == ESP_OK;
    s_battery_available = bsp_battery_init() == ESP_OK;

    if (bsp_lvgl_lock(1000)) {
        egg_catcher_enter(s_buttons_available, s_battery_available,
                          s_audio_available);
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d audio=%d",
             s_buttons_available, s_battery_available, s_audio_available);
}
