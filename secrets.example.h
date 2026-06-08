#pragma once

// rename --> secrets.h
// add secrets.h to your .gitignore file

inline constexpr const char*    WIFI_SSID            = "your_wifi_ssid_here";
inline constexpr const char*    WIFI_PASS            = "your_wifi_password_here";
inline constexpr const char*    WIFI_STATIC_IP       = "192.168.1.50"; // pick an address outside your router's DHCP pool
inline constexpr const char*    WIFI_GATEWAY         = "192.168.1.1";
inline constexpr const char*    WIFI_SUBNET          = "255.255.255.0";
inline constexpr const char*    SK_HOST              = "your_signalk_host_here";
inline constexpr uint16_t       SK_PORT              = 3000; // or whatever your port is
inline constexpr const char*    SK_TOKEN             = "your_signalk_token_here"; 
inline constexpr const char*    OTA_PASS             = "your_ota_password_here";
inline constexpr const char*    DEFAULT_WEB_PASSWORD = "your_default_web_password_here";
inline constexpr const char*    AP_SSID              = "your_ap_ssid_here";   // hidden, name not critical
inline constexpr const char*    AP_PASS              = "your_ap_password_here"; // min 8 chars (WPA2)