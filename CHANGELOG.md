# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-04-26

### Added

- `DS18B20Sensor` — 1-Wire temperature sensor driver (GPIO4). Blocking 750 ms conversion runs in a dedicated FreeRTOS task so the main loop is never stalled.
- `DS18B20Processor` — converts raw °C readings to Kelvin; thread-safe access via `portMUX_TYPE` spinlock between the FreeRTOS task and the main loop.
- `VDOSensor` — reads VDO European resistive fuel sender resistance via ADS1115 ADC (I2C address 0x4B, channel 0) in constant-current-source mode (1 mA CCS; HALMET hardware measured at 1 mA despite 10 mA in Hat Labs documentation).
- `VDOProcessor` — three-phase filtering pipeline: raw value while the median window fills (0–4 min), then EMA initialized to the first median (no warm-up jump), then normal median-120 → EMA α=0.005 operation. See `docs/fuel_level_filtering.md` for design rationale.
- `SignalKBroker` — WebSocket connection to SignalK server. Sends `propulsion.0.exhaustTemperature` [K] and `tanks.fuel.0.currentLevel` [ratio] with deadband filtering and 60 s keepalive; sends static `tanks.fuel.0.capacity` (0.4 m³) once on WebSocket connect.
- `ESPNowBroker` — ESP-NOW broadcast of `HALMETEngineDelta` (exhaust temperature) and `HALMETTankDelta` (fuel level ratio) packets.
- `HALMETPreferences` — NVS skeleton; `load()` and `save()` are empty placeholders for future use.
- `WebUIManager` — HTTP server skeleton; no routes registered yet.
- `HALMETApplication` — main orchestrator owning both sensor/processor pairs and all brokers as stack-allocated members. Manages WiFi state machine, exponential WebSocket reconnect backoff, OTA updates, and AP intruder deauth.
- `espnow_protocol.h` extended with `HALMET_ENGINE_DELTA` (5) and `HALMET_TANK_DELTA` (6) message types and their payload structs.

### Fixed

- `HALMETApplication::initWifiServices()` — added `wifi_services_initialized` guard to prevent `ArduinoOTA.begin()` and `WebUIManager::begin()` from being called on every WiFi reconnect, which caused UDP socket and memory leaks.
- `VDOSensor::begin()` — removed duplicate `Wire.begin()` call (already called by `HALMETApplication`); added one dummy `readADC_SingleEnded()` to prime the ADS1115 before the first real measurement.
- `VDOSensor` — corrected ADS1115 I2C address from `0x48` to `0x4B` (HALMET board wires the ADDR pin to SCL).
- `VDOSensor` — corrected `CCS_CURRENT_A` from `0.010` to `0.001` (1 mA, confirmed by measurement against a known 100 Ω resistor).
- `VDOSensor` — added minimum voltage threshold (`MIN_VOLTAGE_V = 0.005 V`) to discard reads taken before the CCS stabilises; returns `false` instead of storing a spurious near-zero resistance.
- `SignalKBroker` — deadband `static` local variables replaced with member variables so state is not lost across reconnects.
- `SignalKBroker` — added 60 s keepalive (`SK_KEEPALIVE_MS`) to force periodic sends even when the value is constant, preventing SignalK from treating the path as stale.
- `SignalKBroker::connectWebsocket()` — moved WebSocket open/fail logging and `sendTankCapacity()` call from `onEventCallback` into `connectWebsocket()` directly, because the `ConnectionOpened` event fires during `_ws.connect()` before the callback is registered.
