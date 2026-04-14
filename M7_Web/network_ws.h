#pragma once
#include <WiFi.h>
#include "shared_state.h"
#include "config.h"
#include "runtime_settings.h"
#include "web_interface.h"
#include "settings_page.h"

// =============================================================================
// TI4 Hex Riser - M7 Network (SSE + HTTP commands)
// =============================================================================
// Single HTTP server on port 80 handles all traffic.
// WebSocket on port 81 removed — Nina WiFi can't run two listeners.
//
//   GET /           → main page (chunked, 1KB blocks)
//   GET /events     → Server-Sent Events stream (M7→browser, long-lived)
//   GET /cmd?msg=X  → command endpoint (browser→M7→CmdQueue→M4)
//   GET /settings   → settings page
//   GET /getsettings  → settings JSON
//   GET /savesettings?... → save settings
//   GET /reboot     → reboot
//
// initNetwork()   — call once in setup()
// handleNetwork() — call every loop()
// =============================================================================

static WiFiServer httpServer(HTTP_PORT);
static WiFiClient pendingClient;
static WiFiClient sseClient;
static bool       sseReady     = false;  // set when client connects, don't rely on connected()
static bool       networkReady = false;
static uint32_t   lastLedFrame = 0;
static uint32_t   lastGameVer  = 0;

// -----------------------------------------------------------------------------
// Enqueue a command for M4 via SPSC ring buffer
// -----------------------------------------------------------------------------
void enqueueCmd(Cmd c) {
  volatile CmdQueue& q = sharedState.cmds;
  uint8_t next = (q.head + 1) % CMD_QUEUE_DEPTH;
  if (next == q.tail) { sharedState.droppedCmds++; return; }
  q.slots[q.head].type = c.type;
  q.slots[q.head].arg0 = c.arg0;
  q.slots[q.head].arg1 = c.arg1;
  q.slots[q.head].r    = c.r;
  q.slots[q.head].g    = c.g;
  q.slots[q.head].b    = c.b;
  q.head = next;
}

// -----------------------------------------------------------------------------
// Parse command string → enqueue for M4
// -----------------------------------------------------------------------------
void parseCommand(const char* msg) {
  if (rtCfg.debugWeb) { Serial.print(F("[M7] CMD: ")); Serial.println(msg); }

  if (strncmp(msg, "SELECT:", 7) == 0) {
    Cmd c = {}; c.type = CMD_SELECT_HEX; c.arg0 = (uint8_t)atoi(msg+7);
    enqueueCmd(c);

  } else if (strncmp(msg, "BRIGHTNESS:", 11) == 0) {
    Cmd c = {}; c.type = CMD_BRIGHTNESS; c.arg0 = (uint8_t)atoi(msg+11);
    enqueueCmd(c);

  } else if (strcmp(msg, "CLEAR") == 0) {
    Cmd c = {}; c.type = CMD_CLEAR;
    enqueueCmd(c);

  } else if (strncmp(msg, "EFFECT:", 7) == 0) {
    const char* n = msg+7;
    uint8_t eff = 0;
    if      (strcmp(n,"RAINBOW") == 0) eff = 1;
    else if (strcmp(n,"PULSE")   == 0) eff = 2;
    else if (strcmp(n,"SPIRAL")  == 0) eff = 3;
    else if (strcmp(n,"SPARKLE") == 0) eff = 4;
    else if (strcmp(n,"WAVE")    == 0) eff = 5;
    Cmd c = {}; c.type = CMD_EFFECT; c.arg0 = eff;
    enqueueCmd(c);

  } else if (strncmp(msg, "SETHEX:", 7) == 0) {
    int hex = atoi(msg+7);
    const char* col = strchr(msg+7, ':');
    if (col) {
      uint32_t rgb = strtoul(col+1, nullptr, 16);
      Cmd c = {}; c.type = CMD_SET_HEX; c.arg0 = (uint8_t)hex;
      c.r = (rgb>>16)&0xFF; c.g = (rgb>>8)&0xFF; c.b = rgb&0xFF;
      enqueueCmd(c);
    }

  } else if (strncmp(msg, "SETHEXSIDE:", 11) == 0) {
    int hex = atoi(msg+11);
    const char* sp = strchr(msg+11, ':');
    if (sp) {
      int side = atoi(sp+1);
      const char* col = strchr(sp+1, ':');
      if (col) {
        uint32_t rgb = strtoul(col+1, nullptr, 16);
        Cmd c = {}; c.type = CMD_SET_HEX_SIDE;
        c.arg0 = (uint8_t)hex; c.arg1 = (uint8_t)side;
        c.r = (rgb>>16)&0xFF; c.g = (rgb>>8)&0xFF; c.b = rgb&0xFF;
        enqueueCmd(c);
      }
    }

  } else if (strncmp(msg, "ALL:", 4) == 0) {
    uint32_t rgb = strtoul(msg+4, nullptr, 16);
    Cmd c = {}; c.type = CMD_SET_ALL;
    c.r = (rgb>>16)&0xFF; c.g = (rgb>>8)&0xFF; c.b = rgb&0xFF;
    enqueueCmd(c);

  } else if (strncmp(msg, "GAMEKEY:", 8) == 0) {
    int pi = atoi(msg+8);
    const char* kp = strchr(msg+8, ':');
    if (kp) { Cmd c = {}; c.type = CMD_GAME_KEY; c.arg0=(uint8_t)pi; c.arg1=(uint8_t)atoi(kp+1); enqueueCmd(c); }

  } else if (strncmp(msg, "SETPLAYERS:", 11) == 0) {
    Cmd c = {}; c.type = CMD_SET_PLAYERS; c.arg0 = (uint8_t)atoi(msg+11);
    enqueueCmd(c);

  } else if (strcmp(msg, "STARTGAME") == 0) {
    Cmd c = {}; c.type = CMD_START_GAME;
    enqueueCmd(c);

  } else if (strncmp(msg, "PHASE:", 6) == 0) {
    Cmd c = {}; c.type = CMD_PHASE_JUMP; c.arg0 = (uint8_t)atoi(msg+6);
    enqueueCmd(c);

  } else if (strncmp(msg, "BATTLE:", 7) == 0) {
    int p1 = atoi(msg+7);
    const char* p2p = strchr(msg+7, ':');
    if (p2p) { Cmd c = {}; c.type = CMD_BATTLE; c.arg0=(uint8_t)p1; c.arg1=(uint8_t)atoi(p2p+1); enqueueCmd(c); }
  }
}

