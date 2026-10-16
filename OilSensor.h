#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

// === O I L S E N S O R  C L A S S ===
//
// Passively reads the voltage on the S terminal of the existing VDO oil pressure gauge
// via ADS1115 ADC channel 2 (HALMET analog input A3).
// The CCS jumper must NOT be fitted for A3: the gauge itself drives current through the
// sender, and the HALMET constant current source would add to it and corrupt the gauge.
//
// This is the SAME physical ADS1115 that VDOSensor and WaterSensor use, on a different
// channel. A separate Adafruit_ADS1115 instance is safe here: the driver holds no channel
// state (the mux is encoded in every conversion's config write), and all sensors are read
// sequentially from the main loop. If the ADC is ever read from a FreeRTOS task or an ISR,
// replace all instances with one shared, mutex-guarded bus object.

class OilSensor {
public:
    OilSensor();

    bool begin();
    bool available() const { return _available; }
    // Returns the voltage at the HALMET A3 terminal in volts. Returns false on read failure.
    bool readVoltage(float &volts);

    // Public so WebUIManager can reconstruct ADC counts from volts without holding a
    // reference to the hardware layer.
    static constexpr float   ADS_LSB_V     = 0.0001875f; // LSB size at ±6.144 V gain
    static constexpr float   INPUT_DIVIDER = 33.3f / 3.3f; // HALMET analog input divider (Hat Labs example firmware)

private:
    static constexpr uint8_t ADS_ADDR      = 0x4B;       // HALMET: ADDR pin tied to SCL
    static constexpr uint8_t ADS_CHANNEL   = 2;          // A3 input = channel 2

    Adafruit_ADS1115 _ads;
    bool             _available = false;
};
