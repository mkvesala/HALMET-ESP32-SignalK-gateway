#include "DS18B20Sensor.h"

// === P U B L I C ===

// Constructor
DS18B20Sensor::DS18B20Sensor()
    : _one_wire(PIN_ONE_WIRE)
    , _sensors(&_one_wire)
{}

// Scan 1-Wire bus; returns true if at least one sensor found
bool DS18B20Sensor::begin() {
    _sensors.begin();
    _available = (_sensors.getDeviceCount() > 0);
    return _available;
}

// Request temperature conversion and read result — blocks ~750 ms
bool DS18B20Sensor::requestAndRead(float &temp_c) {
    if (!_available) return false;
    _sensors.requestTemperatures();
    float t = _sensors.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) return false;
    temp_c = t;
    return true;
}
