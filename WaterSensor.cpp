#include "WaterSensor.h"

// === P U B L I C ===

// Constructor
WaterSensor::WaterSensor() {}

// Initialize ADS1115 — Wire.begin() already called by HALMETApplication before this
bool WaterSensor::begin() {
    if (!_ads.begin(ADS_ADDR)) { _available = false; return false; }
    _ads.readADC_SingleEnded(ADS_CHANNEL);  // prime ADS1115: first conversion can return stale data
    _available = true;
    return true;
}

// Read sender resistance in ohms via CCS measurement
// Unlike VDOSensor there is NO low-side reject: this sender reads 0 Ω when the tank
// is empty, so discarding low readings would freeze the level on a dry tank.
// The fault case is an open circuit, which the CCS drives to the rail — caught by MAX_OHMS.
bool WaterSensor::readResistance(float &ohms) {
    if (!_available) return false;
    int16_t raw = _ads.readADC_SingleEnded(ADS_CHANNEL);
    float voltage = raw * ADS_LSB_V;
    if (voltage < 0.0f) voltage = 0.0f;   // 0 Ω = empty is valid; a negative raw is noise, not a fault
    float r = voltage / CCS_CURRENT_A;
    if (r > MAX_OHMS) {
        return false;
    }
    ohms = r;
    return true;
}
