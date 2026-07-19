#include "WaterProcessor.h"

// === P U B L I C ===

// Constructor
WaterProcessor::WaterProcessor(WaterSensor &sensor)
    : _sensor(sensor)
{}

// Three-phase filtering: raw → (window full) median → (first median) EMA init → EMA
void WaterProcessor::updateLevel(float ohms) {
    _samples[_sample_idx] = ohms;
    _sample_idx = (_sample_idx + 1) % MEDIAN_WINDOW;
    if (_sample_count < MEDIAN_WINDOW) _sample_count++;

    if (_sample_count < MEDIAN_WINDOW) {
        // Phase 1: window not yet full — send raw value
        _delta.water_level_ratio = fillRatio(ohms);
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
    _delta.water_level_ratio = fillRatio(_ema);
}

// Return current filtered delta
WaterLevelDelta WaterProcessor::getWaterLevelDelta() const {
    return _delta;
}

// Delegate to sensor
bool WaterProcessor::available() const {
    return _sensor.available();
}

// === P R I V A T E ===

// Piecewise-linear interpolation over the calibration table: resistance → fill ratio [0.0..1.0]
float WaterProcessor::fillRatio(float ohms) const {
    // Outside the calibrated range: clamp, do not extrapolate
    if (ohms <= WaterCal::OHMS[0])                        return 0.0f;
    if (ohms >= WaterCal::OHMS[WaterCal::CAL_POINTS - 1]) return 1.0f;

    for (int i = 0; i < WaterCal::CAL_POINTS - 1; i++) {
        if (ohms < WaterCal::OHMS[i + 1]) {
            float span = WaterCal::OHMS[i + 1] - WaterCal::OHMS[i];
            if (span <= 0.0f) return (float)i * WaterCal::RATIO_STEP;  // unreachable while the static_assert holds
            float t = (ohms - WaterCal::OHMS[i]) / span;
            return ((float)i + t) * WaterCal::RATIO_STEP;
        }
    }
    return 1.0f;
}

// Copy samples, sort, return middle element
float WaterProcessor::computeMedian() {
    float buf[MEDIAN_WINDOW];
    memcpy(buf, _samples, sizeof(buf));
    std::sort(buf, buf + MEDIAN_WINDOW);
    return buf[MEDIAN_WINDOW / 2];
}
