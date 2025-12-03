#include "ui_manager.h"
#include "gesture_handler.h"
#include "bitaxe_api.h"
#include "data_refresh_task.h"
#include "best_shares.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include <stdio.h>
#include <math.h>
#include <time.h>

static const char *TAG = "ui_manager";

// Screens
static lv_obj_t *screens[SCREEN_COUNT] = {NULL};
static screen_index_t current_screen = SCREEN_DASHBOARD;

// Status indicators (removed status bar)

// Block found overlay
static lv_obj_t *block_overlay = NULL;
static lv_obj_t *block_label = NULL;

// Page indicator
static lv_obj_t *page_indicator = NULL;
static lv_obj_t *page_dots[SCREEN_COUNT] = {NULL};

// Dashboard widgets
static lv_obj_t *hashrate_arc = NULL;
static lv_obj_t *hashrate_label = NULL;
static lv_obj_t *efficiency_label = NULL;
static lv_obj_t *efficiency_jth_label = NULL;
static lv_obj_t *temp_label = NULL;
static lv_obj_t *temp_vr_label = NULL;
static lv_obj_t *power_label = NULL;
static lv_obj_t *pool_label = NULL;
static lv_obj_t *wifi_status_label = NULL;

// Difficulty widgets
static lv_obj_t *network_diff_label = NULL;
static lv_obj_t *pool_diff_label = NULL;
static lv_obj_t *session_diff_label = NULL;
static lv_obj_t *alltime_diff_label = NULL;
static lv_obj_t *shares_bar = NULL;
static lv_obj_t *shares_label = NULL;
static lv_obj_t *error_label = NULL;
static lv_obj_t *best_shares_list = NULL;
static lv_obj_t *best_shares_labels[10] = {NULL};

// Statistics widgets
static lv_obj_t *uptime_label = NULL;
static lv_obj_t *voltage_label = NULL;
static lv_obj_t *frequency_label = NULL;
static lv_obj_t *fan_label = NULL;
static lv_obj_t *asic_label = NULL;
static lv_obj_t *version_label = NULL;

// Control widgets (removed frequency/voltage controls)
static lv_obj_t *primary_pool_btn = NULL;
static lv_obj_t *fallback_pool_btn = NULL;
static lv_obj_t *restart_btn = NULL;

// Settings widgets
static lv_obj_t *wifi_info_label = NULL;
static lv_obj_t *ip_label = NULL;
static lv_obj_t *board_label = NULL;
static lv_obj_t *response_label = NULL;

// Forward declarations
static void create_page_indicator(void);
static void create_dashboard_screen(void);
static void create_difficulty_screen(void);
static void create_statistics_screen(void);
static void create_control_screen(void);
static void create_settings_screen(void);
static void format_difficulty(char *buf, size_t len, uint64_t diff);
static void format_uptime(char *buf, size_t len, uint32_t seconds);
static void format_time_ago(char *buf, size_t len, time_t timestamp);

// Button callbacks
static void primary_pool_btn_callback(lv_event_t *e);
static void fallback_pool_btn_callback(lv_event_t *e);
static void restart_btn_callback(lv_event_t *e);

// Dark theme colors
#define BG_COLOR lv_color_hex(0x0d0d0d)
#define CARD_COLOR lv_color_hex(0x1a1a1a)
#define TEXT_COLOR lv_color_hex(0xffffff)
#define ACCENT_COLOR lv_color_hex(0x00ff00)
#define ERROR_COLOR lv_color_hex(0xff4444)
#define WARNING_COLOR lv_color_hex(0xffaa00)

