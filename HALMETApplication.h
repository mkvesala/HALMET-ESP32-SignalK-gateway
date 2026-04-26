#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "WifiState.h"
#include "DS18B20Sensor.h"
#include "DS18B20Processor.h"
#include "VDOSensor.h"
#include "VDOProcessor.h"
#include "HALMETPreferences.h"
#include "SignalKBroker.h"
#include "ESPNowBroker.h"
#include "WebUIManager.h"

// === H A L M E T A P P L I C A T I O N  C L A S S ===
//
// Owns all sensor pairs and brokers. Manages WiFi, timers, and the main loop.
// DS18B20 reads via FreeRTOS task (750 ms blocking conversion).
// VDO/ADS1115 reads via 2 s timer in the main loop.

class HALMETApplication {
public:
    explicit HALMETApplication();

    void begin();
    void loop();

    // ADS1115 is critical (onboard hardware). DS18B20 missing logs warning but does not halt.
    bool sensorOk() const { return _ads_ok; }

private:
    // Timing constants (prime-ish numbers to avoid harmonic collisions)
    static constexpr unsigned long VDO_READ_MS          = 2003;
    static constexpr unsigned long SK_ENGINE_TX_MS      = 1009;
    static constexpr unsigned long SK_TANK_TX_MS        = 2999;
    static constexpr unsigned long ESPNOW_TX_MS         = 3011;
    static constexpr unsigned long WIFI_STATUS_CHECK_MS = 503;
    static constexpr unsigned long WIFI_TIMEOUT_MS      = 90001;
    static constexpr unsigned long WS_RETRY_MS          = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS      = 119993;

    // Timers
    unsigned long _last_vdo_read_ms   = 0;
    unsigned long _last_sk_engine_ms  = 0;
    unsigned long _last_sk_tank_ms    = 0;
    unsigned long _last_espnow_ms     = 0;
    unsigned long _wifi_last_check_ms = 0;
    unsigned long _wifi_conn_start_ms = 0;
    unsigned long _next_ws_try_ms     = 0;
    unsigned long _expn_retry_ms      = WS_RETRY_MS;

    bool      _ads_ok     = false;
    bool      _ds18_ok    = false;
    WifiState _wifi_state = WifiState::INIT;

    volatile bool _ap_intruder         = false;
    uint8_t       _ap_intruder_mac[6]  = {};

    // Stack-allocated members — declaration order equals initialization order
    DS18B20Sensor     _ds18b20;
    DS18B20Processor  _ds18b20_proc;
    VDOSensor         _vdo;
    VDOProcessor      _vdo_proc;
    HALMETPreferences _prefs;
    SignalKBroker     _signalk;
    ESPNowBroker      _espnow;
    WebUIManager      _webui;

    // FreeRTOS task for DS18B20 (750 ms blocking reads)
    static void ds18b20Task(void *param);
    TaskHandle_t _ds18b20_task = nullptr;

    // Loop handlers
    void handleWifi(unsigned long now);
    void handleAPIntruder();
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(unsigned long now);
    void handleVDORead(unsigned long now);
    void handleSignalK(unsigned long now);
    void handleESPNow(unsigned long now);

    void initWifiServices();
};
