#include "data_refresh_task.h"
#include "bitaxe_api.h"
#include "ui_manager.h"
#include "data_logger.h"
#include "best_shares.h"
#include "chart_data_buffer.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>
#include <time.h>

static const char *TAG = "data_refresh";
static const char *NVS_NAMESPACE = "bitaxe_data";
static TaskHandle_t task_handle = NULL;
static bool task_running = false;

// Tracking variables
static uint64_t all_time_best_diff = 0;
static uint64_t last_session_best_diff = 0;  // Track session best to detect new session shares
static uint32_t last_block_found_state = 0;
static uint32_t log_counter = 0;

// Connectivity tracking
static TickType_t last_successful_fetch = 0;
static bool connectivity_warning_shown = false;
#define CONNECTIVITY_TIMEOUT_MS 5000  // 5 seconds

// Load all-time best diff from NVS
static void load_all_time_best(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        uint64_t stored_value = 0;
        ret = nvs_get_u64(nvs_handle, "all_time_best", &stored_value);
        if (ret == ESP_OK) {
            all_time_best_diff = stored_value;
            ESP_LOGI(TAG, "Loaded all-time best diff: %llu", (unsigned long long)all_time_best_diff);
        }
        nvs_close(nvs_handle);
    }
}

// Save all-time best diff to NVS
static void save_all_time_best(uint64_t value)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_set_u64(nvs_handle, "all_time_best", value);
        if (ret == ESP_OK) {
            nvs_commit(nvs_handle);
            ESP_LOGI(TAG, "Saved all-time best diff: %llu", (unsigned long long)value);
        }
        nvs_close(nvs_handle);
    }
}

// Main refresh task
static void data_refresh_task(void *pvParameters)
{
    bitaxe_data_t data;
    uint32_t error_count = 0;

    load_all_time_best();

    // Add loaded all-time best to the leaderboard (without timestamp since we don't know when it was found)
    if (all_time_best_diff > 0) {
        best_shares_add(all_time_best_diff, 0, true);
        ESP_LOGI(TAG, "Added all-time best to leaderboard: %llu", (unsigned long long)all_time_best_diff);
    }

    ESP_LOGI(TAG, "Data refresh task started");

    while (task_running) {
        // Fetch data from Bitaxe
        esp_err_t ret = bitaxe_api_get_system_info(&data);

        if (ret == ESP_OK && data.valid) {
            error_count = 0; // Reset error counter on success
            last_successful_fetch = xTaskGetTickCount(); // Update last successful fetch time

            // Hide connectivity warning if it was shown
            if (connectivity_warning_shown) {
                if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                    ui_manager_hide_connectivity_warning();
                    lvgl_port_unlock();
                }
                connectivity_warning_shown = false;
                ESP_LOGI(TAG, "Connectivity restored");
            }

            // Add data point to chart buffer
            chart_buffer_add_point(&data);

            // Update all-time best diff if needed
            if (data.bestDiff > all_time_best_diff) {
                all_time_best_diff = data.bestDiff;
                save_all_time_best(all_time_best_diff);
                ESP_LOGI(TAG, "New all-time best difficulty: %llu", (unsigned long long)all_time_best_diff);

                // Add new all-time best to leaderboard with timestamp (only if time is valid)
                time_t now = time(NULL);
                // Check if time is reasonable (after year 2024 = timestamp > 1704067200)
                time_t timestamp = (now > 1704067200) ? now : 0;
                best_shares_add(all_time_best_diff, timestamp, true);
                ESP_LOGI(TAG, "Added new all-time best with timestamp: %lld", (long long)timestamp);
            }

            // Track session best shares - capture every change in bestSessionDiff
            // This includes both increases within a session AND new sessions starting
            if (data.bestSessionDiff != last_session_best_diff && data.bestSessionDiff > 0) {
                // Detect session restart (value decreased significantly)
                if (data.bestSessionDiff < last_session_best_diff && last_session_best_diff > 0) {
                    ESP_LOGI(TAG, "Session restart detected (diff changed from %llu to %llu)",
                             (unsigned long long)last_session_best_diff, (unsigned long long)data.bestSessionDiff);
                }

                // Update tracker and add to top 10 list
                // best_shares_add() will only add it if it qualifies for top 10
                last_session_best_diff = data.bestSessionDiff;
                time_t now = time(NULL);
                // Check if time is reasonable (after year 2024 = timestamp > 1704067200)
                time_t timestamp = (now > 1704067200) ? now : 0;

                // Check if this session best is also the all-time best
                bool is_all_time = (data.bestSessionDiff >= all_time_best_diff);
                best_shares_add(data.bestSessionDiff, timestamp, is_all_time);

                ESP_LOGI(TAG, "Session best share: %llu (is_all_time: %d, timestamp: %lld)",
                         (unsigned long long)data.bestSessionDiff, is_all_time, (long long)timestamp);
            }

            // Check for block found event
            if (data.blockFound == 1 && last_block_found_state == 0) {
                ESP_LOGI(TAG, "BLOCK FOUND DETECTED! Height: %" PRIu32 ", Diff: %llu",
                         data.blockHeight, (unsigned long long)data.bestDiff);
                
                // Show block found alert (thread-safe)
                if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                    ui_manager_show_block_found(data.blockHeight, data.bestDiff);
                    lvgl_port_unlock();
                }
            } else if (data.blockFound == 0 && last_block_found_state == 1) {
                // Block found flag cleared by API
                if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                    ui_manager_hide_block_found();
                    lvgl_port_unlock();
                }
            }
            last_block_found_state = data.blockFound;
            
            // Update UI (thread-safe)
            if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                ui_manager_update_data(&data);
                lvgl_port_unlock();
            }
            
            // Log data every 12 iterations (60 seconds at 5s interval)
            log_counter++;
            if (log_counter >= 12) {
                log_counter = 0;
                data_logger_log(&data);
            }
            
        } else {
            error_count++;
            ESP_LOGW(TAG, "Failed to fetch data from Bitaxe (error %" PRIu32 ")", error_count);

            // Check if connectivity has been lost for more than 5 seconds
            if (last_successful_fetch > 0) {
                TickType_t time_since_last_success = xTaskGetTickCount() - last_successful_fetch;
                uint32_t ms_since_last_success = pdTICKS_TO_MS(time_since_last_success);

                if (ms_since_last_success > CONNECTIVITY_TIMEOUT_MS && !connectivity_warning_shown) {
                    // Show connectivity warning
                    if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                        ui_manager_show_connectivity_warning();
                        lvgl_port_unlock();
                    }
                    connectivity_warning_shown = true;
                    ESP_LOGW(TAG, "Miner connectivity lost for more than 5 seconds");
                }
            }

            // Wait only 3 seconds on error to retry faster
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        // Wait 5 seconds before next fetch (only on success)
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    
    ESP_LOGI(TAG, "Data refresh task stopped");
    task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t data_refresh_task_start(void)
{
    if (task_running) {
        ESP_LOGW(TAG, "Task already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    task_running = true;
    
    BaseType_t ret = xTaskCreate(
        data_refresh_task,
        "data_refresh",
        4096,
        NULL,
        5,
        &task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        task_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Data refresh task created successfully");
    return ESP_OK;
}

void data_refresh_task_stop(void)
{
    if (task_running) {
        task_running = false;
        ESP_LOGI(TAG, "Stopping data refresh task...");
    }
}

uint64_t data_refresh_task_get_all_time_best(void)
{
    return all_time_best_diff;
}
