#ifndef CHART_DATA_BUFFER_H
#define CHART_DATA_BUFFER_H

#include <stdint.h>
#include "bitaxe_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Store 6 hours of data at 5-second intervals
#define CHART_BUFFER_SIZE 4320

typedef struct {
    uint32_t timestamp;  // Seconds since boot
    float hashrate;      // GH/s
    float temp;          // °C (ASIC temperature)
    float vrTemp;        // °C (VR temperature)
} chart_data_point_t;

/**
 * @brief Initialize chart data buffer
 * @return ESP_OK on success
 */
esp_err_t chart_buffer_init(void);

/**
 * @brief Add a new data point to the buffer
 * @param data Pointer to Bitaxe data to extract point from
 */
void chart_buffer_add_point(const bitaxe_data_t *data);

/**
 * @brief Get pointer to chart data buffer
 * @param count Output parameter for number of valid points
 * @return Pointer to data buffer
 */
const chart_data_point_t* chart_buffer_get_data(uint16_t *count);

/**
 * @brief Get current count of valid data points in buffer
 * @return Number of data points (0 to CHART_BUFFER_SIZE)
 */
uint16_t chart_buffer_get_count(void);

/**
 * @brief Load historical data from SD card into buffer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t chart_buffer_load_from_sd(void);

#ifdef __cplusplus
}
#endif

#endif // CHART_DATA_BUFFER_H
