#include "HALMETPreferences.h"

// === P U B L I C ===

// Constructor
HALMETPreferences::HALMETPreferences(DS18B20Processor &ds18b20_proc,
                                     VDOProcessor     &vdo_proc,
                                     WaterProcessor   &water_proc)
    : _ds18b20_proc(ds18b20_proc)
    , _vdo_proc(vdo_proc)
    , _water_proc(water_proc)
{}

// Placeholder — no settings persisted yet
void HALMETPreferences::load() {}

// Placeholder — no settings persisted yet
void HALMETPreferences::save() {}
