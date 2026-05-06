#include "VDOSensor.h"

// === P U B L I C ===

// Constructor
VDOSensor::VDOSensor() {}

// Initialize ADS1115 — Wire.begin() already called by HALMETApplication before this
bool VDOSensor::begin() {
    if (!_ads.begin(ADS_ADDR)) { _available = false; return false; }
    _ads.readADC_SingleEnded(ADS_CHANNEL);  // prime ADS1115: first conversion can return stale data
    _available = true;
    return true;
}

// Read sender resistance in ohms via CCS measurement
bool VDOSensor::readResistance(float &ohms) {
    if (!_available) return false;
    int16_t raw = _ads.readADC_SingleEnded(ADS_CHANNEL);
    float voltage = raw * ADS_LSB_V;
    if (voltage < 0.0f) voltage = 0.0f;
    Serial.printf("[VDO] raw=%d  voltage=%.4f V  ", raw, voltage);
    if (voltage < MIN_VOLTAGE_V) {
        Serial.println("(below min — CCS not ready, skipping)");
        return false;
    }
    ohms = voltage / CCS_CURRENT_A;
    Serial.printf("ohms=%.1f\n", ohms);
    return true;
}
