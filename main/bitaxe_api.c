#include "bitaxe_api.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "bitaxe_api";
static char bitaxe_ip[64] = {0};
static char url_buffer[256] = {0};
static char response_buffer[8192] = {0};
static int response_len = 0;

// HTTP event handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response_len + evt->data_len < sizeof(response_buffer)) {
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t bitaxe_api_init(const char *ip_address)
{
    if (ip_address == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(bitaxe_ip, ip_address, sizeof(bitaxe_ip) - 1);
    ESP_LOGI(TAG, "Bitaxe API initialized for IP: %s", bitaxe_ip);
    
    return ESP_OK;
}

const char* bitaxe_api_get_ip(void)
{
    return bitaxe_ip;
}

esp_err_t bitaxe_api_get_system_info(bitaxe_data_t *data)
{
    if (data == NULL || bitaxe_ip[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build URL
    snprintf(url_buffer, sizeof(url_buffer), "http://%s/api/system/info", bitaxe_ip);
    
    // Reset response buffer
    memset(response_buffer, 0, sizeof(response_buffer));
    response_len = 0;
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url_buffer,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 4096,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Perform GET request
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 status_code, response_len);
        
        if (status_code == 200) {
            // Null terminate response
            response_buffer[response_len] = '\0';
            
            // Parse JSON
            cJSON *root = cJSON_Parse(response_buffer);
            if (root == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
            
            // Extract all fields
            cJSON *item;
            
            // Power metrics
            if ((item = cJSON_GetObjectItem(root, "power"))) data->power = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "voltage"))) data->voltage = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "current"))) data->current = item->valuedouble;
            
            // Temperature
            if ((item = cJSON_GetObjectItem(root, "temp"))) data->temp = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "temp2"))) data->temp2 = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "vrTemp"))) data->vrTemp = item->valuedouble;
            
            // Hashing
            if ((item = cJSON_GetObjectItem(root, "hashRate"))) data->hashRate = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "expectedHashrate"))) data->expectedHashrate = item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "errorPercentage"))) data->errorPercentage = item->valuedouble;
            
            // Calculate efficiency
            if (data->expectedHashrate > 0) {
                data->efficiency = (data->hashRate / data->expectedHashrate) * 100.0f;
            }
            
            // Difficulty
            if ((item = cJSON_GetObjectItem(root, "bestDiff"))) data->bestDiff = (uint64_t)item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "bestSessionDiff"))) data->bestSessionDiff = (uint64_t)item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "poolDifficulty"))) data->poolDifficulty = (uint64_t)item->valuedouble;
            if ((item = cJSON_GetObjectItem(root, "networkDifficulty"))) data->networkDifficulty = (uint64_t)item->valuedouble;
            
            // Mining status
            if ((item = cJSON_GetObjectItem(root, "blockFound"))) data->blockFound = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "blockHeight"))) data->blockHeight = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "sharesAccepted"))) data->sharesAccepted = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "sharesRejected"))) data->sharesRejected = item->valueint;
            
            // System
            if ((item = cJSON_GetObjectItem(root, "uptimeSeconds"))) data->uptimeSeconds = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "freeHeap"))) data->freeHeap = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "coreVoltage"))) data->coreVoltage = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "coreVoltageActual"))) data->coreVoltageActual = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "frequency"))) data->frequency = item->valueint;
            
            // Fan
            if ((item = cJSON_GetObjectItem(root, "fanspeed"))) data->fanspeed = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "fanrpm"))) data->fanrpm = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "autofanspeed"))) data->autofanspeed = item->valueint != 0;
            if ((item = cJSON_GetObjectItem(root, "overheat_mode"))) data->overheat_mode = item->valueint;
            
            // WiFi
            if ((item = cJSON_GetObjectItem(root, "ssid"))) {
                strncpy(data->ssid, item->valuestring, sizeof(data->ssid) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "wifiStatus"))) {
                strncpy(data->wifiStatus, item->valuestring, sizeof(data->wifiStatus) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "wifiRSSI"))) data->wifiRSSI = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "ipv4"))) {
                strncpy(data->ipv4, item->valuestring, sizeof(data->ipv4) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "hostname"))) {
                strncpy(data->hostname, item->valuestring, sizeof(data->hostname) - 1);
            }
            
            // Pool
            if ((item = cJSON_GetObjectItem(root, "stratumURL"))) {
                strncpy(data->stratumURL, item->valuestring, sizeof(data->stratumURL) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "stratumPort"))) data->stratumPort = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "stratumUser"))) {
                strncpy(data->stratumUser, item->valuestring, sizeof(data->stratumUser) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "fallbackStratumURL"))) {
                strncpy(data->fallbackStratumURL, item->valuestring, sizeof(data->fallbackStratumURL) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "fallbackStratumPort"))) data->fallbackStratumPort = item->valueint;
            if ((item = cJSON_GetObjectItem(root, "fallbackStratumUser"))) {
                strncpy(data->fallbackStratumUser, item->valuestring, sizeof(data->fallbackStratumUser) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "isUsingFallbackStratum"))) {
                data->isUsingFallbackStratum = item->valueint != 0;
            }
            
            // Info
            if ((item = cJSON_GetObjectItem(root, "version"))) {
                strncpy(data->version, item->valuestring, sizeof(data->version) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "ASICModel"))) {
                strncpy(data->ASICModel, item->valuestring, sizeof(data->ASICModel) - 1);
            }
            if ((item = cJSON_GetObjectItem(root, "boardVersion"))) {
                strncpy(data->boardVersion, item->valuestring, sizeof(data->boardVersion) - 1);
            }
            
            // Response time
            if ((item = cJSON_GetObjectItem(root, "responseTime"))) data->responseTime = item->valuedouble;
            
            data->valid = true;
            
            cJSON_Delete(root);
            esp_http_client_cleanup(client);
            
            ESP_LOGI(TAG, "System info fetched - Hashrate: %.2f GH/s, Temp: %.1f°C, Block Found: %u",
                     data->hashRate, data->temp, data->blockFound);
            
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t bitaxe_api_restart(void)
{
    if (bitaxe_ip[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    
    snprintf(url_buffer, sizeof(url_buffer), "http://%s/api/system/restart", bitaxe_ip);
    
    esp_http_client_config_t config = {
        .url = url_buffer,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Restart command sent successfully");
    } else {
        ESP_LOGE(TAG, "Restart command failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t bitaxe_api_set_frequency(uint32_t frequency)
{
    if (bitaxe_ip[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    
    snprintf(url_buffer, sizeof(url_buffer), "http://%s/api/system", bitaxe_ip);
    
    char post_data[128];
    snprintf(post_data, sizeof(post_data), "{\"frequency\":%lu}", (unsigned long)frequency);
    
    esp_http_client_config_t config = {
        .url = url_buffer,
        .method = HTTP_METHOD_PATCH,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Frequency set to %lu MHz", (unsigned long)frequency);
    } else {
        ESP_LOGE(TAG, "Set frequency failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t bitaxe_api_set_voltage(uint32_t voltage)
{
    if (bitaxe_ip[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    
    snprintf(url_buffer, sizeof(url_buffer), "http://%s/api/system", bitaxe_ip);
    
    char post_data[128];
    snprintf(post_data, sizeof(post_data), "{\"coreVoltage\":%lu}", (unsigned long)voltage);
    
    esp_http_client_config_t config = {
        .url = url_buffer,
        .method = HTTP_METHOD_PATCH,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Core voltage set to %lu mV", (unsigned long)voltage);
    } else {
        ESP_LOGE(TAG, "Set voltage failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t bitaxe_api_switch_to_primary_pool(void)
{
    // Note: Pool switching typically requires updating stratum settings
    // This is a placeholder - actual implementation depends on Bitaxe API
    ESP_LOGW(TAG, "Pool switching not fully implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bitaxe_api_switch_to_fallback_pool(void)
{
    // Note: Pool switching typically requires updating stratum settings
    // This is a placeholder - actual implementation depends on Bitaxe API
    ESP_LOGW(TAG, "Pool switching not fully implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}
