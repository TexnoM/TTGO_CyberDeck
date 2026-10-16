# TTGO_CyberDeck — Pocket Cyberdeck Firmware (`v1.2.0-CYBER`)

> 🇺🇦 **[Читати документацію українською мовою (README_UA.md)](README_UA.md)**

**TTGO_CyberDeck** is a high-performance, flicker-free miniature cyberpunk operating system engineered for **Chinese clones of the LilyGO TTGO T-Display 1.14-inch ESP32 board (ST7789 135×240 TFT + 2 physical buttons)**.

---

## 📸 Keychain & Hardware Gallery

<p align="center">
  <img src="docs/images/keychain_front.png" alt="TTGO CyberDeck Keychain Front View" width="280" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/images/keychain_side.png" alt="TTGO CyberDeck Keychain In Action" width="280" />
</p>

> 💡 **Place your photos here:** Save your keychain/case build pictures into [`docs/images/`](docs/images/) as `keychain_front.png` and `keychain_side.png` to display them here automatically.

---

## Key Highlights

- **Soft Multi-Glitch & Zero-Flicker DMA Engine**:
  - Native `swap565_t` SPI DMA transfers (`40 MHz`) with `25 kHz` high-frequency PWM backlight.
  - Non-strobing soft background scanlines & scanner beam that tint only background pixels while keeping bright text 100% solid.
  - **5 Rotating Cyberpunk Glitch Patterns**: `RGB Chromatic Split`, `Sine Wave Ripple`, `Dual Laser Sync Beam`, `Holographic Data Burst`, and `Classic Soft CRT` (configurable in `SETTINGS -> GLITCH FX`: `MULTI`, `CLASSIC`, `OFF`).
- **12 Built-in Cyberdeck Applications**:
  1. **`SYSTEM` (`MOD-01 // SYS`)** — Dual-Core CPU telemetry, SRAM/Heap fragmentation monitor, MAC ID, uptime, live FPS & 40-frame timing oscilloscope.
  2. **`WIFI RADAR` (`MOD-02 // RAD`)** — Non-blocking 2.4 GHz spectrum scanner with RSSI sorting, security inspector (`OPEN/WPA2/WPA3`), and scrollbar navigation.
  3. **`PKT SNIFFER` (`MOD-03 // SNF`)** — Passive 802.11 promiscuous packet monitor (`PKT/s`, `MGMT`, `DATA`), live air-traffic waveform, and **13-channel congestion histogram** with manual channel lock or auto-hopping.
  4. **`NET TOOLS` (`MOD-04 // NET`)** — Local IP/GW/DNS readout + non-blocking Core 0 ICMP/TCP Ping Sonar with 24-sample RTT bar chart and continuous auto-ping mode.
  5. **`WEB DECK` (`MOD-05 // WEB`)** — Built-in HTTP Remote Cyber Console accessible over LAN or Hotspot AP (`http://192.168.4.1`, SSID `TTGO-CyberDeck-XXXX`) to switch apps/themes, view telemetry, send Pager Alerts, set custom time, and configure Wi-Fi.
  6. **`CYBER EYE` (`MOD-06 // EYE`)** — Interactive cyber-pet keychain demo: animated cybernetic eye looking around, blinking, and switching emotion modes (`NORMAL`, `AGGRESSIVE`, `SCANNING`, `SLEEP`) via button `B`.
  7. **`3D CUBE` (`MOD-07 // CUB`)** — Real-time 3D wireframe rotating cube and 4D tesseract hypercube projection demo for keychain visuals.
  8. **`TERMINAL` (`MOD-08 // SHL`)** — Two-button local command shell (`help`, `sysinfo`, `wifi`, `scan`, `ping`, `uptime`, `theme`, `reboot`).
  9. **`MATRIX` (`MOD-09 // MTX`)** — Customizable digital rain stream (`Speed 1..5`, `Density 1..5`).
  10. **`STARFIELD` (`MOD-10 // STR`)** — 3D perspective warp simulator with HUD reticle and Hyperwarp streak mode.
  11. **`CLOCK` (`MOD-11 // CLK`)** — Versatile digital clock: supports manual time setting via buttons on the board (hold `B` to enter editor), NTP Wi-Fi sync, or 1-click sync from your smartphone browser via web console.
  12. **`SETTINGS` (`MOD-12 // CFG`)** — NVS persistent configuration (4 neon themes, brightness, glitch mode selector, UI speed, FPS cap, Wi-Fi AP, hardware pinout viewer, factory reset).

---

## 1. Quick Flashing Guide (Linux & Windows)

Pre-compiled factory images are included in the [`release/`](release) folder for instant flashing at offset `0x0`:

### Linux (One Command)
Connect your TTGO T-Display via USB and run:
```bash
sudo ./flash_linux.sh
```
Or flash directly with `esptool`:
```bash
sudo esptool --chip esp32 -p /dev/ttyACM0 -b 921600 write_flash 0x0 release/TTGO_CyberDeck.bin
```

### Windows (One Click)
Double-click **[`flash_windows.bat`](flash_windows.bat)** and select `[1]`, or run via PlatformIO CLI:
```bat
pio run -e ttgo-t-display-clone -t upload
```

---

## 2. Hardware Compatibility & Pinout Matrix

Configured in [`include/hardware_config.h`](include/hardware_config.h):

| Signal / Function | Variant A (Default ~95% Clones) | Variant B (Alt Clone) | Macro in `hardware_config.h` |
| :--- | :--- | :--- | :--- |
| **Display Controller** | ST7789V (IPS 135×240) | ST7789 (135×240) | `TFT_WIDTH_PX` / `TFT_HEIGHT_PX` |
| **SPI MOSI / SCLK** | `GPIO 19` / `GPIO 18` | `GPIO 19` / `GPIO 18` | `PIN_TFT_MOSI` / `PIN_TFT_SCLK` |
| **TFT CS / DC / RST** | `GPIO 5` / `16` / `23` | `GPIO 5` / `16` / `23` | `PIN_TFT_CS` / `PIN_TFT_DC` / `PIN_TFT_RST` |
| **TFT Backlight (BL)** | `GPIO 4` (25 kHz PWM) | `GPIO 4` | `PIN_TFT_BL` |
| **CGRAM Offset (X, Y)** | `X = 52, Y = 40` | `X = 53, Y = 40` | `TFT_OFFSET_X` / `TFT_OFFSET_Y` |
| **Button A (Top/Left)** | `GPIO 0` (Active LOW) | `GPIO 0` | `PIN_BTN_A` |
| **Button B (Bot/Right)** | `GPIO 35` (Active LOW) | `GPIO 35` | `PIN_BTN_B` |
| **Battery ADC Sense** | `GPIO 34` + `GPIO 14` (EN) | Optional | `PIN_BAT_ADC` / `PIN_BAT_ADC_EN` |

---

## 3. Two-Button Controls

| Gesture | Action in Launcher | Action Inside Applications |
| :--- | :--- | :--- |
| **Short Press `A`** | Next application card | Move selection / step channel / next command |
| **Short Press `B`** | Open selected application | Confirm / execute action / toggle mode |
| **Long Press `A`** (`>520 ms`) | Previous application card | **Return to Main Launcher** (auto-saves settings) |
| **Long Press `B`** (`>520 ms`) | Open **Quick Override Menu** | Open **Quick Override Menu** from anywhere |
| **Simultaneous `A + B`** | Instant Theme Cycle | Instant Theme Cycle |

---

## 4. License

Released under the MIT License.
