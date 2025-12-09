#include "chart_data_buffer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

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

esp_err_t chart_buffer_load_from_sd(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Chart buffer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const char *LOG_FILE_PATH = "/sdcard/bitaxe_log.csv";

    // Check if file exists
    struct stat st;
    if (stat(LOG_FILE_PATH, &st) != 0) {
        ESP_LOGW(TAG, "No log file found on SD card, starting with empty buffer");
        return ESP_OK;  // Not an error, just no data to load
    }

    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for reading");
        return ESP_FAIL;
    }

    // Skip header line
    char line[256];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        ESP_LOGW(TAG, "Empty log file");
        return ESP_OK;
    }

    // Temporary array to store all data points
    chart_data_point_t temp_buffer[CHART_BUFFER_SIZE];
    uint16_t temp_count = 0;

    // Read all lines from the CSV file
    while (fgets(line, sizeof(line), f) != NULL && temp_count < CHART_BUFFER_SIZE) {
        uint32_t timestamp;
        float hashrate, temp, vrTemp, power, efficiency, error_pct;
        unsigned long long session_diff, network_diff;
        unsigned long shares_accepted, shares_rejected;

        // Parse CSV line
        // Format: timestamp,hashrate,temp,vrTemp,power,efficiency,error_pct,session_diff,network_diff,shares_accepted,shares_rejected
        int parsed = sscanf(line, "%lu,%f,%f,%f,%f,%f,%f,%llu,%llu,%lu,%lu",
                           &timestamp, &hashrate, &temp, &vrTemp, &power, &efficiency, &error_pct,
                           &session_diff, &network_diff, &shares_accepted, &shares_rejected);

        if (parsed >= 4) {  // We need at least timestamp, hashrate, temp, vrTemp
            temp_buffer[temp_count].timestamp = timestamp;
            temp_buffer[temp_count].hashrate = hashrate;
            temp_buffer[temp_count].temp = temp;
            temp_buffer[temp_count].vrTemp = vrTemp;
            temp_count++;
        }
    }

    fclose(f);

    if (temp_count == 0) {
        ESP_LOGW(TAG, "No valid data points loaded from SD card");
        return ESP_OK;
    }

    // If we have more data than buffer size, take only the most recent points
    uint16_t start_index = 0;
    uint16_t points_to_load = temp_count;

    if (temp_count > CHART_BUFFER_SIZE) {
        start_index = temp_count - CHART_BUFFER_SIZE;
        points_to_load = CHART_BUFFER_SIZE;
    }

    // Copy data to circular buffer
    for (uint16_t i = 0; i < points_to_load; i++) {
        uint16_t src_idx = start_index + i;
        buffer[i] = temp_buffer[src_idx];
    }

    write_index = points_to_load % CHART_BUFFER_SIZE;
    count = points_to_load;

    ESP_LOGI(TAG, "Loaded %d data points from SD card (total in file: %d)", points_to_load, temp_count);

    return ESP_OK;
}
