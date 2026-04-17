// =============================================================================
// TI4 Hex Riser - Main Firmware
// Target: ESP32-S3-WROOM-1-N16R8
//
// Required libraries (Arduino IDE -> Library Manager):
//   1. FastLED              (by Daniel Garcia)
//   2. Adafruit MCP23X17    (by Adafruit)
//   3. ESPAsyncWebServer    (GitHub: me-no-dev/ESPAsyncWebServer)
//   4. AsyncTCP             (GitHub: me-no-dev/AsyncTCP)
//
// Board: ESP32S3 Dev Module
// CPU: 240 MHz (WiFi), Flash: 16MB QIO 80MHz, Partition: 16M Flash (3MB APP/9.9MB FATFS)
// PSRAM: OPI PSRAM, USB CDC On Boot: Enabled, USB Mode: Hardware CDC and JTAG
//
// Core assignment:
//   Core 0 — LED task (FastLED.show via I2S, ~60 fps) — isolated from WiFi jitter
//   Core 1 — loop(): game state, keyboard, serial commands; AsyncWebServer runs here
// =============================================================================

#define BOARD_HAS_PSRAM
#define CONFIG_SPIRAM_CACHE_WORKAROUND

#include "config.h"
#include "runtime_settings.h"

#include "led_map.h"
#include "hex_neighbors.h"
// ESP32-S3 requires I2S peripheral for FastLED — must be defined before the include
#define FASTLED_USES_ESP32S3_I2S
#include "led_control.h"
#include "keyboard_control.h"
#include "animations.h"
#include "game_state.h"
#include "web_interface.h"
#include "web_server.h"  // renamed from network.h — avoids collision with ESP32 core Network.h

// =============================================================================
// LED Task — runs on Core 0, ~60 fps
// initLEDs() is called in setup() before this task is created.
// =============================================================================
void ledTask(void *parameter) {
  for (;;) {
    updateLEDs();
    vTaskDelay(1);  // yield; millis gate in updateLEDs() controls actual rate
  }
}

// =============================================================================
// Keyboard press callback
// =============================================================================
void onKeyPressed(uint8_t playerIndex, uint8_t key) {
  handleGameKey(playerIndex, key);
}

// =============================================================================
// Hex selection callback (web UI click)
// Called from AsyncWebServer handler — no blocking delay or pushLEDs().
// LED task picks up the color change within one frame (~16ms).
// =============================================================================
void onHexSelected(int hexIdx) {
  if (hexIdx < 0 || hexIdx >= NUM_HEXES) return;
  if (rtCfg.debugSerial) {
    Serial.print(F("Web: hex selected "));
    Serial.println(hexIdx);
  }

  // Brief white flash — LED task renders it; no explicit pushLEDs() needed.
  CRGB prev = hexColor[hexIdx];
  setHexColor(hexIdx, CRGB::White);
  delay(80);
  setHexColor(hexIdx, prev);
}

