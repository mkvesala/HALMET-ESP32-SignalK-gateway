#include "WebUIManager.h"

// === P U B L I C ===

// Constructor
WebUIManager::WebUIManager(DS18B20Processor  &ds18b20_proc,
                           VDOProcessor      &vdo_proc,
                           WaterProcessor    &water_proc,
                           HALMETPreferences &prefs,
                           SignalKBroker     &signalk)
    : _ds18b20_proc(ds18b20_proc)
    , _vdo_proc(vdo_proc)
    , _water_proc(water_proc)
    , _prefs(prefs)
    , _signalk(signalk)
{}

// Placeholder — no routes registered
void WebUIManager::begin() {}

// Placeholder — drives the HTTP server event loop
void WebUIManager::handleRequest() {
    _server.handleClient();
}
