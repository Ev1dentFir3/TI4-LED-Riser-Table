#pragma once
#include <math.h>
#include "config.h"
#include "led_control.h"
#include "hex_neighbors.h"

// =============================================================================
// TI4 Hex Riser - Game State Machine
// =============================================================================
// Reads top-to-bottom following the natural game flow:
//   Boot → Setup (join) → Strategy → Action → Status → Agenda → loop
//
// Key serial commands for testing (handled in TI4_HexRiser.ino):
//   kb <1-8> <0-15>   simulate a keyboard key press
//   setplayers <N>    set number of active players (4-8) for stub testing
//   phase <0-4>       force-jump to a phase
//   startgame         trigger GM start (skip waiting for all locks)
// =============================================================================

// =============================================================================
// SECTION 1: Player Array and Game State Data
// =============================================================================

Player    players[MAX_PLAYERS];

struct GameState {
  GamePhase currentPhase;
  uint8_t   speakerIndex;         // index into players[] (0-7) of current speaker
  uint8_t   numActivePlayers;     // how many keyboards are active this game

  // Color selection tracking (setup phase)
  bool      colorTaken[8];        // true if COLOR_PALETTE[i] is locked by a player

  // Strategy phase
  uint8_t   strategyPickOrder[8]; // player indices in pick order (speaker first)
  uint8_t   currentPickIndex;     // index into strategyPickOrder

  // Action phase
  uint8_t   actionOrder[8];       // player indices sorted by initiative
  uint8_t   actionOrderSize;      // number of players still in action order
  uint8_t   currentActionIndex;   // index into actionOrder

  // Battle mode
  bool      inBattle;
  uint8_t   battleAttacker;
  uint8_t   battleDefender;

  // Status phase
  uint8_t   hexSliceOwner[NUM_HEXES]; // which player index owns each hex in status
};

GameState gameState;

// =============================================================================
// SECTION 2: Grid Geometry Helpers
// =============================================================================
// Used to compute hex positions for pizza-slice assignment (status phase)
// and proximity-based board split (battle mode).

// Column layout — mirrors web_interface.h
static const uint8_t GRID_COL_COUNT[9]    = { 5, 6, 7, 8, 9, 8, 7, 6, 5 };
static const uint8_t GRID_COL_START[9]    = { 0, 5, 11, 18, 26, 35, 43, 50, 56 };
static const bool    GRID_COL_TOP_START[9] = { true, false, true, false, true, false, true, false, true };

// Returns the (x, y) position of hexIdx relative to the center hex (30).
// Uses normalized units where hexRadius = 1.
static void getHexPosition(uint8_t hexIdx, float &outX, float &outY) {
  uint8_t col = 0;
  for (uint8_t c = 0; c < 9; c++) {
    if (hexIdx >= GRID_COL_START[c] && hexIdx < GRID_COL_START[c] + GRID_COL_COUNT[c]) {
      col = c;
      break;
    }
  }
  uint8_t offset = hexIdx - GRID_COL_START[col];
  uint8_t rowFromTop = GRID_COL_TOP_START[col] ? offset : (GRID_COL_COUNT[col] - 1 - offset);

  const float hexH = 1.7320508f;  // sqrt(3), hex height when radius = 1
  outX = col * 1.5f;
  float colTopY = -(GRID_COL_COUNT[col] * hexH) / 2.0f + hexH / 2.0f;
  outY = colTopY + rowFromTop * hexH;

  // Subtract center hex (30) position: col=4, rowFromTop=4
  outX -= 4 * 1.5f;
  float centerColTopY = -(9 * hexH) / 2.0f + hexH / 2.0f;
  outY -= (centerColTopY + 4 * hexH);  // = 0, but written explicitly
}

// Returns angle (0–360°) from center hex to hexIdx. 0° = right, clockwise.
static float getAngleFromCenter(uint8_t hexIdx) {
  if (hexIdx == 30) return 0.0f;
  float x, y;
  getHexPosition(hexIdx, x, y);
  float angle = atan2f(y, x) * 180.0f / (float)M_PI;
  if (angle < 0) angle += 360.0f;
  return angle;
}

