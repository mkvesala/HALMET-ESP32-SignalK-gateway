#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "VDOSensor.h"

// === V D O P R O C E S S O R  C L A S S ===
//
// Converts raw sender resistance to fuel level ratio [0.0..1.0].
// Filtering: median window (120 samples, 2 s interval) → EMA (α=0.005).
// Phase 1 (window filling): raw value is sent immediately.
// Phase 2 (first full window): EMA is initialized to the first median value.
// Phase 3 (normal operation): median → EMA → delta.

struct FuelLevelDelta {
    float fuel_level_ratio = NAN;  // tanks.fuel.0.currentLevel [0.0..1.0]
};

class VDOProcessor {
public:
    explicit VDOProcessor(VDOSensor &sensor);

    void updateLevel(float ohms);
    FuelLevelDelta getFuelLevelDelta() const;
    float getCapacityM3() const { return TANK_CAPACITY_M3; }
    bool available() const;

private:
    static constexpr int   MEDIAN_WINDOW    = 120;
    static constexpr float EMA_ALPHA        = 0.005f;
    static constexpr float VDO_OHMS_FULL    = 10.0f;    // VDO European sender: 10 Ω = full
    static constexpr float VDO_OHMS_EMPTY   = 180.0f;   // 180 Ω = empty
    static constexpr float TANK_CAPACITY_M3 = 0.400f;   // 400 L = 0.4 m³

    float fillRatio(float ohms) const;
    float computeMedian();

    VDOSensor      &_sensor;
    FuelLevelDelta  _delta;

    float _samples[MEDIAN_WINDOW] = {};
    int   _sample_idx             = 0;
    int   _sample_count           = 0;
    float _ema                    = 0.0f;
    bool  _ema_initialized        = false;
};
