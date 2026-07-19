#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"
#include "WaterProcessor.h"
#include "HALMETPreferences.h"
#include "SignalKBroker.h"

// === W E B U I M A N A G E R  C L A S S  ( S K E L E T O N ) ===
//
// HTTP server placeholder — no routes registered yet.
// begin() and handleRequest() are empty stubs.

class WebUIManager {
public:
    explicit WebUIManager(DS18B20Processor  &ds18b20_proc,
                          VDOProcessor      &vdo_proc,
                          WaterProcessor    &water_proc,
                          HALMETPreferences &prefs,
                          SignalKBroker     &signalk);

    void begin();          // placeholder — no routes registered
    void handleRequest();  // placeholder — calls server.handleClient()

private:
    DS18B20Processor  &_ds18b20_proc;
    VDOProcessor      &_vdo_proc;
    WaterProcessor    &_water_proc;
    HALMETPreferences &_prefs;
    SignalKBroker     &_signalk;
    WebServer          _server{80};
};
