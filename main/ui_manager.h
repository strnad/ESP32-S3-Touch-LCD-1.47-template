#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl.h"
#include "bitaxe_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Screen indices
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_DIFFICULTY,
    SCREEN_STATISTICS,
    SCREEN_CONTROL,
    SCREEN_SETTINGS,
    SCREEN_COUNT
} screen_index_t;

/**
 * @brief Initialize UI manager and create all screens
 * @return ESP_OK on success
 */
esp_err_t ui_manager_init(void);

/**
 * @brief Update UI with new Bitaxe data
 * @param data Pointer to Bitaxe data
 * @note This function must be called with LVGL lock acquired
 */
void ui_manager_update_data(const bitaxe_data_t *data);

/**
 * @brief Navigate to specific screen
 * @param screen Screen index
 * @param animate Whether to animate the transition
 * @param direction_left true for left animation, false for right animation
 */
void ui_manager_goto_screen(screen_index_t screen, bool animate, bool direction_left);

/**
 * @brief Get current screen index
 * @return Current screen index
 */
screen_index_t ui_manager_get_current_screen(void);

/**
 * @brief Show block found alert overlay
 * @param block_height Block height
 * @param best_diff Best difficulty
 */
void ui_manager_show_block_found(uint32_t block_height, uint64_t best_diff);

/**
 * @brief Hide block found alert overlay
 */
void ui_manager_hide_block_found(void);

// Status bar removed - status info now integrated into dashboard screen

#ifdef __cplusplus
}
#endif

#endif // UI_MANAGER_H
