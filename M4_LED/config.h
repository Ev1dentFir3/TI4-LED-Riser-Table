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
#define WIFI_HOME_SSID      "The Network"               // your home network SSID (leave blank to use AP mode only)
#define WIFI_HOME_PASSWORD  "letmeinplease"               // your home network password
#define WIFI_HOME_TIMEOUT_MS 10000   // how long to wait before giving up

// Fallback Access Point — used when home network is unavailable or skipped.
#define WIFI_AP_SSID        "TI4-HexRiser"
#define WIFI_AP_PASSWORD    "twilight4"
#define WIFI_AP_IP          "192.168.3.1"

#define HTTP_PORT       80
#define WS_PORT         81
#define BROADCAST_MS    50    // push full LED state to browser (ms)

// -----------------------------------------------------------------------------
// Debug / Test Flags
// -----------------------------------------------------------------------------
// Set to true to enable a mode; only one should be active at a time.

#define SIMULATE_HARDWARE   true    // true → skip FastLED.show(); use Serial cmds
#define DEBUG_LED_TEST      false   // true → rainbow wave across all hexes on boot
#define DEBUG_KEYBOARD_TEST false   // true → print key presses to Serial
#define DEBUG_WEB_TEST      false   // true → log all WebSocket messages to Serial
#define DEBUG_SERIAL        true    // true → general Serial.print() output

// -----------------------------------------------------------------------------
// Game
// -----------------------------------------------------------------------------
#define MAX_PLAYERS     8

// Timing constants
#define TURN_WARNING_MS      300000   // 5 min before red pulse warning
#define BOOT_ANIM_SPEED_MS   50       // ms between hex advances in boot snake
#define BOOT_ANIM_TAIL           5    // number of hexes lit at once in boot snake
#define SPEAKER_ROULETTE_LAPS     10   // full loops around players before landing on winner
#define SPEAKER_ROULETTE_STEP_MS 500  // ms per step in the roulette animation
#define JOIN_FADE_PERIOD_MS   1000    // unlocked color fade cycle (ms)
#define JOIN_FADE_MIN        51       // min brightness when fading (20%)
#define JOIN_FADE_MAX        255      // max brightness when fading (100%)

// Animation constants
#define EDGE_PULSE_SPREAD    3        // neighbor depth for active-player edge pulse
#define PASSED_DIM_PERCENT   50       // home hex brightness % when player passes

// Pulse ranges for each phase (used with beatsin8 at 60 BPM)
#define STRATEGY_PULSE_MIN   128      // strategy picker: 50% to 100%
#define STRATEGY_PULSE_MAX   255
#define STATUS_PULSE_MIN     51       // status phase: 20% to 50%
#define STATUS_PULSE_MAX     128
#define AGENDA_PULSE_MIN     51       // agenda phase: 20% to 50%
#define AGENDA_PULSE_MAX     128

// Home hex assignments per player count (position order matches keyboard order)
static const uint8_t HOME_HEXES_4P[4] = { 12, 44, 48, 16 };
static const uint8_t HOME_HEXES_5P[5] = {  9, 54, 51, 33,  6 };
static const uint8_t HOME_HEXES_6P[6] = {  27, 54, 51, 33, 6, 9 };
static const uint8_t HOME_HEXES_7P[7] = {  26 ,55, 58, 50, 5, 2, 10 };
static const uint8_t HOME_HEXES_8P[8] = {  26 ,55, 58, 50, 34, 5, 2, 10 };

// Returns the home hex for the Nth active-player position given a player count
inline uint8_t getPlayerHomeHex(uint8_t positionIndex, uint8_t numPlayers) {
  switch (numPlayers) {
    case 4: return HOME_HEXES_4P[positionIndex];
    case 5: return HOME_HEXES_5P[positionIndex];
    case 6: return HOME_HEXES_6P[positionIndex];
    case 7: return HOME_HEXES_7P[positionIndex];
    case 8: return HOME_HEXES_8P[positionIndex];
    default: return 30;  // fallback: center hex
  }
}

// Color palette — players select from these 8 colors (keys 1–8)
static const uint32_t COLOR_PALETTE[8] = {
  0xFF0000,  // 1. Red
  0x0000FF,  // 2. Blue
  0x00FF00,  // 3. Green
  0xFFFF00,  // 4. Yellow
  0xFF00FF,  // 5. Purple/Magenta
  0xFF8000,  // 6. Orange
  0x00FFFF,  // 7. Cyan
  0xFFFFFF   // 8. White
};

// Strategy card colors (TI4 standard, cards 1–8)
static const uint32_t STRATEGY_COLORS[8] = {
  0x8B4513,  // 1. Leadership  — Brown
  0x2E8B57,  // 2. Diplomacy   — Green
  0x4169E1,  // 3. Politics    — Blue
  0x8B0000,  // 4. Construction — Dark Red
  0xFF8C00,  // 5. Trade       — Orange
  0x9370DB,  // 6. Warfare     — Purple
  0x20B2AA,  // 7. Technology  — Teal
  0xFFD700   // 8. Imperial    — Gold
};

enum GamePhase {
  PHASE_SETUP    = 0,
  PHASE_STRATEGY = 1,
  PHASE_ACTION   = 2,
  PHASE_STATUS   = 3,
  PHASE_AGENDA   = 4
};

struct Player {
  uint8_t  id;                   // 1–8 (fixed: Player 1 = Keyboard 1)
  bool     active;               // true if keyboard detected / simulated
  uint8_t  initiative;           // derived from strategy card (1–8)
  uint32_t colorHex;             // packed RGB, set from COLOR_PALETTE
  bool     hasPassed;            // true once player passes in action phase

  // --- New fields for game state ---
  uint8_t  selectedColorIndex;   // 0–7, index into COLOR_PALETTE
  bool     colorLocked;          // true once player confirms color in setup
  uint8_t  strategyCard;         // 1–8, 0 = not yet selected
  bool     strategyLocked;       // true once strategy card confirmed
  bool     readyForNext;         // true when player presses End Turn in status/agenda
  uint32_t turnStartMs;          // millis() when current action turn began
  uint8_t  homeHex;              // assigned home hex (0–60), set at game start
};