// -----------------------------------------------------------------------------
// SSE helpers — write events to the long-lived SSE client
// -----------------------------------------------------------------------------
// Write to SSE client; tolerates one transient failure (TCP buffer momentarily
// full) before treating it as a dead connection and closing.
static uint8_t _sseFailCount = 0;
static bool sseWrite(const char* buf, int len) {
  if (sseClient.write((const uint8_t*)buf, len) == 0) {
    if (++_sseFailCount >= 2) {
      _sseFailCount = 0;
      sseReady = false;
      sseClient.stop();
      if (rtCfg.debugSerial) Serial.println(F("[M7] SSE: closing (write failed)"));
      return false;
    }
    return true;  // first failure may be transient buffer-full — skip this frame
  }
  _sseFailCount = 0;
  return true;
}

static void sseSendAllHex() {
  // Single buffer → single SPI write. "data: ALLHEX:" + 61*6 hex chars + "\n\n" = 388 bytes.
  char buf[400];
  int pos = snprintf(buf, sizeof(buf), "data: ALLHEX:");
  for (int i = 0; i < SS_NUM_HEXES && pos < (int)sizeof(buf) - 7; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X%02X",
      (uint8_t)sharedState.leds.r[i],
      (uint8_t)sharedState.leds.g[i],
      (uint8_t)sharedState.leds.b[i]);
  }
  buf[pos++] = '\n'; buf[pos++] = '\n'; buf[pos] = 0;
  sseWrite(buf, pos);
}

static void sseSendBrightness() {
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "data: BRIGHTNESS:%d\n\n", sharedState.leds.brightness);
  sseWrite(buf, len);
}

static void sseSendGameState() {
  // Build entire game state burst into one buffer, then send in one write.
  char buf[1024];
  int pos = snprintf(buf, sizeof(buf),
    "data: GAMESTATE:{\"phase\":%d,\"speaker\":%d,\"players\":%d,\"pick\":%d,\"action\":%d,\"battle\":%s}\n\n",
    sharedState.game.currentPhase,
    sharedState.game.speakerIndex,
    sharedState.game.numActivePlayers,
    sharedState.game.currentPickIndex,
    sharedState.game.currentActionIndex,
    sharedState.game.inBattle ? "true" : "false");

  for (uint8_t i = 0; i < SS_MAX_PLAYERS && pos < (int)sizeof(buf) - 160; i++) {
    if (!sharedState.game.players[i].active) continue;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
      "data: PLAYER:%d:{\"color\":%lu,\"locked\":%s,\"card\":%d,\"home\":%d,\"passed\":%s}\n\n",
      i,
      (unsigned long)sharedState.game.players[i].colorHex,
      sharedState.game.players[i].colorLocked ? "true" : "false",
      sharedState.game.players[i].strategyCard,
      sharedState.game.players[i].homeHex,
      sharedState.game.players[i].hasPassed ? "true" : "false");
  }
  sseWrite(buf, pos);
}

