# TI4 Hex Riser Firmware

ESP32-S3 firmware for a 61-hex Twilight Imperium 4 LED riser table. Dual-core architecture: Core 0 runs the LED strip task (I2S driver, ~60 fps), Core 1 runs the game state machine, keyboard polling, serial commands, and AsyncWebServer.

## Hardware

| Component | Spec |
|---|---|
| MCU | ESP32-S3-WROOM-1-N16R8 (Lonely Binary Gold Edition) |
| CPU | Dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB QSPI |
| PSRAM | 8 MB Octal SPI |
| LEDs | 915x SK6812 RGBW, GPIO 13 |
| Keyboard I2C | MCP23017 x4, GPIO 21 (SDA) / 22 (SCL) |
| Power | 5V 60A PSU |

## Required Libraries

Install via Arduino IDE Library Manager:

| Library | Author | Purpose |
|---|---|---|
| FastLED | Daniel Garcia | LED control and animations |
| Adafruit MCP23X17 | Adafruit | Keyboard I2C (stub until boards arrive) |

Install manually from GitHub:

| Library | Source | Purpose |
|---|---|---|
| ESPAsyncWebServer | me-no-dev/ESPAsyncWebServer | Async HTTP + WebSocket server |
| AsyncTCP | me-no-dev/AsyncTCP | Required dependency for ESPAsyncWebServer |

## Driver

The board uses a **CH343 USB-to-UART bridge**. If your serial port isn't detected, install the official driver:
- **Windows:** https://www.wch-ic.com/downloads/CH343SER_EXE.html
- **macOS:** https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html
- **Linux:** Built-in kernel support (4.x+)

## Upload Steps

1. Open `TI4_HexRiser.ino` in Arduino IDE
2. Install all libraries above
3. Select **Tools > Board > ESP32 Arduino > ESP32S3 Dev Module**
4. Apply these board settings:

| Setting | Value |
|---|---|
| USB CDC On Boot | Enabled |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | OPI PSRAM |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |

5. Select **Tools > Port > your COM port**
6. Click **Upload**

## Core Architecture

| Core | Responsibility |
|---|---|
| Core 0 | LED task (~60 fps via I2S) — timing-critical, isolated from WiFi jitter |
| Core 1 | `loop()`: game state machine, keyboard polling, serial command handler; AsyncWebServer |

The LED task runs independently at ~60 fps. Game state writes to shared `hexColor[]` directly; the LED task picks up changes within one frame (~16ms) with no explicit sync needed.

## First Boot

1. Open Serial Monitor at **115200 baud**
2. The board will attempt to join the configured WiFi in `config.h`, then fall back to AP mode
3. Connect your phone or laptop to WiFi **"TI4-HexRiser"** (password: **"twilight4"**) if using AP mode
4. Open a browser and navigate to the IP shown in Serial Monitor (AP mode default: `http://192.168.3.1`)
5. The hex grid should appear and sync live with the LED state via WebSocket

## Web Interface

- **Brightness slider** - adjusts global LED brightness
- **Effect buttons** - Rainbow, Pulse, Spiral, Sparkle, Wave, Stop
- **Side color controls** - set individual hex sides to any color
- **Player swatches** - shows active player colors and current phase
- **Settings page** (gear icon) - change WiFi, LED, debug, and display settings at runtime
- **Reboot button** - in Settings, reboots the board to apply WiFi credential changes

The browser connects via WebSocket for live bidirectional state sync. The indicator dot in the sidebar shows connection status.

## Settings Page

Navigate to the gear icon at the bottom of the sidebar to access runtime settings. Changes to LED and debug options take effect immediately. WiFi credential changes require a reboot.

| Setting | Description |
|---|---|
| Home SSID / Password | Home network to connect to first |
| AP SSID / Password | Fallback access point credentials |
| Home Timeout | How long to wait for home network before switching to AP |
| Default Brightness | Startup brightness (0-255) |
| Max Brightness | Hard cap for the brightness slider |
| LED Update Rate | Animation tick interval in ms |
| Broadcast Rate | How often the board pushes state to the browser (ms) |
| Side Gap | Inset of the colored side lines in the browser (0 = touching, higher = more gap) |
| Simulate Hardware | Skip FastLED.show() -- use this when testing without the strip connected |
| Debug flags | Enable serial logging for various subsystems |