// BFS-based hex distance (uses HEX_NEIGHBORS table).
static uint8_t hexDistance(uint8_t from, uint8_t to) {
  if (from == to) return 0;
  uint8_t dist[NUM_HEXES];
  memset(dist, 0xFF, sizeof(dist));  // 0xFF = unvisited
  dist[from] = 0;
  uint8_t queue[NUM_HEXES];
  uint8_t head = 0, tail = 0;
  queue[tail++] = from;
  while (head < tail) {
    uint8_t current = queue[head++];
    if (current == to) return dist[to];
    for (int dir = 0; dir < 6; dir++) {
      int neighbor = HEX_NEIGHBORS[current][dir];
      if (neighbor >= 0 && dist[neighbor] == 0xFF) {
        dist[neighbor] = dist[current] + 1;
        queue[tail++] = (uint8_t)neighbor;
      }
    }
  }
  return dist[to];
}

// =============================================================================
// SECTION 3: Helper Utilities
// =============================================================================

// Returns the ordinal position (0, 1, 2, …) of playerIndex among active players.
static uint8_t getActivePositionOf(uint8_t playerIndex) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < playerIndex; i++) {
    if (players[i].active) count++;
  }
  return count;
}

// Returns the playerIndex of the Nth active player (0-based).
static uint8_t getActivePlayerByPosition(uint8_t position) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      if (count == position) return i;
      count++;
    }
  }
  return 0;
}

// Spreads a pulsed color outward from a hex to `depth` rings of neighbors.
// `visited` must be a zeroed NUM_HEXES-byte array on first call.
static void spreadPulseToNeighbors(uint8_t hexIdx, CRGB color, uint8_t depth, uint8_t visited[]) {
  if (depth == 0) return;
  for (int dir = 0; dir < 6; dir++) {
    int neighbor = HEX_NEIGHBORS[hexIdx][dir];
    if (neighbor < 0 || visited[neighbor]) continue;
    visited[neighbor] = 1;
    CRGB fadedColor = color;
    fadedColor.nscale8((uint8_t)((255UL * depth) / EDGE_PULSE_SPREAD));
    for (int side = 0; side < 6; side++) {
      setHexSideColor((uint8_t)neighbor, side, fadedColor);
    }
    spreadPulseToNeighbors((uint8_t)neighbor, color, depth - 1, visited);
  }
}

// =============================================================================
// SECTION 4: PHASE_SETUP — Player Join Mode
// =============================================================================

// Detects which keyboards are connected via I2C (stub: uses players[].active flags
// already set by the serial 'setplayers' command or defaults to 6 players).
static void detectConnectedKeyboards() {
  // Real hardware detection lives in keyboard_control.h (IMPLEMENT HERE marker).
  // In stub mode, active flags are set externally via serial 'setplayers N'.
  gameState.numActivePlayers = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) gameState.numActivePlayers++;
  }

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: "));
    Serial.print(gameState.numActivePlayers);
    Serial.println(F(" active players detected"));
  }
}

// Assigns home hexes to all active players based on how many are active.
static void assignHomeHexes() {
  if (gameState.numActivePlayers < 4) return;  // need at least 4 for a real layout
  uint8_t position = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      players[i].homeHex = getPlayerHomeHex(position, gameState.numActivePlayers);
      position++;
    }
  }
}

// Flashes a player's home hex red 3× to indicate a color is unavailable.
static void flashUnavailableColor(uint8_t playerIndex) {
  uint8_t homeHex = players[playerIndex].homeHex;
  for (int i = 0; i < 3; i++) {
    setHexColor(homeHex, CRGB::Red);
    pushLEDs();
    delay(100);
    setHexColor(homeHex, CRGB::Black);
    pushLEDs();
    delay(100);
  }
}

// Returns true if every active player has locked their color.
static bool allPlayersLocked() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].colorLocked) return false;
  }
  return true;
}

