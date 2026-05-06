#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"
#include "espnow_protocol.h"

// === E S P N O W B R O K E R  C L A S S ===
//
// Broadcasts HALMET sensor data via ESP-NOW.
// No receive commands are expected in this version.

class ESPNowBroker {
public:
    explicit ESPNowBroker(DS18B20Processor &ds18b20_proc, VDOProcessor &vdo_proc);

    bool begin();
    void sendEngineDelta();  // broadcast HALMETEngineDelta
    void sendTankDelta();    // broadcast HALMETTankDelta

private:
    DS18B20Processor &_ds18b20_proc;
    VDOProcessor     &_vdo_proc;
    bool              _initialized = false;

    static constexpr uint8_t  BROADCAST_ADDR[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static constexpr float         DB_TEMP_K       = 0.05f;
    static constexpr float         DB_LEVEL        = 0.00005f;
    static constexpr unsigned long ESPNOW_KEEPALIVE_MS = 60013UL;

    float         _last_sent_temp_k    = NAN;
    float         _last_sent_level     = NAN;
    unsigned long _last_engine_send_ms = 0;
    unsigned long _last_tank_send_ms   = 0;

    static void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
};
