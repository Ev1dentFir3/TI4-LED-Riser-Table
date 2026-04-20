#pragma once
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "runtime_settings.h"
#include "web_interface.h"
#include "settings_page.h"
#include "led_control.h"

// =============================================================================
// TI4 Hex Riser - Network: WiFi + ESPAsyncWebServer
// Target: ESP32-DOWP-V3
// =============================================================================
// ESPAsyncWebServer handles all HTTP in the background on Core 0 via lwIP.
// No polling in loop() — handleNetwork() is a no-op kept for compatibility.
//
// Routes:
//   GET /              -> web page
//   GET /poll          -> current LED state (366 bytes hex, polled every 100ms)
//   GET /cmd?...       -> command from browser (SELECT, BRIGHTNESS, EFFECT, etc.)
//   GET /getsettings   -> current config as JSON
//   GET /savesettings? -> update runtime config
//   GET /reboot        -> restart ESP32
//   GET /settings      -> settings page HTML
//
// Broadcast functions are no-ops — browser gets LED state on next /poll.
// They are kept so game_state.h compiles without changes.
// =============================================================================

static AsyncWebServer _server(HTTP_PORT);
static bool networkReady = false;

// Forward declarations
void broadcastAllHexColors();
void broadcastHexUpdate(int hex, uint8_t r, uint8_t g, uint8_t b);
void broadcastHexSideUpdate(int hex, int side, uint8_t r, uint8_t g, uint8_t b);
void broadcastAll(uint8_t r, uint8_t g, uint8_t b);
void broadcastStatus(uint8_t phase, uint8_t player);
void parseWSCommand(const char* msg);
void parseSaveSettings(const String& query);

// onHexSelected defined in TI4_HexRiser.ino
void onHexSelected(int hexIdx);

// -----------------------------------------------------------------------------
// initNetwork()
// -----------------------------------------------------------------------------
void initNetwork() {
  // ------------------------------------------------------------------
  // 1. Try home network (station mode)
  // ------------------------------------------------------------------
  bool stationOK = false;

  if (strlen(rtCfg.homeSSID) > 0) {
    if (rtCfg.debugSerial) {
      Serial.print(F("WiFi: connecting to '"));
      Serial.print(rtCfg.homeSSID);
      Serial.println(F("'..."));
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(rtCfg.homeSSID, rtCfg.homePass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < rtCfg.homeTimeoutMs) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      stationOK    = true;
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: connected to '"));
        Serial.print(rtCfg.homeSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.localIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: home network unavailable, falling back to AP"));
      WiFi.disconnect(true);
      delay(500);
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

    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(rtCfg.apSSID, rtCfg.apPass)) {
      networkReady = true;
      if (rtCfg.debugSerial) {
        Serial.print(F("WiFi: AP ready — connect to '"));
        Serial.print(rtCfg.apSSID);
        Serial.print(F("' -> http://"));
        Serial.println(WiFi.softAPIP());
      }
    } else {
      if (rtCfg.debugSerial) Serial.println(F("WiFi: AP start FAILED"));
    }
  }

  // ------------------------------------------------------------------
  // 3. Register routes
  // ------------------------------------------------------------------

  // Root — serve web UI
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, F("text/html"), WEB_PAGE);
  });

  // Poll — browser fetches current LED colors every 100ms
  _server.on("/poll", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[370];
    int  pos = 0;
    for (int i = 0; i < NUM_HEXES; i++) {
      int idx = HEX_MAP[i][0][0];
      if (idx < 0 || idx >= NUM_LEDS) idx = i * LEDS_PER_HEX;
      CRGB color = leds[idx];
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X%02X", color.r, color.g, color.b);
    }
    request->send(200, "text/plain", buf);
  });

  // Cmd — browser sends commands (SELECT, BRIGHTNESS, EFFECT, etc.)
  // ESP32Async parses the query string into params; for our colon-delimited
  // format (e.g. SELECT:5) with no '=', the whole token becomes param name[0].
  _server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->params() > 0) {
      String cmd = request->getParam(0)->name();
      if (rtCfg.debugWeb) { Serial.print(F("CMD: ")); Serial.println(cmd); }
      parseWSCommand(cmd.c_str());
    }
    request->send(204);
  });

  // GetSettings — return current rtCfg as JSON
  _server.on("/getsettings", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    request->send(200, "application/json", buf);
  });

  // SaveSettings — rebuild query string from decoded params, pass to parser
  _server.on("/savesettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    String qs = "";
    for (size_t i = 0; i < request->params(); i++) {
      if (i > 0) qs += "&";
      qs += request->getParam(i)->name();
      qs += "=";
      qs += request->getParam(i)->value();
    }
    parseSaveSettings(qs);
    request->send(204);
  });

  // Reboot
  _server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
    delay(100);
    ESP.restart();
  });

  // Players — current player list with colors; used by web UI to build swatches
  _server.on("/players", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[512];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "{\"phase\":%d,\"players\":[", (int)gameState.currentPhase);
    bool first = true;
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (!players[i].active) continue;
      if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
      first = false;
      pos += snprintf(buf + pos, sizeof(buf) - pos,
                      "{\"idx\":%d,\"color\":\"%06lX\",\"locked\":%s}",
                      i,
                      (unsigned long)players[i].colorHex,
                      players[i].colorLocked ? "true" : "false");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");
    request->send(200, "application/json", buf);
  });

  // Settings page
  _server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", SETTINGS_PAGE);
  });

  _server.begin();

  if (rtCfg.debugSerial) {
    Serial.print(F("HTTP: AsyncWebServer started on port "));
    Serial.println(HTTP_PORT);
  }
}

