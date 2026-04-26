#include <Arduino.h>
#include "HALMETApplication.h"

HALMETApplication app;

void setup() {
    Serial.begin(115200);
    delay(47);

    app.begin();

    if (!app.sensorOk()) {
        Serial.println("[HALT] ADS1115 not found! Check HALMET I2C wiring.");
        while (1) delay(1999);
    }
}

void loop() {
    app.loop();
}
