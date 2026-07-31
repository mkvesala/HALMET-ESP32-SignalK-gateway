#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

// === V D O S E N S O R  C L A S S ===
//
// Reads VDO resistive sender resistance via ADS1115 ADC in CCS mode.
// HALMET: ADS1115 at 0x4B, I2C on default Arduino pins (SDA=21, SCL=22).
// CCS jumper must be enabled on HALMET board for input A1.

class VDOSensor {
public:
    VDOSensor();

    bool begin();
    bool available() const { return _available; }
    // Returns sender resistance in ohms. Returns false on read failure.
    bool readResistance(float &ohms);

    // Public so WebUIManager can reconstruct volts/ADC counts from ohms and show
    // the reject threshold, without holding a reference to the hardware layer.
    static constexpr float   CCS_CURRENT_A = 0.001f;     // 1 mA constant current source (measured on HALMET)
    static constexpr float   ADS_LSB_V     = 0.0001875f; // LSB size at ±6.144 V gain
    static constexpr float   MIN_VOLTAGE_V = 0.005f;     // below this CCS is not ready (< 5 Ω threshold)

private:
    static constexpr uint8_t ADS_ADDR      = 0x4B;       // HALMET: ADDR pin tied to SCL
    static constexpr uint8_t ADS_CHANNEL   = 0;          // A1 input = channel 0

    Adafruit_ADS1115 _ads;
    bool             _available = false;
};
