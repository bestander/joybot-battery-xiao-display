#pragma once

#include <lvgl.h>

#include "common_types.h"
#include "display.h"

class DisplayManager {
   public:
    // Delete copy and move operations
    DisplayManager(const DisplayManager &) = delete;
    DisplayManager &operator=(const DisplayManager &) = delete;
    DisplayManager(DisplayManager &&) = delete;
    DisplayManager &operator=(DisplayManager &&) = delete;

    // Singleton access
    static DisplayManager &instance() {
        static DisplayManager instance;
        return instance;
    }

    void setup();
    void update(const BmsData &data);
    void handleTasks() { lv_task_handler(); }
    void updateConnectionState(ConnectionState state);

   private:
    // Make constructor private
    DisplayManager() = default;

    void setupLabels();
    void setupPowerBar();
    void updatePowerBar(const BmsData &data);

    // UI Elements
    lv_obj_t *metrics_label = nullptr;
    lv_obj_t *soc_label = nullptr;
    lv_obj_t *power_bar = nullptr;
    lv_obj_t *power_bar_label = nullptr;
    lv_obj_t *connection_label = nullptr;
    lv_obj_t *latency_label = nullptr;
    lv_obj_t *time_to_full_label = nullptr;

    // Display buffer
    lv_disp_draw_buf_t draw_buf;
    lv_color_t buf[2][240 * 10];

    void flushDisplay(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

    static void flushDisplayStatic(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        auto *instance = (DisplayManager *)disp->user_data;
        instance->flushDisplay(disp, area, color_p);
    }
};