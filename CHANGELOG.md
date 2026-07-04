# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-07-04

### Added

- `SignalKBroker::ping()` / `isStale()` — active WebSocket ping/pong liveness. `ping()` sends a client-initiated ping frame; the `GotPong` event refreshes `_last_pong_ms`. `HALMETApplication::handleWebsocket()` pings every `WS_PING_MS` (~10 s) while the socket is open.
- Half-open TCP detection — `isStale()` reports a connection where `isOpen()` still returns `true` but no pong has arrived within `PONG_TIMEOUT_MS` (~30 s). `handleWebsocket()` then calls `closeWebsocket()` and lets the existing exponential backoff reconnect, recovering from a silently-dead SignalK server (e.g. macOS host power-saving) without dropping WiFi or ESP-NOW.

### Changed

- `SignalKBroker::connectWebsocket()` / `closeWebsocket()` / `onEventCallback()` — seed `_last_pong_ms` on connect and on `ConnectionOpened`, reset it to 0 on close, so liveness tracking starts clean on every (re)connection.
- Liveness recovery is transport-only — a silently-dead connection is healed by a graceful WebSocket reconnect, never an `ESP.restart()`; WiFi, ESP-NOW, and uptime are preserved.

## [1.0.0] - 2026-07-04

### Added

- `DS18B20Sensor` — 1-Wire temperature sensor driver (GPIO4). Blocking 750 ms conversion runs in a dedicated FreeRTOS task so the main loop is never stalled.
- `DS18B20Processor` — converts raw °C readings to Kelvin; thread-safe access via `portMUX_TYPE` spinlock between the FreeRTOS task and the main loop.
- `VDOSensor` — reads VDO European resistive fuel sender resistance via ADS1115 ADC (I2C address 0x4B, channel 0) in constant-current-source mode (1 mA CCS; HALMET hardware measured at 1 mA despite 10 mA in Hat Labs documentation).
- `VDOProcessor` — three-phase filtering pipeline: raw value while the median window fills (0–4 min), then EMA initialized to the first median (no warm-up jump), then normal median-120 → EMA α=0.005 operation. See `docs/fuel_level_filtering.md` for design rationale.
- `SignalKBroker` — WebSocket connection to SignalK server. Sends `propulsion.0.exhaustTemperature` [K] and `tanks.fuel.0.currentLevel` [ratio] on independent timers; sends static `tanks.fuel.0.capacity` (0.4 m³) once on the first `handleStatus()` poll cycle after WebSocket connect.
- `ESPNowBroker` — ESP-NOW broadcast of `HALMETEngineDelta` (exhaust temperature) and `HALMETTankDelta` (fuel level ratio) packets on a ~3 s timer.
- `HALMETPreferences` — NVS skeleton; `load()` and `save()` are empty placeholders for future use.
- `WebUIManager` — HTTP server skeleton; no routes registered yet.
- `HALMETApplication` — main orchestrator owning both sensor/processor pairs and all brokers as stack-allocated members. Manages WiFi state machine, exponential WebSocket reconnect backoff, hardened reconnect with static IP, OTA updates, and AP intruder deauth.
- `HALMETApplication::applyStaticIP()` — configures a fixed IP address (`WIFI_STATIC_IP` / `WIFI_GATEWAY` / `WIFI_SUBNET` in `secrets.h`) via `WiFi.config()`; applied at boot and reapplied after every STA teardown, since the static configuration does not survive `WiFi.disconnect(true)`.
- `espnow_protocol.h` extended with `HALMET_ENGINE_DELTA` (5) and `HALMET_TANK_DELTA` (6) message types and their payload structs.

### Changed

- `SignalKBroker` and `ESPNowBroker` — removed deadband filtering and keepalive timers; both now send unconditionally on their own fixed-interval timers (SignalK: ~1 s engine / ~3 s tank, ESP-NOW: ~3 s). Simplifies the send path and guarantees SignalK/ESP-NOW peers never see a stale path, at the cost of slightly higher constant traffic.
- `VDOProcessor::EMA_ALPHA` — tuned from 0.010 to 0.005 (currentLevel reaches ~99 % of a step change in ~30 min instead of ~15 min); the slower filter tracks the wave-induced noise profile better in practice.

### Fixed

- `HALMETApplication::initWifiServices()` — added `wifi_services_initialized` guard to prevent `ArduinoOTA.begin()` and `WebUIManager::begin()` from being called on every WiFi reconnect, which caused UDP socket and memory leaks.
- `HALMETApplication::handleWifi()` — hardened the `CONNECTED → CONNECTING` reconnect path: closes the SignalK WebSocket, performs a full STA teardown (`WiFi.disconnect(true)` + 200 ms settle + `WiFi.setSleep(false)` + `applyStaticIP()`) before calling `WiFi.begin()` again, replacing a bare `disconnect()`/`begin()` cycle that could leave the radio in a stuck state and the WebSocket stale. Also resets `_next_ws_try_ms` on `CONNECTING → CONNECTED` so a stale backoff timestamp can no longer delay the WebSocket reconnect.
- `VDOSensor::begin()` — removed duplicate `Wire.begin()` call (already called by `HALMETApplication`); added one dummy `readADC_SingleEnded()` to prime the ADS1115 before the first real measurement.
- `VDOSensor` — corrected ADS1115 I2C address from `0x48` to `0x4B` (HALMET board wires the ADDR pin to SCL).
- `VDOSensor` — corrected `CCS_CURRENT_A` from `0.010` to `0.001` (1 mA, confirmed by measurement against a known 100 Ω resistor).
- `VDOSensor` — added minimum voltage threshold (`MIN_VOLTAGE_V = 0.005 V`) to discard reads taken before the CCS stabilises; returns `false` instead of storing a spurious near-zero resistance.
- `VDOProcessor` — corrected the sender resistance mapping for the Wema/VDO European sender actually fitted: `VDO_OHMS_EMPTY` = 3 Ω and `VDO_OHMS_FULL` = 180 Ω (the initial assumption — 10 Ω = full / 180 Ω = empty — was the opposite of how this sender behaves; resistance rises with fill level).
- `SignalKBroker` — deadband `static` local variables replaced with member variables so state is not lost across reconnects (later removed entirely, see Changed).
- `SignalKBroker::connectWebsocket()` — moved `sendTankCapacity()` from `connectWebsocket()` to the first `handleStatus()` poll cycle via `_capacity_sent` flag, because sending immediately after `connect()` (before the server hello is received) caused the server to close the connection on every boot.