static void pushFullStateToSSE() {
  uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
  SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);
  sseSendAllHex();    if (!sseReady) return;
  sseSendBrightness(); if (!sseReady) return;
  sseSendGameState();
}

// -----------------------------------------------------------------------------
// Broadcast new frames at BROADCAST_MS rate — call every loop()
// -----------------------------------------------------------------------------
void broadcastNewState() {
  static uint32_t lastBroadcast  = 0;
  static uint32_t lastKeepalive  = 0;
  uint32_t now = millis();
  if (now - lastBroadcast < rtCfg.broadcastMs) return;
  lastBroadcast = now;

  // Nina WiFi connected() always returns false for write-only SSE streams.
  // Trust sseReady; it is cleared by sseWrite() when a write actually fails.
  if (!sseReady) return;

  // SSE keepalive comment every 3s — prevents Nina's TCP idle timeout from
  // closing the connection when no LED frames or game state changes arrive.
  if (now - lastKeepalive > 3000) {
    lastKeepalive = now;
    if (!sseWrite(": ka\n\n", 6)) return;  // write fail = client gone
  }

  uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
  SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);

  uint32_t fc = sharedState.leds.frameCount;
  if (fc != lastLedFrame) {
    lastLedFrame = fc;
    sseSendAllHex();
    if (!sseReady) return;
  }

  uint32_t sv = sharedState.game.stateVersion;
  if (sv != lastGameVer) {
    lastGameVer = sv;
    sseSendGameState();
  }
}

// -----------------------------------------------------------------------------
// URL decode helpers
// -----------------------------------------------------------------------------
static char _urlDecodeChar(const char*& src) {
  if (*src == '%' && src[1] && src[2]) {
    char hex[3] = { src[1], src[2], 0 };
    src += 3;
    return (char)strtol(hex, nullptr, 16);
  }
  if (*src == '+') { src++; return ' '; }
  return *src++;
}

static void urlDecode(const char* src, char* dst, size_t dstLen) {
  size_t i = 0;
  while (*src && i < dstLen - 1) dst[i++] = _urlDecodeChar(src);
  dst[i] = 0;
}

// Forward declarations for settings helpers
void serveSettings();
void serveGetSettings();
void parseSaveSettings(const String& query);

