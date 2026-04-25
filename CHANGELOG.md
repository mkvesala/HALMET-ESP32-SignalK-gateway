# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.1.0] - 2026-04-23

WiFi AP interface hardened with three-layer security and intrusion detection.
`espnow_protocol.h` extended with GNSS payload structs for the shared fleet protocol.

### Added
- `WiFi.softAP()` called immediately after `WiFi.mode(WIFI_AP_STA)` — secures the AP
  interface before any client can connect (hidden SSID, WPA2 password, max 1 connection)
- `WiFi.onEvent(ARDUINO_EVENT_WIFI_AP_STACONNECTED)` registered in `begin()` before
  `WiFi.begin()` — callback calls `esp_wifi_deauth_sta()` immediately in the FreeRTOS
  `arduino_events` task and sets `volatile bool _ap_intruder` flag
- `handleAPIntruder()` in `VEDApplication` — processes the intruder flag in `loop()`
  context; logs MAC address to Serial and shows `AP: INTRUDER!` on LCD
- `volatile bool _ap_intruder` and `uint8_t _ap_intruder_mac[6]` private members in
  `VEDApplication` for thread-safe event-to-loop handoff (MAC copied before flag is set)
- `#include <esp_wifi.h>` in `VEDApplication.h` for `esp_wifi_deauth_sta()`
- `AP_SSID` and `AP_PASS` added to `secrets.example.h`
- `ESPNow::GnssDelta` payload struct in `espnow_protocol.h` — GNSS position, speed,
  course (lat/lon °, SOG m/s, COG true rad, magnetic variation rad, SIV, fix type, fix OK)
- `ESPNow::GnssData` internal struct in `espnow_protocol.h` — integer×10 representation
  (COG 0–3599, SOG knots×10, fix_ok) with `getCogDeg()`, `getSogKnots()`, `hasFix()` helpers
- `ESPNow::convertGnssDeltaToData()` inline converter from wire float format to internal
  integer format
- `ESPNowMsgType::GNSS_DELTA = 4` added to the shared protocol enum

### Changed
- `WIFI_TIMEOUT_MS` increased from 90 001 ms (90 s) to 179 999 ms (3 min) — allows more
  time for the STA interface to associate before falling back to ESP-NOW-only mode

[v1.1.0]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.0.0...v1.1.0

## [v1.0.0] - 2026-04-10

First versioned release. Complete rewrite from a single-file sketch into a
class-based architecture following the ESP32 Gateway Design Pattern.

### Added
- `VEDSensor` — encapsulates UART2 / HardwareSerial(2) reading into a dedicated
  class with a FreeRTOS task pinned to Core 0 and thread-safe `portMUX_TYPE`
  spinlock cache. Provides `getSnapshot()` for atomic reads from the main loop
- `VEDSensor::getReaderStackWatermark()` — returns the FreeRTOS reader task stack
  high water mark in bytes (ESP-IDF FreeRTOS returns bytes, not words)
- `VEDSensor`: task handle saved from `xTaskCreatePinnedToCore()` to support
  stack watermark queries
- `VEDProcessor` — converts raw VE.Direct integer values to SI units
  (V, A, W, SoC as 0–1 ratio) and populates `ESPNow::BatteryDelta` for brokers
- `SignalKBroker` — WebSocket connection to SignalK server; all five values
  (house voltage, current, power, SoC, start voltage) are sent in a single
  JSON delta every 1 s. MAC-based unique `$source` name via `esp_efuse_mac_get_default()`
- `ESPNowBroker` — ESP-NOW broadcast of `ESPNowPacket<BatteryDelta>` every 1 s
  using the shared `espnow_protocol.h` packet format
- `DisplayManager` — LCD 16x2 I2C management; scans for address 0x27 or 0x3F
  using two stack-allocated `LiquidCrystal_I2C` members and a raw pointer selector
- `DisplayManager::showDiagData()` — LCD screen showing free heap (bytes) and
  task stack high water marks for both the main loop task and the reader task
- `VEDApplication` — orchestrator that owns all subsystems as stack-allocated
  members, wires dependencies via constructor initializer list, runs a
  `WifiState` state machine and timed loop handlers with a three-way LCD display
  rotation: WiFi/WS net status, diagnostics, and battery data on a 30-tick cycle
- `VEDPreferences` — skeleton class for future NVS configuration storage
- `WebUIManager` — skeleton class for future HTTP configuration UI
- `WifiState.h` — `enum class WifiState` (INIT / CONNECTING / CONNECTED /
  FAILED / DISCONNECTED / OFF) with `wifiStateToString()` helper
- `version.h` — `SW_VERSION` compile-time constant
- `secrets.h` / `secrets.example.h` — converted from `#define` macros to
  `inline constexpr` form

### Changed
- WiFi mode changed from `WIFI_STA` to `WIFI_AP_STA` (required for concurrent
  ESP-NOW and WiFi operation)
- SignalK send strategy changed from a 5-state round-robin (one value per
  second, 5 s full cycle) to a single delta with all five values every 1 s
- WebSocket reconnection uses exponential backoff (2 s → 120 s max) instead
  of a fixed 2 s retry interval
- WiFi connection timeout is now non-blocking; the main loop keeps running
  with ESP-NOW active while WiFi is still connecting
- `VEDSensor`: `portMUX_TYPE _mux` renamed to `_spinlock` — the variable is a
  spinlock (busy-wait, disables interrupts), not a mutex (blocking, scheduler-aware)
- `DisplayManager`: `SignalKBroker` dependency removed; constructor signature
  changed from `DisplayManager(VEDProcessor&, SignalKBroker&)` to
  `DisplayManager(VEDProcessor&)` — net status display is handled entirely by
  `VEDApplication`
- All `String` usage replaced with `snprintf` + `char[]` throughout
- All magic numbers promoted to `static constexpr` with `UPPER_SNAKE_CASE` names
- Private class members named `_snake_case`, static file-scope variables `s_snake_case`
- `ESP32_gateway_pattern.md`: coding style rules documented; all code-block
  comments translated to English

### Fixed
- Dead `SignalKBroker &_signalk` member in `DisplayManager` removed — it was
  stored in the constructor initializer list but never accessed by any method

### Removed
- `std::unique_ptr` / `std::make_unique` removed from LCD initialisation
- I2C bus recovery (SCL clock-stretching loop) removed — was a debug workaround
- Global variable spaghetti replaced by encapsulated class state
- `String`-based SK URL, hostname and RSSI label construction removed
- Single-file `.ino` monolith replaced by 11 separate `.h`/`.cpp` modules

[v1.0.0]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/releases/tag/v1.0.0
