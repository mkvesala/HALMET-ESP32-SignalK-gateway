#include "HALMETApplication.h"
#include "secrets.h"

// === P U B L I C ===

// Constructor — wires all dependencies via initializer list
HALMETApplication::HALMETApplication()
    : _ds18b20()
    , _ds18b20_proc(_ds18b20)
    , _vdo()
    , _vdo_proc(_vdo)
    , _prefs(_ds18b20_proc, _vdo_proc)
    , _signalk(_ds18b20_proc, _vdo_proc)
    , _espnow(_ds18b20_proc, _vdo_proc)
    , _webui(_ds18b20_proc, _vdo_proc, _prefs, _signalk)
{}

// Initialize hardware and start subsystems
void HALMETApplication::begin() {
    // I2C for ADS1115 — Wire.begin() uses HALMET default pins (SDA=21, SCL=22)
    Wire.begin();
    delay(47);

    btStop();

    // ADS1115 (VDO fuel sender)
    _ads_ok = _vdo.begin();
    Serial.printf("[VDO] ADS1115: %s\n", _ads_ok ? "OK" : "FAIL");

    // DS18B20 (exhaust temperature)
    _ds18_ok = _ds18b20.begin();
    Serial.printf("[DS18] Sensor: %s\n", _ds18_ok ? "OK" : "NOT FOUND");

    _prefs.load();

    // WiFi — AP_STA mode required for ESP-NOW + WiFi coexistence
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS, 1 /*channel*/, 1 /*hidden*/, 1 /*max_connection*/);

    // Deauth any AP intruder immediately; flag loop() to log the MAC
    WiFi.onEvent([this](arduino_event_id_t /*id*/, arduino_event_info_t info) {
        uint8_t aid = info.wifi_ap_staconnected.aid;
        memcpy(_ap_intruder_mac, info.wifi_ap_staconnected.mac, 6);
        esp_wifi_deauth_sta(aid);
        _ap_intruder = true;
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    _wifi_state = WifiState::CONNECTING;
    _wifi_conn_start_ms = millis();
    Serial.println("[WIFI] Connecting...");

    bool espnow_ok = _espnow.begin();
    Serial.printf("[ESPNOW] Init: %s\n", espnow_ok ? "OK" : "FAIL");

    // DS18B20 FreeRTOS task — runs independently, writes to _ds18b20_proc
    if (_ds18_ok) {
        xTaskCreate(ds18b20Task, "DS18B20Task", 2048, this, 1, &_ds18b20_task);
        Serial.println("[DS18] Task started");
    }
}

// Main loop — call all handlers
void HALMETApplication::loop() {
    const unsigned long now = millis();
    handleWifi(now);
    handleAPIntruder();
    handleOTA();
    handleWebUI();
    handleWebsocket(now);
    handleVDORead(now);
    handleSignalK(now);
    handleESPNow(now);
}

// === P R I V A T E ===

// FreeRTOS task — reads DS18B20 every ~1 s (blocks 750 ms during conversion)
void HALMETApplication::ds18b20Task(void *param) {
    HALMETApplication *app = static_cast<HALMETApplication *>(param);
    for (;;) {
        float temp_c = 0.0f;
        if (app->_ds18b20.requestAndRead(temp_c)) {
            app->_ds18b20_proc.updateTemperature(temp_c);
        }
        vTaskDelay(pdMS_TO_TICKS(1003));
    }
}

// WiFi state machine
void HALMETApplication::handleWifi(unsigned long now) {
    if ((long)(now - _wifi_last_check_ms) < WIFI_STATUS_CHECK_MS) return;
    _wifi_last_check_ms = now;

    switch (_wifi_state) {

        case WifiState::INIT:
            break;

        case WifiState::CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                _wifi_state = WifiState::CONNECTED;
                Serial.printf("[WIFI] Connected, IP %s, RSSI %d dBm\n",
                              WiFi.localIP().toString().c_str(), WiFi.RSSI());
                initWifiServices();
                _expn_retry_ms = WS_RETRY_MS;
            }
            else if ((long)(now - _wifi_conn_start_ms) >= WIFI_TIMEOUT_MS) {
                Serial.println("[WIFI] Timeout — going offline");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                _wifi_state = WifiState::OFF;
            }
            else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                Serial.println("[WIFI] Connection failed — going offline");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                _wifi_state = WifiState::OFF;
            }
            break;
        }

        case WifiState::CONNECTED:
            if (!WiFi.isConnected()) {
                Serial.println("[WIFI] Lost — reconnecting");
                _wifi_state = WifiState::CONNECTING;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                _wifi_conn_start_ms = now;
            }
            break;

        case WifiState::FAILED:
        case WifiState::DISCONNECTED:
        case WifiState::OFF:
            break;
    }
}

