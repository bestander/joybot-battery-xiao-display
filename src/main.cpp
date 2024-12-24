
#include "display_manager.h"
#include "metrics_averager.h"
#include "charge_estimator.h"

#ifdef USE_EMULATOR
#include "bms_client_emulator.h"
using BmsClientType = BmsClientEmulator;
#else
#include "bms_client.h"
using BmsClientType = BmsClient;
#endif

static constexpr const char* BATTERY_ADDRESS = "a4:c1:37:33:43:51";

void onBmsData(const BmsData& rawData) {
    static MetricsAverager averager;
    static ChargeEstimator chargeEstimator;
    uint32_t now = millis();

    averager.addMetrics(rawData, now);
    auto avgData = averager.getAverage();

    chargeEstimator.update(avgData, now);

    BmsData displayData = avgData;
    displayData.time_to_full_s = chargeEstimator.isEstimating() ? chargeEstimator.getTimeToFullCharge() : 0 ;

    DisplayManager::instance().update(displayData);
}

void onConnectionStatus(ConnectionState status) {
    switch(status) {
        case ConnectionState::Connecting:
            Serial.println("Connecting to BMS...");
            DisplayManager::instance().updateConnectionState(ConnectionState::Connecting);
            break;
        case ConnectionState::Connected:
            Serial.println("Connected to BMS");
            DisplayManager::instance().updateConnectionState(ConnectionState::Connected);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");
    DisplayManager::instance().setup();
    BmsClientType::instance(BATTERY_ADDRESS, onBmsData, onConnectionStatus).setup();
}

void loop() {
    DisplayManager::instance().handleTasks();
    BmsClientType::instance().update();
    delay(50);
}
