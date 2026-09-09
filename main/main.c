#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "egg_catcher.h"
#include "esp_log.h"

static const char *TAG = "main";

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    egg_catcher_key(btn, ev);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Egg Catcher");
    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool buttons_available = bsp_button_init(on_key, NULL) == ESP_OK;
    bool battery_available = bsp_battery_init() == ESP_OK;
    if (bsp_lvgl_lock(1000)) {
        egg_catcher_enter(buttons_available, battery_available);
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "Ready: buttons=%d battery=%d", buttons_available, battery_available);
}