// Called every loop() during PHASE_SETUP to animate home hexes.
// Unlocked players: smooth breathing fade (JOIN_FADE_MIN → JOIN_FADE_MAX, 1s cycle).
// Locked players:   solid at full brightness.
static void updateJoinModeDisplay() {
  uint8_t fadeBrightness = beatsin8(60, JOIN_FADE_MIN, JOIN_FADE_MAX);

  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) {
      setHexColor(players[i].homeHex, CRGB::Black);
      continue;
    }
    CRGB playerColor;
    playerColor.r = (players[i].colorHex >> 16) & 0xFF;
    playerColor.g = (players[i].colorHex >>  8) & 0xFF;
    playerColor.b =  players[i].colorHex        & 0xFF;

    if (players[i].colorLocked) {
      setHexColor(players[i].homeHex, playerColor);
    } else {
      CRGB fadedColor = playerColor;
      fadedColor.nscale8(fadeBrightness);
      setHexColor(players[i].homeHex, fadedColor);
    }
  }
  pushLEDs();
}

// Player presses key 1–8 to preview a color during PHASE_SETUP.
static void handleColorSelection(uint8_t playerIndex, uint8_t colorKey) {
  if (!players[playerIndex].active || players[playerIndex].colorLocked) return;
  uint8_t colorIndex = colorKey - 1;  // keys 1-8 → indices 0-7
  if (colorIndex >= 8) return;

  if (gameState.colorTaken[colorIndex]) {
    flashUnavailableColor(playerIndex);
    return;
  }
  players[playerIndex].selectedColorIndex = colorIndex;
  players[playerIndex].colorHex = COLOR_PALETTE[colorIndex];
}

// Player presses Key 15 to lock in their color during PHASE_SETUP.
static void handleColorLockIn(uint8_t playerIndex) {
  if (!players[playerIndex].active || players[playerIndex].colorLocked) return;
  uint8_t colorIndex = players[playerIndex].selectedColorIndex;

  if (gameState.colorTaken[colorIndex]) {
    flashUnavailableColor(playerIndex);
    return;
  }
  players[playerIndex].colorLocked = true;
  gameState.colorTaken[colorIndex] = true;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" locked color"));
  }
}

// Randomly selects the speaker from active players with a roulette animation.
static void selectRandomSpeaker() {
  uint8_t activePlayers[MAX_PLAYERS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) activePlayers[count++] = i;
  }
  if (count == 0) return;

  // Roulette: spin through players, slowing down
  for (int spin = 0; spin < 20; spin++) {
    uint8_t shown = activePlayers[spin % count];
    for (uint8_t p = 0; p < count; p++) {
      setHexColor(players[activePlayers[p]].homeHex, CRGB::Black);
    }
    CRGB gold;
    gold.r = 0xFF; gold.g = 0xD7; gold.b = 0x00;
    setHexColor(players[shown].homeHex, gold);
    pushLEDs();
    delay(60 + spin * 12);  // slow down over time
  }

  // Final pick
  gameState.speakerIndex = activePlayers[random8(count)];

  // Flash winner 3×
  CRGB gold; gold.r = 0xFF; gold.g = 0xD7; gold.b = 0x00;
  for (int i = 0; i < 3; i++) {
    setHexColor(players[gameState.speakerIndex].homeHex, CRGB::Black);
    pushLEDs(); delay(200);
    setHexColor(players[gameState.speakerIndex].homeHex, gold);
    pushLEDs(); delay(200);
  }
  delay(800);

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Speaker is Player "));
    Serial.println(gameState.speakerIndex + 1);
  }

  // Restore all player home hex colors
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active) {
      CRGB color;
      color.r = (players[i].colorHex >> 16) & 0xFF;
      color.g = (players[i].colorHex >>  8) & 0xFF;
      color.b =  players[i].colorHex        & 0xFF;
      setHexColor(players[i].homeHex, color);
    }
  }
  pushLEDs();
}

// =============================================================================
// SECTION 5: PHASE_STRATEGY — Strategy Card Selection
// =============================================================================

