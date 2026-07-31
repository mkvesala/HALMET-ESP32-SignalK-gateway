#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"
#include "WaterProcessor.h"
#include "HALMETPreferences.h"
#include "SignalKBroker.h"

// === W E B U I M A N A G E R  C L A S S ===
//
// Calibration/debug HTTP page — no authentication, no settings, read-only.
// Exists to replace the serial monitor while measuring sender resistance at known
// fill levels (see docs/water_level_calibration.md).
//
//   GET /        static HTML + JS, served from flash in one send_P
//   GET /status  live JSON, polled by the page once a second
//   GET /cal     WaterCal::OHMS table, fetched once on page load
//
// Gated at compile time by HALMETApplication::WEB_UI_ENABLED.
// Reachable over the STA interface only: handleWebUI() requires WifiState::CONNECTED,
// and the SoftAP deauths every station that associates.

class WebUIManager {
public:
    explicit WebUIManager(DS18B20Processor  &ds18b20_proc,
                          VDOProcessor      &vdo_proc,
                          WaterProcessor    &water_proc,
                          HALMETPreferences &prefs,
                          SignalKBroker     &signalk);

    void begin();          // register routes and start the server
    void handleRequest();  // drive the HTTP server event loop

private:
    void handlePage();
    void handleStatus();
    void handleCal();

    // Fill one tank object of the /status response
    void fillTank(JsonObject o, float ohms, float filt, float ratio,
                  int samples, int window, unsigned long age_ms, bool ok);

    DS18B20Processor  &_ds18b20_proc;
    VDOProcessor      &_vdo_proc;
    WaterProcessor    &_water_proc;
    HALMETPreferences &_prefs;
    SignalKBroker     &_signalk;
    WebServer          _server{80};

    StaticJsonDocument<768> _status_doc;
};
