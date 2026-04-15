#pragma once
#include "config.h"

// =============================================================================
// TI4 Hex Riser - Runtime Settings
// =============================================================================
// Shadows config.h #defines with a live-mutable struct so the web settings
// page can change values at runtime.  Include this BEFORE led_control.h,
// keyboard_control.h, and network.h.
//
// All code that previously read a #define constant should instead read
// the matching rtCfg field.  The struct is initialized from the #defines
// in config.h, so the defaults always come from there.
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
  uint16_t ledUpdateMs;
  uint16_t broadcastMs;
  uint8_t  sideGap;             // SVG side line inset (browser-only, served via /getsettings)

  // Debug / simulation
  bool     simulateHardware;
  bool     debugSerial;
  bool     debugWeb;
  bool     debugLed;
  bool     debugKeyboard;
};

// One global instance — initialized from config.h defaults at boot.
// network.h savesettings route writes into this struct; changes take
// effect immediately (some, like WiFi credentials, require a reboot).
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
