#include <ArduinoBLE.h>
#include "bms_client.h"
#include "common_types.h"

// Define the static constants here
static const char* SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb";
static const char* CHAR_NOTIFY = "0000ff01-0000-1000-8000-00805f9b34fb";
static const char* CHAR_WRITE = "0000ff02-0000-1000-8000-00805f9b34fb";
static constexpr uint8_t CMD_BASIC_INFO = 0x03;

BmsClient::BmsClient(const char* address, DataCallback dataCallback, StatusCallback statusCallback)
    : deviceAddress(address), dataCallback(dataCallback), statusCallback(statusCallback) {
    setup();
}

void BmsClient::setup() {
    // Initialize built-in LED for status indication
    pinMode(LED_BUILTIN, OUTPUT);
    
    // Try to initialize BLE multiple times if needed
    int attempts = 0;
    while (!BLE.begin() && attempts < 3) {
        Serial.println("Starting BLE failed! Retrying...");
        delay(1000);
        attempts++;
    }

    if (!BLE.begin()) {
        Serial.println("Failed to initialize BLE!");
        // Indicate error with LED
        while (1) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
        return;
    }

    Serial.println("BLE initialized successfully!");
    Serial.println("BLE scan...");
    BLE.scan();
    statusCallback(ConnectionState::Connecting);
}

void BmsClient::update() {
    BLE.poll();

    if (!isConnected()) {
        BLEDevice peripheral = BLE.available();
        
        if (peripheral) {
            // Check if this is our target device
            if (String(peripheral.address()) == String(deviceAddress)) {
                Serial.println("Found our device! Attempting to connect...");
                connectToServer(peripheral);
            }
        }
    } else if (millis() - lastRequestTime >= 1000) { // Request every second
        requestBmsData();
        lastRequestTime = millis();
    }
}

void BmsClient::decodeBmsData(const uint8_t* data, size_t length) {
    if (length < 4 || data[0] != 0xDD) return;

    switch(data[1]) {
        case 0x03:
            if (length >= 13) {
                BmsData rawData;
                rawData.voltage = (data[4] << 8 | data[5]) / 100.0f;
                rawData.current = ((int16_t)(data[6] << 8 | data[7])) / 100.0f;
                rawData.soc = (data[8] << 8 | data[9]) / 100;
                rawData.power = rawData.voltage * rawData.current;

                if (dataCallback) {
                    dataCallback(rawData);
                }
            }
            break;
    }
}

void BmsClient::requestBmsData() {
    if (!pWriteChar || !pWriteChar.canWrite()) return;
    
    uint8_t cmd[7] = {0xDD, 0xA5, CMD_BASIC_INFO, 0x00, 0xFF, 0xFD, 0x77};
    pWriteChar.writeValue(cmd, sizeof(cmd));
}

void onCharacteristicWritten(BLEDevice device, BLECharacteristic characteristic) {
    uint8_t data[256];
    int dataLength = characteristic.readValue(data, sizeof(data));
    BmsClient::instance().decodeBmsData(data, dataLength);
}

void BmsClient::connectToServer(BLEDevice& peripheral) {
    BLE.stopScan();
    
    if (peripheral.connect()) {
        Serial.println("peripheral.connect()");
        
        if (peripheral.discoverAttributes()) {
            Serial.println("Discovering attributes");
            pService = peripheral.service(SERVICE_UUID);
            if (pService) {
                Serial.println("Found service");
                pNotifyChar = pService.characteristic(CHAR_NOTIFY);
                pWriteChar = pService.characteristic(CHAR_WRITE);
                
                if (pNotifyChar && pWriteChar) {
                    Serial.println("Found notify and write characteristics");
                    if (pNotifyChar.canSubscribe()) {
                        Serial.println("Subscribing to notify characteristic");
                        pNotifyChar.subscribe();
                        pNotifyChar.setEventHandler(BLEWritten, onCharacteristicWritten);
                        statusCallback(ConnectionState::Connected);
                        return;
                    }
                }
            }
        }
        peripheral.disconnect();
    }
    
    statusCallback(ConnectionState::Connecting);
    BLE.scan();
}

bool BmsClient::isConnected() {
    return pNotifyChar && pWriteChar && pNotifyChar.subscribed();
}