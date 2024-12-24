#pragma once

#include <ArduinoBLE.h>
#include "common_types.h"

class BmsClient {
public:
    using DataCallback = void (*)(const BmsData&);
    using StatusCallback = void (*)(ConnectionState);

    friend void onCharacteristicWritten(BLEDevice device, BLECharacteristic characteristic);

    static BmsClient& instance(const char* address = nullptr, 
                             DataCallback dataCallback = nullptr,
                             StatusCallback statusCallback = nullptr) {
        static BmsClient instance(address, dataCallback, statusCallback);
        return instance;
    }

    void setup();
    void update();
    bool isConnected();

private:
    BmsClient(const char* address, DataCallback dataCallback, StatusCallback statusCallback);
    
    void decodeBmsData(const uint8_t* data, size_t length);
    void requestBmsData();
    void connectToServer(BLEDevice& peripheral);

    const char* deviceAddress;
    DataCallback dataCallback;
    StatusCallback statusCallback;
    
    BLEService pService;
    BLECharacteristic pNotifyChar;
    BLECharacteristic pWriteChar;
    
    unsigned long lastRequestTime = 0;
};