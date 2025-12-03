#include "config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

static const char *TAG = "config_manager";
static const char *NVS_NAMESPACE = "bitaxe_cfg";
static const char *CONFIG_FILE_PATH = "/sdcard/config.json";

static bitaxe_config_t current_config = {0};

esp_err_t config_manager_init(void)
{
    esp_err_t ret;
    
    // Try to load from SD card first
    ret = config_manager_load_from_sd(&current_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from SD card");
        // Save to NVS as backup
        config_manager_save(&current_config);
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Failed to load config from SD card, trying NVS...");
    
    // Fallback to NVS
    ret = config_manager_load_from_nvs(&current_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "No configuration found in SD or NVS");
    current_config.valid = false;
    return ESP_FAIL;
}

esp_err_t config_manager_get(bitaxe_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &current_config, sizeof(bitaxe_config_t));
    return current_config.valid ? ESP_OK : ESP_FAIL;
}

esp_err_t config_manager_save(const bitaxe_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Save each field
    ret = nvs_set_str(nvs_handle, "wifi_ssid", config->wifi_ssid);
    ret |= nvs_set_str(nvs_handle, "wifi_pass", config->wifi_password);
    ret |= nvs_set_str(nvs_handle, "bitaxe_ip", config->bitaxe_ip);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config to NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved to NVS");
        memcpy(&current_config, config, sizeof(bitaxe_config_t));
        current_config.valid = true;
    }
    
    return ret;
}

esp_err_t config_manager_load_from_sd(bitaxe_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Try to list root directory to debug
    ESP_LOGI(TAG, "Attempting to open config file: %s", CONFIG_FILE_PATH);
    
    // Check if SD card is accessible by trying to open root
    DIR *dir = opendir("/sdcard");
    if (dir) {
        ESP_LOGI(TAG, "SD card root directory contents:");
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  - %s", entry->d_name);
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "Cannot open /sdcard directory");
    }
    
    FILE *f = fopen(CONFIG_FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Config file not found: %s (errno: %d)", CONFIG_FILE_PATH, errno);
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize <= 0 || fsize > 4096) {
        ESP_LOGE(TAG, "Invalid config file size: %ld", fsize);
        fclose(f);
        return ESP_FAIL;
    }
    
    // Read file content
    char *json_str = malloc(fsize + 1);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for config file");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    size_t read_size = fread(json_str, 1, fsize, f);
    fclose(f);
    json_str[read_size] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse config JSON");
        return ESP_FAIL;
    }
    
    // Extract fields
    cJSON *wifi_ssid = cJSON_GetObjectItem(root, "wifi_ssid");
    cJSON *wifi_password = cJSON_GetObjectItem(root, "wifi_password");
    cJSON *bitaxe_ip = cJSON_GetObjectItem(root, "bitaxe_ip");
    
    if (!cJSON_IsString(wifi_ssid) || !cJSON_IsString(wifi_password) || 
        !cJSON_IsString(bitaxe_ip)) {
        ESP_LOGE(TAG, "Missing required fields in config.json");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // Copy values
    strncpy(config->wifi_ssid, wifi_ssid->valuestring, sizeof(config->wifi_ssid) - 1);
    strncpy(config->wifi_password, wifi_password->valuestring, sizeof(config->wifi_password) - 1);
    strncpy(config->bitaxe_ip, bitaxe_ip->valuestring, sizeof(config->bitaxe_ip) - 1);
    config->valid = true;
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Loaded config - SSID: %s, Bitaxe IP: %s", 
             config->wifi_ssid, config->bitaxe_ip);
    
    return ESP_OK;
}

esp_err_t config_manager_load_from_nvs(bitaxe_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found");
        return ret;
    }
    
    size_t required_size;
    
    // Load WiFi SSID
    required_size = sizeof(config->wifi_ssid);
    ret = nvs_get_str(nvs_handle, "wifi_ssid", config->wifi_ssid, &required_size);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Load WiFi Password
    required_size = sizeof(config->wifi_password);
    ret = nvs_get_str(nvs_handle, "wifi_pass", config->wifi_password, &required_size);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Load Bitaxe IP
    required_size = sizeof(config->bitaxe_ip);
    ret = nvs_get_str(nvs_handle, "bitaxe_ip", config->bitaxe_ip, &required_size);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    config->valid = true;
    
    ESP_LOGI(TAG, "Loaded config from NVS - SSID: %s, Bitaxe IP: %s", 
             config->wifi_ssid, config->bitaxe_ip);
    
    return ESP_OK;
}
