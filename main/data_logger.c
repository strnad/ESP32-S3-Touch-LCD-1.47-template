#include "data_logger.h"
#include "esp_log.h"
#include "ff.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

static const char *TAG = "data_logger";
static const char *LOG_FILE_PATH = "/sdcard/bitaxe_log.csv";
static const uint64_t MAX_LOG_SIZE = 10 * 1024 * 1024; // 10 MB
static bool initialized = false;

esp_err_t data_logger_init(void)
{
    // Check if log file exists, create with header if not
    struct stat st;
    if (stat(LOG_FILE_PATH, &st) != 0) {
        // File doesn't exist, create with CSV header
        FILE *f = fopen(LOG_FILE_PATH, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to create log file");
            return ESP_FAIL;
        }
        
        fprintf(f, "timestamp,hashrate,temp,vrTemp,power,efficiency,error_pct,session_diff,network_diff,shares_accepted,shares_rejected\n");
        fclose(f);
        
        ESP_LOGI(TAG, "Created new log file with header");
    } else {
        ESP_LOGI(TAG, "Log file exists, size: %lld bytes", (long long)st.st_size);
    }
    
    initialized = true;
    return ESP_OK;
}

esp_err_t data_logger_log(const bitaxe_data_t *data)
{
    if (!initialized || !data || !data->valid) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check file size and rotate if needed
    struct stat st;
    if (stat(LOG_FILE_PATH, &st) == 0 && st.st_size >= MAX_LOG_SIZE) {
        ESP_LOGW(TAG, "Log file size limit reached, rotating...");
        
        // Rename old file
        char old_path[64];
        snprintf(old_path, sizeof(old_path), "/sdcard/bitaxe_log_old.csv");
        remove(old_path); // Remove old backup if exists
        rename(LOG_FILE_PATH, old_path);
        
        // Create new file with header
        FILE *f = fopen(LOG_FILE_PATH, "w");
        if (f) {
            fprintf(f, "timestamp,hashrate,temp,vrTemp,power,efficiency,error_pct,session_diff,network_diff,shares_accepted,shares_rejected\n");
            fclose(f);
        }
    }
    
    // Append data
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file for writing");
        return ESP_FAIL;
    }
    
    // Get current timestamp (uptime in seconds)
    uint32_t timestamp = data->uptimeSeconds;
    
    // Write CSV row
    fprintf(f, "%lu,%.2f,%.1f,%.1f,%.2f,%.2f,%.4f,%llu,%llu,%lu,%lu\n",
            (unsigned long)timestamp,
            data->hashRate,
            data->temp,
            data->vrTemp,
            data->power,
            data->efficiency,
            data->errorPercentage,
            (unsigned long long)data->bestSessionDiff,
            (unsigned long long)data->networkDifficulty,
            (unsigned long)data->sharesAccepted,
            (unsigned long)data->sharesRejected);
    
    fclose(f);
    
    ESP_LOGD(TAG, "Data logged successfully");
    return ESP_OK;
}

uint64_t data_logger_get_file_size(void)
{
    struct stat st;
    if (stat(LOG_FILE_PATH, &st) == 0) {
        return st.st_size;
    }
    return 0;
}
