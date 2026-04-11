#pragma once

// =============================================================================
// TI4 Hex Riser - Configuration
// =============================================================================
// Edit this file to change pin assignments, WiFi credentials, and debug flags.
// All hardware-specific settings live here so nothing else needs editing.

// -----------------------------------------------------------------------------
// Hardware
// -----------------------------------------------------------------------------
#define LED_PIN        6        // FastLED data pin (GPIO 6)
#define NUM_LEDS       915      // 61 hexes × 15 LEDs each
#define NUM_HEXES      61
#define LEDS_PER_HEX   15

#define I2C_SDA        20       // MCP23017 I2C SDA
#define I2C_SCL        21       // MCP23017 I2C SCL

// MCP23017 I2C addresses (0x20–0x23, set via A0/A1/A2 pins on board)
#define MCP_ADDR_1     0x20    // Keyboards 1–2  ← STUB: wire when boards arrive
#define MCP_ADDR_2     0x21    // Keyboards 3–4
#define MCP_ADDR_3     0x22    // Keyboards 5–6
#define MCP_ADDR_4     0x23    // Keyboards 7–8
#define NUM_MCP        4

// -----------------------------------------------------------------------------
// LED Defaults
// -----------------------------------------------------------------------------
#define DEFAULT_BRIGHTNESS  128    // 50% — ~27.5 A draw; safe for 60 A PSU
#define MAX_BRIGHTNESS      200    // Hard cap; raise carefully
#define LED_COLOR_ORDER     GRB    // SK6812 RGB channel order
#define LED_UPDATE_MS       16     // ~60 FPS
#define SIDE_GAP            4      // Side line inset (px); higher = more gap between hex colors

// -----------------------------------------------------------------------------
// WiFi / Network
// -----------------------------------------------------------------------------
// Station mode (home network) — tried first.
// Leave WIFI_HOME_SSID empty ("") to skip and go straight to AP mode.
#define WIFI_HOME_SSID      ""               // your home network SSID (leave blank to use AP mode only)
#define WIFI_HOME_PASSWORD  ""               // your home network password
#define WIFI_HOME_TIMEOUT_MS 10000   // how long to wait before giving up

// Fallback Access Point — used when home network is unavailable or skipped.
#define WIFI_AP_SSID        "TI4-HexRiser"
#define WIFI_AP_PASSWORD    "twilight4"
#define WIFI_AP_IP          "192.168.3.1"

#define HTTP_PORT       80
#define WS_PORT         81
#define BROADCAST_MS    100    // push full LED state to browser (ms)

// -----------------------------------------------------------------------------
// Debug / Test Flags
// -----------------------------------------------------------------------------
// Set to true to enable a mode; only one should be active at a time.

#define SIMULATE_HARDWARE   false   // true → skip FastLED.show(); use Serial cmds
#define DEBUG_LED_TEST      false   // true → rainbow wave across all hexes on boot
#define DEBUG_KEYBOARD_TEST false   // true → print key presses to Serial
#define DEBUG_WEB_TEST      false   // true → log all WebSocket messages to Serial
#define DEBUG_SERIAL        true    // true → general Serial.print() output

// -----------------------------------------------------------------------------
// Game
// -----------------------------------------------------------------------------
#define MAX_PLAYERS     8

// Player color defaults (CRGB hex — edit freely)
#define PLAYER_COLOR_1  0xFF0000   // Red
#define PLAYER_COLOR_2  0x0000FF   // Blue
#define PLAYER_COLOR_3  0x00FF00   // Green
#define PLAYER_COLOR_4  0xFFFF00   // Yellow
#define PLAYER_COLOR_5  0xFF00FF   // Purple
#define PLAYER_COLOR_6  0xFF8000   // Orange
#define PLAYER_COLOR_7  0x00FFFF   // Cyan
#define PLAYER_COLOR_8  0xFFFFFF   // White

enum GamePhase {
  PHASE_SETUP    = 0,
  PHASE_STRATEGY = 1,
  PHASE_ACTION   = 2,
  PHASE_STATUS   = 3,
  PHASE_AGENDA   = 4
};

struct Player {
  uint8_t  id;           // 1–8
  bool     active;
  uint8_t  initiative;   // 1–8
  uint32_t colorHex;     // packed RGB (e.g. 0xFF0000)
  bool     hasPassed;
};
