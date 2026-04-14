#pragma once
#include "shared_state.h"
#include "led_control.h"
#include "game_state.h"
#include "animations.h"

// onHexSelected is defined in M4_LED.ino
void onHexSelected(int hexIdx);

// =============================================================================
// TI4 Hex Riser - Command Queue Handler (M4)
// =============================================================================
// Drains commands enqueued by M7 via the SPSC CmdQueue in shared SRAM.
// M7 writes head; M4 reads tail — no mutex needed on ARM Cortex.
// Call drainCmdQueue() once per loop() before updateGameState().
// =============================================================================

void drainCmdQueue() {
  volatile CmdQueue& q = sharedState.cmds;
  while (q.tail != q.head) {
    // memcpy strips the volatile qualifier safely — we own this slot until tail advances
    Cmd c;
    memcpy(&c, (const void*)&q.slots[q.tail], sizeof(Cmd));
    q.tail = (q.tail + 1) % CMD_QUEUE_DEPTH;

    switch (c.type) {
      case CMD_SELECT_HEX:
        onHexSelected(c.arg0);
        break;

      case CMD_SET_HEX:
        setHexColor(c.arg0, CRGB(c.r, c.g, c.b));
        break;

      case CMD_SET_HEX_SIDE:
        setHexSideColor(c.arg0, c.arg1, CRGB(c.r, c.g, c.b));
        break;

      case CMD_SET_ALL:
        setAllHexes(CRGB(c.r, c.g, c.b));
        break;

      case CMD_EFFECT:
        if (c.arg0 == 0) stopEffect();
        else             startEffect((AnimEffect)c.arg0);
        break;

      case CMD_BRIGHTNESS:
        setBrightness(c.arg0);
        break;

      case CMD_CLEAR:
        setAllHexes(CRGB::Black);
        pushLEDs();
        break;

      case CMD_GAME_KEY:
        handleGameKey(c.arg0, c.arg1);
        break;

      case CMD_SET_PLAYERS:
        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
          players[i].active = (i < c.arg0);
        transitionToSetup();
        break;

      case CMD_START_GAME:
        for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
          if (players[i].active && !players[i].colorLocked) {
            uint8_t ci = players[i].selectedColorIndex;
            if (!gameState.colorTaken[ci]) {
              players[i].colorLocked        = true;
              gameState.colorTaken[ci]      = true;
            }
          }
        }
        selectRandomSpeaker();
        transitionToStrategy();
        break;

      case CMD_PHASE_JUMP:
        switch (c.arg0) {
          case 0: transitionToSetup();    break;
          case 1: transitionToStrategy(); break;
          case 2: transitionToAction();   break;
          case 3: transitionToStatus();   break;
          case 4: transitionToAgenda();   break;
        }
        break;

      case CMD_BATTLE:
        startBattle(c.arg0, c.arg1);
        break;

      default:
        break;
    }
  }
}