// Log and ignore AP intruder (already deauthed in event callback)
void HALMETApplication::handleAPIntruder() {
    if (!_ap_intruder) return;
    _ap_intruder = false;
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             _ap_intruder_mac[0], _ap_intruder_mac[1], _ap_intruder_mac[2],
             _ap_intruder_mac[3], _ap_intruder_mac[4], _ap_intruder_mac[5]);
    Serial.printf("[AP] INTRUDER deauthed — MAC %s\n", mac);
}

// OTA
void HALMETApplication::handleOTA() {
    if (_wifi_state != WifiState::CONNECTED) return;
    ArduinoOTA.handle();
}

// Web server
void HALMETApplication::handleWebUI() {
    if (_wifi_state != WifiState::CONNECTED) return;
    _webui.handleRequest();
}

// WebSocket poll and reconnect with exponential backoff
void HALMETApplication::handleWebsocket(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    _signalk.handleStatus();

    if (!_signalk.isOpen() && (long)(now - _next_ws_try_ms) >= 0) {
        Serial.println("[SK] Connecting WebSocket...");
        _signalk.connectWebsocket();
        _next_ws_try_ms = now + _expn_retry_ms;
        _expn_retry_ms = min(_expn_retry_ms * 2UL, WS_RETRY_MAX_MS);
    }
    if (_signalk.isOpen()) _expn_retry_ms = WS_RETRY_MS;
}

// Read VDO sender resistance and feed into processor filter
void HALMETApplication::handleVDORead(unsigned long now) {
    if (!_ads_ok) return;
    if ((long)(now - _last_vdo_read_ms) < VDO_READ_MS) return;
    _last_vdo_read_ms = now;
    float ohms = 0.0f;
    if (_vdo.readResistance(ohms)) {
        _vdo_proc.updateLevel(ohms);
    }
}

// Send SignalK deltas on independent timers
void HALMETApplication::handleSignalK(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;

    if ((long)(now - _last_sk_engine_ms) >= SK_ENGINE_TX_MS) {
        _last_sk_engine_ms = now;
        _signalk.sendEngineDelta();
    }
    if ((long)(now - _last_sk_tank_ms) >= SK_TANK_TX_MS) {
        _last_sk_tank_ms = now;
        _signalk.sendTankDelta();
    }
}

// Broadcast both sensor deltas via ESP-NOW
void HALMETApplication::handleESPNow(unsigned long now) {
    if ((long)(now - _last_espnow_ms) < ESPNOW_TX_MS) return;
    _last_espnow_ms = now;
    _espnow.sendEngineDelta();
    _espnow.sendTankDelta();
}

// Initialize WiFi-dependent services — guarded: OTA and WebServer routes must only be registered once
void HALMETApplication::initWifiServices() {
    if (_wifi_services_initialized) return;
    _wifi_services_initialized = true;

    _signalk.begin();

    ArduinoOTA.setHostname(_signalk.getSignalKSource());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.begin();

    _webui.begin();
}
