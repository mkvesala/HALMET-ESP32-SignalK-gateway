#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "WaterSensor.h"

// === W A T E R  C A L I B R A T I O N  T A B L E ===
//
// The fresh water tank is irregularly shaped, so resistance does not map linearly
// to volume. This table holds the MEASURED sender resistance at each 10 % fill
// step; WaterProcessor interpolates linearly between the points.
//
// HOW TO CALIBRATE (full procedure in docs/water_level_calibration.md):
//   1. Drain the tank completely, let it settle, read the reported ohms.
//   2. Write that number on the "0 %" row.
//   3. Add 10 L, let it settle, read the ohms, write it on the "10 %" row.
//   4. Repeat until the tank is full (100 % row), then reflash.
//
// RULES (enforced at compile time by the static_assert below):
//   - Exactly CAL_POINTS rows, evenly spaced fill steps (the ratio is the row index).
//   - Resistance must INCREASE by at least MIN_STEP_OHMS on every step.
//   A table that violates these is a build error, not a silent wrong reading.
//
// The shipped values are a placeholder linear ramp 0..190 Ω, so the firmware behaves
// as a plain linear map until the tank has been calibrated.

namespace WaterCal {

    static constexpr int   CAL_POINTS    = 11;    // 0 %..100 % in 10 % steps
    static constexpr float MIN_STEP_OHMS = 2.0f;  // ~11 ADC LSB — below this, noise dominates a whole step

    // EDIT ONLY THE NUMBERS IN THE FIRST COLUMN
    static constexpr float OHMS[CAL_POINTS] = {
          0.8f,   //   0 %  —   0 L  (empty)
         30.0f,   //  10 %  —  10 L
         59.3f,   //  20 %  —  20 L
         88.3f,   //  30 %  —  30 L
        102.8f,   //  40 %  —  40 L
        146.1f,   //  50 %  —  50 L
        146.1f,   //  60 %  —  60 L
        146.1f,   //  70 %  —  70 L
        146.1f,   //  80 %  —  80 L (FULL - tank is ONLY 80 L, not 100 L)
        400.0f,   //  90 %  —  90 L (IMPOSSIBLE)
        400.0f,   // 100 %  — 100 L (IMPOSSIBLE)
    };

    static constexpr float RATIO_STEP = 1.0f / (float)(CAL_POINTS - 1);

    // Compile-time table validation — recursive constexpr form is valid C++11
    constexpr bool tableIsValid(int i) {
        return (i >= CAL_POINTS - 1)
             ? true
             : (OHMS[i + 1] - OHMS[i] >= MIN_STEP_OHMS) && tableIsValid(i + 1);
    }

    static_assert(tableIsValid(0),
                  "WaterCal::OHMS is not strictly increasing by at least MIN_STEP_OHMS. "
                  "Re-measure the offending 10 % step, or average the two readings.");

} // namespace WaterCal

// === W A T E R P R O C E S S O R  C L A S S ===
//
// Converts raw sender resistance to fresh water level ratio [0.0..1.0] using the
// piecewise-linear WaterCal table (the tank is irregularly shaped).
// Filtering: median window (60 samples, 2 s interval) → EMA (α=0.02).
// Phase 1 (window filling): raw value is sent immediately.
// Phase 2 (first full window): EMA is initialized to the first median value.
// Phase 3 (normal operation): median → EMA → delta.
// The EMA smooths OHMS; the calibration lookup is applied last.
//
// The filter is deliberately faster than VDOProcessor's: fuel burns continuously at
// a few litres per hour, but water draw is bursty — a shower can take 15 % of the
// tank in minutes, and a 20-minute lag would make the gauge useless.

struct WaterLevelDelta {
    float water_level_ratio = NAN;  // tanks.freshWater.0.currentLevel [0.0..1.0]
};

class WaterProcessor {
public:
    explicit WaterProcessor(WaterSensor &sensor);

    void updateLevel(float ohms);
    WaterLevelDelta getWaterLevelDelta() const;
    float getCapacityM3() const { return TANK_CAPACITY_M3; }
    bool available() const;

    // Calibration/debug accessors — consumed by WebUIManager, see docs/water_level_calibration.md
    float         getLastOhms() const      { return _last_ohms; }
    float         getFilteredOhms() const  { return _ema_initialized ? _ema : NAN; }
    int           getSampleCount() const   { return _sample_count; }
    int           getWindowSize() const    { return MEDIAN_WINDOW; }
    unsigned long getLastUpdateMs() const  { return _last_update_ms; }

private:
    static constexpr int   MEDIAN_WINDOW    = 60;      // ~2 min at 2011 ms sampling
    static constexpr float EMA_ALPHA        = 0.02f;   // τ ≈ 100 s — water draw is bursty, unlike fuel burn
    static constexpr float TANK_CAPACITY_M3 = 0.100f;  // 100 L = 0.1 m³

    float fillRatio(float ohms) const;
    float computeMedian();

    WaterSensor     &_sensor;
    WaterLevelDelta  _delta;

    float _samples[MEDIAN_WINDOW] = {};
    int   _sample_idx             = 0;
    int   _sample_count           = 0;
    float _ema                    = 0.0f;
    bool  _ema_initialized        = false;

    // Last accepted raw reading and its timestamp. A failed read never reaches
    // updateLevel(), so a growing age is how an open circuit is told apart from
    // a genuinely steady tank.
    float         _last_ohms      = NAN;
    unsigned long _last_update_ms = 0;
};
