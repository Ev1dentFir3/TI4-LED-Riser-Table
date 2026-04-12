#pragma once
#include "config.h"
#include "led_control.h"

// =============================================================================
// TI4 Hex Riser - Game Animations
// =============================================================================
// Boot sequence and phase-transition animations.
// Per-phase idle animations (join fade, picker pulse, etc.) live in game_state.h.
// =============================================================================

// network.h is included after this file, so forward-declare what we need.
void handleNetwork();

// -----------------------------------------------------------------------------
// animDelay() — replaces delay() inside animation code.
// Keeps handleNetwork() running so the board stays responsive during long
// blocking animations (roulette, boot snake, etc.).
// -----------------------------------------------------------------------------
static void animDelay(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    handleNetwork();
  }
}

// -----------------------------------------------------------------------------
// Boot animation: white snake crawls through all 61 hexes in order.
// Only BOOT_ANIM_TAIL hexes are lit at a time; tail turns off as head advances.
// -----------------------------------------------------------------------------
void runBootAnimation() {
  if (rtCfg.debugSerial) Serial.println(F("Boot: running snake animation"));

  for (int i = 0; i < NUM_HEXES + BOOT_ANIM_TAIL; i++) {
    if (i < NUM_HEXES) {
      setHexColor(i, CRGB::White);
    }
    if (i >= BOOT_ANIM_TAIL) {
      setHexColor(i - BOOT_ANIM_TAIL, CRGB::Black);
    }
    pushLEDs();
    animDelay(BOOT_ANIM_SPEED_MS);
  }

  FastLED.clear();
  pushLEDs();

  if (rtCfg.debugSerial) Serial.println(F("Boot: animation complete"));
}

// -----------------------------------------------------------------------------
// Center-out pulse: reuses the SPIRAL effect for ~1.8 s then stops.
// Used as the transition animation between strategy -> action and other phases.
// -----------------------------------------------------------------------------
void runCenterOutPulse() {
  startEffect(ANIM_SPIRAL);
  uint32_t startMs = millis();
  while (millis() - startMs < 1800) {
    updateLEDs();
    handleNetwork();
  }
  stopEffect();
}
