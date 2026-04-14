#pragma once

// =============================================================================
// TI4 Hex Riser - Non-blocking Hex Flash (M4)
// =============================================================================
// Replaces the blocking delay(80) in onHexSelected().
// startHexFlash() returns immediately; tickHexFlash() is called every
// updateLEDs() to restore the hex color after the duration elapses.
// =============================================================================

struct HexFlash {
  bool     active      = false;
  int      hexIdx      = -1;
  CRGB     restoreColor;
  uint32_t startMs     = 0;
  uint16_t durationMs  = 80;
};

static HexFlash pendingFlash;

inline void startHexFlash(int hex, CRGB restoreColor) {
  setHexColor(hex, CRGB::White);
  pendingFlash = { true, hex, restoreColor, millis(), 80 };
}

// Call from updateLEDs() every loop — costs ~0 ms when inactive.
inline void tickHexFlash() {
  if (!pendingFlash.active) return;
  if (millis() - pendingFlash.startMs >= pendingFlash.durationMs) {
    setHexColor(pendingFlash.hexIdx, pendingFlash.restoreColor);
    pendingFlash.active = false;
  }
}
