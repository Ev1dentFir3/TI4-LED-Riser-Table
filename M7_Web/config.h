#pragma once

// =============================================================================
// TI4 Hex Riser - M7 Configuration
// =============================================================================
// Network and shared constants only.
// LED hardware, I2C, and game animation constants live in M4's config.h.
// =============================================================================

// -----------------------------------------------------------------------------
// WiFi / Network
// -----------------------------------------------------------------------------
#define WIFI_HOME_SSID       "The Network"
#define WIFI_HOME_PASSWORD   "letmeinplease"
#define WIFI_HOME_TIMEOUT_MS  10000

#define WIFI_AP_SSID         "TI4-HexRiser"
#define WIFI_AP_PASSWORD     "twilight4"
#define WIFI_AP_IP           "192.168.3.1"

#define HTTP_PORT            80
#define WS_PORT              81
#define BROADCAST_MS         50     // ms between LED state pushes to browser

// -----------------------------------------------------------------------------
// LED / Display defaults (used by settings page)
// -----------------------------------------------------------------------------
#define DEFAULT_BRIGHTNESS   128
#define MAX_BRIGHTNESS       200
#define LED_UPDATE_MS        16
#define SIDE_GAP             4

// -----------------------------------------------------------------------------
// Game
// -----------------------------------------------------------------------------
#define MAX_PLAYERS          8

// -----------------------------------------------------------------------------
// Debug flags
// -----------------------------------------------------------------------------
#define SIMULATE_HARDWARE    false
#define DEBUG_SERIAL         true
#define DEBUG_WEB_TEST       true
#define DEBUG_LED_TEST       false
#define DEBUG_KEYBOARD_TEST  false
