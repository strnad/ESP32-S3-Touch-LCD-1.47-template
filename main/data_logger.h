#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "bitaxe_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize data logger
 * @return ESP_OK on success
 */
esp_err_t data_logger_init(void);

/**
 * @brief Log mining data to SD card
 * @param data Pointer to Bitaxe data
 * @return ESP_OK on success
 */
esp_err_t data_logger_log(const bitaxe_data_t *data);

/**
 * @brief Get log file size in bytes
 * @return File size or 0 if error
 */
uint64_t data_logger_get_file_size(void);

#ifdef __cplusplus
}
#endif

#endif // DATA_LOGGER_H
