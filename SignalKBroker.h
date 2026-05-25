#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"

// === S I G N A L K B R O K E R  C L A S S ===
//
// WebSocket connection to SignalK server.
// Sends exhaust temperature and fuel level deltas.
// Tank capacity is sent once on WebSocket connection.

namespace websockets {
    class WebsocketsClient;
    class WebsocketsMessage;
    enum class WebsocketsEvent;
}

class SignalKBroker {
public:
    explicit SignalKBroker(DS18B20Processor &ds18b20_proc, VDOProcessor &vdo_proc);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();

    void sendEngineDelta();    // propulsion.0.exhaustTemperature [K]
    void sendTankDelta();      // tanks.fuel.0.currentLevel [ratio]
    void sendTankCapacity();   // tanks.fuel.0.capacity [m³] — once on connect

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
    websockets::WebsocketsClient  _ws;

    StaticJsonDocument<512> _engine_doc;
    StaticJsonDocument<512> _tank_doc;

    bool _ws_open = false;
    char _sk_url[512]    = {};
    char _sk_source[32]  = {};

    bool _capacity_sent = false;
};
