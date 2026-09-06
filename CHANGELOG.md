# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2026-09-06

### Added

- `WaterSensor` — reads the fresh water tank resistive sender (nominal 0–180 Ω) via the ADS1115 ADC (I2C address 0x4B, channel 1 = HALMET analog input A2) in constant-current-source mode. The CCS jumper must be fitted for A2. It holds its own `Adafruit_ADS1115` instance rather than sharing `VDOSensor`'s: the driver keeps no channel state (the mux is encoded in every conversion's config write) and both sensors are read sequentially from the main loop, so the two instances cannot interleave. If the ADC is ever read from a FreeRTOS task or ISR, both must be replaced by one shared, mutex-guarded bus object.
- `WaterProcessor` — converts sender resistance to a fresh water level ratio through the `WaterCal::OHMS` calibration table, since the tank is irregularly shaped and a two-point linear map (as used for fuel) would be wrong. The table stores measured resistance at each 2.5 L step with the volume implicit in the row index, is validated at compile time by a `static_assert` requiring strictly increasing values at least `MIN_STEP_OHMS` (2 Ω ≈ 11 ADC LSB) apart, and interpolates piecewise-linearly with clamping — never extrapolation — outside the calibrated range. Filtering reuses the three-phase raw → median → EMA pipeline but is tuned much faster than `VDOProcessor` (median-60, α=0.04 giving τ ≈ 50 s, ~2.5 min settle vs ~20 min): water draw is bursty, and a shower can take 15 % of the tank in minutes. See `docs/water_level_calibration.md`.
- `SignalKBroker::sendWaterDelta()` — sends `tanks.freshWater.0.currentLevel` [ratio] on its own `SK_WATER_TX_MS` (~4 s) timer. `sendTankCapacity()` now emits both `tanks.fuel.0.capacity` and `tanks.freshWater.0.capacity` (0.08 m³) as two entries in a single delta, so the existing `_capacity_sent` flag continues to govern both with no new state or reset sites; its scratch document and buffer grew from 256 to 384 bytes to fit the two-path message.
- `ESPNowBroker::sendWaterDelta()` — broadcasts the new `HALMETWaterDelta` packet alongside the engine and tank deltas on the existing `ESPNOW_TX_MS` (~3 s) timer.
- `espnow_protocol.h` — `ESPNowMsgType::HALMET_WATER_DELTA = 7` and the `HALMETWaterDelta` payload struct. A new type and struct rather than a new field on `HALMETTankDelta`, which would have changed that struct's size and corrupted decoding on every already-deployed receiver. Existing receivers ignore type 7 and need no reflash.
- `HALMETApplication::handleWaterRead()` — reads the sender on `WATER_READ_MS` (2011 ms, offset from `VDO_READ_MS` = 2003 so the two ADC reads drift apart rather than phase-locking) and feeds `WaterProcessor`. Contains a commented-out `Serial.printf` that is the fallback instrument for the calibration procedure when WiFi is unavailable.
- `WebUIManager` — implemented as a read-only calibration/debug page, replacing the empty skeleton. Three routes, no authentication and no settings: `GET /` serves a static HTML+JS page from flash in one `send_P`, `GET /status` returns live JSON polled once a second (1009 ms), and `GET /cal` returns the `WaterCal::OHMS` table, fetched once per page load so the static data stays out of the per-second payload. The page shows both tanks side by side — resistance, filtered resistance, millivolts, ADC counts and computed fill ratio — plus uptime, free heap, SignalK connection state and the calibration table with the currently occupied bracket marked. It exists to replace the serial monitor during tank calibration, where a laptop on a USB cable is impractical and a phone is not.
- `HALMETApplication::WEB_UI_ENABLED` — compile-time feature flag, matching the `UBLOX-ESP32-SignalK-gateway` idiom exactly (a `static constexpr bool` in the application header, checked in `initWifiServices()` and `handleWebUI()`). Being `constexpr`, setting it `false` optimizes the entire web UI away; the `WebServer` member is still constructed, which is the same limitation UBLOX carries and not worth introducing the project's first preprocessor conditional to avoid. It ships as `false`: the page has no authentication, so it is enabled only for the duration of a calibration run and turned off again before the firmware goes back on the boat.
- `WaterProcessor` / `VDOProcessor` calibration accessors — `getLastOhms()`, `getFilteredOhms()`, `getSampleCount()`, `getWindowSize()` and `getLastUpdateMs()`, backed by `_last_ohms` / `_last_update_ms` members. Without them the resistance existed only as a stack local in `handleWaterRead()`: the sensors cache nothing, and the processors' `_samples`/`_ema` are private with no accessors, so nothing outside could observe the value the whole calibration procedure is about. `getFilteredOhms()` returns the median→EMA value (NaN until the window fills) — this is the number to write into the table, and it replaces the eyeballed "midpoint of the oscillation".
- `docs/water_level_calibration.md` — the full measurement procedure: why the table has a single column with the fill ratio implicit in the row index, how to read the settled `filt` value off the calibration page, how to set `MAX_OHMS`, what the compile-time table assertion means and the three acceptable ways to resolve it, plus a serial-monitor fallback for when WiFi is unavailable.

### Changed

