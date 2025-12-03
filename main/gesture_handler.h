#ifndef GESTURE_HANDLER_H
#define GESTURE_HANDLER_H

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize gesture handler
 * @param touch_indev LVGL touch input device
 * @return ESP_OK on success
 */
esp_err_t gesture_handler_init(lv_indev_t *touch_indev);

/**
 * @brief Register gesture callback on a screen
 * @param screen Screen object to register callback on
 */
void gesture_handler_register_screen(lv_obj_t *screen);

#ifdef __cplusplus
}
#endif

#endif // GESTURE_HANDLER_H
