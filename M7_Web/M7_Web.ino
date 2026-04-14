// =============================================================================
// TI4 Hex Riser - M7 Core Firmware
// Target: Arduino Giga R1 WiFi — Main Core (M7)
//
// Required libraries: none beyond Arduino Giga core
//
// Flash split: 1MB M7 + 1MB M4
// Target core: Main Core (M7)
//
// This core owns: WiFi, HTTP server (SSE + commands), state broadcaster.
// All LED / game logic lives on M4. Communication via sharedState in SRAM4.
// M4 RPC output is bridged to USB Serial in loop().
// =============================================================================

#include <RPC.h>
#include <WiFi.h>
#include "shared_state.h"
#include "config.h"
#include "runtime_settings.h"
#include "web_interface.h"
#include "settings_page.h"
#include "network_ws.h"

// =============================================================================
// setup()
// =============================================================================
void setup() {
  Serial.begin(115200);
  RPC.begin();
  while (!Serial && millis() < 3000) {}

  Serial.println(F("[M7] =============================="));
  Serial.println(F("[M7]  TI4 Hex Riser v2.0 — M7 Core"));
  Serial.println(F("[M7] =============================="));

  initNetwork();
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
  // Bridge M4 RPC debug output to USB Serial
  while (RPC.available()) {
    Serial.write(RPC.read());
  }

  handleNetwork();   // HTTP (SSE + /cmd) + broadcastNewState()

  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    if (rtCfg.debugSerial) {
      // Must invalidate D-cache — M4 writes sharedState from the other core
      uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
      SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);
      Serial.print(F("[M7] alive — frameCount="));
      Serial.print(sharedState.leds.frameCount);
      Serial.print(F("  stateVer="));
      Serial.println(sharedState.game.stateVersion);
    }
  }
}
