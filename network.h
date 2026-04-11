#pragma once
#include <WiFi.h>
#include "config.h"
#include "runtime_settings.h"
#include "web_interface.h"
#include "settings_page.h"
#include "led_control.h"

// =============================================================================
// TI4 Hex Riser - Network: WiFi + HTTP server + Server-Sent Events (SSE)
// =============================================================================
// No extra library needed — uses only the standard WiFi.h.
//
// HTTP server on port 80 handles three routes:
//   GET /        → serves the web page
//   GET /events  → SSE stream  (server → browser, persistent connection)
//   GET /cmd?... → commands    (browser → server, short-lived request)
//
// initNetwork()   — call once in setup()
// handleNetwork() — call every loop()  (non-blocking)
// broadcastHexUpdate / broadcastAll / etc. — push LED state to browser
// =============================================================================

static WiFiServer httpServer(HTTP_PORT);
static WiFiClient sseClient;          // persistent SSE connection
static WiFiClient pendingClient;      // current incoming HTTP request
static bool       sseActive     = false;
static bool       networkReady  = false;
static uint32_t   lastBroadcast = 0;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
void sseSend(const char* data);
void broadcastAllHexColors();
void broadcastHexUpdate(int hex, uint8_t r, uint8_t g, uint8_t b);
void broadcastHexSideUpdate(int hex, int side, uint8_t r, uint8_t g, uint8_t b);
void broadcastAll(uint8_t r, uint8_t g, uint8_t b);
void broadcastStatus(uint8_t phase, uint8_t player);
void handleHTTPClient();
void parseWSCommand(const char* msg);
void serveSettings();
void serveGetSettings();
void parseSaveSettings(const String& query);
void handleReboot();
void servePoll();

// -----------------------------------------------------------------------------
// initNetwork()
// -----------------------------------------------------------------------------
void initNetwork() {
  // ------------------------------------------------------------------
  // 1. Try home network (station mode) if an SSID is configured
  // ------------------------------------------------------------------
  bool stationOK = false;

  if (strlen(rtCfg.homeSSID) > 0) {
    if (rtCfg.debugSerial) {
      Serial.print(F("WiFi: connecting to '"));
      Serial.print(rtCfg.homeSSID);
      Serial.println(F("'..."));
    }

    WiFi.begin(rtCfg.homeSSID, rtCfg.homePass);

    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < rtCfg.homeTimeoutMs) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      stationOK = true;
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: connected to '"));
        Serial.print(rtCfg.homeSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.localIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: home network unavailable, falling back to AP"));
      WiFi.disconnect();
      WiFi.end();
      delay(1000);
    }
  }

  // ------------------------------------------------------------------
  // 2. Fall back to Access Point mode
  // ------------------------------------------------------------------
  if (!stationOK) {
    if (rtCfg.debugSerial) {
      Serial.print(F("WiFi: starting AP '"));
      Serial.print(rtCfg.apSSID);
      Serial.println(F("'"));
    }

    WiFi.beginAP(rtCfg.apSSID, rtCfg.apPass);

    uint32_t t = millis();
    while (WiFi.status() != WL_AP_LISTENING && millis() - t < 5000) {
      delay(100);
    }

    if (WiFi.status() == WL_AP_LISTENING) {
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: AP ready — connect to '"));
        Serial.print(rtCfg.apSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.localIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: AP start FAILED"));
    }
  }

  // ------------------------------------------------------------------
  // 3. Start HTTP server
  // ------------------------------------------------------------------
  httpServer.begin();

  if (rtCfg.debugSerial) {
    Serial.print(F("HTTP: port "));
    Serial.println(HTTP_PORT);
    Serial.println(F("SSE:  /events  |  CMD: /cmd?...  |  Settings: /settings"));
  }
}

