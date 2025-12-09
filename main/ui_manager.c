#include "ui_manager.h"
#include "gesture_handler.h"
#include "bitaxe_api.h"
#include "data_refresh_task.h"
#include "best_shares.h"
#include "chart_data_buffer.h"
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

// Connectivity warning
static lv_obj_t *connectivity_warning = NULL;
static lv_timer_t *connectivity_blink_timer = NULL;

// Page indicator
static lv_obj_t *page_indicator = NULL;
static lv_obj_t *page_dots[SCREEN_COUNT] = {NULL};
static lv_timer_t *page_indicator_timer = NULL;

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
static lv_obj_t *best_shares_labels[10] = {NULL};

// Statistics widgets
static lv_obj_t *uptime_label = NULL;
static lv_obj_t *voltage_label = NULL;
static lv_obj_t *frequency_label = NULL;
static lv_obj_t *fan_label = NULL;
static lv_obj_t *asic_label = NULL;
static lv_obj_t *version_label = NULL;

// Control widgets
static lv_obj_t *restart_btn = NULL;
static lv_obj_t *primary_pool_label = NULL;
static lv_obj_t *fallback_pool_label = NULL;
static lv_obj_t *current_pool_label = NULL;

// Settings widgets
static lv_obj_t *wifi_info_label = NULL;
static lv_obj_t *ip_label = NULL;
static lv_obj_t *board_label = NULL;
static lv_obj_t *response_label = NULL;

// Charts widgets
static lv_obj_t *hashrate_chart = NULL;
static lv_obj_t *temp_chart = NULL;
static lv_chart_series_t *hashrate_series = NULL;
static lv_chart_series_t *temp_series = NULL;
static lv_chart_series_t *vrtemp_series = NULL;
static bool chart_history_loaded = false;

// Forward declarations
static void create_page_indicator(void);
static void create_dashboard_screen(void);
static void create_difficulty_screen(void);
static void create_statistics_screen(void);
static void create_control_screen(void);
static void create_settings_screen(void);
static void create_charts_screen(void);
static void format_difficulty(char *buf, size_t len, uint64_t diff);
static void format_uptime(char *buf, size_t len, uint32_t seconds);
static void format_time_ago(char *buf, size_t len, time_t timestamp);
static void page_indicator_timer_callback(lv_timer_t *timer);
static void connectivity_blink_timer_callback(lv_timer_t *timer);

// Button callback
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
    create_charts_screen();

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