// Builds pick order: speaker first, then in player-index order wrapping around.
static void buildStrategyPickOrder() {
  gameState.currentPickIndex = 0;
  uint8_t slot = 0;

  // Speaker goes first
  gameState.strategyPickOrder[slot++] = gameState.speakerIndex;

  // Remaining active players in ascending player-index order, wrapping
  for (uint8_t offset = 1; offset < gameState.numActivePlayers; offset++) {
    uint8_t candidatePosition = (getActivePositionOf(gameState.speakerIndex) + offset)
                                % gameState.numActivePlayers;
    gameState.strategyPickOrder[slot++] = getActivePlayerByPosition(candidatePosition);
  }
}

// Colors a player's home hex and all its neighbors with the given color.
static void colorPlayerArea(uint8_t playerIndex, CRGB color) {
  uint8_t homeHex = players[playerIndex].homeHex;
  setHexColor(homeHex, color);
  for (int dir = 0; dir < 6; dir++) {
    int neighbor = HEX_NEIGHBORS[homeHex][dir];
    if (neighbor >= 0) setHexColor((uint8_t)neighbor, color);
  }
  pushLEDs();
}

// Called every loop() during PHASE_STRATEGY.
// Current picker: home hex pulses white (50%→100%, 1s cycle).
// Already picked: home hex + neighbors in strategy card color.
// Waiting:        home hex at player color, full brightness.
static void updateStrategyPickerPulse() {
  uint8_t pulseBrightness = beatsin8(60, STRATEGY_PULSE_MIN, STRATEGY_PULSE_MAX);
  uint8_t currentPicker   = gameState.strategyPickOrder[gameState.currentPickIndex];

  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;

    if (i == currentPicker) {
      CRGB white = CRGB::White;
      white.nscale8(pulseBrightness);
      setHexColor(players[i].homeHex, white);
    } else if (players[i].strategyLocked) {
      uint8_t cardIdx = players[i].strategyCard - 1;
      CRGB stratColor;
      stratColor.r = (STRATEGY_COLORS[cardIdx] >> 16) & 0xFF;
      stratColor.g = (STRATEGY_COLORS[cardIdx] >>  8) & 0xFF;
      stratColor.b =  STRATEGY_COLORS[cardIdx]        & 0xFF;
      colorPlayerArea(i, stratColor);
    } else {
      CRGB playerColor;
      playerColor.r = (players[i].colorHex >> 16) & 0xFF;
      playerColor.g = (players[i].colorHex >>  8) & 0xFF;
      playerColor.b =  players[i].colorHex        & 0xFF;
      playerColor.nscale8(128);  // dim while waiting
      setHexColor(players[i].homeHex, playerColor);
    }
  }
  pushLEDs();
}

// Player presses key 1–8 to select a strategy card.
static void handleStrategyCardSelection(uint8_t playerIndex, uint8_t cardKey) {
  uint8_t currentPicker = gameState.strategyPickOrder[gameState.currentPickIndex];
  if (playerIndex != currentPicker) return;  // not your turn
  if (cardKey < 1 || cardKey > 8) return;
  players[playerIndex].strategyCard = cardKey;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.print(F(" selected strategy card "));
    Serial.println(cardKey);
  }
}

// Player presses Key 15 to lock strategy card and hand off to next picker.
static void handleStrategyLockIn(uint8_t playerIndex) {
  uint8_t currentPicker = gameState.strategyPickOrder[gameState.currentPickIndex];
  if (playerIndex != currentPicker) return;
  if (players[playerIndex].strategyCard == 0) return;  // must select first

  players[playerIndex].strategyLocked = true;
  players[playerIndex].initiative     = players[playerIndex].strategyCard;

  uint8_t cardIdx = players[playerIndex].strategyCard - 1;
  CRGB stratColor;
  stratColor.r = (STRATEGY_COLORS[cardIdx] >> 16) & 0xFF;
  stratColor.g = (STRATEGY_COLORS[cardIdx] >>  8) & 0xFF;
  stratColor.b =  STRATEGY_COLORS[cardIdx]        & 0xFF;
  colorPlayerArea(playerIndex, stratColor);

  gameState.currentPickIndex++;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" locked strategy card"));
  }
}

