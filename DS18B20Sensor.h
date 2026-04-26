#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === D S 1 8 B 2 0 S E N S O R  C L A S S ===
//
// Raw 1-Wire I/O only — no business logic.
// requestAndRead() blocks ~750 ms; call only from a FreeRTOS task.

class DS18B20Sensor {
public:
    DS18B20Sensor();

    bool begin();
    bool available() const { return _available; }
    // Blocking (~750 ms) — call only from FreeRTOS task
    bool requestAndRead(float &temp_c);

private:
    static constexpr uint8_t PIN_ONE_WIRE = 4;   // HALMET 1-Wire DQ pin

    OneWire           _one_wire;
    DallasTemperature _sensors;
    bool              _available = false;
};
