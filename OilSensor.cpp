#include "OilSensor.h"

// === P U B L I C ===

// Constructor
OilSensor::OilSensor() {}

// Initialize ADS1115 — Wire.begin() already called by HALMETApplication before this
bool OilSensor::begin() {
    if (!_ads.begin(ADS_ADDR)) { _available = false; return false; }
    _ads.readADC_SingleEnded(ADS_CHANNEL);  // prime ADS1115: first conversion can return stale data
    _available = true;
    return true;
}

// Read the gauge S terminal voltage. No range checks here — whether a voltage is
// plausible depends on the gauge calibration, which belongs to OilProcessor.
bool OilSensor::readVoltage(float &volts) {
    if (!_available) return false;
    int16_t raw = _ads.readADC_SingleEnded(ADS_CHANNEL);
    float v = raw * ADS_LSB_V * INPUT_DIVIDER;
    if (v < 0.0f) v = 0.0f;   // a negative raw is noise around 0 V, not a fault
    volts = v;
    return true;
}
