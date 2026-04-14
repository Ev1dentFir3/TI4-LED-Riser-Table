// =============================================================================
// TI4 Hex Riser - M4 Core Firmware
// Target: Arduino Giga R1 WiFi — M4 Co-processor
//
// Required libraries (Arduino IDE → Library Manager):
//   1. FastLED           (by Daniel Garcia)
//   2. Adafruit MCP23X17 (by Adafruit)
//
// Flash split: 1MB M7 + 1MB M4
// Target core: M4 Co-processor
//
// This core owns: LEDs, game state, keyboard scanning.
// All network/WiFi lives on M7. Communication is via sharedState in SRAM4.
// Debug output goes through RPC so M7 can bridge it to USB Serial.
// =============================================================================

#include <RPC.h>
#include "shared_state.h"
#include "config.h"
#include "runtime_settings.h"
#include "led_map.h"
#include "hex_neighbors.h"
#include "led_control.h"
#include "flash_state.h"
#include "keyboard_control.h"
#include "animations.h"
#include "game_state.h"
#include "cmd_handler.h"

// =============================================================================
// Keyboard press callback — wired to both physical keyboards and serial sim
// =============================================================================
void onKeyPressed(uint8_t playerIndex, uint8_t key) {
  handleGameKey(playerIndex, key);
}

// =============================================================================
// Hex selection callback — from M7 via CMD_SELECT_HEX in CmdQueue
// Non-blocking: starts a 80ms white flash, returns immediately.
// =============================================================================
void onHexSelected(int hexIdx) {
  if (hexIdx < 0 || hexIdx >= NUM_HEXES) return;
  if (rtCfg.debugSerial) {
    RPC.print("[M4] Hex selected: ");
    RPC.println(hexIdx);
  }
  startHexFlash(hexIdx, hexColor[hexIdx]);
}

// =============================================================================
// Serial command handler (M4 Serial — bridged via RPC from M7)
// Commands: status, effect, bright, clear, test, kb, setplayers, startgame,
//           phase, battle
// =============================================================================
void handleSerialCommand() {
  if (!RPC.available()) return;

  // Read line from RPC stream
  String line = "";
  while (RPC.available()) {
    char c = (char)RPC.read();
    if (c == '\n') break;
    if (c != '\r') line += c;
  }
  line.trim();
  if (line.length() == 0) return;

  if (rtCfg.debugSerial) {
    RPC.print("[M4] CMD: ");
    RPC.println(line);
  }

  if (line.startsWith("kb ")) {
    int spacePos = line.indexOf(' ', 3);
    if (spacePos < 0) { RPC.println("Usage: kb <1-8> <0-15>"); return; }
    int playerNum = line.substring(3, spacePos).toInt();
    int keyNum    = line.substring(spacePos + 1).toInt();
    if (playerNum < 1 || playerNum > 8) { RPC.println("Player must be 1-8"); return; }
    if (keyNum < 0 || keyNum > 15)      { RPC.println("Key must be 0-15"); return; }
    uint8_t playerIndex = (uint8_t)(playerNum - 1);
    if (!players[playerIndex].active) { RPC.println("Player not active"); return; }
    onKeyPressed(playerIndex, (uint8_t)keyNum);

  } else if (line.startsWith("setplayers ")) {
    int count = line.substring(11).toInt();
    if (count < 4 || count > 8) { RPC.println("Player count must be 4-8"); return; }
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) players[i].active = (i < (uint8_t)count);
    transitionToSetup();

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
    selectRandomSpeaker();
    transitionToStrategy();

  } else if (line.startsWith("phase ")) {
    int phaseNum = line.substring(6).toInt();
    switch (phaseNum) {
      case 0: transitionToSetup();    break;
      case 1: transitionToStrategy(); break;
      case 2: transitionToAction();   break;
      case 3: transitionToStatus();   break;
      case 4: transitionToAgenda();   break;
      default: RPC.println("phase 0-4");
    }

  } else if (line.startsWith("battle ")) {
    int spacePos = line.indexOf(' ', 7);
    if (spacePos < 0) { RPC.println("Usage: battle <P1> <P2>"); return; }
    int p1 = line.substring(7, spacePos).toInt() - 1;
    int p2 = line.substring(spacePos + 1).toInt() - 1;
    if (p1 < 0 || p1 >= MAX_PLAYERS || p2 < 0 || p2 >= MAX_PLAYERS || p1 == p2) {
      RPC.println("Invalid player numbers"); return;
    }
    startBattle((uint8_t)p1, (uint8_t)p2);

  } else if (line.startsWith("effect ")) {
    String n = line.substring(7); n.toUpperCase();
    if      (n == "RAINBOW") startEffect(ANIM_RAINBOW);
    else if (n == "PULSE")   startEffect(ANIM_PULSE);
    else if (n == "SPIRAL")  startEffect(ANIM_SPIRAL);
    else if (n == "SPARKLE") startEffect(ANIM_SPARKLE);
    else if (n == "WAVE")    startEffect(ANIM_WAVE);
    else if (n == "NONE")    stopEffect();
    else RPC.println("Unknown effect");

  } else if (line.startsWith("bright ")) {
    int b = line.substring(7).toInt();
    setBrightness((uint8_t)constrain(b, 0, rtCfg.maxBrightness));

  } else if (line == "clear") {
    setAllHexes(CRGB::Black); pushLEDs();

  } else if (line == "test") {
    runLEDTest();

  } else if (line == "status") {
    const char* phaseNames[] = { "SETUP","STRATEGY","ACTION","STATUS","AGENDA" };
    RPC.print("[M4] Phase: "); RPC.println(phaseNames[(int)gameState.currentPhase]);
    RPC.print("[M4] Players: "); RPC.println(gameState.numActivePlayers);
    RPC.print("[M4] Speaker: P"); RPC.println(gameState.speakerIndex + 1);
    RPC.print("[M4] frameCount: "); RPC.println(sharedState.leds.frameCount);
  }
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
  RPC.begin();
  delay(500);  // give M7 time to boot and start bridging RPC

  RPC.println("[M4] ==============================");
  RPC.println("[M4]  TI4 Hex Riser v2.0 — M4 Core");
  RPC.println("[M4] ==============================");

  // Zero shared state — M4 owns it, M7 only reads
  memset(reinterpret_cast<void*>(SHARED_STATE_BASE), 0, sizeof(SharedState));
  sharedState.m4Ready = false;

  initLEDs();
  initKeyboard();
  setKeyPressCallback(onKeyPressed);

  initGameState(6);

  runBootAnimation();
  transitionToSetup();

  sharedState.m4Ready = true;
  RPC.println("[M4] Ready.");
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
  handleSerialCommand();
  handleKeyboard();
  drainCmdQueue();      // consume commands from M7 web UI
  updateGameState();
  updateLEDs();         // ticks flash state machine + writes LED snapshot to SRAM

  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    RPC.print("[M4] alive — frame=");
    RPC.println(sharedState.leds.frameCount);
  }
}
