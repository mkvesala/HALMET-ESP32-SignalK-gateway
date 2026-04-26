#pragma once

enum class WifiState {
    INIT,
    CONNECTING,
    CONNECTED,
    FAILED,
    DISCONNECTED,
    OFF
};

inline const char* wifiStateToString(WifiState s) {
    switch (s) {
        case WifiState::INIT:         return "INIT";
        case WifiState::CONNECTING:   return "CONNECTING";
        case WifiState::CONNECTED:    return "CONNECTED";
        case WifiState::FAILED:       return "FAILED";
        case WifiState::DISCONNECTED: return "DISCONNECTED";
        case WifiState::OFF:          return "OFF";
        default:                      return "UNKNOWN";
    }
}