## Serial Commands

Open Serial Monitor at **115200 baud**. All commands are case-insensitive where noted.

### Game Simulation Commands

These simulate physical keyboard presses and game flow for testing without hardware.

| Command | Effect |
|---|---|
| `setplayers <4-8>` | Set how many players are active and restart the setup phase |
| `kb <1-8> <0-15>` | Simulate player N pressing key K on their keyboard |
| `startgame` | GM force-start: locks any unlocked players and runs speaker selection |
| `phase <0-4>` | Jump directly to a phase (0=Setup, 1=Strategy, 2=Action, 3=Status, 4=Agenda) |
| `battle <P1> <P2>` | Trigger battle mode between two players (e.g. `battle 1 3`) |
| `status` | Print current phase, all player states, home hexes, and WiFi IP |

### Keyboard Key Reference

Each player has a 4x4 keyboard (keys 0-15). What each key does depends on the current phase:

| Key | Setup phase | Strategy phase | Action phase | Status phase | Agenda phase |
|---|---|---|---|---|---|
| 1-8 | Select color (preview only, not locked yet) | Select strategy card 1-8 | -- | -- | -- |
| 13 | -- | -- | End battle mode | -- | -- |
| 14 | -- | -- | Pass this round | -- | -- |
| 15 | Lock in color choice | Lock in strategy card and hand off | End turn | Mark ready | End agenda (speaker only) |
| 0 | Start game (any player, only after all locked) | -- | -- | -- | -- |

### Full Round Walkthrough

The board boots with 6 players active by default. To simulate a complete round:

**Setup phase (color selection)**
```
status                  check starting state
setplayers 4            use a 4-player game for simplicity
kb 1 1                  P1 previews Red
kb 1 15                 P1 locks Red
kb 2 2                  P2 previews Blue
kb 2 15                 P2 locks Blue
kb 3 3                  P3 previews Green
kb 3 15                 P3 locks Green
kb 4 4                  P4 previews Yellow
kb 4 15                 P4 locks Yellow
kb 1 0                  GM starts game -- speaker roulette runs, then moves to Strategy
```

**Strategy phase (card selection)**
```
status                  check pick order (speaker goes first)
kb 1 1                  current picker selects card 1 (Leadership)
kb 1 15                 picker locks card, next player's home hex starts pulsing
kb 2 4                  next picker selects card 4 (Construction)
kb 2 15                 locks, next picker
kb 3 6                  selects card 6 (Warfare)
kb 3 15                 locks
kb 4 8                  selects card 8 (Imperial)
kb 4 15                 locks -- all done, center pulse plays, moves to Action
```

**Action phase (taking turns)**

Initiative order is determined by card number (lowest card = first turn).
```
status                  check initiative order
kb 1 15                 active player ends their turn, passes to next
kb 2 15                 next player ends turn
kb 3 14                 P3 passes this round (home hex dims to 50%)
kb 4 15                 P4 ends turn
kb 1 15                 P1 ends turn again (cycling through non-passed players)
kb 2 14                 P2 passes
kb 4 14                 P4 passes
kb 1 14                 P1 passes -- all passed, moves to Status automatically
```

**Status phase (end-of-round cleanup)**

The board splits into radial slices, one per player, each pulsing their color.
```
kb 1 15                 P1 marks ready (their slice goes solid)
kb 2 15                 P2 marks ready
kb 3 15                 P3 marks ready
kb 4 15                 P4 marks ready -- all ready, moves to Agenda automatically
```

**Agenda phase**
```
status                  check who the speaker is
kb 1 15                 speaker ends agenda -- round resets, moves back to Strategy
```

### Utility Commands

| Command | Effect |
|---|---|
| `effect rainbow` | Start rainbow animation |
| `effect pulse` | Pulsing glow with slow hue drift |
| `effect spiral` | Spiral outward from center hex |
| `effect sparkle` | Random sparkle across all hexes |
| `effect wave` | Color wave sweeping left to right |
| `effect none` | Stop animation and clear |
| `bright N` | Set brightness 0-200 |
| `clear` | Clear all hexes |
| `test` | Run LED hardware test (scans all 915 LEDs) |

## File Map

