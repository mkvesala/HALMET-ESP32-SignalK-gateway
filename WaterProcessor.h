#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "WaterSensor.h"

// === W A T E R  C A L I B R A T I O N  T A B L E ===
//
// The fresh water tank is an inverted pyramid: it widens towards the top, so the surface
// rises more slowly per litre the fuller the tank gets, and the sender's ohms-per-litre
// FALLS with fill level (~2.92 Ohm/L over 0..30 L, ~1.73 Ohm/L over 30..55 L). A linear
// map would be wrong everywhere except at the endpoints, which is why this table exists.
//
// The row INDEX carries the volume: row i is i * CAL_STEP_L litres. Only the resistance is
// stored, one number per row — a second volume column could silently desynchronise from it.
// Provenance is marked per row: (m) measured on the boat, (i) linear interpolation between
// two measured rows, (t) derived, see "the 57.5..80 L segment" below.
//
// SENDER BEHAVIOUR (measured 2026-08-21, full procedure in docs/water_level_calibration.md)
//   The sender is a wire-wound card with a wiper, so it steps in ~14.53 Ohm increments: all
//   eight measured points fit R = 0.8 + 14.53*k (k integer) to within 0.4 Ohm. Its volume
//   resolution is therefore ~5 L low in the tank and ~10 L high up, and calibrating finer
//   than that is meaningless. 174.8 Ohm (k = 12) is the card's TOP contact — no higher
//   reading exists — which is consistent with the nominal 0..180 Ohm rating.
//
// TWO PLATEAUS, DIFFERENT CAUSES — do not confuse them:
//
//   174.8 Ohm holds all the way from 80 L down to 57.5 L, because it is the top contact
//   above. The whole upper third of the tank reads the same, so the 57.5..80 L rows are not
//   measurable and are derived instead: a straight segment from the measured
//   (57.5 L, 146.1 Ohm) to the sender's nominal (80 L, 180.0 Ohm). That needs 1.507 Ohm/L,
//   which is LOWER than the 1.732 Ohm/L measured just below it — exactly what a tank that
//   keeps widening requires, so the 0..180 Ohm assumption checks out against the measured
//   shape rather than being assumed into it.
//
//   146.1 Ohm is the AIR POCKET ceiling. At 57.5 L the surface drops to the level of the
//   inspection hatch the sender is mounted on, an air pocket re-forms under the hatch, and
//   the reading steps 174.8 -> 146.1 with no change in volume. The step is accepted, not
//   corrected: a pinned float always reads LOW, which fails safe, and no filter can recover
//   a level the float never reached. The real fix is mechanical — vent the pocket.
//
// CONSEQUENCE: anywhere between 57.5 L and 80 L the tank reports a constant 76.6 L, so a
// tank holding 58 L reads ~19 L high until the level crosses the step. The 0..57.5 L range
// — the part that decides whether the water lasts — is measured and accurate to the
// sender's ~5 L resolution.
//
// RULES (enforced at compile time by the static_asserts below):
//   - Exactly CAL_POINTS rows, evenly spaced in volume (the volume is the row index).
//   - Resistance must INCREASE by at least MIN_STEP_OHMS on every step.
//   - The table must not extend past the tank's physical capacity.
//   A table that violates these is a build error, not a silent wrong reading.

namespace WaterCal {

    static constexpr float TANK_CAPACITY_L = 80.0f;  // physical tank volume
    static constexpr float CAL_STEP_L      = 2.5f;   // litres per table row
    static constexpr int   CAL_POINTS      = 33;     // rows at 0.0 .. 80.0 L
    static constexpr float MIN_STEP_OHMS   = 2.0f;   // ~11 ADC LSB — below this, noise dominates a whole step

