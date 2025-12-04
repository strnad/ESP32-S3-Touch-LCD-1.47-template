#ifndef BEST_SHARES_H
#define BEST_SHARES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BEST_SHARES 10

typedef struct {
    uint64_t difficulty;
    time_t timestamp;
    bool valid;
    bool is_all_time_best;  // Flag to mark the all-time best share
} best_share_t;

/**
 * @brief Initialize best shares tracker
 * @return ESP_OK on success
 */
esp_err_t best_shares_init(void);

/**
 * @brief Add a new share (will be added only if it's in top 10)
 * @param diff Difficulty of the share
 * @param timestamp Timestamp when share was found (0 if unknown)
 * @param is_all_time_best True if this is the all-time best share
 */
void best_shares_add(uint64_t diff, time_t timestamp, bool is_all_time_best);

/**
 * @brief Get array of top 10 best shares
 * @return Pointer to array of best shares (sorted by difficulty, highest first)
 */
const best_share_t* best_shares_get_top10(void);

/**
 * @brief Get count of valid shares in the list
 * @return Number of valid shares (0-10)
 */
uint8_t best_shares_get_count(void);

#ifdef __cplusplus
}
#endif

#endif // BEST_SHARES_H
