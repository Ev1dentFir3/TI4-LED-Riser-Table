#pragma once
#include "config.h"
#include "runtime_settings.h"

// =============================================================================
// TI4 Hex Riser - Keyboard Control (STUB)
// =============================================================================
// MCP23017 boards have not yet arrived. This file is a complete stub that
// compiles and runs cleanly. When the boards arrive, implement the four
// sections marked  ← IMPLEMENT HERE.
//
// Library needed (install in Arduino IDE Library Manager):
//   "Adafruit MCP23X17" by Adafruit
//
// Wiring reference (per HARDWARE_SPEC.md):
//   MCP #1 (0x20) → keyboards 1–2
//   MCP #2 (0x21) → keyboards 3–4
//   MCP #3 (0x22) → keyboards 5–6
//   MCP #4 (0x23) → keyboards 7–8
//   Each MCP: GPA0–3 = rows, GPA4–7 = cols for keyboard A
//             GPB0–3 = rows, GPB4–7 = cols for keyboard B
// =============================================================================

// Callback type: called when a key is pressed.
// player: 0–7 (0 = admin), key: 0–15 (4×4 matrix)
typedef void (*KeyPressCallback)(uint8_t player, uint8_t key);
static KeyPressCallback onKeyPress = nullptr;

void setKeyPressCallback(KeyPressCallback cb) { onKeyPress = cb; }

// -----------------------------------------------------------------------------
// initKeyboard() — call once in setup()
// -----------------------------------------------------------------------------
void initKeyboard() {
  // ← IMPLEMENT HERE (step 1 of 4): uncomment and wire up the MCP23017 boards.
  //
  // #include <Adafruit_MCP23X17.h>
  // Adafruit_MCP23X17 mcp[NUM_MCP];
  //
  // Wire.begin();
  // for (int i = 0; i < NUM_MCP; i++) {
  //   uint8_t addr = MCP_ADDR_1 + i;
  //   if (!mcp[i].begin_I2C(addr)) {
  //     Serial.print(F("KB: MCP23017 not found at 0x"));
  //     Serial.println(addr, HEX);
  //   } else {
  //     // Rows: INPUT_PULLUP; Cols: OUTPUT
  //     for (int p = 0; p < 8; p++) mcp[i].pinMode(p, INPUT_PULLUP);   // GPA = rows
  //     for (int p = 8; p < 16; p++) mcp[i].pinMode(p, OUTPUT);        // GPB = cols
  //     Serial.print(F("KB: MCP23017 OK at 0x"));
  //     Serial.println(addr, HEX);
  //   }
  // }

  if (rtCfg.debugSerial) {
    RPC.println("[M4] KB: keyboard stub active — MCP23017 not yet wired");
  }
}

// -----------------------------------------------------------------------------
// handleKeyboard() — call every loop()
// Non-blocking 4×4 matrix scan with 20 ms debounce.
// -----------------------------------------------------------------------------
void handleKeyboard() {
  // ← IMPLEMENT HERE (step 2 of 4): scan each MCP's keyboard matrix.
  //
  // static uint32_t lastScanMs = 0;
  // static uint16_t prevState[NUM_MCP * 2] = {0};  // 2 keyboards per MCP
  //
  // if (millis() - lastScanMs < 20) return;   // 20 ms debounce
  // lastScanMs = millis();
  //
  // for (int m = 0; m < NUM_MCP; m++) {
  //   for (int kb = 0; kb < 2; kb++) {          // 2 keyboards per MCP
  //     uint8_t rowBase = kb * 8;               // GPA or GPB
  //     uint16_t state  = 0;
  //     for (int col = 0; col < 4; col++) {
  //       mcp[m].digitalWrite(rowBase + 8 + col, LOW);   // drive col low
  //       for (int row = 0; row < 4; row++) {
  //         if (!mcp[m].digitalRead(rowBase + row)) {    // active low
  //           state |= (1 << (col * 4 + row));
  //         }
  //       }
  //       mcp[m].digitalWrite(rowBase + 8 + col, HIGH);  // restore
  //     }
  //     uint16_t changed = state & ~prevState[m * 2 + kb]; // newly pressed
  //     prevState[m * 2 + kb] = state;
  //     if (changed && onKeyPress) {
  //       for (int k = 0; k < 16; k++) {
  //         if (changed & (1 << k)) {
  //           uint8_t player = (uint8_t)(m * 2 + kb);
  //           onKeyPress(player, (uint8_t)k);
  //           if (DEBUG_KEYBOARD_TEST) {
  //             Serial.print(F("KB: player "));
  //             Serial.print(player);
  //             Serial.print(F(" key "));
  //             Serial.println(k);
  //           }
  //         }
  //       }
  //     }
  //   }
  // }

  // ← IMPLEMENT HERE (step 3 of 4): Serial command fallback for simulation.
  // (Already handled in TI4_HexRiser.ino handleSerialCommand())

  // ← IMPLEMENT HERE (step 4 of 4): key-to-action mapping.
  // Define what each of the 16 keys does per player (color cycle, pass, etc.)
  // Recommend a lookup table:  const char* KEY_ACTIONS[16] = {"confirm","pass",...}
}
