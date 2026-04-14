#pragma once
#include <stdint.h>

#define SS_NUM_HEXES    61
#define SS_MAX_PLAYERS   8
#define CMD_QUEUE_DEPTH 16

struct LedSnapshot {
  uint8_t  r[SS_NUM_HEXES];
  uint8_t  g[SS_NUM_HEXES];
  uint8_t  b[SS_NUM_HEXES];
  uint8_t  brightness;
  uint8_t  activeEffect;
  uint32_t frameCount;
};

struct PlayerSnapshot {
  bool    active;
  bool    colorLocked;
  bool    strategyLocked;
  bool    hasPassed;
  bool    readyForNext;
  uint8_t homeHex;
  uint8_t strategyCard;
  uint8_t initiative;
  uint32_t colorHex;
};

struct GameSnapshot {
  uint8_t currentPhase;
  uint8_t speakerIndex;
  uint8_t numActivePlayers;
  uint8_t currentPickIndex;
  uint8_t currentActionIndex;
  bool    inBattle;
  uint8_t battleAttacker;
  uint8_t battleDefender;
  PlayerSnapshot players[SS_MAX_PLAYERS];
  uint32_t stateVersion;
};

enum CmdType : uint8_t {
  CMD_NONE        = 0,
  CMD_SELECT_HEX  = 1,
  CMD_SET_HEX     = 2,
  CMD_SET_HEX_SIDE= 3,
  CMD_SET_ALL     = 4,
  CMD_EFFECT      = 5,
  CMD_BRIGHTNESS  = 6,
  CMD_CLEAR       = 7,
  CMD_GAME_KEY    = 8,
  CMD_SET_PLAYERS = 9,
  CMD_START_GAME  = 10,
  CMD_PHASE_JUMP  = 11,
  CMD_BATTLE      = 12
};

struct Cmd {
  CmdType type;
  uint8_t arg0, arg1;
  uint8_t r, g, b;
};

struct CmdQueue {
  Cmd     slots[CMD_QUEUE_DEPTH];
  uint8_t head;   // M7 writes/advances head
  uint8_t tail;   // M4 reads/advances tail
};

struct SharedState {
  LedSnapshot  leds;   // M4 writes, M7 reads
  GameSnapshot game;   // M4 writes, M7 reads
  CmdQueue     cmds;   // M7 writes, M4 reads
  uint32_t     droppedCmds;
  bool         m4Ready;
};

// SRAM4 (0x38000000) — 64KB D3 domain, accessible by both M7 and M4.
// volatile prevents the compiler from caching reads across cores.
#define SHARED_STATE_BASE 0x38000000UL
#define sharedState (*reinterpret_cast<volatile SharedState*>(SHARED_STATE_BASE))
