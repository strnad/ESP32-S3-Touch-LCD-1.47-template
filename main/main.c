#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "nvs_flash.h"

#include "esp_lvgl_port.h"

#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_i2c.h"
#include "bsp_wifi.h"
#include "bsp_sdcard.h"
#include "bsp_battery.h"

#include "config_manager.h"
#include "bitaxe_api.h"
#include "ui_manager.h"
#include "gesture_handler.h"
#include "data_logger.h"
#include "data_refresh_task.h"

#define EXAMPLE_DISPLAY_ROTATION 0

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES (320)
#define EXAMPLE_LCD_V_RES (172)
#else
#define EXAMPLE_LCD_H_RES (172)
#define EXAMPLE_LCD_V_RES (320)
#endif

#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)


static char *TAG = "bitaxe_dashboard";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

static esp_err_t app_lvgl_init(void);

void app_main(void)
{
    i2c_master_bus_handle_t i2c_bus_handle;
    bitaxe_config_t config;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    i2c_bus_handle = bsp_i2c_init();
    bsp_battery_init();
    
    // Initialize SD card
    bsp_sdcard_init();
    
    // Load configuration from SD card or NVS
    ESP_LOGI(TAG, "Loading configuration...");
    ret = config_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load configuration! Check SD card for config.json");
        ESP_LOGE(TAG, "Expected format: {\"wifi_ssid\":\"...\", \"wifi_password\":\"...\", \"bitaxe_ip\":\"...\"}");
        return;
    }
    
    ret = config_manager_get(&config);
    if (ret != ESP_OK || !config.valid) {
        ESP_LOGE(TAG, "Invalid configuration!");
        return;
    }
    
    ESP_LOGI(TAG, "Configuration loaded successfully");
    ESP_LOGI(TAG, "WiFi SSID: %s", config.wifi_ssid);
    ESP_LOGI(TAG, "Bitaxe IP: %s", config.bitaxe_ip);
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Connecting to WiFi...");
    bsp_wifi_init(config.wifi_ssid, config.wifi_password);
    ret = bsp_wifi_sta_connect(config.wifi_ssid, config.wifi_password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed!");
        return;
    }
    
    char ip_addr[16];
    bsp_wifi_get_ip(ip_addr);
    ESP_LOGI(TAG, "WiFi connected! IP: %s", ip_addr);
    
    // Initialize Bitaxe API
    ret = bitaxe_api_init(config.bitaxe_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Bitaxe API");
        return;
    }
    
    // Initialize display and touch
    bsp_display_init(&io_handle, &panel_handle, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT);
    bsp_touch_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);
    
    // Initialize LVGL
    ESP_ERROR_CHECK(app_lvgl_init());

    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

    // Initialize UI with dark theme and all screens
    if (lvgl_port_lock(0))
    {
        ESP_LOGI(TAG, "Initializing UI...");
        ui_manager_init();
        
        // Initialize gesture handler for swipe navigation
        gesture_handler_init(lvgl_touch_indev);
        
        lvgl_port_unlock();
    }
    
    // Initialize data logger
    ret = data_logger_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Data logger initialization failed (SD card may not be available)");
    }
    
    // Start data refresh task
    ESP_LOGI(TAG, "Starting data refresh task...");
    ret = data_refresh_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start data refresh task");
        return;
    }
    
    ESP_LOGI(TAG, "Bitaxe Dashboard initialized successfully!");
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 1024 * 10,  /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_spiram = false,
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }};
#if EXAMPLE_DISPLAY_ROTATION == 90
    disp_cfg.rotation.swap_xy = true;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = false;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
#elif EXAMPLE_DISPLAY_ROTATION == 180
    disp_cfg.rotation.swap_xy = false;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = true;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
#elif EXAMPLE_DISPLAY_ROTATION == 270
    disp_cfg.rotation.swap_xy = true;
    disp_cfg.rotation.mirror_x = false;
    disp_cfg.rotation.mirror_y = true;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
#endif
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}
