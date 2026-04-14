#pragma once
#include "config.h"

// =============================================================================
// TI4 Hex Riser - M4 Runtime Settings
// =============================================================================
// WiFi fields removed — M4 never touches the network.
// M7 has its own runtime_settings.h with network fields only.
// =============================================================================

struct RuntimeConfig {
  // LED
  uint8_t  defaultBrightness;
  uint8_t  maxBrightness;
  uint16_t ledUpdateMs;
  uint16_t broadcastMs;   // kept for struct compat; unused on M4
  uint8_t  sideGap;       // browser-only, served by M7; unused on M4

  // Debug / simulation
  bool     simulateHardware;
  bool     debugSerial;
  bool     debugWeb;
  bool     debugLed;
  bool     debugKeyboard;
};

RuntimeConfig rtCfg = {
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
