#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "DS18B20Processor.h"
#include "VDOProcessor.h"

// === H A L M E T P R E F E R E N C E S  C L A S S  ( S K E L E T O N ) ===
//
// NVS persistence placeholder — load() and save() are not yet implemented.

class HALMETPreferences {
public:
    explicit HALMETPreferences(DS18B20Processor &ds18b20_proc, VDOProcessor &vdo_proc);

    void load();  // placeholder
    void save();  // placeholder

private:
    DS18B20Processor &_ds18b20_proc;
    VDOProcessor     &_vdo_proc;
    Preferences       _prefs;
};