| File | Purpose |
|---|---|
| `TI4_HexRiser.ino` | Main sketch: setup, loop, serial handler, key/hex callbacks, LED task (Core 0) |
| `config.h` | Edit this -- pins, WiFi credentials, brightness limits, debug flags, game constants |
| `runtime_settings.h` | Live config updated by the settings page |
| `game_state.h` | Full game state machine: all phases, player data, key dispatch |
| `animations.h` | Boot snake animation and center-out phase transition |
| `led_map.h` | HEX_MAP[61][6][3] mapping hex/side/slot to LED index (0-914) |
| `hex_neighbors.h` | HEX_NEIGHBORS[61][6] adjacency table |
| `led_control.h` | FastLED init, per-hex color management, 5 animation effects |
| `keyboard_control.h` | MCP23017 stub -- follow the IMPLEMENT HERE markers when boards arrive |
| `web_interface.h` | Web UI served from root |
| `settings_page.h` | Settings page HTML served at /settings |
| `web_server.h` | WiFi station+AP, AsyncWebServer routes, WebSocket handler |

## LED Map

Each hex has 15 LEDs across 6 sides. The strip enters each hex mid-side:

```
Side 0 (top):          2 LEDs  -- slots [base+14, base+0]
Side 1 (top-right):    3 LEDs  -- slots [base+1,  base+2,  base+3]
Side 2 (bottom-right): 2 LEDs  -- slots [base+4,  base+5]
Side 3 (bottom):       3 LEDs  -- slots [base+6,  base+7,  base+8]
Side 4 (bottom-left):  2 LEDs  -- slots [base+9,  base+10]
Side 5 (top-left):     3 LEDs  -- slots [base+11, base+12, base+13]
```

Where `base = hex_index * 15`. Hex 30 is the center hex.

## Wiring Reference

```
ESP32-S3-WROOM-1          SK6812 strip
GPIO 13 ─────────────── DIN (LED 0 end)
GND     ─────────────── GND
                         5V from external PSU

ESP32-S3-WROOM-1          MCP23017 (x4)
GPIO 21 (SDA) ────────── SDA
GPIO 22 (SCL) ────────── SCL
3.3V ─────────────────── VCC
GND ──────────────────── GND
                          A0/A1/A2 sets address (0x20-0x23)
```

## Power Notes

- At 50% brightness (default 128): approximately 27.5A draw from LEDs
- At max brightness (200): approximately 44A
- Minimum recommended PSU: 5V 60A
- Connect PSU ground to ESP32 ground
- Do not power the LED strip from the ESP32 3.3V or 5V pins

## Troubleshooting

**Serial port not detected:** Install the CH343 driver from WCH (see Driver section above). Use the UART USB-C port, not the data port.

**LEDs don't light:** Check GPIO 13 data wire, confirm PSU is powered, verify shared GND between PSU and ESP32-S3. On ESP32-S3, avoid GPIO 0, 45, 46 (strapping pins) for LED data — GPIO 13 is safe.

**Web page won't load:** Confirm you are connected to the correct WiFi; check Serial Monitor for the IP address.

**WebSocket not connecting:** Hard-refresh the browser (Ctrl+Shift+R). If the board rebooted, the WebSocket client reconnects automatically within a few seconds.

**Animation not showing in browser:** Enable "Simulate Hardware" in Settings when testing without the strip connected.

**Compile error — FASTLED_USES_ESP32S3_I2S not defined:** The `#define FASTLED_USES_ESP32S3_I2S` line in `TI4_HexRiser.ino` must appear before `#include "led_control.h"`. Do not move or remove it — the ESP32-S3 I2S driver is required for FastLED to work on this chip.

**Wrong colors:** Edit `LED_COLOR_ORDER` in `config.h` (try `GRB`, `RGB`, or `BGR`).

**Compile errors — ESPAsyncWebServer:** This library must be installed manually from GitHub (me-no-dev/ESPAsyncWebServer and me-no-dev/AsyncTCP). It is not available in the Arduino Library Manager.

**Watchdog reset / core panic:** If the LED task triggers a watchdog, increase the `vTaskDelay` in `ledTask()` or reduce `LED_UPDATE_MS` in `config.h`.

**PSRAM not detected:** Confirm **Tools > PSRAM > OPI PSRAM** is selected in Arduino IDE. The N16R8 module has 8MB Octal SPI PSRAM — if disabled, large buffers may cause heap exhaustion.
