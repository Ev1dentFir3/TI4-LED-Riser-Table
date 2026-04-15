#pragma once
#include <WiFi.h>
#include "shared_state.h"
#include "config.h"
#include "runtime_settings.h"
#include "web_interface.h"
#include "settings_page.h"

// =============================================================================
// TI4 Hex Riser - M7 Network (WebSocket + HTTP commands)
// =============================================================================
// Single HTTP server on port 80 handles all traffic.
// WebSocket upgrade on GET /ws — binary push from M7 to browser.
//
//   GET /               → main page (chunked, 1KB blocks)
//   GET /ws             → WebSocket upgrade (M7→browser binary push)
//   GET /cmd?msg=X      → command endpoint (browser→M7→CmdQueue→M4)
//   GET /settings       → settings page
//   GET /getsettings    → settings JSON
//   GET /savesettings?  → save settings
//   GET /reboot         → reboot
//
// WebSocket binary frame payloads (server→browser, no masking required):
//   Type 0x00  Keepalive     1 byte  [type]
//   Type 0x01  ALLHEX      184 bytes [type, R0,G0,B0, ..., R60,G60,B60]
//   Type 0x02  BRIGHTNESS    2 bytes [type, value]
//   Type 0x03  GAMESTATE    63 bytes [type, phase, speaker, numPlayers,
//                                     pick, action, flags, players×8×7]
//
// initNetwork()   — call once in setup()
// handleNetwork() — call every loop()
// =============================================================================

static WiFiServer httpServer(HTTP_PORT);
static WiFiClient pendingClient;
static WiFiClient wsClient;
static bool       wsReady     = false;
static bool       networkReady = false;
static uint32_t   lastLedFrame = 0;
static uint32_t   lastGameVer  = 0;

