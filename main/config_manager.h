#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for Bitaxe dashboard
 */
typedef struct {
    char wifi_ssid[64];
    char wifi_password[128];
    char bitaxe_ip[64];
    bool valid;
} bitaxe_config_t;

/**
 * @brief Initialize configuration manager
 * Attempts to load config from SD card, falls back to NVS if SD unavailable
 * @return ESP_OK on success
 */
esp_err_t config_manager_init(void);

/**
 * @brief Get current configuration
 * @param config Pointer to configuration structure to fill
 * @return ESP_OK on success
 */
esp_err_t config_manager_get(bitaxe_config_t *config);

/**
 * @brief Save configuration to NVS
 * @param config Pointer to configuration to save
 * @return ESP_OK on success
 */
esp_err_t config_manager_save(const bitaxe_config_t *config);

/**
 * @brief Load configuration from SD card config.json
 * @param config Pointer to configuration structure to fill
 * @return ESP_OK on success, ESP_FAIL if file doesn't exist or parse error
 */
esp_err_t config_manager_load_from_sd(bitaxe_config_t *config);

/**
 * @brief Load configuration from NVS flash
 * @param config Pointer to configuration structure to fill
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not found
 */
esp_err_t config_manager_load_from_nvs(bitaxe_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