static bool checkStrategyComplete() {
  return (gameState.currentPickIndex >= gameState.numActivePlayers);
}

// =============================================================================
// SECTION 6: PHASE_ACTION — Main Turn Sequence
// =============================================================================

// Sorts active, non-passed players by initiative (strategy card) ascending.
static void buildActionOrder() {
  uint8_t sortedPlayers[MAX_PLAYERS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].hasPassed) sortedPlayers[count++] = i;
  }

  // Bubble sort by initiative (lower = higher priority)
  for (uint8_t i = 0; i < count - 1; i++) {
    for (uint8_t j = 0; j < count - i - 1; j++) {
      if (players[sortedPlayers[j]].initiative > players[sortedPlayers[j + 1]].initiative) {
        uint8_t temp       = sortedPlayers[j];
        sortedPlayers[j]   = sortedPlayers[j + 1];
        sortedPlayers[j+1] = temp;
      }
    }
  }

  gameState.actionOrderSize = count;
  for (uint8_t i = 0; i < count; i++) gameState.actionOrder[i] = sortedPlayers[i];
}

// Called every loop() during PHASE_ACTION.
// Active player: edge pulse (white → red after 5 min) spreading to neighbors.
// Passed players: home hex at 50% brightness.
// Others:         home hex at full player color.
static void updateActivePlayerPulse() {
  if (gameState.actionOrderSize == 0) return;

  uint8_t activePlayerIdx = gameState.actionOrder[gameState.currentActionIndex];
  uint32_t elapsed        = millis() - players[activePlayerIdx].turnStartMs;
  bool     overTimeWarning = (elapsed >= TURN_WARNING_MS);

  uint8_t pulseBrightness = beatsin8(60, 128, 255);
  CRGB pulseColor = overTimeWarning ? CRGB::Red : CRGB::White;
  pulseColor.nscale8(pulseBrightness);

  // Draw all player home hexes first
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    CRGB playerColor;
    playerColor.r = (players[i].colorHex >> 16) & 0xFF;
    playerColor.g = (players[i].colorHex >>  8) & 0xFF;
    playerColor.b =  players[i].colorHex        & 0xFF;

    if (players[i].hasPassed) {
      playerColor.nscale8((uint8_t)((255UL * PASSED_DIM_PERCENT) / 100));
    }
    setHexColor(players[i].homeHex, playerColor);
  }

  // Overlay pulse on active player home hex edges and spread to neighbors
  uint8_t homeHex = players[activePlayerIdx].homeHex;
  for (int side = 0; side < 6; side++) {
    setHexSideColor(homeHex, side, pulseColor);
  }
  uint8_t visited[NUM_HEXES] = {0};
  visited[homeHex] = 1;
  spreadPulseToNeighbors(homeHex, pulseColor, EDGE_PULSE_SPREAD, visited);

  pushLEDs();
}

// Player presses Key 14 to pass — removed from initiative order this round.
static void handlePlayerPass(uint8_t playerIndex) {
  if (!players[playerIndex].active || players[playerIndex].hasPassed) return;
  players[playerIndex].hasPassed = true;

  CRGB playerColor;
  playerColor.r = (players[playerIndex].colorHex >> 16) & 0xFF;
  playerColor.g = (players[playerIndex].colorHex >>  8) & 0xFF;
  playerColor.b =  players[playerIndex].colorHex        & 0xFF;
  playerColor.nscale8((uint8_t)((255UL * PASSED_DIM_PERCENT) / 100));
  setHexColor(players[playerIndex].homeHex, playerColor);
  pushLEDs();

  buildActionOrder();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" passed"));
  }

  // If this was the current active player, keep currentActionIndex valid
  if (gameState.actionOrderSize > 0) {
    if (gameState.currentActionIndex >= gameState.actionOrderSize) {
      gameState.currentActionIndex = 0;
    }
    players[gameState.actionOrder[gameState.currentActionIndex]].turnStartMs = millis();
  }
}

