// =============================================================================
// TI4 Hex Riser - Main Firmware
// Target: Arduino Giga R1 WiFi
//
// Required libraries (Arduino IDE → Library Manager):
//   1. FastLED                  (by Daniel Garcia)
//   2. WebSockets               (by Markus Sattler)
//   3. Adafruit MCP23X17        (by Adafruit)
//
// Board: Arduino Giga R1 WiFi
// 
// =============================================================================

#include "config.h"
#include "runtime_settings.h"  // must come before led_control/keyboard/network

// Include order matters: led_map & hex_neighbors provide data, led_control
// reads it, web_interface provides the HTML string, network.h uses all of them.
#include "led_map.h"
#include "hex_neighbors.h"
#include "led_control.h"
#include "keyboard_control.h"
#include "web_interface.h"
#include "network.h"

// =============================================================================
// Game state
// =============================================================================
static Player     players[MAX_PLAYERS];
static GamePhase  gamePhase    = PHASE_SETUP;
static uint8_t    activePlayer = 0;   // index 0–7

// =============================================================================
// Hex selection callback (called from network.h when web client selects a hex)
// =============================================================================
void onHexSelected(int hex) {
  if (hex < 0 || hex >= NUM_HEXES) return;
  if (rtCfg.debugSerial) {
    Serial.print(F("Game: hex selected "));
    Serial.println(hex);
  }

  // Brief white flash on the selected hex, then restore owner color
  CRGB prev = hexColor[hex];
  setHexColor(hex, CRGB::White);
  pushLEDs();
  delay(80);
  setHexColor(hex, prev);
  pushLEDs();

  // Assign selected hex to active player
  hexOwner[hex] = activePlayer;
  uint32_t c = players[activePlayer].colorHex;
  CRGB playerCRGB = CRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  setHexColor(hex, playerCRGB);
  broadcastHexUpdate(hex, playerCRGB.r, playerCRGB.g, playerCRGB.b);
}

// =============================================================================
// Key press callback (called from keyboard_control.h — stub for now)
// =============================================================================
void onKeyPressed(uint8_t player, uint8_t key) {
  if (rtCfg.debugSerial) {
    Serial.print(F("Key: player="));
    Serial.print(player);
    Serial.print(F(" key="));
    Serial.println(key);
  }
  // Key 15 = confirm, Key 14 = pass, Key 0–7 = color shortcuts (placeholder)
  switch (key) {
    case 15: // confirm / advance
      break;
    case 14: // pass this round
      if (player < MAX_PLAYERS) {
        players[player].hasPassed = true;
        broadcastStatus(gamePhase, activePlayer);
      }
      break;
    default:
      break;
  }
}

// =============================================================================
// Serial command handler (for testing without keyboards)
// Commands:
//   hex N       — select hex N
//   player N    — set active player (0–7)
//   color RRGGBB — set active player color
//   effect NAME — start effect (rainbow/pulse/spiral/sparkle/wave/none)
//   bright N    — set brightness 0–200
//   clear       — clear all hexes
//   test        — run LED test
//   status      — print game state to Serial
// =============================================================================
void handleSerialCommand() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (rtCfg.debugSerial) {
    Serial.print(F("CMD: "));
    Serial.println(line);
  }

  if (line.startsWith("hex ")) {
    int h = line.substring(4).toInt();
    onHexSelected(h);

  } else if (line.startsWith("player ")) {
    int p = line.substring(7).toInt();
    if (p >= 0 && p < MAX_PLAYERS) {
      activePlayer = (uint8_t)p;
      Serial.print(F("Active player: "));
      Serial.println(activePlayer);
    }

  } else if (line.startsWith("color ")) {
    uint32_t rgb = strtoul(line.substring(6).c_str(), nullptr, 16);
    players[activePlayer].colorHex = rgb;
    Serial.print(F("Player "));
    Serial.print(activePlayer);
    Serial.print(F(" color: #"));
    Serial.println(rgb, HEX);

  } else if (line.startsWith("effect ")) {
    String name = line.substring(7);
    name.toUpperCase();
    if      (name == "RAINBOW") startEffect(ANIM_RAINBOW);
    else if (name == "PULSE")   startEffect(ANIM_PULSE);
    else if (name == "SPIRAL")  startEffect(ANIM_SPIRAL);
    else if (name == "SPARKLE") startEffect(ANIM_SPARKLE);
    else if (name == "WAVE")    startEffect(ANIM_WAVE);
    else if (name == "NONE")    stopEffect();
    else Serial.println(F("Unknown effect. Try: rainbow pulse spiral sparkle wave none"));

  } else if (line.startsWith("bright ")) {
    int b = line.substring(7).toInt();
    setBrightness((uint8_t)constrain(b, 0, rtCfg.maxBrightness));
    Serial.print(F("Brightness: "));
    Serial.println(b);

  } else if (line == "clear") {
    setAllHexes(CRGB::Black);
    pushLEDs();
    broadcastAll(0, 0, 0);

  } else if (line == "test") {
    runLEDTest();

  } else if (line == "status") {
    Serial.println(F("--- Game Status ---"));
    Serial.print(F("Phase: ")); Serial.println((int)gamePhase);
    Serial.print(F("Active player: ")); Serial.println(activePlayer);
    Serial.print(F("WiFi IP: ")); Serial.println(WiFi.localIP());
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (players[i].active) {
        Serial.print(F("  P")); Serial.print(i);
        Serial.print(F(" color=#")); Serial.println(players[i].colorHex, HEX);
      }
    }

  } else {
    Serial.println(F("Commands: hex N | player N | color RRGGBB | effect NAME | bright N | clear | test | status"));
  }
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 2000) {}  // wait up to 2 s for Serial Monitor

  if (rtCfg.debugSerial) {
    Serial.println();
    Serial.println(F("=============================="));
    Serial.println(F(" TI4 Hex Riser v1.0"));
    Serial.println(F("=============================="));
  }

  // Initialize players with default colors
  uint32_t defaultColors[MAX_PLAYERS] = {
    PLAYER_COLOR_1, PLAYER_COLOR_2, PLAYER_COLOR_3, PLAYER_COLOR_4,
    PLAYER_COLOR_5, PLAYER_COLOR_6, PLAYER_COLOR_7, PLAYER_COLOR_8
  };
  for (int i = 0; i < MAX_PLAYERS; i++) {
    players[i] = { (uint8_t)(i + 1), true, (uint8_t)(i + 1), defaultColors[i], false };
  }

  // Hardware init
  initLEDs();
  initKeyboard();
  setKeyPressCallback(onKeyPressed);
  initNetwork();

  if (rtCfg.debugSerial) {
    Serial.println(F("Ready. Type 'status' for info, 'test' to run LED test."));
    Serial.println(F("Web: connect to WiFi, then check Serial for IP address."));
  }

  // Boot animation
  startEffect(ANIM_SPIRAL);
  uint32_t animStart = millis();
  while (millis() - animStart < 2000) {
    updateLEDs();
  }
  stopEffect();
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
  handleSerialCommand();
  handleKeyboard();
  handleNetwork();
  updateLEDs();

  // Heartbeat LED so you know the board is alive
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 1000) {
    lastBeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
