#include "DS18B20Processor.h"

// === P U B L I C ===

// Constructor
DS18B20Processor::DS18B20Processor(DS18B20Sensor &sensor)
    : _sensor(sensor)
{}

// Convert °C to K and store atomically — called from FreeRTOS task
void DS18B20Processor::updateTemperature(float temp_c) {
    portENTER_CRITICAL(&_mux);
    _delta.exhaust_temp_k = temp_c + 273.15f;
    portEXIT_CRITICAL(&_mux);
}

// Return a copy of the delta struct — called from main loop
ExhaustTempDelta DS18B20Processor::getExhaustTempDelta() const {
    ExhaustTempDelta copy;
    portENTER_CRITICAL(&_mux);
    copy = _delta;
    portEXIT_CRITICAL(&_mux);
    return copy;
}

// Delegate to sensor
bool DS18B20Processor::available() const {
    return _sensor.available();
}