// =============================================================================
// Serial command handler
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

  // --- kb <player 1-8> <key 0-15> ---
  if (line.startsWith("kb ")) {
    int spacePos = line.indexOf(' ', 3);
    if (spacePos < 0) {
      Serial.println(F("Usage: kb <player 1-8> <key 0-15>"));
      return;
    }
    int playerNum = line.substring(3, spacePos).toInt();
    int keyNum    = line.substring(spacePos + 1).toInt();
    if (playerNum < 1 || playerNum > 8) { Serial.println(F("Player must be 1-8")); return; }
    if (keyNum < 0 || keyNum > 15)      { Serial.println(F("Key must be 0-15"));   return; }
    uint8_t playerIndex = (uint8_t)(playerNum - 1);
    if (!players[playerIndex].active) {
      Serial.print(F("Player ")); Serial.print(playerNum);
      Serial.println(F(" is not active — use 'setplayers N' first"));
      return;
    }
    Serial.print(F("Simulating P")); Serial.print(playerNum);
    Serial.print(F(" key ")); Serial.println(keyNum);
    onKeyPressed(playerIndex, (uint8_t)keyNum);

  // --- setplayers <4-8> ---
  } else if (line.startsWith("setplayers ")) {
    int count = line.substring(11).toInt();
    if (count < 4 || count > 8) { Serial.println(F("Player count must be 4-8")); return; }
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) players[i].active = (i < (uint8_t)count);
    Serial.print(F("Set ")); Serial.print(count); Serial.println(F(" active players"));
    transitionToSetup();

  // --- startgame ---
  } else if (line == "startgame") {
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
      if (players[i].active && !players[i].colorLocked) {
        uint8_t colorIdx = players[i].selectedColorIndex;
        if (!gameState.colorTaken[colorIdx]) {
          players[i].colorLocked         = true;
          gameState.colorTaken[colorIdx] = true;
        }
      }
    }
    Serial.println(F("Starting game — speaker roulette running..."));
    selectRandomSpeaker();
    transitionToStrategy();

  // --- phase <0-4> ---
  } else if (line.startsWith("phase ")) {
    int phaseNum = line.substring(6).toInt();
    if (phaseNum < 0 || phaseNum > 4) {
      Serial.println(F("Usage: phase <0-4>  (0=Setup 1=Strategy 2=Action 3=Status 4=Agenda)"));
      return;
    }
    switch (phaseNum) {
      case 0: transitionToSetup();    break;
      case 1: transitionToStrategy(); break;
      case 2: transitionToAction();   break;
      case 3: transitionToStatus();   break;
      case 4: transitionToAgenda();   break;
    }
    Serial.print(F("Jumped to phase ")); Serial.println(phaseNum);

  // --- battle <P1> <P2> ---
  } else if (line.startsWith("battle ")) {
    int spacePos = line.indexOf(' ', 7);
    if (spacePos < 0) { Serial.println(F("Usage: battle <player1 1-8> <player2 1-8>")); return; }
    int p1 = line.substring(7, spacePos).toInt() - 1;
    int p2 = line.substring(spacePos + 1).toInt() - 1;
    if (p1 < 0 || p1 >= MAX_PLAYERS || p2 < 0 || p2 >= MAX_PLAYERS || p1 == p2) {
      Serial.println(F("Invalid player numbers")); return;
    }
    startBattle((uint8_t)p1, (uint8_t)p2);

  // --- effect NAME ---
  } else if (line.startsWith("effect ")) {
    String effectName = line.substring(7);
    effectName.toUpperCase();
    if      (effectName == "RAINBOW") startEffect(ANIM_RAINBOW);
    else if (effectName == "PULSE")   startEffect(ANIM_PULSE);
    else if (effectName == "SPIRAL")  startEffect(ANIM_SPIRAL);
    else if (effectName == "RIPPLE")  startEffect(ANIM_RIPPLE);
    else if (effectName == "SPARKLE") startEffect(ANIM_SPARKLE);
    else if (effectName == "WAVE")    startEffect(ANIM_WAVE);
    else if (effectName == "NONE")    stopEffect();
    else Serial.println(F("Unknown effect. Try: rainbow pulse spiral ripple sparkle wave none"));

  // --- bright N ---
  } else if (line.startsWith("bright ")) {
    int brightness = line.substring(7).toInt();
    setBrightness((uint8_t)constrain(brightness, 0, rtCfg.maxBrightness));
    Serial.print(F("Brightness: ")); Serial.println(brightness);

  // --- clear ---
  } else if (line == "clear") {
    setAllHexes(CRGB::Black);

  // --- test ---
  } else if (line == "test") {
    runLEDTest();

  // --- status ---
  } else if (line == "status") {
    const char* phaseNames[] = { "SETUP", "STRATEGY", "ACTION", "STATUS", "AGENDA" };
    Serial.println(F("--- Game Status ---"));
    Serial.print(F("Phase: ")); Serial.println(phaseNames[(int)gameState.currentPhase]);
    Serial.print(F("Active players: ")); Serial.println(gameState.numActivePlayers);
    Serial.print(F("Speaker: P")); Serial.println(gameState.speakerIndex + 1);
    Serial.print(F("WiFi IP: ")); Serial.println(WiFi.localIP());
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
      if (players[i].active) {
        Serial.print(F("  P")); Serial.print(i + 1);
        Serial.print(F(" color=#")); Serial.print(players[i].colorHex, HEX);
        Serial.print(F(" home=")); Serial.print(players[i].homeHex);
        Serial.print(F(" locked=")); Serial.print(players[i].colorLocked ? "Y" : "N");
        if (players[i].strategyCard > 0) { Serial.print(F(" card=")); Serial.print(players[i].strategyCard); }
        if (players[i].hasPassed) Serial.print(F(" PASSED"));
        Serial.println();
      }
    }

  } else {
    Serial.println(F("Commands:"));
    Serial.println(F("  kb <1-8> <0-15>     simulate key press"));
    Serial.println(F("  setplayers <4-8>    set active player count"));
    Serial.println(F("  startgame           GM starts the game"));
    Serial.println(F("  phase <0-4>         force jump to phase"));
    Serial.println(F("  battle <P1> <P2>    trigger battle mode"));
    Serial.println(F("  effect NAME         LED effect"));
    Serial.println(F("  bright N            set brightness"));
    Serial.println(F("  clear / test / status"));
  }
}

// =============================================================================
// setup() — runs on Core 1
// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(500);
  delay(500);  // let serial settle (ESP32 USB CDC initialises faster than Giga)

  if (rtCfg.debugSerial) {
    Serial.println();
    Serial.println(F("=============================="));
    Serial.println(F(" TI4 Hex Riser v2.0 - ESP32-S3"));
    Serial.println(F("=============================="));
  }

  // Hardware + network init
  initLEDs();
  initKeyboard();
  setKeyPressCallback(onKeyPressed);
  initNetwork();

  // Game state — default 6 players; override with 'setplayers N'
  initGameState(6);

  // Boot animation runs on Core 1 before LED task is created
  runBootAnimation();
  transitionToSetup();

  // Spawn LED task on Core 0 — takes over FastLED.show() from here on
  xTaskCreatePinnedToCore(
    ledTask,      // task function
    "LED_Task",   // name (debug)
    4096,         // stack size (bytes)
    NULL,         // parameter
    2,            // priority (higher = more preemptive)
    NULL,         // task handle
    0             // core 0
  );

  if (rtCfg.debugSerial) {
    Serial.println(F("Ready. LED task on Core 0. Type 'status' for game state."));
  }
}

// =============================================================================
// loop() — runs on Core 1
// updateLEDs() removed — LED task on Core 0 handles it.
// =============================================================================
void loop() {
  handleSerialCommand();
  handleKeyboard();
  handleNetwork();   // no-op; kept for animDelay() compatibility
  updateGameState();

  // Heartbeat
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    if (rtCfg.debugSerial) Serial.println(F("[ESP32] alive"));
  }
}