// Player presses Key 15 to end their turn and pass to the next.
static void handleEndTurn(uint8_t playerIndex) {
  if (gameState.actionOrderSize == 0) return;
  if (gameState.actionOrder[gameState.currentActionIndex] != playerIndex) return;

  gameState.currentActionIndex = (gameState.currentActionIndex + 1) % gameState.actionOrderSize;
  players[gameState.actionOrder[gameState.currentActionIndex]].turnStartMs = millis();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.print(F(" ended turn — now Player "));
    Serial.println(gameState.actionOrder[gameState.currentActionIndex] + 1);
  }
}

static bool checkActionComplete() {
  return (gameState.actionOrderSize == 0);
}

// Battle mode: splits the board by proximity to each combatant's home hex.
static void startBattle(uint8_t attackerIndex, uint8_t defenderIndex) {
  gameState.inBattle       = true;
  gameState.battleAttacker = attackerIndex;
  gameState.battleDefender = defenderIndex;

  uint8_t hex1 = players[attackerIndex].homeHex;
  uint8_t hex2 = players[defenderIndex].homeHex;
  CRGB color1, color2;
  color1.r = (players[attackerIndex].colorHex >> 16) & 0xFF;
  color1.g = (players[attackerIndex].colorHex >>  8) & 0xFF;
  color1.b =  players[attackerIndex].colorHex        & 0xFF;
  color2.r = (players[defenderIndex].colorHex >> 16) & 0xFF;
  color2.g = (players[defenderIndex].colorHex >>  8) & 0xFF;
  color2.b =  players[defenderIndex].colorHex        & 0xFF;

  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    setHexColor(h, (hexDistance(h, hex1) <= hexDistance(h, hex2)) ? color1 : color2);
  }
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Battle — P"));
    Serial.print(attackerIndex + 1);
    Serial.print(F(" vs P"));
    Serial.println(defenderIndex + 1);
  }
}

static void endBattle() {
  gameState.inBattle = false;
  if (rtCfg.debugSerial) Serial.println(F("Game: Battle ended"));
}

// =============================================================================
// SECTION 7: PHASE_STATUS — End-of-Round Cleanup
// =============================================================================

// Divides the 61 hexes into radial slices (one per active player) by angle
// from center hex 30. Assigns each hex to the nearest slice owner.
static void assignSlicesToPlayers() {
  if (gameState.numActivePlayers == 0) return;
  float sliceDegrees = 360.0f / gameState.numActivePlayers;

  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    float angle    = getAngleFromCenter(h);
    uint8_t sliceIdx = (uint8_t)(angle / sliceDegrees) % gameState.numActivePlayers;
    gameState.hexSliceOwner[h] = getActivePlayerByPosition(sliceIdx);
  }
}

// Called every loop() during PHASE_STATUS.
// Each player's slice pulses their color (20%→50%, 1s).
// When a player is ready: slice shows full brightness, solid.
static void updateStatusPulse() {
  uint8_t pulseBrightness = beatsin8(60, STATUS_PULSE_MIN, STATUS_PULSE_MAX);

  for (uint8_t h = 0; h < NUM_HEXES; h++) {
    uint8_t owner = gameState.hexSliceOwner[h];
    CRGB color;
    color.r = (players[owner].colorHex >> 16) & 0xFF;
    color.g = (players[owner].colorHex >>  8) & 0xFF;
    color.b =  players[owner].colorHex        & 0xFF;

    if (!players[owner].readyForNext) {
      color.nscale8(pulseBrightness);
    }
    setHexColor(h, color);
  }
  pushLEDs();
}

// Player presses Key 15 in status phase to mark themselves ready.
static void handleStatusReady(uint8_t playerIndex) {
  if (!players[playerIndex].active) return;
  players[playerIndex].readyForNext = true;

  if (rtCfg.debugSerial) {
    Serial.print(F("Game: Player "));
    Serial.print(playerIndex + 1);
    Serial.println(F(" ready (status)"));
  }
}

static bool checkStatusComplete() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (players[i].active && !players[i].readyForNext) return false;
  }
  return true;
}

