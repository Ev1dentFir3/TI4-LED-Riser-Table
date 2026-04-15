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
  // Bridge M4 RPC debug output to USB Serial.
  // Always drain the RPC buffer so M4's RPC.print() calls never block.
  // Discard bytes if the USB CDC TX buffer is full rather than blocking here —
  // a blocked Serial.write() here would stall loop(), which in turn fills the
  // RPC buffer and blocks M4, freezing animations.
  while (RPC.available()) {
    uint8_t b = (uint8_t)RPC.read();
    if (Serial.availableForWrite() > 0) Serial.write(b);
  }

  handleNetwork();   // WebSocket push + HTTP /cmd

  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // Guard: only print if the TX buffer has room — a blocking print here
    // would stall loop() and cascade into M4 blocking on RPC.print().
    if (rtCfg.debugSerial && Serial.availableForWrite() > 20) {
      Serial.println(F("[M7] alive"));
    }
  }
}
