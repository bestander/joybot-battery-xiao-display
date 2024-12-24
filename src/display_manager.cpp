#include "display_manager.h"

static constexpr int16_t POWER_BAR_DISCHARGING_MAX = 4000;  // 4kW
static constexpr int16_t POWER_BAR_CHARGING_MAX = 1000;     // 1kW

void DisplayManager::flushDisplay(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    tft.drawRGBBitmap(
        area->x1, 
        area->y1, 
        (uint16_t*)&color_p->full,  // Changed from lgfx::swap565_t
        area->x2 - area->x1 + 1, 
        area->y2 - area->y1 + 1
    );
    lv_disp_flush_ready(disp);
}

void DisplayManager::setup() {
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);

    // 1. Configure SPI with maximum reliable speed
    SPI.begin();
    SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
    
    tft.begin();
    tft.setRotation(0);  // Hardware rotation
    tft.setSPISpeed(80000000);  // Match SPI speed
    
    // 2. Optimize LVGL settings
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], 240 * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = flushDisplayStatic;
    disp_drv.user_data = this;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 0; 
    lv_disp_drv_register(&disp_drv);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lv_scr_act(), lv_color_white(), LV_PART_MAIN);

    setupLabels();

    // Force initial display update
    lv_task_handler();
}

void DisplayManager::setupLabels() {
    metrics_label = lv_label_create(lv_scr_act());
    soc_label = lv_label_create(lv_scr_act());
    connection_label = lv_label_create(lv_scr_act());
    latency_label = lv_label_create(lv_scr_act());
    time_to_full_label = lv_label_create(lv_scr_act());

    lv_label_set_text(metrics_label, "0.0V  |  0.0A");
    lv_label_set_text(soc_label, "Charge: --%");
    lv_label_set_text(connection_label, "Waiting...");
    lv_label_set_text(latency_label, "Latency: --ms");
    lv_label_set_text(time_to_full_label, "Full in --h --m");

    lv_obj_set_style_text_font(metrics_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_font(soc_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(connection_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(latency_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_font(time_to_full_label, &lv_font_montserrat_10, 0);

    lv_obj_set_style_text_color(metrics_label, lv_color_make(140, 140, 140), 0);
    lv_obj_set_style_text_color(connection_label, lv_color_make(100, 100, 100), 0);
    lv_obj_set_style_text_color(time_to_full_label, lv_color_make(0, 200, 0), 0);

    lv_obj_align(metrics_label, LV_ALIGN_CENTER, 0, -70);
    setupPowerBar();
    lv_obj_align(soc_label, LV_ALIGN_CENTER, 0, -35);
    lv_obj_align(time_to_full_label, LV_ALIGN_CENTER, 0, -15);
    lv_obj_align(connection_label, LV_ALIGN_CENTER, 0, 80);
    lv_obj_align(latency_label, LV_ALIGN_CENTER, 0, 100);

    lv_obj_add_flag(time_to_full_label, LV_OBJ_FLAG_HIDDEN);
}

void DisplayManager::setupPowerBar() {
    // Create container for power bar and label
    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 220, 40);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(cont, 0, 0);

    // Create power bar
    power_bar = lv_bar_create(cont);
    lv_obj_set_size(power_bar, 220, 40);
    lv_obj_align(power_bar, LV_ALIGN_CENTER, 0, 00);
    lv_bar_set_value(power_bar, 0, LV_ANIM_ON);

    // Style for background
    lv_obj_set_style_bg_color(power_bar, lv_color_make(40, 40, 40), LV_PART_MAIN);

    // Style for indicator
    lv_obj_set_style_bg_color(power_bar, lv_color_make(0, 150, 0), LV_PART_INDICATOR);
    lv_obj_set_style_anim_time(power_bar, 400, LV_PART_INDICATOR);  // Animation duration

    // Create label that matches bar size
    power_bar_label = lv_label_create(cont);
    lv_obj_set_style_text_font(power_bar_label, &lv_font_montserrat_20, 0);
    lv_obj_set_size(power_bar_label, 220, 40);
    lv_obj_set_style_text_align(power_bar_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(power_bar_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_ver(power_bar_label, 8, 0);
    lv_label_set_text(power_bar_label, "0 W");
}

void DisplayManager::updatePowerBar(const BmsData &data) {
    lv_color_t barColor, barTextColor;
    // Adjust range and value based on charging/discharging.
    if (data.power > 0) {
        // Charging mode
        lv_bar_set_range(power_bar, 0, POWER_BAR_CHARGING_MAX);
        lv_bar_set_value(power_bar, data.power, LV_ANIM_ON);
        barColor = lv_color_make(0, 150, 0);  // Green for charging
        barTextColor = lv_color_white();
    } else if (data.power < 0) {
        // Discharging mode
        lv_bar_set_range(power_bar, 0, POWER_BAR_DISCHARGING_MAX);
        lv_bar_set_value(power_bar, -data.power, LV_ANIM_ON);
        barColor = lv_color_make(150, 0, 0);  // Red for discharging
        barTextColor = lv_color_white();
    } else {
        // Zero power: show minimal bar
        lv_bar_set_range(power_bar, 0, 0);
        lv_bar_set_value(power_bar, data.power, LV_ANIM_ON);
        barColor = lv_color_make(100, 100, 100);  // Grey for no power
        barTextColor = lv_color_make(100, 100, 100);
    }

    lv_obj_set_style_bg_color(power_bar, barColor, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(power_bar_label, barTextColor, 0);

    // Update text
    static char buf[32];
    if (abs(data.power) >= 1000) {
        snprintf(buf, sizeof(buf), "%.2f kW", data.power / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%.0f W", data.power);
    }
    lv_label_set_text(power_bar_label, buf);
}

void DisplayManager::update(const BmsData &data) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%5.1fV  |  %5.1fA", data.voltage, data.current);
    lv_label_set_text(metrics_label, buf);

    updatePowerBar(data);

    snprintf(buf, sizeof(buf), "Charge: %d%%", data.soc);
    lv_label_set_text(soc_label, buf);

    if (data.soc <= 15) {
        lv_obj_set_style_text_color(soc_label, lv_color_make(255, 0, 0), 0);
    } else if (data.soc >= 80) {
        lv_obj_set_style_text_color(soc_label, lv_color_make(0, 255, 0), 0);
    } else {
        lv_obj_set_style_text_color(soc_label, lv_color_white(), 0);
    }

    // Update connection status with latency
    if (data.latency_ms > 0) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "Latency: %dms", data.latency_ms);
        lv_label_set_text(latency_label, buf);
        lv_obj_set_style_text_color(latency_label, lv_color_make(0, 255, 0), 0);
    }

    // Update time to full if available
    if (data.time_to_full_s > 0) {
        static char buf[32];
        uint32_t hours = data.time_to_full_s / 3600;
        uint32_t minutes = (data.time_to_full_s % 3600) / 60;

        if (hours > 0) {
            snprintf(buf, sizeof(buf), "Full in %dh %dm", hours, minutes);
        } else {
            snprintf(buf, sizeof(buf), "Full in %dm", minutes);
        }

        lv_label_set_text(time_to_full_label, buf);
        lv_obj_clear_flag(time_to_full_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(time_to_full_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void DisplayManager::updateConnectionState(ConnectionState state) {
    switch (state) {
        case ConnectionState::Connecting:
            lv_label_set_text(connection_label, "Connecting...");
            lv_obj_set_style_text_color(connection_label, lv_color_make(255, 255, 0), 0);
            break;
        case ConnectionState::Connected:
            lv_label_set_text(connection_label, "Connected");  // Initial connected state, will be updated with latency
            lv_obj_set_style_text_color(connection_label, lv_color_make(0, 255, 0), 0);
            break;
    }
}