// =============================================================================
// SECTION 8: PHASE_AGENDA — Speaker Actions
// =============================================================================

// Called every loop() during PHASE_AGENDA.
// Entire board pulses white gently (20%→50%, 1s cycle).
static void updateAgendaPulse() {
  uint8_t pulseBrightness = beatsin8(60, AGENDA_PULSE_MIN, AGENDA_PULSE_MAX);
  CRGB white = CRGB::White;
  white.nscale8(pulseBrightness);
  setAllHexes(white);
  pushLEDs();
}

// Resets all per-round player flags so the next round starts clean.
static void resetRoundState() {
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].hasPassed      = false;
    players[i].readyForNext   = false;
    players[i].strategyCard   = 0;
    players[i].strategyLocked = false;
  }
  memset(gameState.colorTaken, 0, sizeof(gameState.colorTaken));
}

// =============================================================================
// SECTION 9: Phase Transitions
// =============================================================================

// Inline center-out pulse reusing ANIM_SPIRAL (avoids circular include with animations.h).
static void runCenterOutPulse() {
  startEffect(ANIM_SPIRAL);
  uint32_t startMs = millis();
  while (millis() - startMs < 1800) updateLEDs();
  stopEffect();
}

void transitionToSetup() {
  gameState.currentPhase = PHASE_SETUP;
  memset(gameState.colorTaken, 0, sizeof(gameState.colorTaken));

  detectConnectedKeyboards();
  assignHomeHexes();

  // Assign default colors (first N from palette, in player order)
  uint8_t colorSlot = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    players[i].selectedColorIndex = colorSlot % 8;
    players[i].colorHex           = COLOR_PALETTE[colorSlot % 8];
    players[i].colorLocked        = false;
    colorSlot++;
  }

  setAllHexes(CRGB::Black);
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: SETUP — players selecting colors"));
    Serial.println(F("  Keys 1-8: select color  |  Key 15: lock color  |  Key 0: start game (GM)"));
  }
}

void transitionToStrategy() {
  gameState.currentPhase = PHASE_STRATEGY;
  buildStrategyPickOrder();

  setAllHexes(CRGB::Black);
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: STRATEGY — players selecting strategy cards"));
    Serial.println(F("  Keys 1-8: select card  |  Key 15: lock card"));
    Serial.print(F("  Pick order: "));
    for (uint8_t i = 0; i < gameState.numActivePlayers; i++) {
      Serial.print(F("P")); Serial.print(gameState.strategyPickOrder[i] + 1);
      if (i < gameState.numActivePlayers - 1) Serial.print(F(" → "));
    }
    Serial.println();
  }
}

void transitionToAction() {
  gameState.currentPhase      = PHASE_ACTION;
  gameState.currentActionIndex = 0;
  gameState.inBattle           = false;

  buildActionOrder();
  if (gameState.actionOrderSize > 0) {
    players[gameState.actionOrder[0]].turnStartMs = millis();
  }

  // Return home hexes to player colors
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    CRGB color;
    color.r = (players[i].colorHex >> 16) & 0xFF;
    color.g = (players[i].colorHex >>  8) & 0xFF;
    color.b =  players[i].colorHex        & 0xFF;
    setHexColor(players[i].homeHex, color);
  }
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: ACTION — initiative order:"));
    for (uint8_t i = 0; i < gameState.actionOrderSize; i++) {
      Serial.print(F("  P")); Serial.print(gameState.actionOrder[i] + 1);
      Serial.print(F(" (card ")); Serial.print(players[gameState.actionOrder[i]].strategyCard);
      Serial.println(F(")"));
    }
    Serial.println(F("  Key 14: pass  |  Key 15: end turn  |  Key 13: battle mode"));
  }
}

void transitionToStatus() {
  gameState.currentPhase = PHASE_STATUS;
  gameState.inBattle     = false;
  assignSlicesToPlayers();

  if (rtCfg.debugSerial) {
    Serial.println(F("Phase: STATUS — all players ready up"));
    Serial.println(F("  Key 15: ready"));
  }
}