// -----------------------------------------------------------------------------
// HTTP server — handles all endpoints including SSE and commands
// -----------------------------------------------------------------------------
void handleHTTPClient() {
  WiFiClient newClient = httpServer.available();
  if (!newClient) return;

  // Nina accepts the TCP connection before HTTP bytes arrive — wait up to 30ms.
  uint32_t _t = millis();
  while (!newClient.available() && millis() - _t < 30) {}

  newClient.setTimeout(50);
  String req = newClient.readStringUntil('\n'); req.trim();
  if (req.length() == 0) { newClient.stop(); return; }  // nothing arrived, discard
  while (newClient.available()) {
    String line = newClient.readStringUntil('\n'); line.trim();
    if (line.length() == 0 || line == "\r") break;
  }

  // --- SSE stream: keep connection alive, push events every BROADCAST_MS ---
  if (req.startsWith("GET /events")) {
    sseClient.stop();  // always close any existing SSE socket (connected() unreliable on Nina)
    newClient.println(F("HTTP/1.1 200 OK"));
    newClient.println(F("Content-Type: text/event-stream"));
    newClient.println(F("Cache-Control: no-cache"));
    newClient.println(F("Connection: keep-alive"));
    newClient.println(F("retry: 500"));  // browser reconnects in 500ms if stream drops
    newClient.println();   // blank line ends headers
    sseClient = newClient;
    sseReady  = true;
    // Do NOT push state immediately — let broadcastNewState() handle it within broadcastMs.
    // Pushing right after headers fills Nina's ~512B per-socket buffer → first broadcast fails.
    if (rtCfg.debugSerial) Serial.println(F("[M7] SSE: client connected"));
    return;                // do NOT stop — stays alive for events

  // --- Command endpoint: browser sends commands to M4 ---
  } else if (req.startsWith("GET /cmd?msg=")) {
    int sp = req.indexOf(' ', 13);
    String enc = (sp < 0) ? req.substring(13) : req.substring(13, sp);
    char msg[128];
    urlDecode(enc.c_str(), msg, sizeof(msg));
    parseCommand(msg);
    newClient.println(F("HTTP/1.1 204 No Content\r\nConnection: close\r\n"));
    newClient.stop();

  // --- All other requests: use pendingClient for settings helpers ---
  } else {
    pendingClient = newClient;

    if (req.startsWith("GET /getsettings")) {
      serveGetSettings();

    } else if (req.startsWith("GET /savesettings?")) {
      int end = req.indexOf(' ', 18);
      String query = (end < 0) ? req.substring(18) : req.substring(18, end);
      parseSaveSettings(query);
      pendingClient.println(F("HTTP/1.1 204 No Content\r\nConnection: close\r\n"));

    } else if (req.startsWith("GET /reboot")) {
      pendingClient.println(F("HTTP/1.1 204 No Content\r\nConnection: close\r\n"));
      pendingClient.stop();
      delay(100);
      NVIC_SystemReset();
      return;

    } else if (req.startsWith("GET /settings")) {
      serveSettings();

    } else if (req.startsWith("GET")) {
      pendingClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-cache, no-store\r\nConnection: close\r\n"));
      const char* p = WEB_PAGE;
      size_t remaining = strlen(WEB_PAGE);
      while (remaining > 0) {
        size_t chunk = (remaining > 1024) ? 1024 : remaining;
        pendingClient.write((const uint8_t*)p, chunk);
        p         += chunk;
        remaining -= chunk;
      }

    } else {
      pendingClient.println(F("HTTP/1.1 204 No Content\r\nConnection: close\r\n"));
    }
    pendingClient.stop();
  }
}

// -----------------------------------------------------------------------------
// Settings page helpers
// -----------------------------------------------------------------------------
void serveSettings() {
  pendingClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"));
  const char* p = SETTINGS_PAGE;
  size_t remaining = strlen(SETTINGS_PAGE);
  while (remaining > 0) {
    size_t chunk = (remaining > 1024) ? 1024 : remaining;
    pendingClient.write((const uint8_t*)p, chunk);
    p         += chunk;
    remaining -= chunk;
  }
  pendingClient.stop();
}

void serveGetSettings() {
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"homeSSID\":\"%s\",\"homePass\":\"%s\","
    "\"apSSID\":\"%s\",\"apPass\":\"%s\","
    "\"homeTimeoutMs\":%lu,"
    "\"defaultBrightness\":%d,\"maxBrightness\":%d,"
    "\"ledUpdateMs\":%d,\"broadcastMs\":%d,\"sideGap\":%d,"
    "\"simulateHardware\":%s,\"debugSerial\":%s,"
    "\"debugWeb\":%s,\"debugLed\":%s,\"debugKeyboard\":%s}",
    rtCfg.homeSSID, rtCfg.homePass,
    rtCfg.apSSID,   rtCfg.apPass,
    (unsigned long)rtCfg.homeTimeoutMs,
    rtCfg.defaultBrightness, rtCfg.maxBrightness,
    rtCfg.ledUpdateMs, rtCfg.broadcastMs, rtCfg.sideGap,
    rtCfg.simulateHardware ? "true":"false",
    rtCfg.debugSerial      ? "true":"false",
    rtCfg.debugWeb         ? "true":"false",
    rtCfg.debugLed         ? "true":"false",
    rtCfg.debugKeyboard    ? "true":"false");

  pendingClient.println(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"));
  pendingClient.print(buf);
  pendingClient.stop();
}

