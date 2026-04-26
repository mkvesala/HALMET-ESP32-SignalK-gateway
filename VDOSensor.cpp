#include "VDOSensor.h"
#include <Wire.h>

// === P U B L I C ===

// Constructor
VDOSensor::VDOSensor() {}

// Initialize ADS1115 over I2C — HALMET uses default Arduino I2C pins (SDA=21, SCL=22)
bool VDOSensor::begin() {
    Wire.begin();
    _available = _ads.begin(ADS_ADDR);
    return _available;
}

// Read sender resistance in ohms via CCS measurement
bool VDOSensor::readResistance(float &ohms) {
    if (!_available) return false;
    int16_t raw = _ads.readADC_SingleEnded(ADS_CHANNEL);
    float voltage = raw * ADS_LSB_V;
    if (voltage < 0.0f) voltage = 0.0f;
    ohms = voltage / CCS_CURRENT_A;
    return true;
}
