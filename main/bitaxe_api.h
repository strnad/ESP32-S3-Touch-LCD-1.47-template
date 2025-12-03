#ifndef BITAXE_API_H
#define BITAXE_API_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bitaxe mining data structure
 */
typedef struct {
    // Power metrics
    float power;
    float voltage;
    float current;
    
    // Temperature
    float temp;
    float temp2;
    float vrTemp;
    
    // Hashing
    float hashRate;
    float expectedHashrate;
    float errorPercentage;
    float efficiency; // Calculated: hashRate/expectedHashrate * 100
    
    // Difficulty
    uint64_t bestDiff;
    uint64_t bestSessionDiff;
    uint64_t poolDifficulty;
    uint64_t networkDifficulty;
    
    // Mining status
    uint32_t blockFound;
    uint32_t blockHeight;
    uint32_t sharesAccepted;
    uint32_t sharesRejected;
    
    // System
    uint32_t uptimeSeconds;
    uint32_t freeHeap;
    int32_t coreVoltage;
    int32_t coreVoltageActual;
    uint32_t frequency;
    
    // Fan
    uint32_t fanspeed;
    uint32_t fanrpm;
    bool autofanspeed;
    uint32_t overheat_mode;
    
    // WiFi
    char ssid[64];
    char wifiStatus[32];
    int32_t wifiRSSI;
    char ipv4[32];
    char hostname[64];
    
    // Pool
    char stratumURL[128];
    uint32_t stratumPort;
    char stratumUser[256];
    char fallbackStratumURL[128];
    uint32_t fallbackStratumPort;
    char fallbackStratumUser[256];
    bool isUsingFallbackStratum;
    
    // Info
    char version[32];
    char ASICModel[32];
    char boardVersion[16];
    
    // Response
    float responseTime;
    bool valid;
} bitaxe_data_t;

/**
 * @brief Initialize Bitaxe API client
 * @param ip_address IP address of Bitaxe device
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_init(const char *ip_address);

/**
 * @brief Fetch system info from Bitaxe
 * @param data Pointer to data structure to fill
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_get_system_info(bitaxe_data_t *data);

/**
 * @brief Restart Bitaxe device
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_restart(void);

/**
 * @brief Update Bitaxe frequency
 * @param frequency Frequency in MHz
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_set_frequency(uint32_t frequency);

/**
 * @brief Update Bitaxe core voltage
 * @param voltage Voltage in mV
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_set_voltage(uint32_t voltage);

/**
 * @brief Switch to primary pool
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_switch_to_primary_pool(void);

/**
 * @brief Switch to fallback pool
 * @return ESP_OK on success
 */
esp_err_t bitaxe_api_switch_to_fallback_pool(void);

/**
 * @brief Get Bitaxe IP address
 * @return IP address string
 */
const char* bitaxe_api_get_ip(void);

#ifdef __cplusplus
}
#endif

#endif // BITAXE_API_H
