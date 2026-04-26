# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-04-26

### Added

- `DS18B20Sensor` — 1-Wire temperature sensor driver (GPIO4). Blocking 750 ms conversion runs in a dedicated FreeRTOS task so the main loop is never stalled.
- `DS18B20Processor` — converts raw °C readings to Kelvin; thread-safe access via `portMUX_TYPE` spinlock between the FreeRTOS task and the main loop.
- `VDOSensor` — reads VDO European resistive fuel sender resistance via ADS1115 ADC (I2C address 0x48, channel 0) in constant-current-source mode (10 mA CCS jumper).
- `VDOProcessor` — three-phase filtering pipeline: raw value while the median window fills (0–4 min), then EMA initialized to the first median (no warm-up jump), then normal median-120 → EMA α=0.005 operation. See `docs/fuel_level_filtering.md` for design rationale.
- `SignalKBroker` — WebSocket connection to SignalK server. Sends `propulsion.0.exhaustTemperature` [K] and `tanks.fuel.0.currentLevel` [ratio] with deadband filtering; sends static `tanks.fuel.0.capacity` (0.4 m³) once on WebSocket connect.
- `ESPNowBroker` — ESP-NOW broadcast of `HALMETEngineDelta` (exhaust temperature) and `HALMETTankDelta` (fuel level ratio) packets.
- `HALMETPreferences` — NVS skeleton; `load()` and `save()` are empty placeholders for future use.
- `WebUIManager` — HTTP server skeleton; no routes registered yet.
- `HALMETApplication` — main orchestrator owning both sensor/processor pairs and all brokers as stack-allocated members. Manages WiFi state machine, exponential WebSocket reconnect backoff, OTA updates, and AP intruder deauth.
- `espnow_protocol.h` extended with `HALMET_ENGINE_DELTA` (5) and `HALMET_TANK_DELTA` (6) message types and their payload structs.
