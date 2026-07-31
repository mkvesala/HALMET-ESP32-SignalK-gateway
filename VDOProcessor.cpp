#include "VDOProcessor.h"

// === P U B L I C ===

// Constructor
VDOProcessor::VDOProcessor(VDOSensor &sensor)
    : _sensor(sensor)
{}

// Three-phase filtering: raw → (window full) median → (first median) EMA init → EMA
void VDOProcessor::updateLevel(float ohms) {
    _last_ohms      = ohms;
    _last_update_ms = millis();

    _samples[_sample_idx] = ohms;
    _sample_idx = (_sample_idx + 1) % MEDIAN_WINDOW;
    if (_sample_count < MEDIAN_WINDOW) _sample_count++;

    if (_sample_count < MEDIAN_WINDOW) {
        // Phase 1: window not yet full — send raw value
        _delta.fuel_level_ratio = fillRatio(ohms);
        return;
    }

    float median = computeMedian();

    if (!_ema_initialized) {
        // Phase 2: initialize EMA to first median to avoid a long warm-up ramp
        _ema = median;
        _ema_initialized = true;
    }

    // Phase 3: normal filtered operation
    _ema = EMA_ALPHA * median + (1.0f - EMA_ALPHA) * _ema;
    _delta.fuel_level_ratio = fillRatio(_ema);
}

// Return current filtered delta
FuelLevelDelta VDOProcessor::getFuelLevelDelta() const {
    return _delta;
}

// Delegate to sensor
bool VDOProcessor::available() const {
    return _sensor.available();
}

// === P R I V A T E ===

// Linear interpolation: resistance → fill ratio [0.0..1.0]
float VDOProcessor::fillRatio(float ohms) const {
    float r = (ohms - VDO_OHMS_EMPTY) / (VDO_OHMS_FULL - VDO_OHMS_EMPTY);
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return r;
}

// Copy samples, sort, return middle element
float VDOProcessor::computeMedian() {
    float buf[MEDIAN_WINDOW];
    memcpy(buf, _samples, sizeof(buf));
    std::sort(buf, buf + MEDIAN_WINDOW);
    return buf[MEDIAN_WINDOW / 2];
}