void parseSaveSettings(const String& query) {
  int pos = 0;
  while (pos < (int)query.length()) {
    int eqPos  = query.indexOf('=', pos);   if (eqPos  < 0) break;
    int ampPos = query.indexOf('&', eqPos+1); if (ampPos < 0) ampPos = query.length();
    String key = query.substring(pos, eqPos);
    String val = query.substring(eqPos+1, ampPos);
    char decoded[128]; urlDecode(val.c_str(), decoded, sizeof(decoded));

    if      (key == "homeSSID")          strncpy(rtCfg.homeSSID, decoded, sizeof(rtCfg.homeSSID)-1);
    else if (key == "homePass")          strncpy(rtCfg.homePass, decoded, sizeof(rtCfg.homePass)-1);
    else if (key == "apSSID")            strncpy(rtCfg.apSSID,   decoded, sizeof(rtCfg.apSSID)-1);
    else if (key == "apPass")            strncpy(rtCfg.apPass,   decoded, sizeof(rtCfg.apPass)-1);
    else if (key == "homeTimeoutMs")     rtCfg.homeTimeoutMs     = (uint32_t)atol(decoded);
    else if (key == "defaultBrightness") rtCfg.defaultBrightness = (uint8_t)constrain(atoi(decoded),0,255);
    else if (key == "maxBrightness")     rtCfg.maxBrightness     = (uint8_t)constrain(atoi(decoded),0,255);
    else if (key == "ledUpdateMs")       rtCfg.ledUpdateMs       = (uint16_t)constrain(atoi(decoded),1,1000);
    else if (key == "broadcastMs")       rtCfg.broadcastMs       = (uint16_t)constrain(atoi(decoded),50,5000);
    else if (key == "sideGap")           rtCfg.sideGap           = (uint8_t)constrain(atoi(decoded),0,20);
    else if (key == "simulateHardware")  rtCfg.simulateHardware  = (decoded[0]=='1');
    else if (key == "debugSerial")       rtCfg.debugSerial       = (decoded[0]=='1');
    else if (key == "debugWeb")          rtCfg.debugWeb          = (decoded[0]=='1');
    else if (key == "debugLed")          rtCfg.debugLed          = (decoded[0]=='1');
    else if (key == "debugKeyboard")     rtCfg.debugKeyboard     = (decoded[0]=='1');

    pos = ampPos + 1;
  }

  Cmd c = {}; c.type = CMD_BRIGHTNESS; c.arg0 = rtCfg.defaultBrightness;
  enqueueCmd(c);

  if (rtCfg.debugSerial) Serial.println(F("[M7] Settings: updated"));
}

// -----------------------------------------------------------------------------
// initNetwork() — wait for M4, connect WiFi, start HTTP server
// -----------------------------------------------------------------------------
void initNetwork() {
  Serial.println(F("[M7] Waiting for M4..."));
  uint32_t waitStart = millis();
  uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
  while (millis() - waitStart < 8000) {
    SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);
    if (sharedState.m4Ready) break;
    delay(100);
  }
  if (sharedState.m4Ready) Serial.println(F("[M7] M4 ready"));
  else                     Serial.println(F("[M7] M4 timeout — continuing"));

  bool stationOK = false;
  if (strlen(rtCfg.homeSSID) > 0) {
    Serial.print(F("[M7] WiFi: connecting to '")); Serial.print(rtCfg.homeSSID); Serial.println(F("'..."));
    WiFi.begin(rtCfg.homeSSID, rtCfg.homePass);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis()-t < rtCfg.homeTimeoutMs) delay(200);
    if (WiFi.status() == WL_CONNECTED) {
      stationOK = networkReady = true;
      Serial.print(F("[M7] WiFi: http://")); Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("[M7] WiFi: home failed, starting AP"));
      WiFi.disconnect(); WiFi.end(); delay(1000);
    }
  }
  if (!stationOK) {
    WiFi.beginAP(rtCfg.apSSID, rtCfg.apPass);
    uint32_t t = millis();
    while (WiFi.status() != WL_AP_LISTENING && millis()-t < 5000) delay(100);
    if (WiFi.status() == WL_AP_LISTENING) {
      networkReady = true;
      Serial.print(F("[M7] AP: http://")); Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("[M7] AP start FAILED"));
    }
  }

  httpServer.begin();
  Serial.println(F("[M7] =============================="));
  Serial.println(F("[M7] HTTP server ONLINE — navigate to the IP above"));
  Serial.println(F("[M7] =============================="));
}

// -----------------------------------------------------------------------------
// handleNetwork() — call every loop()
// -----------------------------------------------------------------------------
void handleNetwork() {
  if (!networkReady) return;

  // Rate-limit httpServer.available() — it's a SPI call to Nina.
  // Calling it every loop() at bare-metal speed saturates the SPI bus and
  // causes rhythmic hangs as Nina falls behind processing TCP ACKs.
  // 10ms = 100 checks/second — plenty for a responsive web UI.
  static uint32_t lastHTTPCheck = 0;
  uint32_t now = millis();
  if (now - lastHTTPCheck >= 10) {
    lastHTTPCheck = now;
    handleHTTPClient();
  }

  broadcastNewState();
}