static void page_indicator_timer_callback(lv_timer_t *timer)
{
    // Hide the page indicator
    if (page_indicator) {
        lv_obj_add_flag(page_indicator, LV_OBJ_FLAG_HIDDEN);
    }

    // Delete the timer
    if (page_indicator_timer) {
        lv_timer_del(page_indicator_timer);
        page_indicator_timer = NULL;
    }
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
    lv_obj_set_style_pad_top(content, 2, 0);  // Reduced from 5 to 2
    lv_obj_set_style_pad_bottom(content, 25, 0);   // Padding for page indicator
    lv_obj_set_style_pad_left(content, 5, 0);
    lv_obj_set_style_pad_right(content, 5, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

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
    lv_obj_set_size(left_col, 110, LV_SIZE_CONTENT);  // Reduced from 130
    lv_obj_set_style_bg_opa(left_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_col, 0, 0);
    lv_obj_set_style_pad_all(left_col, 0, 0);
    lv_obj_set_flex_flow(left_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hashrate_arc = lv_arc_create(left_col);
    lv_obj_set_size(hashrate_arc, 100, 100);  // Reduced from 120x120
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

    // WiFi status below the circle
    wifi_status_label = lv_label_create(left_col);
    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI " ---");
    lv_obj_set_style_text_color(wifi_status_label, ACCENT_COLOR, 0);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(wifi_status_label, LV_TEXT_ALIGN_CENTER, 0);

    // Right column: All info
    lv_obj_t *right_col = lv_obj_create(main_row);
    lv_obj_set_flex_grow(right_col, 1);
    lv_obj_set_height(right_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);
    lv_obj_set_style_pad_all(right_col, 3, 0);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Efficiency card (percentage)
    lv_obj_t *efficiency_card = lv_obj_create(right_col);
    lv_obj_set_size(efficiency_card, LV_PCT(95), 24);
    lv_obj_set_style_bg_color(efficiency_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(efficiency_card, 0, 0);
    lv_obj_set_style_pad_all(efficiency_card, 2, 0);
    efficiency_label = lv_label_create(efficiency_card);
    lv_label_set_text(efficiency_label, "Eff: ---%");
    lv_obj_center(efficiency_label);
    lv_obj_set_style_text_color(efficiency_label, TEXT_COLOR, 0);

    // Efficiency card (J/TH)
    lv_obj_t *efficiency_jth_card = lv_obj_create(right_col);
    lv_obj_set_size(efficiency_jth_card, LV_PCT(95), 24);
    lv_obj_set_style_bg_color(efficiency_jth_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(efficiency_jth_card, 0, 0);
    lv_obj_set_style_pad_all(efficiency_jth_card, 2, 0);
    efficiency_jth_label = lv_label_create(efficiency_jth_card);
    lv_label_set_text(efficiency_jth_label, "--- J/TH");
    lv_obj_center(efficiency_jth_label);
    lv_obj_set_style_text_color(efficiency_jth_label, TEXT_COLOR, 0);

    // Temperature cards (stacked vertically)
    lv_obj_t *temp_card = lv_obj_create(right_col);
    lv_obj_set_size(temp_card, LV_PCT(95), 24);  // Reduced from 28
    lv_obj_set_style_bg_color(temp_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(temp_card, 0, 0);
    lv_obj_set_style_pad_all(temp_card, 2, 0);  // Reduced padding
    temp_label = lv_label_create(temp_card);
    lv_label_set_text(temp_label, LV_SYMBOL_IMAGE " --°C");
    lv_obj_center(temp_label);
    lv_obj_set_style_text_color(temp_label, TEXT_COLOR, 0);

    lv_obj_t *temp_vr_card = lv_obj_create(right_col);
    lv_obj_set_size(temp_vr_card, LV_PCT(95), 24);  // Reduced from 28
    lv_obj_set_style_bg_color(temp_vr_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(temp_vr_card, 0, 0);
    lv_obj_set_style_pad_all(temp_vr_card, 2, 0);  // Reduced padding
    temp_vr_label = lv_label_create(temp_vr_card);
    lv_label_set_text(temp_vr_label, "VR --°C");
    lv_obj_center(temp_vr_label);
    lv_obj_set_style_text_color(temp_vr_label, TEXT_COLOR, 0);

    lv_obj_t *power_card = lv_obj_create(right_col);
    lv_obj_set_size(power_card, LV_PCT(95), 24);  // Reduced from 28
    lv_obj_set_style_bg_color(power_card, CARD_COLOR, 0);
    lv_obj_set_style_border_width(power_card, 0, 0);
    lv_obj_set_style_pad_all(power_card, 2, 0);  // Reduced padding
    power_label = lv_label_create(power_card);
    lv_label_set_text(power_label, LV_SYMBOL_CHARGE " --W");
    lv_obj_center(power_label);
    lv_obj_set_style_text_color(power_label, TEXT_COLOR, 0);

    // Pool status (full width below)
    pool_label = lv_label_create(content);
    lv_label_set_text(pool_label, "Pool: Connecting...");
    lv_obj_set_style_text_color(pool_label, WARNING_COLOR, 0);
    lv_obj_set_style_text_font(pool_label, &lv_font_montserrat_10, 0);  // Reduced from 12
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
    
    // Shares bar (green for accepted shares)
    shares_bar = lv_bar_create(content);
    lv_obj_set_size(shares_bar, LV_PCT(95), 20);
    lv_bar_set_range(shares_bar, 0, 100);
    lv_bar_set_value(shares_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(shares_bar, ACCENT_COLOR, LV_PART_INDICATOR);
    
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
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Pool information labels (pool switching removed - BitAxe handles failover automatically)
    lv_obj_t *pool_info_title = lv_label_create(content);
    lv_label_set_text(pool_info_title, "Pool Configuration:");
    lv_obj_set_style_text_color(pool_info_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_pad_top(pool_info_title, 10, 0);

    primary_pool_label = lv_label_create(content);
    lv_label_set_text(primary_pool_label, "Primary: ---");
    lv_obj_set_style_text_color(primary_pool_label, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(primary_pool_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(primary_pool_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(primary_pool_label, LV_PCT(90));

    fallback_pool_label = lv_label_create(content);
    lv_label_set_text(fallback_pool_label, "Fallback: ---");
    lv_obj_set_style_text_color(fallback_pool_label, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(fallback_pool_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(fallback_pool_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(fallback_pool_label, LV_PCT(90));

    current_pool_label = lv_label_create(content);
    lv_label_set_text(current_pool_label, "Current: Primary");
    lv_obj_set_style_text_color(current_pool_label, ACCENT_COLOR, 0);
    lv_obj_set_style_text_font(current_pool_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_bottom(current_pool_label, 15, 0);

    // Restart button
    restart_btn = lv_btn_create(content);
    lv_obj_set_size(restart_btn, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(restart_btn, CARD_COLOR, 0);  // Dark background for better readability
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, LV_SYMBOL_POWER " Restart Miner");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(restart_label, ERROR_COLOR, 0);  // Red text
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

static void create_charts_screen(void)
{
    screens[SCREEN_CHARTS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_CHARTS], BG_COLOR, 0);

    lv_obj_t *content = lv_obj_create(screens[SCREEN_CHARTS]);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_top(content, 5, 0);
    lv_obj_set_style_pad_bottom(content, 25, 0);
    lv_obj_set_style_pad_left(content, 8, 0);
    lv_obj_set_style_pad_right(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "Session Charts");
    lv_obj_set_style_text_color(title, ACCENT_COLOR, 0);
    lv_obj_set_style_pad_bottom(title, 5, 0);

    // Hashrate chart
    hashrate_chart = lv_chart_create(content);
    lv_obj_set_size(hashrate_chart, 300, 125);
    lv_chart_set_type(hashrate_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(hashrate_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);  // Will be adjusted dynamically
    lv_chart_set_point_count(hashrate_chart, 60);  // Show last 60 points (5 minutes)
    lv_chart_set_update_mode(hashrate_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_color(hashrate_chart, CARD_COLOR, 0);
    lv_obj_set_style_border_width(hashrate_chart, 0, 0);

    // Enable axes with ticks
    lv_chart_set_axis_tick(hashrate_chart, LV_CHART_AXIS_PRIMARY_Y, 5, 3, 5, 2, true, 40);
    lv_chart_set_axis_tick(hashrate_chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 7, 2, true, 30);

    // Hashrate series
    hashrate_series = lv_chart_add_series(hashrate_chart, ACCENT_COLOR, LV_CHART_AXIS_PRIMARY_Y);

    // Hashrate label
    lv_obj_t *hashrate_chart_label = lv_label_create(content);
    lv_label_set_text(hashrate_chart_label, "Hashrate (GH/s)");
    lv_obj_set_style_text_color(hashrate_chart_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(hashrate_chart_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(hashrate_chart_label, 3, 0);

    // Temperature chart
    temp_chart = lv_chart_create(content);
    lv_obj_set_size(temp_chart, 300, 125);
    lv_chart_set_type(temp_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);  // 0-100°C
    lv_chart_set_point_count(temp_chart, 60);  // Show last 60 points (5 minutes)
    lv_chart_set_update_mode(temp_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_color(temp_chart, CARD_COLOR, 0);
    lv_obj_set_style_border_width(temp_chart, 0, 0);

    // Enable axes with ticks
    lv_chart_set_axis_tick(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 5, 3, 5, 2, true, 40);
    lv_chart_set_axis_tick(temp_chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 7, 2, true, 30);

    // Temperature series (ASIC temp = green, VR temp = orange)
    temp_series = lv_chart_add_series(temp_chart, ACCENT_COLOR, LV_CHART_AXIS_PRIMARY_Y);
    vrtemp_series = lv_chart_add_series(temp_chart, WARNING_COLOR, LV_CHART_AXIS_PRIMARY_Y);

    // Temperature label
    lv_obj_t *temp_chart_label = lv_label_create(content);
    lv_label_set_text(temp_chart_label, "Temp (green=ASIC, orange=VR)");
    lv_obj_set_style_text_color(temp_chart_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(temp_chart_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_top(temp_chart_label, 3, 0);
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

    // Update WiFi status below circle
    if (wifi_status_label) {
        bool wifi_ok = (data->wifiRSSI > -80);
        lv_obj_set_style_text_color(wifi_status_label, wifi_ok ? ACCENT_COLOR : WARNING_COLOR, 0);

        if (data->isUsingFallbackStratum) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s " LV_SYMBOL_WARNING, data->ssid);
            lv_obj_set_style_text_color(wifi_status_label, WARNING_COLOR, 0);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s %ld", data->ssid, (long)data->wifiRSSI);
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

    // Update top 10 best shares leaderboard
    // All shares are now stored in the best_shares array, sorted by difficulty
    // Position #1 will be all-time best (may not have timestamp if loaded from old NVS)
    // Positions #2-10 will be next best session shares found over time
    const best_share_t* top_shares = best_shares_get_top10();
    uint8_t share_count = best_shares_get_count();

    for (int i = 0; i < 10; i++) {
        if (best_shares_labels[i]) {
            if (i < share_count && top_shares[i].valid) {
                char diff_str[32];
                format_difficulty(diff_str, sizeof(diff_str), top_shares[i].difficulty);

                // Format display based on whether we have timestamp and if it's all-time best
                if (top_shares[i].is_all_time_best) {
                    if (top_shares[i].timestamp > 0) {
                        char time_str[32];
                        format_time_ago(time_str, sizeof(time_str), top_shares[i].timestamp);
                        snprintf(buf, sizeof(buf), "%d. %s - %s (ATH)", i + 1, diff_str, time_str);
                    } else {
                        snprintf(buf, sizeof(buf), "%d. %s (ATH)", i + 1, diff_str);
                    }
                } else {
                    if (top_shares[i].timestamp > 0) {
                        char time_str[32];
                        format_time_ago(time_str, sizeof(time_str), top_shares[i].timestamp);
                        snprintf(buf, sizeof(buf), "%d. %s - %s", i + 1, diff_str, time_str);
                    } else {
                        snprintf(buf, sizeof(buf), "%d. %s", i + 1, diff_str);
                    }
                }
                lv_label_set_text(best_shares_labels[i], buf);
            } else {
                snprintf(buf, sizeof(buf), "%d. ---", i + 1);
                lv_label_set_text(best_shares_labels[i], buf);
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

    // Update Control screen pool information
    if (primary_pool_label) {
        char pool_buf[160];
        snprintf(pool_buf, sizeof(pool_buf), "Primary: %s:%lu", data->stratumURL, (unsigned long)data->stratumPort);
        lv_label_set_text(primary_pool_label, pool_buf);
    }

    if (fallback_pool_label) {
        char pool_buf[160];
        snprintf(pool_buf, sizeof(pool_buf), "Fallback: %s:%lu", data->fallbackStratumURL, (unsigned long)data->fallbackStratumPort);
        lv_label_set_text(fallback_pool_label, pool_buf);
    }

    if (current_pool_label) {
        if (data->isUsingFallbackStratum) {
            snprintf(buf, sizeof(buf), "Current: Fallback " LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(current_pool_label, WARNING_COLOR, 0);
        } else {
            snprintf(buf, sizeof(buf), "Current: Primary");
            lv_obj_set_style_text_color(current_pool_label, ACCENT_COLOR, 0);
        }
        lv_label_set_text(current_pool_label, buf);
    }

    // Update Charts screen
    if (hashrate_series && temp_series && vrtemp_series) {
        // Add new data points to charts
        lv_chart_set_next_value(hashrate_chart, hashrate_series, (int32_t)data->hashRate);
        lv_chart_set_next_value(temp_chart, temp_series, (int32_t)data->temp);
        lv_chart_set_next_value(temp_chart, vrtemp_series, (int32_t)data->vrTemp);

        // Dynamically adjust hashrate chart Y-axis range
        int32_t max_hashrate = (int32_t)(data->expectedHashrate * 1.2f);
        if (max_hashrate < 100) max_hashrate = 100;
        lv_chart_set_range(hashrate_chart, LV_CHART_AXIS_PRIMARY_Y, 0, max_hashrate);
    }
}

void ui_manager_goto_screen(screen_index_t screen, bool animate, bool direction_left)
{
    if (screen >= SCREEN_COUNT) {
        return;
    }

    current_screen = screen;

    // Load historical chart data when switching to charts screen for the first time
    if (screen == SCREEN_CHARTS && !chart_history_loaded) {
        ui_manager_load_chart_history();
        chart_history_loaded = true;
    }

    // Show page indicator
    if (page_indicator) {
        lv_obj_clear_flag(page_indicator, LV_OBJ_FLAG_HIDDEN);
    }

    // Update page indicator
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (page_dots[i]) {
            lv_obj_set_style_bg_color(page_dots[i],
                                      i == screen ? TEXT_COLOR : lv_color_hex(0x666666), 0);
        }
    }

    // Delete existing timer if any
    if (page_indicator_timer) {
        lv_timer_del(page_indicator_timer);
        page_indicator_timer = NULL;
    }

    // Create timer to hide page indicator after 1 second
    page_indicator_timer = lv_timer_create(page_indicator_timer_callback, 1000, NULL);
    lv_timer_set_repeat_count(page_indicator_timer, 1);

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

static void connectivity_blink_timer_callback(lv_timer_t *timer)
{
    if (connectivity_warning) {
        // Toggle visibility for blinking effect
        if (lv_obj_has_flag(connectivity_warning, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(connectivity_warning, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(connectivity_warning, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_manager_show_connectivity_warning(void)
{
    if (connectivity_warning) {
        return; // Already showing
    }

    // Create warning indicator on top layer (center of screen)
    connectivity_warning = lv_obj_create(lv_layer_top());
    lv_obj_set_size(connectivity_warning, 180, 70);
    lv_obj_align(connectivity_warning, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(connectivity_warning, ERROR_COLOR, 0);
    lv_obj_set_style_border_width(connectivity_warning, 2, 0);
    lv_obj_set_style_border_color(connectivity_warning, TEXT_COLOR, 0);
    lv_obj_set_style_radius(connectivity_warning, 10, 0);

    lv_obj_t *warning_label = lv_label_create(connectivity_warning);
    lv_label_set_text(warning_label, LV_SYMBOL_WARNING " NO MINER");
    lv_obj_center(warning_label);
    lv_obj_set_style_text_align(warning_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(warning_label, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_20, 0);

    // Create blinking timer (500ms interval)
    if (!connectivity_blink_timer) {
        connectivity_blink_timer = lv_timer_create(connectivity_blink_timer_callback, 500, NULL);
    }

    ESP_LOGI(TAG, "Connectivity warning displayed");
}

void ui_manager_hide_connectivity_warning(void)
{
    if (connectivity_warning) {
        lv_obj_del(connectivity_warning);
        connectivity_warning = NULL;
    }

    if (connectivity_blink_timer) {
        lv_timer_del(connectivity_blink_timer);
        connectivity_blink_timer = NULL;
    }
}

// Status bar removed - status info now integrated into dashboard screen

// Button callback implementation
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
    if (timestamp == 0) {
        snprintf(buf, len, "unknown");
        return;
    }

    time_t now = time(NULL);
    int64_t diff = (int64_t)(now - timestamp);

    if (diff < 0) {
        snprintf(buf, len, "future");
    } else if (diff < 60) {
        snprintf(buf, len, "%ds ago", (int)diff);
    } else if (diff < 3600) {
        snprintf(buf, len, "%dm ago", (int)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, len, "%dh ago", (int)(diff / 3600));
    } else {
        // For older shares, show date and time
        struct tm *timeinfo = localtime(&timestamp);
        strftime(buf, len, "%m/%d %H:%M", timeinfo);
    }
}

void ui_manager_load_chart_history(void)
{
    if (!hashrate_series || !temp_series || !vrtemp_series) {
        ESP_LOGW(TAG, "Chart series not initialized");
        return;
    }

    // Get historical data from buffer
    uint16_t count = 0;
    const chart_data_point_t* data = chart_buffer_get_data(&count);

    if (count == 0 || !data) {
        ESP_LOGI(TAG, "No historical data to load into charts");
        return;
    }

    ESP_LOGI(TAG, "Loading %d historical data points into charts", count);

    // Load all historical data points into the charts using set_next_value
    // This is safer than changing point count dynamically
    for (uint16_t i = 0; i < count; i++) {
        lv_chart_set_next_value(hashrate_chart, hashrate_series, (int32_t)data[i].hashrate);
        lv_chart_set_next_value(temp_chart, temp_series, (int32_t)data[i].temp);
        lv_chart_set_next_value(temp_chart, vrtemp_series, (int32_t)data[i].vrTemp);
    }

    // Refresh the charts to update display
    lv_chart_refresh(hashrate_chart);
    lv_chart_refresh(temp_chart);

    ESP_LOGI(TAG, "Historical chart data loaded successfully");
}
