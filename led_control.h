#pragma once
#include <FastLED.h>
#include "config.h"
#include "runtime_settings.h"
#include "led_map.h"
#include "hex_neighbors.h"

// =============================================================================
// TI4 Hex Riser - LED Control
// Target: ESP32-S3-WROOM-1-N16R8
// =============================================================================
// FastLED setup, per-hex color management, and all animation effects.
// Call initLEDs() once in setup(), then create the LED task on Core 0.
//
// Dual-core safety:
//   ledMutex protects FastLED.show() in pushLEDs(). Game state writes to
//   leds[]/hexColor[] on Core 1 are not mutex-guarded for simplicity; the
//   ESP32 RMT peripheral does not disable interrupts, so the worst case is
//   one frame of visual tearing, which is acceptable.
// =============================================================================

CRGB leds[NUM_LEDS];
CRGB hexColor[NUM_HEXES];
int8_t hexOwner[NUM_HEXES];

// Mutex protecting FastLED.show() — taken by pushLEDs() on Core 0.
SemaphoreHandle_t ledMutex = nullptr;

enum AnimEffect {
  ANIM_NONE    = 0,
  ANIM_RAINBOW = 1,
  ANIM_PULSE   = 2,
  ANIM_SPIRAL  = 3,
  ANIM_SPARKLE = 4,
  ANIM_WAVE    = 5,
};

static AnimEffect currentEffect      = ANIM_NONE;
static uint32_t   animStartMs        = 0;
static uint32_t   lastLEDUpdate      = 0;

// Snapshot saved when an effect starts; restored when stopped.
// Lets effects temporarily hijack the LEDs without touching game state.
static CRGB hexColorSnapshot[NUM_HEXES];
static bool effectSnapshotValid = false;

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
  ledMutex = xSemaphoreCreateMutex();

  FastLED.addLeds<SK6812, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(rtCfg.defaultBrightness);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_HEXES; i++) {
    hexColor[i] = CRGB::Black;
    hexOwner[i] = -1;
  }

  if (rtCfg.debugSerial) {
    Serial.println(F("LEDs: FastLED init OK — 915 LEDs on GPIO 13 (I2S)"));
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
  for (int side = 0; side < 6; side++) {
    for (int slot = 0; slot < 3; slot++) {
      int idx = HEX_MAP[hex][side][slot];
      if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = color;
      }
    }
  }
}

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

void setAllHexes(CRGB color) {
  for (int i = 0; i < NUM_HEXES; i++) {
    setHexColor(i, color);
  }
}

void applyHexColors() {
  for (int i = 0; i < NUM_HEXES; i++) {
    setHexColor(i, hexColor[i]);
  }
}

// Push to hardware — mutex-guarded so Core 0 LED task and Core 1 won't clash.
void pushLEDs() {
  if (rtCfg.simulateHardware) return;
  if (ledMutex) xSemaphoreTake(ledMutex, portMAX_DELAY);
  FastLED.show();
  if (ledMutex) xSemaphoreGive(ledMutex);
}

// -----------------------------------------------------------------------------
// Effect runner
// -----------------------------------------------------------------------------
void startEffect(AnimEffect effect) {
  if (currentEffect == ANIM_NONE) {
    // Save board state so stopEffect() can restore it
    memcpy(hexColorSnapshot, hexColor, sizeof(hexColor));
    effectSnapshotValid = true;
  }
  currentEffect = effect;
  animStartMs   = millis();
}

void stopEffect() {
  currentEffect = ANIM_NONE;
  FastLED.setBrightness(rtCfg.defaultBrightness);
  if (effectSnapshotValid) {
    memcpy(hexColor, hexColorSnapshot, sizeof(hexColor));
    applyHexColors();
    effectSnapshotValid = false;
  } else {
    setAllHexes(CRGB::Black);
  }
  pushLEDs();
}