esp_err_t ui_manager_init(void)
{
    // Set dark theme
    lv_theme_t *theme = lv_theme_default_init(
        lv_disp_get_default(),
        ACCENT_COLOR,
        ERROR_COLOR,
        true,
        LV_FONT_DEFAULT
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);

    // Create all screens (without status bar and page indicator)
    create_dashboard_screen();
    create_difficulty_screen();
    create_statistics_screen();
    create_control_screen();
    create_settings_screen();

    // Register gesture callbacks on all screens
    for (int i = 0; i < SCREEN_COUNT; i++) {
        gesture_handler_register_screen(screens[i]);
    }

    // Load dashboard screen
    lv_scr_load(screens[SCREEN_DASHBOARD]);

    // Create global page indicator on top layer
    // This will be visible across all screens without duplication
    create_page_indicator();

    ESP_LOGI(TAG, "UI Manager initialized with %d screens", SCREEN_COUNT);
    return ESP_OK;
}

static void create_page_indicator(void)
{
    // Create page indicator on top layer so it's visible across all screens
    page_indicator = lv_obj_create(lv_layer_top());
    lv_obj_set_size(page_indicator, LV_PCT(100), 20);
    lv_obj_align(page_indicator, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(page_indicator, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_indicator, 0, 0);
    lv_obj_set_style_pad_all(page_indicator, 0, 0);

    // Create dots
    int dot_spacing = 15;
    int total_width = (SCREEN_COUNT - 1) * dot_spacing;
    int start_x = -total_width / 2;

    for (int i = 0; i < SCREEN_COUNT; i++) {
        page_dots[i] = lv_obj_create(page_indicator);
        lv_obj_set_size(page_dots[i], 8, 8);
        lv_obj_align(page_dots[i], LV_ALIGN_CENTER, start_x + i * dot_spacing, 0);
        lv_obj_set_style_radius(page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(page_dots[i], 0, 0);
        lv_obj_set_style_bg_color(page_dots[i], lv_color_hex(0x666666), 0);
    }

    // Highlight first dot
    lv_obj_set_style_bg_color(page_dots[0], TEXT_COLOR, 0);
}

static void create_dashboard_screen(void)
{
    screens[SCREEN_DASHBOARD] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DASHBOARD], BG_COLOR, 0);

    // Main content container - full screen with padding only for page indicator
    lv_obj_t *content = lv_obj_create(screens[SCREEN_DASHBOARD]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);   // Padding for page indicator
    lv_obj_set_style_pad_left(content, 5, 0);
    lv_obj_set_style_pad_right(content, 5, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // WiFi status at top (full width)
    wifi_status_label = lv_label_create(content);
    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI " ---");
    lv_obj_set_style_text_color(wifi_status_label, ACCENT_COLOR, 0);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_12, 0);

    // Main horizontal container (circle left, info right)
    lv_obj_t *main_row = lv_obj_create(content);
    lv_obj_set_size(main_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 3, 0);
    lv_obj_set_flex_flow(main_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Left column: Hashrate arc
    lv_obj_t *left_col = lv_obj_create(main_row);
    lv_obj_set_size(left_col, 130, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_col, 0, 0);
    lv_obj_set_style_pad_all(left_col, 0, 0);
    lv_obj_set_flex_flow(left_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hashrate_arc = lv_arc_create(left_col);
    lv_obj_set_size(hashrate_arc, 120, 120);
    lv_arc_set_range(hashrate_arc, 0, 100);
    lv_arc_set_value(hashrate_arc, 0);
    lv_obj_remove_style(hashrate_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(hashrate_arc, ACCENT_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(hashrate_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(hashrate_arc, 10, LV_PART_MAIN);

    hashrate_label = lv_label_create(hashrate_arc);
    lv_label_set_text(hashrate_label, "0.0\nGH/s");
    lv_obj_center(hashrate_label);
    lv_obj_set_style_text_align(hashrate_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hashrate_label, TEXT_COLOR, 0);

    // Right column: All info
    lv_obj_t *right_col = lv_obj_create(main_row);
    lv_obj_set_flex_grow(right_col, 1);
    lv_obj_set_height(right_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);
    lv_obj_set_style_pad_all(right_col, 3, 0);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Efficiency labels
    efficiency_label = lv_label_create(right_col);
    lv_label_set_text(efficiency_label, "Eff: ---%");
    lv_obj_set_style_text_color(efficiency_label, ACCENT_COLOR, 0);

    efficiency_jth_label = lv_label_create(right_col);
    lv_label_set_text(efficiency_jth_label, "--- J/TH");
    lv_obj_set_style_text_color(efficiency_jth_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(efficiency_jth_label, &lv_font_montserrat_12, 0);

    // Temperature cards (stacked vertically)
    lv_obj_t *temp_card = lv_obj_create(right_col);
    lv_obj_set_size(temp_card, LV_PCT(95), 28);
    lv_obj_set_style_bg_color(temp_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(temp_card, 0, 0);
    lv_obj_set_style_pad_all(temp_card, 3, 0);
    temp_label = lv_label_create(temp_card);
    lv_label_set_text(temp_label, LV_SYMBOL_IMAGE " --°C");
    lv_obj_center(temp_label);
    lv_obj_set_style_text_color(temp_label, TEXT_COLOR, 0);

    lv_obj_t *temp_vr_card = lv_obj_create(right_col);
    lv_obj_set_size(temp_vr_card, LV_PCT(95), 28);
    lv_obj_set_style_bg_color(temp_vr_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(temp_vr_card, 0, 0);
    lv_obj_set_style_pad_all(temp_vr_card, 3, 0);
    temp_vr_label = lv_label_create(temp_vr_card);
    lv_label_set_text(temp_vr_label, "VR --°C");
    lv_obj_center(temp_vr_label);
    lv_obj_set_style_text_color(temp_vr_label, TEXT_COLOR, 0);

    lv_obj_t *power_card = lv_obj_create(right_col);
    lv_obj_set_size(power_card, LV_PCT(95), 28);
    lv_obj_set_style_bg_color(power_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(power_card, 0, 0);
    lv_obj_set_style_pad_all(power_card, 3, 0);
    power_label = lv_label_create(power_card);
    lv_label_set_text(power_label, LV_SYMBOL_CHARGE " --W");
    lv_obj_center(power_label);
    lv_obj_set_style_text_color(power_label, TEXT_COLOR, 0);

    // Pool status (full width below)
    pool_label = lv_label_create(content);
    lv_label_set_text(pool_label, "Pool: Connecting...");
    lv_obj_set_style_text_color(pool_label, WARNING_COLOR, 0);
    lv_obj_set_style_text_font(pool_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(pool_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(pool_label, LV_PCT(95));
}

static void create_difficulty_screen(void)
{
    screens[SCREEN_DIFFICULTY] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DIFFICULTY], BG_COLOR, 0);

    lv_obj_t *content = lv_obj_create(screens[SCREEN_DIFFICULTY]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);
    lv_obj_set_style_pad_left(content, 8, 0);
    lv_obj_set_style_pad_right(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Network difficulty
    lv_obj_t *title1 = lv_label_create(content);
    lv_label_set_text(title1, "Network Difficulty:");
    lv_obj_set_style_text_color(title1, lv_color_hex(0xaaaaaa), 0);
    
    network_diff_label = lv_label_create(content);
    lv_label_set_text(network_diff_label, "---");
    lv_obj_set_style_text_color(network_diff_label, ACCENT_COLOR, 0);
    lv_obj_set_style_text_font(network_diff_label, &lv_font_montserrat_20, 0);
    
    // Pool difficulty
    lv_obj_t *title2 = lv_label_create(content);
    lv_label_set_text(title2, "Pool Difficulty:");
    lv_obj_set_style_text_color(title2, lv_color_hex(0xaaaaaa), 0);
    
    pool_diff_label = lv_label_create(content);
    lv_label_set_text(pool_diff_label, "---");
    lv_obj_set_style_text_color(pool_diff_label, TEXT_COLOR, 0);
    
    // Session best
    lv_obj_t *title3 = lv_label_create(content);
    lv_label_set_text(title3, "Session Best:");
    lv_obj_set_style_text_color(title3, lv_color_hex(0xaaaaaa), 0);
    
    session_diff_label = lv_label_create(content);
    lv_label_set_text(session_diff_label, "---");
    lv_obj_set_style_text_color(session_diff_label, WARNING_COLOR, 0);
    
    // All-time best
    lv_obj_t *title4 = lv_label_create(content);
    lv_label_set_text(title4, "All-Time Best:");
    lv_obj_set_style_text_color(title4, lv_color_hex(0xaaaaaa), 0);
    
    alltime_diff_label = lv_label_create(content);
    lv_label_set_text(alltime_diff_label, "---");
    lv_obj_set_style_text_color(alltime_diff_label, ACCENT_COLOR, 0);
    
    // Shares bar
    shares_bar = lv_bar_create(content);
    lv_obj_set_size(shares_bar, LV_PCT(95), 20);
    lv_bar_set_range(shares_bar, 0, 100);
    lv_bar_set_value(shares_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(shares_bar, ERROR_COLOR, LV_PART_INDICATOR);
    
    shares_label = lv_label_create(content);
    lv_label_set_text(shares_label, "Shares: 0 / 0");
    lv_obj_set_style_text_color(shares_label, TEXT_COLOR, 0);
    
    // Error percentage
    error_label = lv_label_create(content);
    lv_label_set_text(error_label, "Error: --%");
    lv_obj_set_style_text_color(error_label, TEXT_COLOR, 0);

    // Top 10 best shares section
    lv_obj_t *title5 = lv_label_create(content);
    lv_label_set_text(title5, "Top 10 Best Shares:");
    lv_obj_set_style_text_color(title5, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_pad_top(title5, 8, 0);

    // Create labels for top 10 shares
    for (int i = 0; i < 10; i++) {
        best_shares_labels[i] = lv_label_create(content);
        lv_label_set_text(best_shares_labels[i], "--");
        lv_obj_set_style_text_color(best_shares_labels[i], TEXT_COLOR, 0);
        lv_obj_set_style_text_font(best_shares_labels[i], &lv_font_montserrat_12, 0);
    }
}

static void create_statistics_screen(void)
{
    screens[SCREEN_STATISTICS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_STATISTICS], BG_COLOR, 0);

    lv_obj_t *content = lv_obj_create(screens[SCREEN_STATISTICS]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);
    lv_obj_set_style_pad_left(content, 8, 0);
    lv_obj_set_style_pad_right(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    uptime_label = lv_label_create(content);
    lv_label_set_text(uptime_label, "Uptime: --:--:--");
    lv_obj_set_style_text_color(uptime_label, TEXT_COLOR, 0);
    
    voltage_label = lv_label_create(content);
    lv_label_set_text(voltage_label, "Voltage: ---- mV");
    lv_obj_set_style_text_color(voltage_label, TEXT_COLOR, 0);
    
    frequency_label = lv_label_create(content);
    lv_label_set_text(frequency_label, "Frequency: --- MHz");
    lv_obj_set_style_text_color(frequency_label, TEXT_COLOR, 0);
    
    fan_label = lv_label_create(content);
    lv_label_set_text(fan_label, "Fan: ---- RPM");
    lv_obj_set_style_text_color(fan_label, TEXT_COLOR, 0);
    
    asic_label = lv_label_create(content);
    lv_label_set_text(asic_label, "ASIC: Unknown");
    lv_obj_set_style_text_color(asic_label, ACCENT_COLOR, 0);
    
    version_label = lv_label_create(content);
    lv_label_set_text(version_label, "Version: ---");
    lv_obj_set_style_text_color(version_label, TEXT_COLOR, 0);
}

static void create_control_screen(void)
{
    screens[SCREEN_CONTROL] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_CONTROL], BG_COLOR, 0);

    lv_obj_t *content = lv_obj_create(screens[SCREEN_CONTROL]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);
    lv_obj_set_style_pad_left(content, 5, 0);
    lv_obj_set_style_pad_right(content, 5, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Pool buttons
    primary_pool_btn = lv_btn_create(content);
    lv_obj_set_size(primary_pool_btn, LV_PCT(90), 45);
    lv_obj_t *primary_label = lv_label_create(primary_pool_btn);
    lv_label_set_text(primary_label, "Primary Pool");
    lv_obj_set_style_text_font(primary_label, &lv_font_montserrat_16, 0);
    lv_obj_center(primary_label);
    lv_obj_add_event_cb(primary_pool_btn, primary_pool_btn_callback, LV_EVENT_CLICKED, NULL);

    fallback_pool_btn = lv_btn_create(content);
    lv_obj_set_size(fallback_pool_btn, LV_PCT(90), 45);
    lv_obj_t *fallback_label = lv_label_create(fallback_pool_btn);
    lv_label_set_text(fallback_label, "Fallback Pool");
    lv_obj_set_style_text_font(fallback_label, &lv_font_montserrat_16, 0);
    lv_obj_center(fallback_label);
    lv_obj_add_event_cb(fallback_pool_btn, fallback_pool_btn_callback, LV_EVENT_CLICKED, NULL);

    // Restart button
    restart_btn = lv_btn_create(content);
    lv_obj_set_size(restart_btn, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(restart_btn, ERROR_COLOR, 0);
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, LV_SYMBOL_POWER " Restart Miner");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_16, 0);
    lv_obj_center(restart_label);
    lv_obj_add_event_cb(restart_btn, restart_btn_callback, LV_EVENT_CLICKED, NULL);
}

static void create_settings_screen(void)
{
    screens[SCREEN_SETTINGS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SETTINGS], BG_COLOR, 0);

    lv_obj_t *content = lv_obj_create(screens[SCREEN_SETTINGS]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);
    lv_obj_set_style_pad_left(content, 8, 0);
    lv_obj_set_style_pad_right(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    wifi_info_label = lv_label_create(content);
    lv_label_set_text(wifi_info_label, "WiFi: ---");
    lv_obj_set_style_text_color(wifi_info_label, TEXT_COLOR, 0);
    
    ip_label = lv_label_create(content);
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_set_style_text_color(ip_label, TEXT_COLOR, 0);
    
    board_label = lv_label_create(content);
    lv_label_set_text(board_label, "Board: ---");
    lv_obj_set_style_text_color(board_label, TEXT_COLOR, 0);
    
    response_label = lv_label_create(content);
    lv_label_set_text(response_label, "Response: --- ms");
    lv_obj_set_style_text_color(response_label, TEXT_COLOR, 0);
}

void ui_manager_update_data(const bitaxe_data_t *data)
{
    if (!data || !data->valid) {
        return;
    }
    
    char buf[128];
    
    // Update Dashboard
    if (hashrate_arc) {
        int pct = (int)((data->hashRate / data->expectedHashrate) * 100);
        if (pct > 100) pct = 100;
        lv_arc_set_value(hashrate_arc, pct);

        snprintf(buf, sizeof(buf), "%.1f\nGH/s", data->hashRate);
        lv_label_set_text(hashrate_label, buf);

        snprintf(buf, sizeof(buf), "Eff: %.1f%%", data->efficiency);
        lv_label_set_text(efficiency_label, buf);

        // Color efficiency based on value
        if (data->efficiency >= 95) {
            lv_obj_set_style_text_color(efficiency_label, ACCENT_COLOR, 0);
        } else if (data->efficiency >= 85) {
            lv_obj_set_style_text_color(efficiency_label, WARNING_COLOR, 0);
        } else {
            lv_obj_set_style_text_color(efficiency_label, ERROR_COLOR, 0);
        }
    }

    // Calculate and display J/TH efficiency
    if (efficiency_jth_label && data->hashRate > 0) {
        // J/TH = (Power in W / Hashrate in TH/s)
        // Hashrate is in GH/s, so divide by 1000 to get TH/s
        float jth = (data->power / (data->hashRate / 1000.0f));
        snprintf(buf, sizeof(buf), "%.1f J/TH", jth);
        lv_label_set_text(efficiency_jth_label, buf);
    }

    if (temp_label) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_IMAGE " %.0f°C", data->temp);
        lv_label_set_text(temp_label, buf);

        // Color temperature based on value
        if (data->temp >= 75) {
            lv_obj_set_style_text_color(temp_label, ERROR_COLOR, 0);
        } else if (data->temp >= 65) {
            lv_obj_set_style_text_color(temp_label, WARNING_COLOR, 0);
        } else {
            lv_obj_set_style_text_color(temp_label, TEXT_COLOR, 0);
        }
    }

    if (temp_vr_label) {
        snprintf(buf, sizeof(buf), "VR %.0f°C", data->vrTemp);
        lv_label_set_text(temp_vr_label, buf);

        // Color VR temperature
        if (data->vrTemp >= 85) {
            lv_obj_set_style_text_color(temp_vr_label, ERROR_COLOR, 0);
        } else if (data->vrTemp >= 75) {
            lv_obj_set_style_text_color(temp_vr_label, WARNING_COLOR, 0);
        } else {
            lv_obj_set_style_text_color(temp_vr_label, TEXT_COLOR, 0);
        }
    }

    if (power_label) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " %.0fW", data->power);
        lv_label_set_text(power_label, buf);
    }

    // Update WiFi status at top of dashboard
    if (wifi_status_label) {
        bool wifi_ok = (data->wifiRSSI > -80);
        lv_obj_set_style_text_color(wifi_status_label, wifi_ok ? ACCENT_COLOR : WARNING_COLOR, 0);

        if (data->isUsingFallbackStratum) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s " LV_SYMBOL_WARNING " Fallback", data->ssid);
            lv_obj_set_style_text_color(wifi_status_label, WARNING_COLOR, 0);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s (RSSI: %ld)", data->ssid, (long)data->wifiRSSI);
        }
        lv_label_set_text(wifi_status_label, buf);
    }
    
    if (pool_label) {
        if (data->isUsingFallbackStratum) {
            snprintf(buf, sizeof(buf), "Pool: %.110s (Fallback)", data->fallbackStratumURL);
            lv_obj_set_style_text_color(pool_label, WARNING_COLOR, 0);
        } else {
            snprintf(buf, sizeof(buf), "Pool: %.120s", data->stratumURL);
            lv_obj_set_style_text_color(pool_label, ACCENT_COLOR, 0);
        }
        lv_label_set_text(pool_label, buf);
    }
    
    // Update Difficulty screen
    if (network_diff_label) {
        format_difficulty(buf, sizeof(buf), data->networkDifficulty);
        lv_label_set_text(network_diff_label, buf);
    }
    
    if (pool_diff_label) {
        format_difficulty(buf, sizeof(buf), data->poolDifficulty);
        lv_label_set_text(pool_diff_label, buf);
    }
    
    if (session_diff_label) {
        format_difficulty(buf, sizeof(buf), data->bestSessionDiff);
        lv_label_set_text(session_diff_label, buf);
    }

    if (alltime_diff_label) {
        uint64_t all_time_best = data_refresh_task_get_all_time_best();
        format_difficulty(buf, sizeof(buf), all_time_best);
        lv_label_set_text(alltime_diff_label, buf);
    }
    
    if (shares_bar && shares_label) {
        uint32_t total = data->sharesAccepted + data->sharesRejected;
        int accept_pct = total > 0 ? (data->sharesAccepted * 100 / total) : 0;
        lv_bar_set_value(shares_bar, accept_pct, LV_ANIM_ON);
        
        snprintf(buf, sizeof(buf), "Shares: %lu / %lu", (unsigned long)data->sharesAccepted, (unsigned long)data->sharesRejected);
        lv_label_set_text(shares_label, buf);
    }
    
    if (error_label) {
        snprintf(buf, sizeof(buf), "Error: %.2f%%", data->errorPercentage);
        lv_label_set_text(error_label, buf);
    }

    // Update top 10 best shares
    const best_share_t* top_shares = best_shares_get_top10();
    uint8_t share_count = best_shares_get_count();

    for (int i = 0; i < 10; i++) {
        if (best_shares_labels[i]) {
            if (i < share_count && top_shares[i].valid) {
                char diff_str[32];
                char time_str[32];
                format_difficulty(diff_str, sizeof(diff_str), top_shares[i].difficulty);
                format_time_ago(time_str, sizeof(time_str), top_shares[i].timestamp);
                snprintf(buf, sizeof(buf), "%d. %s - %s", i + 1, diff_str, time_str);
                lv_label_set_text(best_shares_labels[i], buf);
            } else {
                lv_label_set_text(best_shares_labels[i], "--");
            }
        }
    }
    
    // Update Statistics screen
    if (uptime_label) {
        format_uptime(buf, sizeof(buf), data->uptimeSeconds);
        lv_label_set_text(uptime_label, buf);
    }
    
    if (voltage_label) {
        snprintf(buf, sizeof(buf), "Voltage: %ld mV (actual: %ld mV)", 
                 (long)data->coreVoltage, (long)data->coreVoltageActual);
        lv_label_set_text(voltage_label, buf);
    }
    
    if (frequency_label) {
        snprintf(buf, sizeof(buf), "Frequency: %lu MHz", (unsigned long)data->frequency);
        lv_label_set_text(frequency_label, buf);
    }
    
    if (fan_label) {
        snprintf(buf, sizeof(buf), "Fan: %lu RPM (%lu%%)", (unsigned long)data->fanrpm, (unsigned long)data->fanspeed);
        lv_label_set_text(fan_label, buf);
    }
    
    if (asic_label) {
        snprintf(buf, sizeof(buf), "ASIC: %s", data->ASICModel);
        lv_label_set_text(asic_label, buf);
    }
    
    if (version_label) {
        snprintf(buf, sizeof(buf), "Version: %s", data->version);
        lv_label_set_text(version_label, buf);
    }
    
    // Update Settings screen
    if (wifi_info_label) {
        snprintf(buf, sizeof(buf), "WiFi: %.60s (RSSI: %ld)", data->ssid, (long)data->wifiRSSI);
        lv_label_set_text(wifi_info_label, buf);
    }
    
    if (ip_label) {
        snprintf(buf, sizeof(buf), "IP: %s\nHostname: %s", data->ipv4, data->hostname);
        lv_label_set_text(ip_label, buf);
    }
    
    if (board_label) {
        snprintf(buf, sizeof(buf), "Board: %s", data->boardVersion);
        lv_label_set_text(board_label, buf);
    }
    
    if (response_label) {
        snprintf(buf, sizeof(buf), "Response: %.1f ms", data->responseTime);
        lv_label_set_text(response_label, buf);
    }
}

void ui_manager_goto_screen(screen_index_t screen, bool animate, bool direction_left)
{
    if (screen >= SCREEN_COUNT) {
        return;
    }

    current_screen = screen;

    // Update page indicator
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (page_dots[i]) {
            lv_obj_set_style_bg_color(page_dots[i],
                                      i == screen ? TEXT_COLOR : lv_color_hex(0x666666), 0);
        }
    }

    if (animate) {
        lv_scr_load_anim_t anim_type = direction_left ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        lv_scr_load_anim(screens[screen], anim_type, 250, 0, false);
    } else {
        lv_scr_load(screens[screen]);
    }
}

screen_index_t ui_manager_get_current_screen(void)
{
    return current_screen;
}

void ui_manager_show_block_found(uint32_t block_height, uint64_t best_diff)
{
    if (block_overlay) {
        return; // Already showing
    }
    
    // Create overlay on top of all screens
    block_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(block_overlay, LV_PCT(100), 70);
    lv_obj_align(block_overlay, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(block_overlay, ACCENT_COLOR, 0);
    lv_obj_set_style_border_width(block_overlay, 0, 0);
    lv_obj_set_style_radius(block_overlay, 0, 0);
    
    block_label = lv_label_create(block_overlay);
    char buf[128];
    snprintf(buf, sizeof(buf), LV_SYMBOL_OK " BLOCK FOUND! " LV_SYMBOL_OK "\nHeight: %lu\nDiff: %llu", 
             (unsigned long)block_height, (unsigned long long)best_diff);
    lv_label_set_text(block_label, buf);
    lv_obj_center(block_label);
    lv_obj_set_style_text_align(block_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(block_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(block_label, &lv_font_montserrat_16, 0);
    
    // TODO: Add pulsing animation
    
    ESP_LOGI(TAG, "Block found alert displayed!");
}

void ui_manager_hide_block_found(void)
{
    if (block_overlay) {
        lv_obj_del(block_overlay);
        block_overlay = NULL;
        block_label = NULL;
    }
}

// Status bar removed - status info now integrated into dashboard screen

// Button callback implementations
static void primary_pool_btn_callback(lv_event_t *e)
{
    ESP_LOGI(TAG, "Primary pool button clicked");
    esp_err_t err = bitaxe_api_switch_to_primary_pool();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to switch to primary pool");
    }
}

static void fallback_pool_btn_callback(lv_event_t *e)
{
    ESP_LOGI(TAG, "Fallback pool button clicked");
    esp_err_t err = bitaxe_api_switch_to_fallback_pool();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to switch to fallback pool");
    }
}

static void restart_btn_callback(lv_event_t *e)
{
    ESP_LOGI(TAG, "Restart button clicked");
    esp_err_t err = bitaxe_api_restart();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Restart command sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send restart command");
    }
}

static void format_difficulty(char *buf, size_t len, uint64_t diff)
{
    if (diff >= 1000000000000ULL) {
        snprintf(buf, len, "%.2f T", diff / 1000000000000.0);
    } else if (diff >= 1000000000ULL) {
        snprintf(buf, len, "%.2f G", diff / 1000000000.0);
    } else if (diff >= 1000000ULL) {
        snprintf(buf, len, "%.2f M", diff / 1000000.0);
    } else if (diff >= 1000ULL) {
        snprintf(buf, len, "%.2f K", diff / 1000.0);
    } else {
        snprintf(buf, len, "%llu", (unsigned long long)diff);
    }
}

static void format_uptime(char *buf, size_t len, uint32_t seconds)
{
    unsigned long hours = seconds / 3600;
    unsigned long mins = (seconds % 3600) / 60;
    unsigned long secs = seconds % 60;
    snprintf(buf, len, "Uptime: %02lu:%02lu:%02lu", hours, mins, secs);
}

static void format_time_ago(char *buf, size_t len, time_t timestamp)
{
    time_t now = time(NULL);
    int64_t diff = (int64_t)(now - timestamp);

    if (diff < 0) {
        snprintf(buf, len, "future");
    } else if (diff < 60) {
        snprintf(buf, len, "%ds ago", (int)diff);
    } else if (diff < 3600) {
        snprintf(buf, len, "%dm ago", (int)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, len, "%dh %dm ago", (int)(diff / 3600), (int)((diff % 3600) / 60));
    } else {
        snprintf(buf, len, "%dd ago", (int)(diff / 86400));
    }
}
