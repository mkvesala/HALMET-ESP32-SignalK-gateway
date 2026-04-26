#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "DS18B20Sensor.h"

// === D S 1 8 B 2 0 P R O C E S S O R  C L A S S ===
//
// Converts raw °C reading to Kelvin and stores a thread-safe delta struct.
// updateTemperature() is called from the DS18B20 FreeRTOS task.
// getExhaustTempDelta() is called from the main loop.

struct ExhaustTempDelta {
    float exhaust_temp_k = NAN;  // propulsion.0.exhaustTemperature [K]
};

class DS18B20Processor {
public:
    explicit DS18B20Processor(DS18B20Sensor &sensor);

    // Called from FreeRTOS task — portMUX guards the write
    void updateTemperature(float temp_c);

    // Called from main loop — portMUX guards the read
    ExhaustTempDelta getExhaustTempDelta() const;

    bool available() const;

private:
    DS18B20Sensor    &_sensor;
    ExhaustTempDelta  _delta;
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