void transitionToAgenda() {
  gameState.currentPhase = PHASE_AGENDA;

  setAllHexes(CRGB::Black);
  pushLEDs();

  if (rtCfg.debugSerial) {
    Serial.print(F("Phase: AGENDA — speaker is P"));
    Serial.println(gameState.speakerIndex + 1);
    Serial.println(F("  Key 15 (speaker): end agenda / start next round"));
  }
}

// =============================================================================
// SECTION 10: Key Dispatch
// =============================================================================
// Called from onKeyPressed() in TI4_HexRiser.ino (keyboard hardware)
// and also from the serial 'kb' command (simulation).

void handleGameKey(uint8_t playerIndex, uint8_t key) {
  if (rtCfg.debugSerial) {
    Serial.print(F("Key: P")); Serial.print(playerIndex + 1);
    Serial.print(F(" key=")); Serial.println(key);
  }

  switch (gameState.currentPhase) {

    case PHASE_SETUP:
      if (key >= 1 && key <= 8) {
        handleColorSelection(playerIndex, key);
      } else if (key == 15) {
        handleColorLockIn(playerIndex);
      } else if (key == 0) {
        // Key 0 = GM "Start Game" — any player can trigger if all are locked
        if (allPlayersLocked()) {
          selectRandomSpeaker();
          transitionToStrategy();
        }
      }
      break;

    case PHASE_STRATEGY:
      if (key >= 1 && key <= 8) {
        handleStrategyCardSelection(playerIndex, key);
      } else if (key == 15) {
        handleStrategyLockIn(playerIndex);
      }
      break;

    case PHASE_ACTION:
      if (key == 15) {
        handleEndTurn(playerIndex);
      } else if (key == 14) {
        handlePlayerPass(playerIndex);
      } else if (key == 13) {
        // Key 13 = battle mode toggle (select opponent via keys 1-8 next)
        // Simplified: toggle battle off if already in battle
        if (gameState.inBattle) {
          endBattle();
        }
        // Starting battle requires two presses — handled elsewhere or via serial
      }
      break;

    case PHASE_STATUS:
      if (key == 15) {
        handleStatusReady(playerIndex);
      }
      break;

    case PHASE_AGENDA:
      if (key == 15 && playerIndex == gameState.speakerIndex) {
        resetRoundState();
        runCenterOutPulse();
        transitionToStrategy();
      }
      break;
  }
}

// =============================================================================
// SECTION 11: Main State Machine Update (called every loop())
// =============================================================================

void updateGameState() {
  switch (gameState.currentPhase) {

    case PHASE_SETUP:
      updateJoinModeDisplay();
      break;

    case PHASE_STRATEGY:
      updateStrategyPickerPulse();
      if (checkStrategyComplete()) {
        runCenterOutPulse();
        transitionToAction();
      }
      break;

    case PHASE_ACTION:
      if (checkActionComplete()) {
        transitionToStatus();
      } else if (!gameState.inBattle) {
        updateActivePlayerPulse();
      }
      break;

    case PHASE_STATUS:
      updateStatusPulse();
      if (checkStatusComplete()) {
        transitionToAgenda();
      }
      break;

    case PHASE_AGENDA:
      updateAgendaPulse();
      break;
  }
}

// =============================================================================
// SECTION 12: initGameState() — call once in setup()
// =============================================================================

void initGameState(uint8_t defaultPlayerCount) {
  // Initialize all player slots
  for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
    players[i].id                 = i + 1;
    players[i].active             = (i < defaultPlayerCount);
    players[i].initiative         = i + 1;
    players[i].colorHex           = COLOR_PALETTE[i];
    players[i].hasPassed          = false;
    players[i].selectedColorIndex = i;
    players[i].colorLocked        = false;
    players[i].strategyCard       = 0;
    players[i].strategyLocked     = false;
    players[i].readyForNext       = false;
    players[i].turnStartMs        = 0;
    players[i].homeHex            = 30;  // will be set properly in transitionToSetup()
  }

  memset(&gameState, 0, sizeof(gameState));
  gameState.currentPhase = PHASE_SETUP;
}
