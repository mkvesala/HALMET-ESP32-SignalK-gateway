#include "ESPNowBroker.h"
#include "helpers.h"

// === P U B L I C ===

// Constructor
ESPNowBroker::ESPNowBroker(DS18B20Processor &ds18b20_proc, VDOProcessor &vdo_proc)
    : _ds18b20_proc(ds18b20_proc)
    , _vdo_proc(vdo_proc)
{}

// Initialize ESP-NOW with broadcast peer and callbacks
bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) return false;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    _initialized = true;
    return true;
}

// Broadcast exhaust temperature if changed beyond deadband or keepalive interval elapsed
void ESPNowBroker::sendEngineDelta() {
    if (!_initialized) return;
    auto delta = _ds18b20_proc.getExhaustTempDelta();
    if (!validf(delta.exhaust_temp_k)) return;

    unsigned long now = millis();
    bool changed   = !validf(_last_sent_temp_k) || fabsf(delta.exhaust_temp_k - _last_sent_temp_k) >= DB_TEMP_K;
    bool keepalive = (long)(now - _last_engine_send_ms) >= (long)ESPNOW_KEEPALIVE_MS;
    if (!changed && !keepalive) return;
    _last_sent_temp_k    = delta.exhaust_temp_k;
    _last_engine_send_ms = now;

    ESPNow::ESPNowPacket<ESPNow::HALMETEngineDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::HALMET_ENGINE_DELTA,
                       sizeof(ESPNow::HALMETEngineDelta));
    pkt.payload.exhaust_temp_k = delta.exhaust_temp_k;
    esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
}

// Broadcast fuel level ratio if changed beyond deadband or keepalive interval elapsed
void ESPNowBroker::sendTankDelta() {
    if (!_initialized) return;
    auto delta = _vdo_proc.getFuelLevelDelta();
    if (!validf(delta.fuel_level_ratio)) return;

    unsigned long now = millis();
    bool changed   = !validf(_last_sent_level) || fabsf(delta.fuel_level_ratio - _last_sent_level) >= DB_LEVEL;
    bool keepalive = (long)(now - _last_tank_send_ms) >= (long)ESPNOW_KEEPALIVE_MS;
    if (!changed && !keepalive) return;
    _last_sent_level   = delta.fuel_level_ratio;
    _last_tank_send_ms = now;

    ESPNow::ESPNowPacket<ESPNow::HALMETTankDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::HALMET_TANK_DELTA,
                       sizeof(ESPNow::HALMETTankDelta));
    pkt.payload.fuel_level_ratio = delta.fuel_level_ratio;
    esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
}

// === P R I V A T E ===

// Send callback — not used
void ESPNowBroker::onDataSent(const esp_now_send_info_t * /*info*/,
                               esp_now_send_status_t /*status*/) {}

// Receive callback — validate magic, ignore all incoming packets (no commands defined yet)
void ESPNowBroker::onDataRecv(const esp_now_recv_info_t * /*recv_info*/,
                               const uint8_t *data, int len) {
    if (len < static_cast<int>(sizeof(ESPNow::ESPNowHeader))) return;
    ESPNow::ESPNowHeader hdr;
    memcpy(&hdr, data, sizeof(ESPNow::ESPNowHeader));
    if (hdr.magic != ESPNow::ESPNOW_MAGIC) return;
    // No incoming commands handled in this version
}
