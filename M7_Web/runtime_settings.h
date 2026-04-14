#pragma once
#include "config.h"

// =============================================================================
// TI4 Hex Riser - M7 Runtime Settings
// =============================================================================
// Mirrors legacy runtime_settings.h so the settings page still works fully.
// Fields marked (M4 only) are kept for settings page round-trip but are
// not acted on by M7 — M4 acts on them when settings are saved.
// =============================================================================

struct RuntimeConfig {
  // WiFi
  char     homeSSID[64];
  char     homePass[64];
  char     apSSID[32];
  char     apPass[32];
  uint32_t homeTimeoutMs;

  // LED
  uint8_t  defaultBrightness;
  uint8_t  maxBrightness;
  uint16_t ledUpdateMs;       // M4 only — kept for settings page
  uint16_t broadcastMs;
  uint8_t  sideGap;

  // Debug / simulation
  bool     simulateHardware;  // M4 only
  bool     debugSerial;
  bool     debugWeb;
  bool     debugLed;          // M4 only
  bool     debugKeyboard;     // M4 only
};

RuntimeConfig rtCfg = {
  WIFI_HOME_SSID,
  WIFI_HOME_PASSWORD,
  WIFI_AP_SSID,
  WIFI_AP_PASSWORD,
  WIFI_HOME_TIMEOUT_MS,
  DEFAULT_BRIGHTNESS,
  MAX_BRIGHTNESS,
  LED_UPDATE_MS,
  BROADCAST_MS,
  SIDE_GAP,
  SIMULATE_HARDWARE,
  DEBUG_SERIAL,
  DEBUG_WEB_TEST,
  DEBUG_LED_TEST,
  DEBUG_KEYBOARD_TEST
};