// -----------------------------------------------------------------------------
// handleNetwork() — call every loop()
// -----------------------------------------------------------------------------
void handleNetwork() {
  if (!networkReady) return;

  // Detect SSE client disconnect
  if (sseActive && !sseClient.connected()) {
    sseActive = false;
    sseClient.stop();
    if (rtCfg.debugSerial) Serial.println(F("SSE: client disconnected"));
  }

  // Broadcast full LED state to browser at broadcastMs interval
  if (sseActive) {
    uint32_t now = millis();
    if (now - lastBroadcast >= rtCfg.broadcastMs) {
      lastBroadcast = now;
      broadcastAllHexColors();
    }
  }

  // Accept new HTTP connection
  pendingClient = httpServer.available();
  if (pendingClient) {
    handleHTTPClient();
  }
}

// -----------------------------------------------------------------------------
// SSE push — send one event to the browser
// -----------------------------------------------------------------------------
void sseSend(const char* data) {
  if (!sseActive) return;
  if (!sseClient.connected()) { sseActive = false; return; }
  sseClient.print(F("data: "));
  sseClient.println(data);   // "data: <msg>\r\n"
  sseClient.println();       // "\r\n"  — blank line terminates the SSE event
}

// -----------------------------------------------------------------------------
// HTTP request handler — routes GET / | /events | /cmd?...
// Uses global pendingClient so SSE assignment survives scope changes.
// -----------------------------------------------------------------------------
void handleHTTPClient() {
  if (!pendingClient.connected()) return;

  // Read request line
  String req = pendingClient.readStringUntil('\n');
  req.trim();

  // Drain remaining headers
  while (pendingClient.connected() && pendingClient.available()) {
    String line = pendingClient.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line == "\r") break;
  }

  // --- Route: SSE stream ---
  if (req.startsWith("GET /events")) {
    if (sseActive) sseClient.stop();          // replace any existing SSE client

    pendingClient.println(F("HTTP/1.1 200 OK"));
    pendingClient.println(F("Content-Type: text/event-stream"));
    pendingClient.println(F("Cache-Control: no-cache"));
    pendingClient.println(F("Connection: keep-alive"));
    pendingClient.println();

    sseClient = pendingClient;                // copy handle — keeps socket alive
    sseActive = true;

    if (rtCfg.debugSerial) Serial.println(F("SSE: client connected"));

    // Push initial brightness
    char buf[24];
    snprintf(buf, sizeof(buf), "BRIGHTNESS:%d", FastLED.getBrightness());
    sseSend(buf);

    return;   // do NOT stop — connection must stay open

  // --- Route: command ---
  } else if (req.startsWith("GET /cmd?")) {
    int end = req.indexOf(' ', 9);
    String cmd = (end < 0) ? req.substring(9) : req.substring(9, end);
    if (rtCfg.debugWeb) { Serial.print(F("CMD: ")); Serial.println(cmd); }
    parseWSCommand(cmd.c_str());

    pendingClient.println(F("HTTP/1.1 204 No Content"));
    pendingClient.println(F("Connection: close"));
    pendingClient.println();

  // --- Route: get current settings as JSON ---
  } else if (req.startsWith("GET /getsettings")) {
    serveGetSettings();
    return;

  // --- Route: save settings ---
  } else if (req.startsWith("GET /savesettings?")) {
    int end = req.indexOf(' ', 18);
    String query = (end < 0) ? req.substring(18) : req.substring(18, end);
    parseSaveSettings(query);

    pendingClient.println(F("HTTP/1.1 204 No Content"));
    pendingClient.println(F("Connection: close"));
    pendingClient.println();

  // --- Route: reboot ---
  } else if (req.startsWith("GET /reboot")) {
    pendingClient.println(F("HTTP/1.1 204 No Content"));
    pendingClient.println(F("Connection: close"));
    pendingClient.println();
    delay(50);
    pendingClient.stop();
    delay(100);
    NVIC_SystemReset();
    return;

  // --- Route: settings page ---
  } else if (req.startsWith("GET /settings")) {
    serveSettings();
    return;

  // --- Route: poll — browser requests current LED state every 100 ms ---
  } else if (req.startsWith("GET /poll")) {
    servePoll();
    return;

  // --- Route: web page ---
  } else if (req.startsWith("GET")) {
    pendingClient.println(F("HTTP/1.1 200 OK"));
    pendingClient.println(F("Content-Type: text/html; charset=utf-8"));
    pendingClient.println(F("Connection: close"));
    pendingClient.println();
    pendingClient.print(WEB_PAGE);

  } else {
    pendingClient.println(F("HTTP/1.1 204 No Content"));
    pendingClient.println(F("Connection: close"));
    pendingClient.println();
  }

  delay(2);
  pendingClient.stop();
}