- `HALMETPreferences` and `WebUIManager` constructors take a `WaterProcessor &`. `HALMETPreferences` remains a skeleton — no `load()`/`save()` body.
- `HALMETApplication::sensorOk()` deliberately still gates on `_ads_ok` alone. `WaterSensor::begin()` probes the same I2C address on the same chip as `VDOSensor::begin()`, so `_water_ok` carries no information the existing check lacks; folding it in would add a way to halt the board in `setup()` — killing exhaust temperature, fuel, SignalK and ESP-NOW — over a redundant probe. `_water_ok` still exists so the read handler skips cleanly instead of hammering a dead driver every 2 s.
- `WaterSensor::CCS_CURRENT_A`, `ADS_LSB_V` and `MAX_OHMS`, and the same trio on `VDOSensor` (with `MIN_VOLTAGE_V` in place of `MAX_OHMS`), are `public` rather than `private`. They are `static constexpr` constants rather than state, so `WebUIManager` can reconstruct volts and ADC counts from ohms and display the fault threshold without holding a reference to the hardware layer — which would have let the HTTP handler trigger I2C conversions and broken the sensor/processor/broker separation.
- `VDOSensor.h` — corrected a stale class comment that still claimed the ADS1115 sits at 0x48; the code has used 0x4B (ADDR tied to SCL, as on HALMET) since bring-up.
- `espnow_protocol.h` — replaced with the fleet-wide superset agreed on 2026-07-28, after the per-project copies had drifted into four incompatible versions. Beyond `HALMET_WATER_DELTA`, this brings in `DEPTH_DELTA = 8` / `DepthDelta` and `DATETIME_DELTA = 9` / `DateTimeDelta` (sent by other projects, never by HALMET), the receiver-side conversion helpers, the split of `fix_ok` from `cog_valid` in `GNSSDelta`, and a header comment recording which project owns which `ESPNowMsgType` value. HALMET compiles the unused structs and inline functions but emits nothing for them. The file is copied by hand between projects: a change is not finished until every copy is identical.

### Notes

- `WaterSensor::readResistance()` intentionally does **not** mirror `VDOSensor`'s `MIN_VOLTAGE_V` low-side reject. The VDO sender's empty point is 3 Ω, but the water sender reads 0 Ω when empty, so rejecting low readings would discard the bottom ~2.6 % of the tank — exactly the about-to-run-dry region — leaving SignalK reporting a stale non-zero level indefinitely. The fault case for a constant-current source is an *open* circuit, which drives the input to the rail, so the guard is a high-side `MAX_OHMS` instead. A shorted sender reads ~0 Ω and is indistinguishable from empty; reporting empty is the safe direction to fail.
- The calibration table holds the measured curve for Frida's 80 L tank in 2.5 L rows; see `docs/water_level_calibration.md` for the measurement and for what the two plateaus at 146.1 Ω and 174.8 Ω mean.
- The calibration page's millivolt and ADC-count columns are **reconstructed** from ohms (`raw = ohms / 0.1875`), not resampled. The sensor's raw-to-ohms map is linear and lossless so the value round-trips exactly, but it bypasses the negative-voltage clamp in `readResistance()`. Surfacing the genuine sampled integer would require caching it in the sensor and handing `WebUIManager` sensor references — the layering cost outweighs the diagnostic value.
- Reading `filt` instead of the raw oscillation costs waiting time, and the procedure says so: it is an EMA with τ ≈ 50 s, so 60 s after a pour some 30 % of the step is still unresolved. The documented wait per step is ~2.5 min, making a full calibration run about half an hour. Recording too early biases every row the same way, which the table's monotonicity `static_assert` cannot catch — the curve stays plausible and is permanently wrong.
- Sender faults are visible on the calibration page as a growing `age_ms` rather than an error flag. A failed `readResistance()` never reaches `updateLevel()`, so an open circuit freezes the last reading; the page flags anything older than 6 s (≈3× the 2011 ms read interval) as stale. `ok` reflects only whether the ADS1115 initialized, since both sensors probe the same chip at the same address.

## [1.2.0] - 2026-07-16

### Fixed

- **Permanent WebSocket reconnect failure** - the gateway could lose its SignalK WebSocket connection after ~12–48 h of continuous operation and never recover until a manual reboot, even though WiFi stayed connected, heap and stack were healthy, the main loop kept running, and stale detection plus exponential backoff fired correctly; `SignalKBroker` reused a single lifetime `WebsocketsClient` (and thus the same underlying `WiFiClient` / lwIP socket) across every reconnect, so once that socket entered a stuck state every subsequent `connect()` reused the corrupted transport and failed forever. `connectWebsocket()` now constructs a brand-new `WebsocketsClient` on every attempt (registering callbacks before `connect()`) and destroys it immediately on a failed connect, guaranteeing each reconnect starts from a clean TCP / WebSocket state; the lwIP socket fd is released by the client destructor.

### Changed

- **`SignalKBroker` owns the WebSocket client via `std::unique_ptr`** - the client is created per connect and destroyed on teardown rather than kept as a permanent value member; `closeWebsocket()` now destroys the object (previously it only called `close()` and left it for reuse), all `_ws` operations (`poll()`, `ping()`, `pong()`, `send()`) are pointer-guarded, and the send-failure paths in `sendDoc()` and `sendTankCapacity()` route through `closeWebsocket()` so client destruction happens in exactly one place. The reconnect, exponential backoff, ping/pong liveness and SignalK protocol handling in `HALMETApplication::handleWebsocket()` are unchanged.

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
