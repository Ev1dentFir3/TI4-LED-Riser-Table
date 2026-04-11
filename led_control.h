#pragma once
#include <FastLED.h>
#include "config.h"
#include "runtime_settings.h"
#include "led_map.h"
#include "hex_neighbors.h"

// =============================================================================
// TI4 Hex Riser - LED Control
// =============================================================================
// FastLED setup, per-hex color management, and all animation effects.
// Call initLEDs() once in setup(), updateLEDs() every loop iteration.
// =============================================================================

CRGB leds[NUM_LEDS];

// Per-hex target colors (what each hex should display)
CRGB hexColor[NUM_HEXES];

// Per-hex owner (-1 = unowned, 0–7 = player index)
int8_t hexOwner[NUM_HEXES];

// Animation state
enum AnimEffect {
  ANIM_NONE        = 0,
  ANIM_RAINBOW     = 1,  // rotating rainbow across all hexes
  ANIM_PULSE       = 2,  // gentle brightness pulse on all hexes
  ANIM_SPIRAL      = 3,  // spiral out from hex 30 (center)
  ANIM_SPARKLE     = 4,  // random sparkle across the grid
  ANIM_WAVE        = 5,  // color wave sweeping left to right
};

static AnimEffect currentEffect  = ANIM_NONE;
static uint32_t   animStartMs    = 0;
static uint32_t   lastLEDUpdate  = 0;

// Forward declarations
void setHexColor(int hex, CRGB color);
void setHexSideColor(int hex, int side, CRGB color);
void applyHexColors();
void pushLEDs();
void runLEDTest();

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
void initLEDs() {
  FastLED.addLeds<SK6812, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(rtCfg.defaultBrightness);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_HEXES; i++) {
    hexColor[i] = CRGB::Black;
    hexOwner[i] = -1;
  }

  if (rtCfg.debugSerial) {
    Serial.println(F("LEDs: FastLED init OK — 915 LEDs on GPIO 6"));
    Serial.print(F("LEDs: Brightness "));
    Serial.println(rtCfg.defaultBrightness);
  }

  if (rtCfg.debugLed) {
    runLEDTest();
  }
}

// -----------------------------------------------------------------------------
// Core: set a hex's color and propagate to the LED array
// -----------------------------------------------------------------------------
void setHexColor(int hex, CRGB color) {
  if (hex < 0 || hex >= NUM_HEXES) return;
  hexColor[hex] = color;

  // Write to every LED slot in this hex (skip -1 sentinels)
  for (int side = 0; side < 6; side++) {
    for (int slot = 0; slot < 3; slot++) {
      int idx = HEX_MAP[hex][side][slot];
      if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = color;
      }
    }
  }
}

// Set only the LEDs on one side of a hex (does not update hexColor[])
void setHexSideColor(int hex, int side, CRGB color) {
  if (hex < 0 || hex >= NUM_HEXES) return;
  if (side < 0 || side > 5) return;
  for (int slot = 0; slot < 3; slot++) {
    int idx = HEX_MAP[hex][side][slot];
    if (idx >= 0 && idx < NUM_LEDS) {
      leds[idx] = color;
    }
  }
}

// Set multiple hexes at once
void setAllHexes(CRGB color) {
  for (int i = 0; i < NUM_HEXES; i++) {
    setHexColor(i, color);
  }
}

// Re-sync leds[] from hexColor[] (call after direct hexColor edits)
void applyHexColors() {
  for (int i = 0; i < NUM_HEXES; i++) {
    setHexColor(i, hexColor[i]);
  }
}

// Push to hardware (non-blocking via millis gate)
void pushLEDs() {
  if (rtCfg.simulateHardware) return;
  FastLED.show();
}

// -----------------------------------------------------------------------------
// Effect runner — call from updateLEDs() every loop()
// -----------------------------------------------------------------------------
void startEffect(AnimEffect effect) {
  currentEffect = effect;
  animStartMs   = millis();
}

void stopEffect() {
  currentEffect = ANIM_NONE;
  FastLED.setBrightness(rtCfg.defaultBrightness);
  setAllHexes(CRGB::Black);
  pushLEDs();
}

// Rainbow: rotating hue mapped across hex index
static void tickRainbow(uint32_t elapsed) {
  uint8_t hueOffset = (elapsed / 20) & 0xFF; // full cycle ~5 s
  for (int i = 0; i < NUM_HEXES; i++) {
    uint8_t hue = hueOffset + (uint8_t)((i * 255) / NUM_HEXES);
    CRGB c = CHSV(hue, 255, 200);  // convert once per hex, reuse for all slots
    for (int side = 0; side < 6; side++) {
      for (int slot = 0; slot < 3; slot++) {
        int idx = HEX_MAP[i][side][slot];
        if (idx >= 0) leds[idx] = c;
      }
    }
  }
}

// Pulse: sine-wave brightness, slow hue drift across all hexes
static void tickPulse(uint32_t elapsed) {
  uint8_t val = beatsin8(30, 40, 255);  // 30 BPM, 40–255 value
  uint8_t hue = elapsed / 200;          // full hue cycle ~51 s
  fill_solid(leds, NUM_LEDS, CHSV(hue, 200, val));
}

