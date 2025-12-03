#include "data_refresh_task.h"
#include "bitaxe_api.h"
#include "ui_manager.h"
#include "data_logger.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "data_refresh";
static const char *NVS_NAMESPACE = "bitaxe_data";
static TaskHandle_t task_handle = NULL;
static bool task_running = false;

// Tracking variables
static uint64_t all_time_best_diff = 0;
static uint32_t last_block_found_state = 0;
static uint32_t log_counter = 0;

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
    const uint32_t MAX_ERRORS = 3;
    
    load_all_time_best();
    
    ESP_LOGI(TAG, "Data refresh task started");
    
    while (task_running) {
        // Fetch data from Bitaxe
        esp_err_t ret = bitaxe_api_get_system_info(&data);
        
        if (ret == ESP_OK && data.valid) {
            error_count = 0; // Reset error counter on success
            
            // Update all-time best diff if needed
            if (data.bestDiff > all_time_best_diff) {
                all_time_best_diff = data.bestDiff;
                save_all_time_best(all_time_best_diff);
                ESP_LOGI(TAG, "New all-time best difficulty: %llu", (unsigned long long)all_time_best_diff);
            }
            
            // Check for block found event
            if (data.blockFound == 1 && last_block_found_state == 0) {
                ESP_LOGI(TAG, "BLOCK FOUND DETECTED! Height: %u, Diff: %llu", 
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
                
                // Update status bar
                bool wifi_connected = strcmp(data.wifiStatus, "Connected!") == 0;
                ui_manager_update_status_bar(wifi_connected, data.isUsingFallbackStratum, 
                                             data.overheat_mode != 0);
                
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
            ESP_LOGW(TAG, "Failed to fetch data from Bitaxe (error %d/%d)", error_count, MAX_ERRORS);
            
            if (error_count >= MAX_ERRORS) {
                ESP_LOGE(TAG, "Multiple consecutive errors, API may be unreachable");
                // Update status bar to show error
                if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
                    ui_manager_update_status_bar(false, false, false);
                    lvgl_port_unlock();
                }
            }
        }
        
        // Wait 5 seconds before next fetch
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
