#include "gesture_handler.h"
#include "ui_manager.h"
#include "esp_log.h"

static const char *TAG = "gesture_handler";
static lv_indev_t *touch_dev = NULL;

// Gesture callback
static void gesture_callback(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(touch_dev);
    screen_index_t current = ui_manager_get_current_screen();
    screen_index_t next = current;
    
    if (dir == LV_DIR_LEFT) {
        // Swipe left - go to next screen
        next = (current + 1) % SCREEN_COUNT;
        ESP_LOGI(TAG, "Swipe left: %d -> %d", current, next);
    } else if (dir == LV_DIR_RIGHT) {
        // Swipe right - go to previous screen (circular)
        next = (current == 0) ? (SCREEN_COUNT - 1) : (current - 1);
        ESP_LOGI(TAG, "Swipe right: %d -> %d", current, next);
    }
    
    if (next != current) {
        ui_manager_goto_screen(next, true);
    }
}

esp_err_t gesture_handler_init(lv_indev_t *touch_indev)
{
    if (touch_indev == NULL) {
        ESP_LOGE(TAG, "Invalid touch input device");
        return ESP_ERR_INVALID_ARG;
    }
    
    touch_dev = touch_indev;
    
    // Add gesture event handler to the active screen
    lv_obj_t *screen = lv_scr_act();
    lv_obj_add_event_cb(screen, gesture_callback, LV_EVENT_GESTURE, NULL);
    
    ESP_LOGI(TAG, "Gesture handler initialized");
    return ESP_OK;
}