static void tickRainbow(uint32_t elapsed) {
  uint8_t hueOffset = (elapsed / 20) & 0xFF;
  for (int i = 0; i < NUM_HEXES; i++) {
    uint8_t hue = hueOffset + (uint8_t)((i * 255) / NUM_HEXES);
    CRGB c = CHSV(hue, 255, 200);
    for (int side = 0; side < 6; side++) {
      for (int slot = 0; slot < 3; slot++) {
        int idx = HEX_MAP[i][side][slot];
        if (idx >= 0) leds[idx] = c;
      }
    }
  }
}

static void tickPulse(uint32_t elapsed) {
  uint8_t val = beatsin8(30, 40, 255);
  uint8_t hue = elapsed / 200;
  fill_solid(leds, NUM_LEDS, CHSV(hue, 200, val));
}

// Spiral path: center outward, each ring traversed clockwise before stepping out.
// Ring boundaries: [0]=center, [1-6]=ring1, [7-18]=ring2, [19-36]=ring3, [37-60]=ring4.
// Every adjacent pair is verified as a neighbor in HEX_NEIGHBORS.
static const uint8_t SPIRAL_PATH[61] = {
  30,                                                             // ring 0
  29, 39, 38, 31, 21, 22,                                        // ring 1
  13, 23, 28, 40, 45, 46, 47, 37, 32, 20, 15, 14,               // ring 2
   8,  9, 12, 24, 27, 41, 44, 54, 53, 52, 51, 48, 36, 33, 19, 16,  6,  7,  // ring 3
   3,  2,  1,  0, 10, 11, 25, 26, 42, 43, 55, 56, 57, 58, 59, 60, 50, 49, 35, 34, 18, 17,  5,  4  // ring 4
};

static const int SPIRAL_TAIL = 14;   // hexes in the fading tail
static const int SPIRAL_STEP_MS = 70; // ms per hex advance (~4.3s per full loop)

static void tickSpiral(uint32_t elapsed) {
  int   head    = (int)(elapsed / SPIRAL_STEP_MS) % 61;
  uint8_t hue   = (uint8_t)(elapsed / 400);

  bool painted[NUM_HEXES] = {};

  for (int i = 0; i < SPIRAL_TAIL; i++) {
    int     idx  = ((head - i) + 61) % 61;
    uint8_t hex  = SPIRAL_PATH[idx];
    uint8_t bri  = (i == 0) ? 255 : (uint8_t)(255 * (SPIRAL_TAIL - i) / SPIRAL_TAIL);
    setHexColor(hex, CHSV(hue + (uint8_t)(i * 6), 220, bri));
    painted[hex] = true;
  }

  // Explicitly black unpainted hexes — no FastLED.clear() needed
  for (int i = 0; i < NUM_HEXES; i++) {
    if (!painted[i]) setHexColor(i, CRGB::Black);
  }
}

static void tickSparkle(uint32_t elapsed) {
  fadeToBlackBy(leds, NUM_LEDS, 10);
  if (random8() < 40) {
    int hexIdx = random8(NUM_HEXES);
    setHexColor(hexIdx, CRGB::White);
  }
}

static const uint8_t WAVE_COL_START[9] = {0, 5,11,18,26,35,43,50,56};
static const uint8_t WAVE_COL_SIZE[9]  = {5, 6, 7, 8, 9, 8, 7, 6, 5};
static void tickWave(uint32_t elapsed) {
  uint8_t  hue    = (elapsed / 15) & 0xFF;
  uint32_t period = 1500;
  int      waveCol = (int)(((elapsed % period) * 9UL) / period);

  for (int col = 0; col < 9; col++) {
    uint8_t brightness = (col == waveCol) ? 255
                       : (col == (waveCol + 1) % 9) ? 128 : 20;
    CRGB c = CHSV(hue + col * 20, 240, brightness);
    for (int i = 0; i < WAVE_COL_SIZE[col]; i++) {
      setHexColor(WAVE_COL_START[col] + i, c);
    }
  }
}

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
// LED test
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
// updateLEDs() — called by the LED task on Core 0 every loop
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