    // EDIT ONLY THE NUMBERS IN THE FIRST COLUMN
    static constexpr float OHMS[CAL_POINTS] = {
          0.8f,   //   0.0 L  (m) empty
          8.1f,   //   2.5 L  (i)
         15.4f,   //   5.0 L  (i)
         22.7f,   //   7.5 L  (i)
         30.0f,   //  10.0 L  (m)
         37.3f,   //  12.5 L  (i)
         44.6f,   //  15.0 L  (i)
         52.0f,   //  17.5 L  (i)
         59.3f,   //  20.0 L  (m)
         66.5f,   //  22.5 L  (i)
         73.8f,   //  25.0 L  (i)
         81.0f,   //  27.5 L  (i)
         88.3f,   //  30.0 L  (m)
         91.9f,   //  32.5 L  (i)
         95.5f,   //  35.0 L  (i)
         99.2f,   //  37.5 L  (i)
        102.8f,   //  40.0 L  (m)
        110.0f,   //  42.5 L  (i)
        117.2f,   //  45.0 L  (m)
        120.8f,   //  47.5 L  (i)
        124.4f,   //  50.0 L  (i)
        128.0f,   //  52.5 L  (i)
        131.6f,   //  55.0 L  (m)
        146.1f,   //  57.5 L  (m) air pocket ceiling — the reading parks here while the pocket is present
        149.9f,   //  60.0 L  (t)
        153.6f,   //  62.5 L  (t)
        157.4f,   //  65.0 L  (t)
        161.2f,   //  67.5 L  (t)
        164.9f,   //  70.0 L  (t)
        168.7f,   //  72.5 L  (t)
        172.5f,   //  75.0 L  (t)
        176.2f,   //  77.5 L  (t)
        180.0f,   //  80.0 L  (t) full — sender's nominal maximum
    };

    static constexpr float CAL_MAX_L = (float)(CAL_POINTS - 1) * CAL_STEP_L;  // 80.0 L

    // Compile-time table validation — recursive constexpr form is valid C++11
    constexpr bool tableIsValid(int i) {
        return (i >= CAL_POINTS - 1)
             ? true
             : (OHMS[i + 1] - OHMS[i] >= MIN_STEP_OHMS) && tableIsValid(i + 1);
    }

    static_assert(tableIsValid(0),
                  "WaterCal::OHMS is not strictly increasing by at least MIN_STEP_OHMS. "
                  "Re-measure the offending step, or average the two readings.");

    static_assert(CAL_MAX_L <= TANK_CAPACITY_L,
                  "WaterCal: the calibration table extends past the tank's physical capacity.");

} // namespace WaterCal

// === W A T E R P R O C E S S O R  C L A S S ===
//
// Converts raw sender resistance to fresh water volume and level ratio using the
// piecewise-linear WaterCal table. Litres are the primary quantity; the ratio is derived
// from the tank's physical capacity, so a full tank reads 1.0 at the sender's nominal
// maximum (in practice ~0.957, since 174.8 Ohm is the highest contact that exists).
// Filtering: median window (60 samples, 2 s interval) -> EMA (alpha=0.04, tau ~ 50 s).
// Phase 1 (window filling): raw value is sent immediately.
// Phase 2 (first full window): EMA is initialized to the first median value.
// Phase 3 (normal operation): median -> EMA -> delta.
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
    float getCapacityM3() const { return WaterCal::TANK_CAPACITY_L / 1000.0f; }
    bool available() const;

    // Calibration/debug accessors — consumed by WebUIManager, see docs/water_level_calibration.md
    float         getLastOhms() const      { return _last_ohms; }
    float         getFilteredOhms() const  { return _ema_initialized ? _ema : NAN; }
    float         getVolumeLitres() const  { return _delta.water_level_ratio * WaterCal::TANK_CAPACITY_L; }
    bool          isAboveFloatCeiling() const;
    int           getSampleCount() const   { return _sample_count; }
    int           getWindowSize() const    { return MEDIAN_WINDOW; }
    unsigned long getLastUpdateMs() const  { return _last_update_ms; }

private:
    static constexpr int   MEDIAN_WINDOW      = 60;      // ~2 min at 2011 ms sampling
    static constexpr float EMA_ALPHA          = 0.04f;   // tau ~ 50 s — water draw is bursty, unlike fuel burn
    static constexpr float FLOAT_CEILING_OHMS = 174.8f;  // sender's top contact — at or above this the volume is ambiguous

    float fillRatio(float ohms) const;
    float volumeLitres(float ohms) const;
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
