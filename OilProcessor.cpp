#include "OilProcessor.h"
#include "helpers.h"

// === P U B L I C ===

// Constructor
OilProcessor::OilProcessor(OilSensor &sensor)
    : _sensor(sensor)
{}

// Validate voltage → resistance, then median → EMA → pressure
void OilProcessor::updateVoltage(float volts) {
    _last_volts    = volts;
    _last_volts_ms = millis();

    float ohms = ohmsFromVolts(volts);
    if (!validf(ohms)) return;
    if (ohms < R_SHORT_OHMS || ohms > MAX_OHMS) return;

    _last_ohms      = ohms;
    _last_update_ms = _last_volts_ms;
    _has_sample     = true;

    _samples[_sample_idx] = ohms;
    _sample_idx = (_sample_idx + 1) % MEDIAN_WINDOW;
    if (_sample_count < MEDIAN_WINDOW) _sample_count++;

    if (_sample_count < MEDIAN_WINDOW) {
        // Window not yet full — use raw value
        _pressure_bar = pressureBar(ohms);
    } else {
        float median = computeMedian();
        if (!_ema_initialized) {
            _ema = median;   // initialize EMA to first median to avoid a warm-up ramp
            _ema_initialized = true;
        }
        _ema = EMA_ALPHA * median + (1.0f - EMA_ALPHA) * _ema;
        _pressure_bar = pressureBar(_ema);
    }

    // An uncalibrated model is shown on the calibration page but never published
    _delta.oil_pressure_pa = OilCal::CALIBRATED ? _pressure_bar * 100000.0f : NAN;
}

// Return current delta, or NAN if the last accepted sample is stale
OilPressureDelta OilProcessor::getOilPressureDelta() const {
    if (!_has_sample || (long)(millis() - _last_update_ms) >= (long)STALE_MS) {
        return OilPressureDelta{};
    }
    return _delta;
}

// Delegate to sensor
bool OilProcessor::available() const {
    return _sensor.available();
}

// === P R I V A T E ===

// Thevenin model: R = R_TH * V / (V_OC - V). NAN when the sender is open (V >= V_OC).
float OilProcessor::ohmsFromVolts(float volts) const {
    if (!validf(volts) || volts < 0.0f) return NAN;
    float headroom = OilCal::V_OC - volts;
    if (headroom <= 0.0f) return NAN;
    return OilCal::R_TH * volts / headroom;
}

// Piecewise-linear interpolation over the VDO sender curve: resistance → bar
float OilProcessor::pressureBar(float ohms) const {
    // Outside the sender range: clamp, do not extrapolate
    if (ohms <= OilCal::OHMS[0])                      return 0.0f;
    if (ohms >= OilCal::OHMS[OilCal::CAL_POINTS - 1]) return OilCal::CAL_MAX_BAR;

    for (int i = 0; i < OilCal::CAL_POINTS - 1; i++) {
        if (ohms < OilCal::OHMS[i + 1]) {
            float span = OilCal::OHMS[i + 1] - OilCal::OHMS[i];
            if (span <= 0.0f) return (float)i * OilCal::CAL_STEP_BAR;  // unreachable while the static_assert holds
            float t = (ohms - OilCal::OHMS[i]) / span;
            return ((float)i + t) * OilCal::CAL_STEP_BAR;
        }
    }
    return OilCal::CAL_MAX_BAR;
}

// Copy samples, sort, return middle element
float OilProcessor::computeMedian() {
    float buf[MEDIAN_WINDOW];
    memcpy(buf, _samples, sizeof(buf));
    std::sort(buf, buf + MEDIAN_WINDOW);
    return buf[MEDIAN_WINDOW / 2];
}