// -----------------------------------------------------------------------------
// handleNetwork() — no-op with AsyncWebServer
// Kept for compatibility with loop() and animDelay() in animations.h.
// -----------------------------------------------------------------------------
void handleNetwork() {}

// -----------------------------------------------------------------------------
// Broadcast functions — no-ops with polling transport.
// Browser gets LED state on next /poll. Kept for game_state.h compatibility.
// -----------------------------------------------------------------------------
void broadcastAllHexColors() {}
void broadcastHexUpdate(int hex, uint8_t r, uint8_t g, uint8_t b) {}
void broadcastHexSideUpdate(int hex, int side, uint8_t r, uint8_t g, uint8_t b) {}
void broadcastAll(uint8_t r, uint8_t g, uint8_t b) {}
void broadcastStatus(uint8_t phase, uint8_t player) {}

// -----------------------------------------------------------------------------
// parseWSCommand — parse browser command string (unchanged from legacy)
// -----------------------------------------------------------------------------
void parseWSCommand(const char* msg) {
  if (strncmp(msg, "SELECT:", 7) == 0) {
    int hex = atoi(msg + 7);
    if (rtCfg.debugSerial) { Serial.print(F("CMD: SELECT hex ")); Serial.println(hex); }
    onHexSelected(hex);

  } else if (strncmp(msg, "BRIGHTNESS:", 11) == 0) {
    int b = atoi(msg + 11);
    setBrightness((uint8_t)constrain(b, 0, rtCfg.maxBrightness));

  } else if (strncmp(msg, "EFFECT:", 7) == 0) {
    const char* name = msg + 7;
    if      (strcmp(name, "RAINBOW") == 0) startEffect(ANIM_RAINBOW);
    else if (strcmp(name, "PULSE")   == 0) startEffect(ANIM_PULSE);
    else if (strcmp(name, "RIPPLE")  == 0) startEffect(ANIM_RIPPLE);
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
      CRGB color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      setHexColor(hex, color);
    }

  } else if (strncmp(msg, "SETHEXSIDE:", 11) == 0) {
    int hex = atoi(msg + 11);
    const char* sidePtr = strchr(msg + 11, ':');
    if (sidePtr) {
      sidePtr++;
      int side = atoi(sidePtr);
      const char* colorStr = strchr(sidePtr, ':');
      if (colorStr) {
        colorStr++;
        uint32_t rgb = strtoul(colorStr, nullptr, 16);
        CRGB color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        setHexSideColor(hex, side, color);
      }
    }

  } else if (strncmp(msg, "ALL:", 4) == 0) {
    uint32_t rgb = strtoul(msg + 4, nullptr, 16);
    setAllHexes(CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF));

  } else if (strncmp(msg, "BATTLE:", 7) == 0) {
    // BATTLE:attackerIdx:defenderIdx  (0-based player indices)
    int attacker = atoi(msg + 7);
    const char* defPtr = strchr(msg + 7, ':');
    if (defPtr) {
      int defender = atoi(defPtr + 1);
      if (attacker >= 0 && attacker < MAX_PLAYERS && defender >= 0 && defender < MAX_PLAYERS
          && attacker != defender && players[attacker].active && players[defender].active) {
        battlePending = false;
        startBattle((uint8_t)attacker, (uint8_t)defender);
      }
    }

  } else if (strcmp(msg, "ENDBATTLE") == 0) {
    endBattle();
    battlePending = false;

  } else if (strncmp(msg, "CLAIMHEX:", 9) == 0) {
    // CLAIMHEX:hexIdx:playerIdx  (playerIdx 255 = unclaim)
    int hexIdx = atoi(msg + 9);
    const char* pPtr = strchr(msg + 9, ':');
    if (pPtr && hexIdx >= 0 && hexIdx < NUM_HEXES) {
      int pIdx = atoi(pPtr + 1);
      if (pIdx < 0 || pIdx >= MAX_PLAYERS || !players[pIdx].active) {
        // unclaim
        hexOwner[hexIdx] = -1;
        setHexColor(hexIdx, CRGB::Black);
      } else {
        hexOwner[hexIdx] = (int8_t)pIdx;
        CRGB c;
        c.r = (players[pIdx].colorHex >> 16) & 0xFF;
        c.g = (players[pIdx].colorHex >>  8) & 0xFF;
        c.b =  players[pIdx].colorHex        & 0xFF;
        setHexColor(hexIdx, c);
      }
      if (rtCfg.debugSerial) {
        Serial.print(F("CMD: CLAIMHEX ")); Serial.print(hexIdx);
        Serial.print(F(" → player ")); Serial.println(pIdx);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// URL decode helpers
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
// parseSaveSettings — update rtCfg from /savesettings query string
// -----------------------------------------------------------------------------
void parseSaveSettings(const String& query) {
  int pos = 0;
  while (pos < (int)query.length()) {
    int eqPos  = query.indexOf('=', pos);
    if (eqPos < 0) break;
    int ampPos = query.indexOf('&', eqPos + 1);
    if (ampPos < 0) ampPos = query.length();

    String key = query.substring(pos, eqPos);
    String val = query.substring(eqPos + 1, ampPos);

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

    pos = ampPos + 1;
  }

  FastLED.setBrightness(rtCfg.defaultBrightness);
  if (rtCfg.debugSerial) Serial.println(F("Settings: updated OK"));
}
