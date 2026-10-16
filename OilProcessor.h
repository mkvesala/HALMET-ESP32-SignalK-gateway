#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "OilSensor.h"

// === O I L  C A L I B R A T I O N ===
//
// HALMET measures the oil pressure PASSIVELY, in parallel with the existing VDO cockpit
// gauge (0-5 bar, ratio-meter movement). The gauge feeds the sender through its own coils,
// so HALMET sees a voltage, not a resistance. The gauge's internal network is linear and
// resistive and is fed from a fixed 12.3 V DC-DC supply, so at the S terminal it is exactly
// a Thevenin source:
//
//     V = V_OC * R / (R + R_TH)    =>    R = R_TH * V / (V_OC - V)
//
// V_OC and R_TH are measured once on the boat (docs/oil_pressure.md). Both are taken from
// HALMET's own readings at the same tap point, so the formula is a pure ratio: the exact
// input divider, an optional series resistor and the wiring resistance all cancel out.
//
// Until CALIBRATED is true, voltages and ohms are shown on the calibration page but no
// pressure is published — a guessed model must never reach SignalK as an oil pressure.
//
// OHMS is the VDO 0-5 bar sender curve from the VDO cockpit manual (TU00-0755-0000002,
// chapter 6.5). The row INDEX carries the pressure: row i is i * CAL_STEP_BAR bar.
// Validated at compile time like WaterCal::OHMS.

namespace OilCal {

    static constexpr bool  CALIBRATED   = false;   // set true once V_OC and R_TH are measured
    static constexpr float V_OC         = 6.0f;    // TODO measured: S voltage with the sender disconnected [V]
    static constexpr float R_TH         = 100.0f;  // TODO measured: gauge source resistance seen at S [Ohm]

    static constexpr float CAL_STEP_BAR  = 0.5f;   // bar per table row
    static constexpr int   CAL_POINTS    = 11;     // rows at 0.0 .. 5.0 bar
    static constexpr float MIN_STEP_OHMS = 2.0f;   // ~11 ADC LSB

    static constexpr float OHMS[CAL_POINTS] = {
         10.0f,   //  0.0 bar
         30.0f,   //  0.5 bar
         48.0f,   //  1.0 bar
         65.0f,   //  1.5 bar
         82.0f,   //  2.0 bar
         99.0f,   //  2.5 bar
        116.0f,   //  3.0 bar
        134.0f,   //  3.5 bar
        151.0f,   //  4.0 bar
        168.0f,   //  4.5 bar
        184.0f,   //  5.0 bar
    };

    static constexpr float CAL_MAX_BAR = (float)(CAL_POINTS - 1) * CAL_STEP_BAR;  // 5.0 bar

    // Compile-time table validation — recursive constexpr form is valid C++11
    constexpr bool tableIsValid(int i) {
        return (i >= CAL_POINTS - 1)
             ? true
             : (OHMS[i + 1] - OHMS[i] >= MIN_STEP_OHMS) && tableIsValid(i + 1);
    }

    static_assert(tableIsValid(0),
                  "OilCal::OHMS is not strictly increasing by at least MIN_STEP_OHMS.");

    static_assert(V_OC > 0.0f && R_TH > 0.0f,
                  "OilCal: V_OC and R_TH must be positive.");

} // namespace OilCal

// === O I L P R O C E S S O R  C L A S S ===
//
// Converts the gauge S terminal voltage to oil pressure [Pa].
// Rejects: V >= V_OC (sender open), R < R_SHORT_OHMS (S shorted or gauge unpowered),
// R > MAX_OHMS (implausible). A rejected sample never updates the delta, and the delta
// turns NAN once the last accepted sample is older than STALE_MS: a frozen "pressure OK"
// is more dangerous than a missing value.
// Filtering: median of MEDIAN_WINDOW samples -> EMA. Deliberately fast — oil pressure is
// dynamic, unlike tank levels. The EMA smooths OHMS; the sender curve is applied last.

struct OilPressureDelta {
    float oil_pressure_pa = NAN;  // propulsion.0.oilPressure [Pa]
};

class OilProcessor {
public:
    explicit OilProcessor(OilSensor &sensor);

    void updateVoltage(float volts);
    OilPressureDelta getOilPressureDelta() const;
    bool available() const;

    // Calibration/debug accessors — consumed by WebUIManager, see docs/oil_pressure.md
    float         getLastVolts() const     { return _last_volts; }
    float         getLastOhms() const      { return _last_ohms; }
    float         getFilteredOhms() const  { return _ema_initialized ? _ema : NAN; }
    float         getPressureBar() const   { return _pressure_bar; }
    bool          isCalibrated() const     { return OilCal::CALIBRATED; }
    unsigned long getLastVoltsMs() const   { return _last_volts_ms; }
    unsigned long getLastUpdateMs() const  { return _last_update_ms; }

private:
    static constexpr int           MEDIAN_WINDOW = 5;       // ~2.5 s at 499 ms sampling
    static constexpr float         EMA_ALPHA     = 0.3f;    // settles in a few seconds
    static constexpr float         R_SHORT_OHMS  = 3.0f;    // below: S shorted to ground, or gauge unpowered
    static constexpr float         MAX_OHMS      = 300.0f;  // above: sender open / broken S wire
    static constexpr unsigned long STALE_MS      = 4999;    // ~10 missed reads at 499 ms

    float ohmsFromVolts(float volts) const;
    float pressureBar(float ohms) const;
    float computeMedian();

    OilSensor        &_sensor;
    OilPressureDelta  _delta;

    float _samples[MEDIAN_WINDOW] = {};
    int   _sample_idx             = 0;
    int   _sample_count           = 0;
    float _ema                    = 0.0f;
    bool  _ema_initialized        = false;
    float _pressure_bar           = NAN;

    // Every reading, accepted or not — the calibration procedure needs the open-circuit
    // voltage, which is exactly the reading that is rejected.
    float         _last_volts     = NAN;
    unsigned long _last_volts_ms  = 0;

    // Last ACCEPTED reading and its timestamp — drives the stale check.
    float         _last_ohms      = NAN;
    unsigned long _last_update_ms = 0;
    bool          _has_sample     = false;
};