// Spiral: ring-by-ring outward from center hex 30
// Ring sizes: 1, 6, 12, 18, 24  (total = 61)
// Derived from the actual grid geometry — every hex appears exactly once.
// 255 = sentinel (end of ring).
static const uint8_t SPIRAL_RINGS[5][24] = {
  // Ring 0 — center (1 hex)
  {30,
   255,255,255,255,255,255,255,255,255,255,255,
   255,255,255,255,255,255,255,255,255,255,255,255},
  // Ring 1 — 6 hexes
  {29,31,21,22,38,39,
   255,255,255,255,255,255,255,255,255,255,255,
   255,255,255,255,255,255,255},
  // Ring 2 — 12 hexes
  {28,23,40,32,20,37,14,15,13,46,47,45,
   255,255,255,255,255,255,255,255,255,255,255,255},
  // Ring 3 — 18 hexes
  {27,24,41,12,44,33,19,36,16,48,8,7,6,9,53,52,51,54,
   255,255,255,255,255,255},
  // Ring 4 — 24 hexes (outer edge)
  {0,1,2,3,4,5,10,11,17,18,25,26,34,35,42,43,49,50,55,56,57,58,59,60},
};

static void tickSpiral(uint32_t elapsed) {
  uint32_t ringMs = 350; // ms per ring
  int activeRing  = (int)(elapsed / ringMs);
  if (activeRing > 4) activeRing = 4;

  uint8_t hue = (elapsed / 10) & 0xFF;
  FastLED.clear();

  for (int r = 0; r <= activeRing && r < 5; r++) {
    CRGB c = CHSV(hue + r * 40, 255, 200);
    for (int j = 0; j < 24; j++) {
      uint8_t h = SPIRAL_RINGS[r][j];
      if (h == 255) break;
      setHexColor(h, c);
    }
  }
}

// Sparkle: random hex flickers white
static void tickSparkle(uint32_t elapsed) {
  // Fade everything slightly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  // Randomly light a hex
  if (random8() < 40) {
    int h = random8(NUM_HEXES);
    setHexColor(h, CRGB::White);
  }
}

// Wave: color sweeps left-to-right by column (col 0–8)
static const uint8_t WAVE_COL_START[9] = {0, 5,11,18,26,35,43,50,56};
static const uint8_t WAVE_COL_SIZE[9]  = {5, 6, 7, 8, 9, 8, 7, 6, 5};
static void tickWave(uint32_t elapsed) {
  uint8_t  hue       = (elapsed / 15) & 0xFF;
  uint32_t period    = 1500; // ms for full sweep
  int      waveCol   = (int)(((elapsed % period) * 9UL) / period);

  for (int col = 0; col < 9; col++) {
    uint8_t brightness = (col == waveCol) ? 255
                       : (col == (waveCol + 1) % 9) ? 128 : 20;
    CRGB c = CHSV(hue + col * 20, 240, brightness);
    for (int i = 0; i < WAVE_COL_SIZE[col]; i++) {
      setHexColor(WAVE_COL_START[col] + i, c);
    }
  }
}

// Main animation tick — call every loop()
static void tickEffect() {
  if (currentEffect == ANIM_NONE) return;
  uint32_t elapsed = millis() - animStartMs;
  switch (currentEffect) {
    case ANIM_RAINBOW: tickRainbow(elapsed); break;
    case ANIM_PULSE:   tickPulse(elapsed);   break;
    case ANIM_SPIRAL:  tickSpiral(elapsed);  break;
    case ANIM_SPARKLE: tickSparkle(elapsed); break;
    case ANIM_WAVE:    tickWave(elapsed);    break;
    default: break;
  }
}

// -----------------------------------------------------------------------------
// LED test: rainbow wave through all hexes sequentially
// Run once in setup() when DEBUG_LED_TEST is true.
// -----------------------------------------------------------------------------
void runLEDTest() {
  if (rtCfg.debugSerial) Serial.println(F("LED test: scanning all 915 LEDs..."));
  for (int h = 0; h < NUM_HEXES; h++) {
    FastLED.clear();
    uint8_t hue = (uint8_t)((h * 255) / NUM_HEXES);
    setHexColor(h, CHSV(hue, 255, 200));
    FastLED.show();
    delay(60);
  }
  FastLED.clear();
  FastLED.show();
  if (rtCfg.debugSerial) Serial.println(F("LED test: complete"));
}

// -----------------------------------------------------------------------------
// updateLEDs() — call every loop()
// -----------------------------------------------------------------------------
void updateLEDs() {
  uint32_t now = millis();
  if (now - lastLEDUpdate < rtCfg.ledUpdateMs) return;
  lastLEDUpdate = now;

  if (currentEffect != ANIM_NONE) {
    tickEffect();
  }

  pushLEDs();
}

// -----------------------------------------------------------------------------
// Brightness control
// -----------------------------------------------------------------------------
void setBrightness(uint8_t b) {
  if (b > rtCfg.maxBrightness) b = rtCfg.maxBrightness;
  FastLED.setBrightness(b);
}
