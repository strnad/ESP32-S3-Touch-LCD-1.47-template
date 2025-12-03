#ifndef DATA_REFRESH_TASK_H
#define DATA_REFRESH_TASK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start data refresh task
 * @return ESP_OK on success
 */
esp_err_t data_refresh_task_start(void);

/**
 * @brief Stop data refresh task
 */
void data_refresh_task_stop(void);

/**
 * @brief Get all-time best difficulty
 * @return All-time best difficulty value
 */
uint64_t data_refresh_task_get_all_time_best(void);

#ifdef __cplusplus
}
#endif

#endif // DATA_REFRESH_TASK_H
