#include "chart_data_buffer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "chart_buffer";

// Circular buffer
static chart_data_point_t buffer[CHART_BUFFER_SIZE] = {0};
static uint16_t write_index = 0;
static uint16_t count = 0;
static bool initialized = false;

esp_err_t chart_buffer_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Chart buffer already initialized");
        return ESP_OK;
    }

    memset(buffer, 0, sizeof(buffer));
    write_index = 0;
    count = 0;
    initialized = true;

    ESP_LOGI(TAG, "Chart data buffer initialized (size: %d points, ~%d KB)",
             CHART_BUFFER_SIZE, (int)(sizeof(buffer) / 1024));

    return ESP_OK;
}

void chart_buffer_add_point(const bitaxe_data_t *data)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Chart buffer not initialized");
        return;
    }

    if (!data || !data->valid) {
        return;
    }

    // Get current timestamp in seconds since boot
    uint32_t timestamp = (uint32_t)(esp_timer_get_time() / 1000000);

    // Add point to circular buffer
    buffer[write_index].timestamp = timestamp;
    buffer[write_index].hashrate = data->hashRate;
    buffer[write_index].temp = data->temp;
    buffer[write_index].vrTemp = data->vrTemp;

    // Advance write index (circular)
    write_index = (write_index + 1) % CHART_BUFFER_SIZE;

    // Update count (max at CHART_BUFFER_SIZE)
    if (count < CHART_BUFFER_SIZE) {
        count++;
    }
}

const chart_data_point_t* chart_buffer_get_data(uint16_t *out_count)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Chart buffer not initialized");
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    if (out_count) {
        *out_count = count;
    }

    return buffer;
}

uint16_t chart_buffer_get_count(void)
{
    return initialized ? count : 0;
}
