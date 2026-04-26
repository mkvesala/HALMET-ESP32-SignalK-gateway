#include "SignalKBroker.h"
#include "helpers.h"
#include "secrets.h"

using namespace websockets;

// === P U B L I C ===

// Constructor
SignalKBroker::SignalKBroker(DS18B20Processor &ds18b20_proc, VDOProcessor &vdo_proc)
    : _ds18b20_proc(ds18b20_proc)
    , _vdo_proc(vdo_proc)
{}

// Set up URL and source, connect WebSocket
bool SignalKBroker::begin() {
    if (strlen(SK_HOST) <= 0 || SK_PORT <= 0) return false;
    setSignalKURL();
    setSignalKSource();
    return connectWebsocket();
}

// Keep WebSocket alive
void SignalKBroker::handleStatus() {
    if (_ws_open) _ws.poll();
}

// Connect WebSocket and register callbacks
bool SignalKBroker::connectWebsocket() {
    _ws_open = _ws.connect(_sk_url);
    if (_ws_open) {
        _ws.onMessage([this](WebsocketsMessage msg) {
            onMessageCallback(msg);
        });
        _ws.onEvent([this](WebsocketsEvent event, const String &data) {
            onEventCallback(event, data);
        });
    }
    return _ws_open;
}

// Close WebSocket
void SignalKBroker::closeWebsocket() {
    _ws.close();
    _ws_open = false;
}

// Send exhaust temperature delta if changed beyond deadband
void SignalKBroker::sendEngineDelta() {
    if (!_ws_open) return;
    auto delta = _ds18b20_proc.getExhaustTempDelta();
    if (!validf(delta.exhaust_temp_k)) return;

    static float last_temp_k = NAN;
    if (validf(last_temp_k) && fabsf(delta.exhaust_temp_k - last_temp_k) < DB_TEMP_K) return;
    last_temp_k = delta.exhaust_temp_k;

    _engine_doc.clear();
    _engine_doc["context"] = "vessels.self";
    auto updates = _engine_doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values  = up.createNestedArray("values");
    auto v       = values.createNestedObject();
    v["path"]  = "propulsion.0.exhaustTemperature";
    v["value"] = delta.exhaust_temp_k;

    sendDoc(_engine_doc);
}

// Send fuel level ratio delta if changed beyond deadband
void SignalKBroker::sendTankDelta() {
    if (!_ws_open) return;
    auto delta = _vdo_proc.getFuelLevelDelta();
    if (!validf(delta.fuel_level_ratio)) return;

    static float last_level = NAN;
    if (validf(last_level) && fabsf(delta.fuel_level_ratio - last_level) < DB_LEVEL) return;
    last_level = delta.fuel_level_ratio;

    _tank_doc.clear();
    _tank_doc["context"] = "vessels.self";
    auto updates = _tank_doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values  = up.createNestedArray("values");
    auto v       = values.createNestedObject();
    v["path"]  = "tanks.fuel.0.currentLevel";
    v["value"] = delta.fuel_level_ratio;

    sendDoc(_tank_doc);
}

// Send static tank capacity once (called on WebSocket connect)
void SignalKBroker::sendTankCapacity() {
    if (!_ws_open) return;
    StaticJsonDocument<256> doc;
    doc["context"] = "vessels.self";
    auto updates = doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values  = up.createNestedArray("values");
    auto v       = values.createNestedObject();
    v["path"]  = "tanks.fuel.0.capacity";
    v["value"] = _vdo_proc.getCapacityM3();

    char buf[256];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    bool ok = _ws.send(buf, n);
    if (!ok) { _ws.close(); _ws_open = false; }
}

// === P R I V A T E ===

// Build WebSocket URL with optional token
void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(_sk_url, sizeof(_sk_url), "ws://%s:%d/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(_sk_url, sizeof(_sk_url), "ws://%s:%d/signalk/v1/stream", SK_HOST, SK_PORT);
}

// Derive source name from last 3 bytes of ESP32 MAC
void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(_sk_source, sizeof(_sk_source), "esp32.halmet-%02x%02x%02x", m[3], m[4], m[5]);
}

// Incoming messages are not expected for HALMET — ignore
void SignalKBroker::onMessageCallback(WebsocketsMessage /*msg*/) {}

// Handle WebSocket lifecycle events
void SignalKBroker::onEventCallback(WebsocketsEvent event, const String & /*data*/) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            _ws_open = true;
            sendTankCapacity();
            break;
        case WebsocketsEvent::ConnectionClosed:
            _ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            _ws.pong();
            break;
        default:
            break;
    }
}

// Serialize doc and send; close WebSocket on failure
bool SignalKBroker::sendDoc(StaticJsonDocument<512> &doc) {
    char buf[640];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    bool ok = _ws.send(buf, n);
    if (!ok) { _ws.close(); _ws_open = false; }
    return ok;
}