// =============================================================================
// SHA1 — specialized for exactly 60-byte input (24-char WS key + 36-char GUID)
// Pads to two 64-byte blocks: [data|0x80|zeros] and [zeros|bitlen@56]
// =============================================================================
static void ws_sha1_60(const uint8_t* in, uint8_t out[20]) {
  uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE,h3=0x10325476,h4=0xC3D2E1F0;
  uint32_t W[80];
  uint8_t  block[64];

  // --- Block 1: data[0..59] + 0x80 + zeros[61..63] ---
  memcpy(block, in, 60);
  block[60]=0x80; block[61]=block[62]=block[63]=0;
  for (int i=0;i<16;i++)
    W[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
         ((uint32_t)block[i*4+2]<<8)|block[i*4+3];
  for (int i=16;i<80;i++){uint32_t t=W[i-3]^W[i-8]^W[i-14]^W[i-16];W[i]=(t<<1)|(t>>31);}
  {
    uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
    for (int i=0;i<80;i++){
      uint32_t f,k;
      if     (i<20){f=(b&c)|(~b&d);       k=0x5A827999;}
      else if(i<40){f=b^c^d;              k=0x6ED9EBA1;}
      else if(i<60){f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC;}
      else         {f=b^c^d;              k=0xCA62C1D6;}
      uint32_t t=((a<<5)|(a>>27))+f+e+k+W[i];
      e=d; d=c; c=(b<<30)|(b>>2); b=a; a=t;
    }
    h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;
  }

  // --- Block 2: zeros + bit-length (480 = 0x1E0) at bytes [56..63] ---
  memset(block, 0, 64);
  block[62]=0x01; block[63]=0xE0;  // 60 bytes * 8 bits = 480 = 0x000001E0
  for (int i=0;i<16;i++)
    W[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
         ((uint32_t)block[i*4+2]<<8)|block[i*4+3];
  for (int i=16;i<80;i++){uint32_t t=W[i-3]^W[i-8]^W[i-14]^W[i-16];W[i]=(t<<1)|(t>>31);}
  {
    uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
    for (int i=0;i<80;i++){
      uint32_t f,k;
      if     (i<20){f=(b&c)|(~b&d);       k=0x5A827999;}
      else if(i<40){f=b^c^d;              k=0x6ED9EBA1;}
      else if(i<60){f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC;}
      else         {f=b^c^d;              k=0xCA62C1D6;}
      uint32_t t=((a<<5)|(a>>27))+f+e+k+W[i];
      e=d; d=c; c=(b<<30)|(b>>2); b=a; a=t;
    }
    h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;
  }

  uint32_t H[5]={h0,h1,h2,h3,h4};
  for (int i=0;i<5;i++){
    out[i*4]  =(H[i]>>24); out[i*4+1]=(H[i]>>16);
    out[i*4+2]=(H[i]>>8);  out[i*4+3]= H[i];
  }
}

// Base64 encoder — out must hold ceil(inLen/3)*4+1 bytes
static void ws_b64enc(const uint8_t* in, int inLen, char* out) {
  static const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int j=0;
  for (int i=0;i<inLen;i+=3){
    uint32_t b=(uint32_t)in[i]<<16;
    if(i+1<inLen) b|=(uint32_t)in[i+1]<<8;
    if(i+2<inLen) b|=in[i+2];
    out[j++]=B64[(b>>18)&0x3F];
    out[j++]=B64[(b>>12)&0x3F];
    out[j++]=(i+1<inLen)?B64[(b>>6)&0x3F]:'=';
    out[j++]=(i+2<inLen)?B64[b&0x3F]     :'=';
  }
  out[j]=0;
}

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
// WebSocket frame write helpers
// Tolerates one transient failure (Nina TCP buffer momentarily full) before
// treating the connection as dead and closing it.
// -----------------------------------------------------------------------------
static uint8_t _wsFailCount = 0;
static bool wsWrite(const uint8_t* data, int len) {
  if (wsClient.write(data, len) == 0) {
    if (++_wsFailCount >= 2) {
      _wsFailCount = 0;
      wsReady = false;
      wsClient.stop();
      if (rtCfg.debugSerial) Serial.println(F("[M7] WS: closing (write failed)"));
      return false;
    }
    return true;  // first failure may be transient — skip this frame
  }
  _wsFailCount = 0;
  return true;
}

// Type 0x00: keepalive — 3-byte frame [0x82, 0x01, 0x00]
static void wsSendKeepalive() {
  uint8_t buf[3] = {0x82, 0x01, 0x00};
  wsWrite(buf, 3);
}

// Type 0x01: ALLHEX — 188-byte frame (4-byte WS header + 184-byte payload)
// Header: FIN|bin(0x82), ext16(0x7E), len=184(0x00,0xB8)
static void wsSendAllHex() {
  uint8_t buf[188];
  buf[0]=0x82; buf[1]=0x7E; buf[2]=0x00; buf[3]=0xB8;
  buf[4]=0x01;  // payload type ALLHEX
  for (int i = 0; i < SS_NUM_HEXES; i++) {
    buf[5 + i*3]   = sharedState.leds.r[i];
    buf[5 + i*3+1] = sharedState.leds.g[i];
    buf[5 + i*3+2] = sharedState.leds.b[i];
  }
  wsWrite(buf, 188);
}

// Type 0x02: BRIGHTNESS — 4-byte frame [0x82, 0x02, 0x02, value]
static void wsSendBrightness() {
  uint8_t buf[4] = {0x82, 0x02, 0x02, sharedState.leds.brightness};
  wsWrite(buf, 4);
}

// Type 0x03: GAMESTATE — 65-byte frame (2-byte WS header + 63-byte payload)
// Payload layout: type(1) + phase,speaker,numPlayers,pick,action,flags(6) + players×8×7
static void wsSendGameState() {
  uint8_t buf[65];
  buf[0]=0x82; buf[1]=63;  // FIN|bin, len=63
  buf[2]=0x03;  // payload type GAMESTATE
  buf[3]=sharedState.game.currentPhase;
  buf[4]=sharedState.game.speakerIndex;
  buf[5]=sharedState.game.numActivePlayers;
  buf[6]=sharedState.game.currentPickIndex;
  buf[7]=sharedState.game.currentActionIndex;
  buf[8]=sharedState.game.inBattle ? 0x01 : 0x00;
  for (int i = 0; i < SS_MAX_PLAYERS; i++) {
    int b = 9 + i*7;
    buf[b  ] = (sharedState.game.players[i].colorHex >> 16) & 0xFF;
    buf[b+1] = (sharedState.game.players[i].colorHex >>  8) & 0xFF;
    buf[b+2] =  sharedState.game.players[i].colorHex        & 0xFF;
    buf[b+3] =  sharedState.game.players[i].strategyCard;
    buf[b+4] =  sharedState.game.players[i].homeHex;
    buf[b+5] =  sharedState.game.players[i].initiative;
    buf[b+6] = (sharedState.game.players[i].active       ? 0x01 : 0)
             | (sharedState.game.players[i].colorLocked  ? 0x02 : 0)
             | (sharedState.game.players[i].hasPassed    ? 0x04 : 0)
             | (sharedState.game.players[i].readyForNext ? 0x08 : 0);
  }
  wsWrite(buf, 65);
}

static void pushFullStateToWS() {
  uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
  SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);
  wsSendAllHex();     if (!wsReady) return;
  wsSendBrightness(); if (!wsReady) return;
  wsSendGameState();
}

// -----------------------------------------------------------------------------
// WebSocket handshake — completes the HTTP→WS upgrade
// Computes SHA1(key + GUID) and sends the 101 response.
// -----------------------------------------------------------------------------
static bool doWSHandshake(WiFiClient& client, const char* key) {
  if (strlen(key) != 24) return false;
  const char* GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t input[60];
  memcpy(input,    key,  24);
  memcpy(input+24, GUID, 36);
  uint8_t digest[20];
  ws_sha1_60(input, digest);
  char accept[32];
  ws_b64enc(digest, 20, accept);  // always 28 chars for 20-byte input
  client.print(F("HTTP/1.1 101 Switching Protocols\r\n"));
  client.print(F("Upgrade: websocket\r\n"));
  client.print(F("Connection: Upgrade\r\n"));
  client.print(F("Sec-WebSocket-Accept: "));
  client.print(accept);
  client.print(F("\r\n\r\n"));
  return true;
}

// -----------------------------------------------------------------------------
// Broadcast new frames at BROADCAST_MS rate — call every loop()
// -----------------------------------------------------------------------------
void broadcastNewState() {
  static uint32_t lastBroadcast = 0;
  static uint32_t lastKeepalive = 0;
  uint32_t now = millis();
  if (now - lastBroadcast < rtCfg.broadcastMs) return;
  lastBroadcast = now;

  if (!wsReady) return;

  // WS keepalive every 3s — prevents Nina TCP idle timeout and tickles the
  // browser's 6s heartbeat watchdog so it detects board disconnect quickly.
  if (now - lastKeepalive > 3000) {
    lastKeepalive = now;
    wsSendKeepalive();
    if (!wsReady) return;
  }

  uint32_t cacheSize = ((sizeof(SharedState) + 31) / 32) * 32;
  SCB_InvalidateDCache_by_Addr((uint32_t*)SHARED_STATE_BASE, cacheSize);

  uint32_t fc = sharedState.leds.frameCount;
  if (fc != lastLedFrame) {
    lastLedFrame = fc;
    wsSendAllHex();
    if (!wsReady) return;
  }

  uint32_t sv = sharedState.game.stateVersion;
  if (sv != lastGameVer) {
    lastGameVer = sv;
    wsSendGameState();
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
// HTTP server — handles all endpoints including WebSocket upgrade and commands
// -----------------------------------------------------------------------------
void handleHTTPClient() {
  WiFiClient newClient = httpServer.available();
  if (!newClient) return;

  // Nina accepts the TCP connection before HTTP bytes arrive — wait up to 30ms.
  uint32_t _t = millis();
  while (!newClient.available() && millis() - _t < 30) {}

  newClient.setTimeout(50);
  String req = newClient.readStringUntil('\n'); req.trim();
  if (req.length() == 0) { newClient.stop(); return; }

  // --- WebSocket upgrade: parse headers before draining ---
  if (req.startsWith("GET /ws")) {
    char wsKey[32] = {};
    bool isUpgrade = false;
    while (newClient.available()) {
      String line = newClient.readStringUntil('\n');
      line.trim();
      if (line.length() == 0 || line == "\r") break;
      // Case-insensitive header matching
      String lower = line; lower.toLowerCase();
      if (lower.startsWith("upgrade:") && lower.indexOf("websocket") >= 0)
        isUpgrade = true;
      if (lower.startsWith("sec-websocket-key:")) {
        // Key value is case-sensitive base64 — use original line
        String k = line.substring(18); k.trim();
        k.toCharArray(wsKey, sizeof(wsKey));
      }
    }
    if (!isUpgrade || wsKey[0] == 0) {
      newClient.println(F("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"));
      newClient.stop();
      return;
    }
    wsClient.stop();  // close any existing WS socket
    if (doWSHandshake(newClient, wsKey)) {
      wsClient = newClient;
      wsReady  = true;
      _wsFailCount = 0;
      if (rtCfg.debugSerial) Serial.println(F("[M7] WS: client connected"));
    } else {
      if (rtCfg.debugSerial) Serial.println(F("[M7] WS: handshake failed (bad key length?)"));
      newClient.stop();
    }
    return;  // keep connection alive for pushes
  }

  // Drain remaining headers for all other routes
  while (newClient.available()) {
    String line = newClient.readStringUntil('\n'); line.trim();
    if (line.length() == 0 || line == "\r") break;
  }

  // --- Command endpoint: browser sends commands to M4 ---
  if (req.startsWith("GET /cmd?msg=")) {
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
