#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

// === W A T E R S E N S O R  C L A S S ===
//
// Reads the fresh water tank resistive sender (0-190 Ω) via ADS1115 ADC in CCS mode.
// HALMET: ADS1115 at 0x4B, I2C on default Arduino pins (SDA=21, SCL=22).
// CCS jumper must be enabled on HALMET board for input A2.
//
// This is the SAME physical ADS1115 that VDOSensor uses, on a different channel.
// A separate Adafruit_ADS1115 instance is safe here: the driver holds no channel
// state (the mux is encoded in every conversion's config write), and both sensors
// are read sequentially from the main loop. If the ADC is ever read from a FreeRTOS
// task or an ISR, replace both instances with one shared, mutex-guarded bus object.

class WaterSensor {
public:
    WaterSensor();

    bool begin();
    bool available() const { return _available; }
    // Returns sender resistance in ohms. Returns false on read failure.
    bool readResistance(float &ohms);

private:
    static constexpr uint8_t ADS_ADDR      = 0x4B;       // HALMET: ADDR pin tied to SCL
    static constexpr uint8_t ADS_CHANNEL   = 1;          // A2 input = channel 1
    static constexpr float   CCS_CURRENT_A = 0.001f;     // 1 mA constant current source (measured on HALMET)
    static constexpr float   ADS_LSB_V     = 0.0001875f; // LSB size at ±6.144 V gain
    static constexpr float   MAX_OHMS      = 400.0f;     // above this: open circuit / sender disconnected

    Adafruit_ADS1115 _ads;
    bool             _available = false;
};
