#pragma once
#include "config.h"
#include "led_control.h"

// =============================================================================
// TI4 Hex Riser - Game Animations (M4)
// =============================================================================
// handleNetwork() removed — M7 handles WiFi on its own core.
// animDelay() now calls updateLEDs() to keep LED frames smooth during
// long blocking sequences (roulette, boot snake, phase transitions).
// =============================================================================

// -----------------------------------------------------------------------------
// animDelay() — replaces delay() inside animation code.
// Keeps LED frames smooth without touching network (M7's job now).
// -----------------------------------------------------------------------------
static void animDelay(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    updateLEDs();
  }
}

// -----------------------------------------------------------------------------
// Boot animation: white snake crawls through all 61 hexes in order.
// Only BOOT_ANIM_TAIL hexes are lit at a time; tail turns off as head advances.
// -----------------------------------------------------------------------------
void runBootAnimation() {
  if (rtCfg.debugSerial) RPC.println("[M4] Boot: running snake animation");

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

  if (rtCfg.debugSerial) RPC.println("[M4] Boot: animation complete");
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
  }
  stopEffect();
}
