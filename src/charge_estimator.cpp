#include "charge_estimator.h"
#include <Arduino.h>

static constexpr float BATTERY_CAPACITY_AH = 100.0f;

void ChargeEstimator::update(const BmsData& data, uint32_t timestamp) {
    bool is_charging = data.current > 0;

    if (is_charging && !was_charging) {
        // Just started charging
        charging_start_ms = timestamp;
        avg_charge_current = data.current;
        avg_charge_rate = 0.0f;
        last_soc = data.soc;
        time_to_full_s = 0;
    } else if (is_charging) {
        // Update charging duration
        charging_duration_ms = timestamp - charging_start_ms;

        // After required time, start estimating
        if (charging_duration_ms >= REQUIRED_CHARGING_TIME_MS) {
            // Update running average of charge current
            avg_charge_current = avg_charge_current * (1.0f - CHARGE_MA_FACTOR) +
                               data.current * CHARGE_MA_FACTOR;

            // Calculate SOC-based rate (percent per hour)
            float time_delta_h = (timestamp - last_update_ms) / 1000.0f / 3600.0f;
            if (time_delta_h > 0) {
                float instantaneous_rate = (data.soc - last_soc) / time_delta_h;

                // Update running average of SOC-based rate
                if (avg_charge_rate == 0.0f) {
                    avg_charge_rate = instantaneous_rate;
                } else {
                    avg_charge_rate = avg_charge_rate * (1.0f - RATE_MA_FACTOR) +
                                    instantaneous_rate * RATE_MA_FACTOR;
                }
            }

            // Calculate time to full using both methods
            float remaining_soc = 100.0f - data.soc;
            float hours_to_full;

            if (avg_charge_rate > 0.1f) {
                // If we have meaningful SOC rate, use weighted average of both methods
                float current_based_hours = (remaining_soc * BATTERY_CAPACITY_AH/100.0f) / avg_charge_current;
                float soc_based_hours = remaining_soc / avg_charge_rate;

                // Weight more towards SOC-based as time progresses
                float soc_weight = std::min(charging_duration_ms / 60000.0f, 0.8f);  // Max 80% weight after 1 minute
                hours_to_full = soc_based_hours * soc_weight + current_based_hours * (1.0f - soc_weight);
            } else {
                // Initially use current-based estimation
                hours_to_full = (remaining_soc * BATTERY_CAPACITY_AH/100.0f) / avg_charge_current;
            }

            time_to_full_s = hours_to_full * 3600;

            // Replace Serial.printf with Serial.print statements
            Serial.print("Charge Stats: Rate=");
            Serial.print(avg_charge_rate, 2);  // 2 decimal places
            Serial.print("%/h, Current=");
            Serial.print(avg_charge_current, 1);  // 1 decimal place
            Serial.print("A, Time=");
            Serial.print(hours_to_full, 1);  // 1 decimal place
            Serial.println("h");
        }
    } else {
        // Reset when not charging
        charging_duration_ms = 0;
        time_to_full_s = 0;
        avg_charge_rate = 0.0f;
    }

    was_charging = is_charging;
    last_update_ms = timestamp;
    last_soc = data.soc;
}