#include "best_shares.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "best_shares";
static const char *NVS_NAMESPACE = "best_shares";
static const char *NVS_KEY = "shares";

static best_share_t best_shares[MAX_BEST_SHARES] = {0};
static bool initialized = false;

// Load best shares from NVS
static esp_err_t load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (ret == ESP_OK) {
        size_t required_size = sizeof(best_shares);
        ret = nvs_get_blob(nvs_handle, NVS_KEY, best_shares, &required_size);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Loaded %d best shares from NVS", best_shares_get_count());
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved best shares found, starting fresh");
            ret = ESP_OK;
        }

        nvs_close(nvs_handle);
    }

    return ret;
}

// Save best shares to NVS
static esp_err_t save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);

    if (ret == ESP_OK) {
        ret = nvs_set_blob(nvs_handle, NVS_KEY, best_shares, sizeof(best_shares));

        if (ret == ESP_OK) {
            ret = nvs_commit(nvs_handle);
            ESP_LOGI(TAG, "Saved %d best shares to NVS", best_shares_get_count());
        }

        nvs_close(nvs_handle);
    }

    return ret;
}

esp_err_t best_shares_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    // Initialize array
    memset(best_shares, 0, sizeof(best_shares));

    // Load from NVS
    esp_err_t ret = load_from_nvs();

    if (ret == ESP_OK) {
        initialized = true;
        ESP_LOGI(TAG, "Best shares tracker initialized");
    } else {
        ESP_LOGE(TAG, "Failed to initialize best shares tracker");
    }

    return ret;
}

void best_shares_add(uint64_t diff, time_t timestamp)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Best shares not initialized");
        return;
    }

    // Find position to insert (sorted by difficulty, highest first)
    int insert_pos = -1;

    for (int i = 0; i < MAX_BEST_SHARES; i++) {
        if (!best_shares[i].valid || diff > best_shares[i].difficulty) {
            insert_pos = i;
            break;
        }
    }

    // If not in top 10, ignore
    if (insert_pos == -1) {
        return;
    }

    // Shift entries down to make room
    for (int i = MAX_BEST_SHARES - 1; i > insert_pos; i--) {
        best_shares[i] = best_shares[i - 1];
    }

    // Insert new share
    best_shares[insert_pos].difficulty = diff;
    best_shares[insert_pos].timestamp = timestamp;
    best_shares[insert_pos].valid = true;

    ESP_LOGI(TAG, "New share added at position %d: %llu", insert_pos + 1, (unsigned long long)diff);

    // Save to NVS
    save_to_nvs();
}

const best_share_t* best_shares_get_top10(void)
{
    return best_shares;
}

uint8_t best_shares_get_count(void)
{
    uint8_t count = 0;

    for (int i = 0; i < MAX_BEST_SHARES; i++) {
        if (best_shares[i].valid) {
            count++;
        }
    }

    return count;
}
