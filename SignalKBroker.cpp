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

// Keep WebSocket alive; send tank capacity once per connection after server hello is received
void SignalKBroker::handleStatus() {
    if (!_ws_open) return;
    if (_ws) _ws->poll();
    if (!_capacity_sent) {
        sendTankCapacity();
        _capacity_sent = true;
    }
}

// Connect WebSocket — build a brand-new client so every attempt starts from a clean
// TCP / lwIP socket state; a reused client can retain a stuck socket that never recovers
bool SignalKBroker::connectWebsocket() {
    _ws = std::make_unique<WebsocketsClient>();
    _ws->onMessage([this](WebsocketsMessage msg) {
        onMessageCallback(msg);
    });
    _ws->onEvent([this](WebsocketsEvent event, const String &data) {
        onEventCallback(event, data);
    });
    _ws_open = _ws->connect(_sk_url);
    if (_ws_open) {
        //Serial.println("[SK] WebSocket connected");
        _capacity_sent = false;
        _last_pong_ms = millis();   // seed liveness so a fresh socket is not flagged stale
    } else {
        //Serial.println("[SK] WebSocket connect FAILED");
        _ws.reset();                // failed connect → destroy immediately, free the socket
    }
    return _ws_open;
}

// Close WebSocket — destroy the client so no stale transport state survives
void SignalKBroker::closeWebsocket() {
    if (_ws) {
        _ws->close();
        _ws.reset();
    }
    _ws_open = false;
    _last_pong_ms = 0;
}

// Send a client-initiated ping frame to probe liveness
void SignalKBroker::ping() {
    if (_ws_open && _ws) _ws->ping();
}

// Half-open detection: open, has been connected, but no pong within timeout
bool SignalKBroker::isStale(unsigned long now) const {
    return _ws_open && _last_pong_ms != 0 &&
           (long)(now - _last_pong_ms) >= (long)PONG_TIMEOUT_MS;
}

// Send exhaust temperature delta
void SignalKBroker::sendEngineDelta() {
    if (!_ws_open || !_ws) return;
    auto delta = _ds18b20_proc.getExhaustTempDelta();
    if (!validf(delta.exhaust_temp_k)) return;

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

// Send fuel level ratio delta
void SignalKBroker::sendTankDelta() {
    if (!_ws_open || !_ws) return;
    auto delta = _vdo_proc.getFuelLevelDelta();
    if (!validf(delta.fuel_level_ratio)) return;

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
    if (!_ws_open || !_ws) return;
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
    bool ok = _ws->send(buf, n);
    if (!ok) closeWebsocket();
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
            _last_pong_ms = millis();   // seed liveness on open
            break;
        case WebsocketsEvent::ConnectionClosed:
            //Serial.println("[SK] WebSocket closed");
            _ws_open = false;
            _capacity_sent = false;
            break;
        case WebsocketsEvent::GotPing:
            if (_ws) _ws->pong();
            break;
        case WebsocketsEvent::GotPong:
            _last_pong_ms = millis();   // liveness refresh — feeds isStale()
            break;
        default:
            break;
    }
}

// Serialize doc and send; close WebSocket on failure
bool SignalKBroker::sendDoc(StaticJsonDocument<512> &doc) {
    char buf[640];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    bool ok = _ws->send(buf, n);
    if (!ok) closeWebsocket();
    return ok;
}
