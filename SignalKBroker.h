#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include <memory>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"
#include "WaterProcessor.h"

// === S I G N A L K B R O K E R  C L A S S ===
//
// WebSocket connection to SignalK server.
// Sends exhaust temperature, fuel level and fresh water level deltas.
// Tank capacities are sent once on WebSocket connection.

namespace websockets {
    class WebsocketsClient;
    class WebsocketsMessage;
    enum class WebsocketsEvent;
}

class SignalKBroker {
public:
    explicit SignalKBroker(DS18B20Processor &ds18b20_proc,
                           VDOProcessor     &vdo_proc,
                           WaterProcessor   &water_proc);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();

    void sendEngineDelta();    // propulsion.0.exhaustTemperature [K]
    void sendTankDelta();      // tanks.fuel.0.currentLevel [ratio]
    void sendWaterDelta();     // tanks.freshWater.0.currentLevel [ratio]
    void sendTankCapacity();   // tanks.*.capacity [m³] — both tanks, once on connect

    void ping();                             // send a client ping frame if open
    bool isStale(unsigned long now) const;   // open but no pong within PONG_TIMEOUT_MS

    const char* getSignalKSource() { return _sk_source; }
    bool isOpen() const { return _ws_open; }

private:
    void setSignalKURL();
    void setSignalKSource();
    void onMessageCallback(websockets::WebsocketsMessage msg);
    void onEventCallback(websockets::WebsocketsEvent event, const String &data);

    bool sendDoc(StaticJsonDocument<512> &doc);

    DS18B20Processor             &_ds18b20_proc;
    VDOProcessor                 &_vdo_proc;
    WaterProcessor               &_water_proc;
    std::unique_ptr<websockets::WebsocketsClient> _ws;  // fresh instance every connect

    StaticJsonDocument<512> _engine_doc;
    StaticJsonDocument<512> _tank_doc;
    StaticJsonDocument<512> _water_doc;

    bool _ws_open = false;
    char _sk_url[512]    = {};
    char _sk_source[32]  = {};

    bool _capacity_sent = false;

    // Liveness — half-open TCP detection via client ping / server pong
    static constexpr unsigned long PONG_TIMEOUT_MS = 29989UL;  // ~30 s w/o pong → stale
    unsigned long _last_pong_ms = 0;   // millis() of last GotPong / open; 0 = not connected
};
