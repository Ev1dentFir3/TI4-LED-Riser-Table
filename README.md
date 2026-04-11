# TI4 Hex Riser Firmware

Arduino Giga R1 WiFi firmware for a 61-hex Twilight Imperium 4 LED riser table.

## Hardware

| Component | Spec |
|---|---|
| MCU | Arduino Giga R1 WiFi |
| LEDs | 915x SK6812 RGBW, GPIO 6 |
| Keyboard I2C | MCP23017 x4, GPIO 20 (SDA) / 21 (SCL) |
| Power | 5V 60A PSU |

## Required Libraries

Install via Arduino IDE Library Manager:

| Library | Author | Purpose |
|---|---|---|
| FastLED | Daniel Garcia | LED control and animations |
| Adafruit MCP23X17 | Adafruit | Keyboard I2C (stub until boards arrive) |

## Upload Steps

1. Open `TI4_HexRiser.ino` in Arduino IDE
2. Install the libraries above
3. Select **Tools > Board > Arduino Mbed OS Giga Boards > Arduino Giga R1 WiFi**
4. Select **Tools > Port > your COM port**
5. Click **Upload**

## First Boot

1. Open Serial Monitor at **115200 baud**
2. The board will attempt to join the home WiFi configured in `config.h`, then fall back to AP mode
3. Connect your phone or laptop to WiFi **"TI4-HexRiser"** (password: **"twilight4"**) if using AP mode
4. Open a browser and navigate to the IP shown in Serial Monitor (AP mode default: `http://192.168.3.1`)
5. The hex grid should appear and sync live with the LED state

## Web Interface

The browser-based UI is served directly from the board. No external server needed.

- **Brightness slider** - adjusts global LED brightness
- **Effect buttons** - Rainbow, Pulse, Spiral, Sparkle, Wave, Stop
- **Side color controls** - set individual hex sides to any color
- **Settings page** (gear icon) - change WiFi, LED, debug, and display settings at runtime
- **Reboot button** - in Settings, reboots the board to apply WiFi credential changes

The browser polls the board every 100ms for the current LED state. The indicator dot in the sidebar shows connection status.

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
| Broadcast Rate | How often the board sends state to the browser (ms) |
| Side Gap | Inset of the colored side lines in the browser (0 = touching, higher = more gap) |
| Simulate Hardware | Skip FastLED.show() -- use this when testing without the strip connected |
| Debug flags | Enable serial logging for various subsystems |

## Serial Commands

For testing without the physical keyboards connected:

| Command | Effect |
|---|---|
| `hex N` | Select hex N (0-60) |
| `player N` | Set active player 0-7 |
| `color RRGGBB` | Set active player color |
| `effect rainbow` | Start rainbow animation |
| `effect pulse` | Pulsing glow with slow hue drift |
| `effect spiral` | Spiral outward from center hex |
| `effect sparkle` | Random sparkle across all hexes |
| `effect wave` | Color wave sweeping left to right |
| `effect none` | Stop animation and clear |
| `bright N` | Set brightness 0-200 |
| `clear` | Clear all hexes |
| `test` | Run LED hardware test |
| `status` | Print current game state and IP |

## File Map

| File | Purpose |
|---|---|
| `TI4_HexRiser.ino` | Main sketch: setup, loop, game logic, serial handler |
| `config.h` | Edit this -- pins, WiFi credentials, brightness limits, debug flags |
| `runtime_settings.h` | Live-mutable shadow of config.h; updated by the settings page |
| `led_map.h` | HEX_MAP[61][6][3] mapping hex/side/slot to LED index (0-914) |
| `hex_neighbors.h` | HEX_NEIGHBORS[61][6] adjacency table |
| `led_control.h` | FastLED init, per-hex color management, 5 animation effects |
| `keyboard_control.h` | MCP23017 stub -- follow the 4 IMPLEMENT HERE markers when boards arrive |
| `web_interface.h` | Full HTML/CSS/JS web UI served from flash |
| `settings_page.h` | Settings page HTML served at /settings |
| `network.h` | WiFi station+AP, HTTP server, poll/cmd/settings routes |

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

## Adding Keyboard Support

When MCP23017 boards arrive, open `keyboard_control.h` and follow the four
`IMPLEMENT HERE` markers in order. No other files need editing.

## Wiring Reference

```
Arduino Giga R1           SK6812 strip
GPIO 6 ──────────────── DIN (LED 0 end)
GND   ──────────────── GND
                        5V from external PSU

Arduino Giga R1           MCP23017 (x4)
GPIO 20 (SDA) ────────── SDA
GPIO 21 (SCL) ────────── SCL
3.3V ─────────────────── VCC
GND ──────────────────── GND
                          A0/A1/A2 sets address (0x20-0x23)
```

## Power Notes

- At 50% brightness (default 128): approximately 27.5A draw from LEDs
- At max brightness (200): approximately 44A
- Minimum recommended PSU: 5V 60A
- Connect PSU ground to Arduino ground
- Do not power the LED strip from the Arduino 5V pin

## Troubleshooting

**LEDs don't light:** Check GPIO 6 data wire, confirm PSU is powered, verify shared GND between PSU and Arduino

**Web page won't load:** Confirm you are connected to the correct WiFi; check Serial Monitor for the IP address

**Animation not showing in browser:** Enable "Simulate Hardware" in Settings when testing without the strip connected -- FastLED.show() blocks the loop for ~27ms when driving GPIO with no strip attached, which can interfere with the WiFi stack

**Wrong colors:** Edit `LED_COLOR_ORDER` in `config.h` (try `GRB`, `RGB`, or `BGR`)

**Compile errors:** Confirm FastLED and Adafruit MCP23X17 are installed and the board is set to Giga R1 WiFi