// -----------------------------------------------------------------------------
// Parse incoming commands  (same protocol as before)
// -----------------------------------------------------------------------------
void parseWSCommand(const char* msg) {
  if (strncmp(msg, "SELECT:", 7) == 0) {
    int hex = atoi(msg + 7);
    if (rtCfg.debugSerial) { Serial.print(F("CMD: SELECT hex ")); Serial.println(hex); }
    extern void onHexSelected(int hex);
    onHexSelected(hex);

  } else if (strncmp(msg, "BRIGHTNESS:", 11) == 0) {
    int b = atoi(msg + 11);
    setBrightness((uint8_t)constrain(b, 0, rtCfg.maxBrightness));

  } else if (strncmp(msg, "EFFECT:", 7) == 0) {
    const char* name = msg + 7;
    if      (strcmp(name, "RAINBOW") == 0) startEffect(ANIM_RAINBOW);
    else if (strcmp(name, "PULSE")   == 0) startEffect(ANIM_PULSE);
    else if (strcmp(name, "SPIRAL")  == 0) startEffect(ANIM_SPIRAL);
    else if (strcmp(name, "SPARKLE") == 0) startEffect(ANIM_SPARKLE);
    else if (strcmp(name, "WAVE")    == 0) startEffect(ANIM_WAVE);
    else if (strcmp(name, "NONE")    == 0) stopEffect();

  } else if (strncmp(msg, "PLAYER:", 7) == 0) {
    int player = atoi(msg + 7);
    const char* colorStr = strchr(msg + 7, ':');
    if (colorStr && player >= 0 && player < MAX_PLAYERS) {
      colorStr++;
      uint32_t rgb = strtoul(colorStr, nullptr, 16);
      broadcastAll((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

  } else if (strncmp(msg, "SETHEX:", 7) == 0) {
    int hex = atoi(msg + 7);
    const char* colorStr = strchr(msg + 7, ':');
    if (colorStr) {
      colorStr++;
      uint32_t rgb = strtoul(colorStr, nullptr, 16);
      CRGB c((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      setHexColor(hex, c);
      broadcastHexUpdate(hex, c.r, c.g, c.b);
    }

  } else if (strncmp(msg, "SETHEXSIDE:", 11) == 0) {
    int hex = atoi(msg + 11);
    const char* p = strchr(msg + 11, ':');
    if (p) {
      p++;
      int side = atoi(p);
      const char* colorStr = strchr(p, ':');
      if (colorStr) {
        colorStr++;
        uint32_t rgb = strtoul(colorStr, nullptr, 16);
        CRGB c((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        setHexSideColor(hex, side, c);
        broadcastHexSideUpdate(hex, side, c.r, c.g, c.b);
      }
    }

  } else if (strncmp(msg, "ALL:", 4) == 0) {
    uint32_t rgb = strtoul(msg + 4, nullptr, 16);
    setAllHexes(CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));
  }
}

// -----------------------------------------------------------------------------
// Broadcast full LED state — samples first valid LED per hex from leds[]
// so it captures live animation state, not just hexColor[].
// Format: ALLHEX:<RRGGBB × 61>  (~373 chars, sent at 10 fps)
// -----------------------------------------------------------------------------
void broadcastAllHexColors() {
  if (!sseActive) return;
  char buf[400];
  int  pos = snprintf(buf, sizeof(buf), "ALLHEX:");
  for (int i = 0; i < NUM_HEXES; i++) {
    int idx = HEX_MAP[i][0][0];          // side 0, slot 0 is always a valid LED
    if (idx < 0 || idx >= NUM_LEDS) idx = i * LEDS_PER_HEX;  // safety fallback
    CRGB c = leds[idx];
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X%02X", c.r, c.g, c.b);
  }
  sseSend(buf);
}

// -----------------------------------------------------------------------------
// Broadcast helpers — push state to browser via SSE
// -----------------------------------------------------------------------------
void broadcastHexUpdate(int hex, uint8_t r, uint8_t g, uint8_t b) {
  char buf[24];
  snprintf(buf, sizeof(buf), "HEX:%d:%02X%02X%02X", hex, r, g, b);
  sseSend(buf);
  if (rtCfg.debugWeb) Serial.println(buf);
}

void broadcastAll(uint8_t r, uint8_t g, uint8_t b) {
  char buf[16];
  snprintf(buf, sizeof(buf), "ALL:%02X%02X%02X", r, g, b);
  sseSend(buf);
}

void broadcastHexSideUpdate(int hex, int side, uint8_t r, uint8_t g, uint8_t b) {
  char buf[32];
  snprintf(buf, sizeof(buf), "HEXSIDE:%d:%d:%02X%02X%02X", hex, side, r, g, b);
  sseSend(buf);
}

void broadcastStatus(uint8_t phase, uint8_t player) {
  char buf[32];
  snprintf(buf, sizeof(buf), "STATUS:%d,%d", phase, player);
  sseSend(buf);
}

// -----------------------------------------------------------------------------
// Poll — browser requests current LED state (replaces SSE push)
// Returns 61×RRGGBB packed (366 bytes) as plain text, connection: close.
// Browser polls this every 100 ms; no persistent connection needed.
// -----------------------------------------------------------------------------
void servePoll() {
  char buf[370];
  int  pos = 0;
  for (int i = 0; i < NUM_HEXES; i++) {
    int idx = HEX_MAP[i][0][0];
    if (idx < 0 || idx >= NUM_LEDS) idx = i * LEDS_PER_HEX;
    CRGB c = leds[idx];
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X%02X", c.r, c.g, c.b);
  }
  pendingClient.println(F("HTTP/1.1 200 OK"));
  pendingClient.println(F("Content-Type: text/plain"));
  pendingClient.println(F("Cache-Control: no-store"));
  pendingClient.println(F("Connection: close"));
  pendingClient.println();
  pendingClient.print(buf);
  delay(2);
  pendingClient.stop();
}

// -----------------------------------------------------------------------------
// Settings page — serve HTML
// -----------------------------------------------------------------------------
void serveSettings() {
  pendingClient.println(F("HTTP/1.1 200 OK"));
  pendingClient.println(F("Content-Type: text/html; charset=utf-8"));
  pendingClient.println(F("Connection: close"));
  pendingClient.println();
  pendingClient.print(SETTINGS_PAGE);
  delay(2);
  pendingClient.stop();
}

// -----------------------------------------------------------------------------
// Settings GET — return current rtCfg as JSON
// -----------------------------------------------------------------------------
void serveGetSettings() {
  // Build JSON from rtCfg — keep it compact, no extra spaces
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"homeSSID\":\"%s\","
    "\"homePass\":\"%s\","
    "\"apSSID\":\"%s\","
    "\"apPass\":\"%s\","
    "\"homeTimeoutMs\":%lu,"
    "\"defaultBrightness\":%d,"
    "\"maxBrightness\":%d,"
    "\"ledUpdateMs\":%d,"
    "\"broadcastMs\":%d,"
    "\"sideGap\":%d,"
    "\"simulateHardware\":%s,"
    "\"debugSerial\":%s,"
    "\"debugWeb\":%s,"
    "\"debugLed\":%s,"
    "\"debugKeyboard\":%s}",
    rtCfg.homeSSID, rtCfg.homePass,
    rtCfg.apSSID,   rtCfg.apPass,
    (unsigned long)rtCfg.homeTimeoutMs,
    rtCfg.defaultBrightness, rtCfg.maxBrightness,
    rtCfg.ledUpdateMs, rtCfg.broadcastMs, rtCfg.sideGap,
    rtCfg.simulateHardware ? "true" : "false",
    rtCfg.debugSerial      ? "true" : "false",
    rtCfg.debugWeb         ? "true" : "false",
    rtCfg.debugLed         ? "true" : "false",
    rtCfg.debugKeyboard    ? "true" : "false"
  );

  pendingClient.println(F("HTTP/1.1 200 OK"));
  pendingClient.println(F("Content-Type: application/json"));
  pendingClient.println(F("Connection: close"));
  pendingClient.println();
  pendingClient.print(buf);
  delay(2);
  pendingClient.stop();
}

// -----------------------------------------------------------------------------
// URL-decode a single %XX sequence in-place
// Returns decoded char, advances src past the sequence.
// -----------------------------------------------------------------------------
static char urlDecodeChar(const char*& src) {
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
  while (*src && i < dstLen - 1) {
    dst[i++] = urlDecodeChar(src);
  }
  dst[i] = 0;
}

// -----------------------------------------------------------------------------
// Parse /savesettings? query string and update rtCfg
// -----------------------------------------------------------------------------
void parseSaveSettings(const String& query) {
  // Walk key=value pairs separated by '&'
  int pos = 0;
  while (pos < (int)query.length()) {
    int eq  = query.indexOf('=', pos);
    if (eq < 0) break;
    int amp = query.indexOf('&', eq + 1);
    if (amp < 0) amp = query.length();

    String key = query.substring(pos, eq);
    String val = query.substring(eq + 1, amp);

    char decoded[128];
    urlDecode(val.c_str(), decoded, sizeof(decoded));

    if      (key == "homeSSID")          strncpy(rtCfg.homeSSID,  decoded, sizeof(rtCfg.homeSSID)  - 1);
    else if (key == "homePass")          strncpy(rtCfg.homePass,  decoded, sizeof(rtCfg.homePass)  - 1);
    else if (key == "apSSID")            strncpy(rtCfg.apSSID,    decoded, sizeof(rtCfg.apSSID)    - 1);
    else if (key == "apPass")            strncpy(rtCfg.apPass,    decoded, sizeof(rtCfg.apPass)    - 1);
    else if (key == "homeTimeoutMs")     rtCfg.homeTimeoutMs     = (uint32_t)atol(decoded);
    else if (key == "defaultBrightness") rtCfg.defaultBrightness = (uint8_t)constrain(atoi(decoded), 0, 255);
    else if (key == "maxBrightness")     rtCfg.maxBrightness     = (uint8_t)constrain(atoi(decoded), 0, 255);
    else if (key == "ledUpdateMs")       rtCfg.ledUpdateMs       = (uint16_t)constrain(atoi(decoded), 1, 1000);
    else if (key == "broadcastMs")       rtCfg.broadcastMs       = (uint16_t)constrain(atoi(decoded), 50, 5000);
    else if (key == "sideGap")           rtCfg.sideGap           = (uint8_t)constrain(atoi(decoded), 0, 20);
    else if (key == "simulateHardware")  rtCfg.simulateHardware  = (decoded[0] == '1');
    else if (key == "debugSerial")       rtCfg.debugSerial       = (decoded[0] == '1');
    else if (key == "debugWeb")          rtCfg.debugWeb          = (decoded[0] == '1');
    else if (key == "debugLed")          rtCfg.debugLed          = (decoded[0] == '1');
    else if (key == "debugKeyboard")     rtCfg.debugKeyboard     = (decoded[0] == '1');

    pos = amp + 1;
  }

  // Apply immediate non-WiFi changes
  FastLED.setBrightness(rtCfg.defaultBrightness);

  if (rtCfg.debugSerial) Serial.println(F("Settings: updated OK"));
}
