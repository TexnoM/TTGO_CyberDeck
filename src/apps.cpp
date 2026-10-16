#include "apps.h"
#include "display_manager.h"
#include "animation_engine.h"
#include "settings_manager.h"
#include "wifi_manager.h"
#include <cmath>
#include <cstring>
#include <mbedtls/sha256.h>

static const AppMetadata kAppMeta[APP_COUNT] = {
    { APP_SYSTEM,      "SYSTEM",      "DUAL-CORE & RAM HUD",   "MOD-01 // SYS" },
    { APP_WIFI_RADAR,  "WIFI RADAR",  "2.4GHZ AP INSPECTOR",   "MOD-02 // RAD" },
    { APP_PKT_SNIFFER, "PKT SNIFFER", "802.11 TRAFFIC & CH",   "MOD-03 // SNF" },
    { APP_NET_TOOLS,   "NET TOOLS",   "IP SONAR & HOST PING",  "MOD-04 // NET" },
    { APP_WEB_DECK,    "WEB DECK",    "REMOTE HTTP CONSOLE",   "MOD-05 // WEB" },
    { APP_CRYPTO_LAB,  "CRYPTO LAB",  "HW RNG & SHA256 CORE",  "MOD-06 // CRY" },
    { APP_TERMINAL,    "TERMINAL",    "LOCAL DECK SHELL",      "MOD-07 // SHL" },
    { APP_MATRIX,      "MATRIX",      "DIGITAL RAIN STREAM",   "MOD-08 // MTX" },
    { APP_STARFIELD,   "STARFIELD",   "3D WARP VECTOR SIM",    "MOD-09 // STR" },
    { APP_CLOCK,       "CLOCK",       "NTP CYBER CHRONO",      "MOD-10 // CLK" },
    { APP_SETTINGS,    "SETTINGS",    "NVS CONFIG & TUNING",   "MOD-11 // CFG" }
};

const AppMetadata& getAppMeta(uint8_t idx) {
    return kAppMeta[idx % APP_COUNT];
}

// ============================================================================
// 1. SYSTEM APPLICATION
// ============================================================================
class AppSystem : public CyberApp {
    uint8_t  _page         = 0;
    uint32_t _lastRenderMs = 0;
    bool     _forceRedraw  = true;
public:
    void onEnter() override {
        _page        = 0;
        _forceRedraw = true;
    }

    const char* hintA() const override { return "PAGE"; }
    const char* hintB() const override { return "FPS HUD"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _page = (_page + 1) % 2;
            _forceRedraw = true;
        } else if (ev == ButtonEvent::B_SHORT) {
            Settings.get().showFpsOverlay = !Settings.get().showFpsOverlay;
            Settings.markDirty();
            _forceRedraw = true;
            UI.showToast(Settings.get().showFpsOverlay ? "FPS HUD: ON" : "FPS HUD: OFF");
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        if (yOffset == 0 && !_forceRedraw && (now - _lastRenderMs) < 250) {
            return; // Avoid redundant SPI redraws -> zero flicker
        }
        _lastRenderMs = now;
        _forceRedraw  = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        char buf[32];
        if (_page == 0) {
            Display.drawCyberPanel(4, by + 4, 127, 62, pal.panelBg, pal.primary, pal.secondary, 4);
            c.setTextColor(pal.secondary);
            c.setCursor(8, by + 10);
            c.print("MCU // ESP32 DUAL-CORE");

            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 24); c.print("CLK :");
            c.setTextColor(pal.textMain);
            snprintf(buf, sizeof(buf), "%lu MHz (REV %d)", (unsigned long)ESP.getCpuFreqMHz(), ESP.getChipRevision());
            c.setCursor(42, by + 24); c.print(buf);

            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 36); c.print("ROM :");
            c.setTextColor(pal.textMain);
            snprintf(buf, sizeof(buf), "%lu MB FLASH DIO", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
            c.setCursor(42, by + 36); c.print(buf);

            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 48); c.print("MAC :");
            c.setTextColor(pal.primary);
            c.setCursor(42, by + 48);
            String mac = Net.macAddress();
            c.print(mac.substring(mac.length() > 11 ? mac.length() - 11 : 0));

            // Heap & Fragmentation Panel
            Display.drawCyberPanel(4, by + 70, 127, 66, pal.panelBg, pal.secondary, pal.primary, 4);
            uint32_t freeHeap  = ESP.getFreeHeap();
            uint32_t totalHeap = ESP.getHeapSize();
            uint32_t maxAlloc  = ESP.getMaxAllocHeap();
            float usedRatio = (totalHeap > 0) ? (1.0f - ((float)freeHeap / (float)totalHeap)) : 0.5f;

            c.setTextColor(pal.primary);
            c.setCursor(8, by + 76);
            c.print("SRAM & HEAP ENGINE");

            snprintf(buf, sizeof(buf), "FREE: %lu / %lu KB",
                     (unsigned long)(freeHeap / 1024), (unsigned long)(totalHeap / 1024));
            c.setTextColor(pal.textMain);
            c.setCursor(8, by + 90);
            c.print(buf);

            snprintf(buf, sizeof(buf), "MAX BLK: %lu KB", (unsigned long)(maxAlloc / 1024));
            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 102);
            c.print(buf);

            Display.drawProgressBar(8, by + 116, 119, 10, usedRatio, pal.primary, pal.gridDim, pal.bg);

            // Uptime & Link Panel
            Display.drawCyberPanel(4, by + 140, 127, 60, pal.panelBg, pal.gridDim, pal.secondary, 4);
            uint32_t upSec = millis() / 1000UL;
            c.setTextColor(pal.secondary);
            c.setCursor(8, by + 146);
            c.print("UPTIME & RADIO");

            snprintf(buf, sizeof(buf), "UP: %02lu:%02lu:%02lu",
                     (unsigned long)(upSec / 3600), (unsigned long)((upSec % 3600) / 60), (unsigned long)(upSec % 60));
            c.setTextColor(pal.highlight);
            c.setCursor(8, by + 160);
            c.print(buf);

            c.setTextColor(Net.isConnected() ? pal.ok : pal.warning);
            c.setCursor(8, by + 174);
            if (Net.isConnected()) {
                snprintf(buf, sizeof(buf), "WIFI: %ld dBm", (long)Net.currentRssi());
                c.print(buf);
            } else {
                c.print("WIFI: STANDBY");
            }
            c.setTextColor(pal.textDim);
            c.setCursor(96, by + 184);
            c.print("[1/2]");
        } else {
            Display.drawCyberPanel(4, by + 4, 127, 84, pal.panelBg, pal.primary, pal.secondary, 4);
            c.setTextColor(pal.primary);
            c.setCursor(8, by + 10);
            c.print("DISPLAY DIAGNOSTICS");

            snprintf(buf, sizeof(buf), "FPS:  %.1f (CAP %u)", Anim.fps(), Settings.get().targetFps);
            c.setTextColor(pal.highlight);
            c.setCursor(8, by + 26);
            c.print(buf);

            snprintf(buf, sizeof(buf), "FRAME:  %.2f ms", Anim.frameTimeMs());
            c.setTextColor(pal.textMain);
            c.setCursor(8, by + 40);
            c.print(buf);

            snprintf(buf, sizeof(buf), "DMA TX: %.2f ms", Anim.renderTimeMs());
            c.setTextColor(pal.secondary);
            c.setCursor(8, by + 54);
            c.print(buf);

            snprintf(buf, sizeof(buf), "SPI: %u MHz 25kHz BL", (unsigned)(TFT_SPI_FREQ_HZ / 1000000UL));
            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 68);
            c.print(buf);

            // Real-time Frame-Time Graph
            Display.drawCyberPanel(4, by + 92, 127, 108, pal.panelBg, pal.secondary, pal.primary, 4);
            c.setTextColor(pal.secondary);
            c.setCursor(8, by + 98);
            c.print("FRAME TIMING (MS)");

            int16_t gx = 8, gy = by + 114, gw = 119, gh = 68;
            c.fillRect(gx, gy, gw, gh, pal.bg);
            c.drawRect(gx, gy, gw, gh, pal.gridDim);
            int16_t refY = gy + gh - (int16_t)((16.6f / 40.0f) * gh);
            c.drawFastHLine(gx + 1, refY, gw - 2, pal.gridDim);

            const uint16_t* hist = Anim.frameTimeHistoryUs();
            size_t head = Anim.historyIndex();
            for (size_t i = 0; i < AnimationEngine::kHistorySize; ++i) {
                size_t idx = (head + i) % AnimationEngine::kHistorySize;
                float ms = hist[idx] * 0.001f;
                int16_t barH = constrain((int16_t)((ms / 40.0f) * gh), 2, gh - 2);
                int16_t bx = gx + 2 + i * 2;
                if (bx + 2 < gx + gw) {
                    uint16_t col = (ms <= 20.0f) ? pal.primary : pal.warning;
                    c.fillRect(bx, gy + gh - 1 - barH, 2, barH, col);
                }
            }
            c.setTextColor(pal.textDim);
            c.setCursor(96, by + 184);
            c.print("[2/2]");
        }
    }
};

// ============================================================================
// 2. WIFI RADAR APPLICATION
// ============================================================================
class AppWifiRadar : public CyberApp {
    uint8_t  _selectedIdx  = 0;
    uint32_t _lastRenderMs = 0;
    bool     _dirty        = true;
public:
    void onEnter() override {
        _selectedIdx = 0;
        _dirty       = true;
        Net.startAsyncScan();
    }

    const char* hintA() const override { return "SELECT"; }
    const char* hintB() const override { return "RESCAN"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            if (Net.scannedCount() > 0) {
                _selectedIdx = (_selectedIdx + 1) % Net.scannedCount();
                _dirty = true;
            }
        } else if (ev == ButtonEvent::B_SHORT) {
            if (Net.startAsyncScan()) {
                _dirty = true;
                UI.showToast("2.4GHz SWEEP STARTED", Settings.palette().secondary, 1000);
            }
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        if (yOffset == 0 && !Net.isScanning() && !_dirty && (now - _lastRenderMs) < 400) {
            return;
        }
        _lastRenderMs = now;
        _dirty = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberPanel(3, by + 3, 129, 24, pal.panelBg, pal.secondary, pal.primary, 3);
        if (Net.isScanning()) {
            UI.drawLoadingSpinner(14, by + 15, 6, pal.primary);
            c.setTextColor(pal.primary);
            c.setCursor(25, by + 11);
            c.print("SCANNING 2.4GHz...");
        } else {
            char hdr[28];
            snprintf(hdr, sizeof(hdr), "AP NODES: %u", (unsigned)Net.scannedCount());
            c.setTextColor(pal.highlight);
            c.setCursor(8, by + 11);
            c.print(hdr);
        }

        size_t count = Net.scannedCount();
        if (_selectedIdx >= count && count > 0) {
            _selectedIdx = 0;
        }

        if (count == 0 && !Net.isScanning()) {
            Display.drawCyberPanel(6, by + 48, 123, 70, pal.panelBg, pal.gridDim, pal.warning, 4);
            c.setTextColor(pal.warning);
            c.setCursor(18, by + 70);
            c.print("NO SIGNALS FOUND");
            c.setTextColor(pal.textDim);
            c.setCursor(14, by + 88);
            c.print("PRESS [B] TO SWEEP");
            return;
        }

        constexpr size_t kVisRows = 5;
        size_t startIdx = 0;
        if (_selectedIdx >= kVisRows) {
            startIdx = _selectedIdx - (kVisRows - 1);
        }

        for (size_t row = 0; row < kVisRows && (startIdx + row) < count; ++row) {
            size_t idx = startIdx + row;
            const ScannedApInfo& ap = Net.scannedAp(idx);
            bool sel = (idx == _selectedIdx);
            int16_t ry = by + 30 + row * 24;

            uint16_t rowBg = sel ? AnimationEngine::lerpColor565(pal.panelBg, pal.primary, 50) : pal.panelBg;
            uint16_t brd   = ap.isConnectedAp ? pal.ok : (sel ? pal.primary : pal.gridDim);
            c.fillRect(3, ry, 125, 22, rowBg);
            c.drawRect(3, ry, 125, 22, brd);

            int bars = (ap.rssi > -58) ? 4 : (ap.rssi > -70) ? 3 : (ap.rssi > -82) ? 2 : 1;
            uint16_t sigCol = (bars >= 3) ? pal.ok : (bars == 2) ? pal.warning : pal.danger;
            for (int b = 0; b < 4; ++b) {
                int16_t bh = 3 + b * 3;
                c.fillRect(6 + b * 3, ry + 18 - bh, 2, bh, (b < bars) ? sigCol : pal.gridDim);
            }

            char ssidShort[13];
            std::strncpy(ssidShort, ap.ssid, 11);
            ssidShort[11] = '\0';
            c.setTextColor(ap.isConnectedAp ? pal.ok : (sel ? pal.highlight : pal.textMain), rowBg);
            c.setCursor(20, ry + 7);
            c.print(ssidShort);

            char rssiBuf[10];
            snprintf(rssiBuf, sizeof(rssiBuf), "%ld", (long)ap.rssi);
            c.setTextColor(pal.secondary, rowBg);
            c.setCursor(98, ry + 7);
            c.print(rssiBuf);
        }

        Display.drawScrollbar(by + 30, 118, (uint8_t)count, kVisRows, _selectedIdx);

        if (count > 0) {
            const ScannedApInfo& selAp = Net.scannedAp(_selectedIdx);
            int16_t iy = by + 152;
            Display.drawCyberPanel(3, iy, 129, 48, pal.panelBg, pal.primary, pal.secondary, 4);

            c.setTextColor(pal.primary);
            c.setCursor(8, iy + 6);
            char line1[24];
            snprintf(line1, sizeof(line1), "CH:%02ld [%s] %lddBm",
                     (long)selAp.channel, WifiManager::authModeName(selAp.authMode), (long)selAp.rssi);
            c.print(line1);

            c.setTextColor(selAp.isConnectedAp ? pal.ok : pal.textDim);
            c.setCursor(8, iy + 20);
            c.print(selAp.isConnectedAp ? "LINK: ACTIVE DECK AP" : "LINK: EXTERNAL NODE");

            float sigNorm = constrain((selAp.rssi + 100.0f) / 65.0f, 0.05f, 1.0f);
            Display.drawProgressBar(8, iy + 33, 119, 8, sigNorm, pal.secondary, pal.gridDim, pal.bg);
        }
    }
};

// ============================================================================
// 3. PACKET SNIFFER & 13-CHANNEL SPECTRUM ANALYZER (NEW!)
// ============================================================================
class AppPacketSniffer : public CyberApp {
    uint32_t _lastRenderMs = 0;
public:
    void onEnter() override {
        Net.startSniffer();
        _lastRenderMs = 0;
    }

    void onExit() override {
        Net.stopSniffer();
    }

    const char* hintA() const override { return "CH STEP"; }
    const char* hintB() const override { return Net.sniffer().hopping ? "LOCK CH" : "HOP ALL"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Net.stepSnifferChannel();
            _lastRenderMs = 0;
        } else if (ev == ButtonEvent::B_SHORT) {
            Net.toggleSnifferHopping();
            _lastRenderMs = 0;
            UI.showToast(Net.sniffer().hopping ? "AUTO HOP: CH 1..13" : "CHANNEL LOCKED",
                         Settings.palette().secondary, 1100);
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        if (yOffset == 0 && (now - _lastRenderMs) < 180) {
            return;
        }
        _lastRenderMs = now;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        const SnifferStats& sn = Net.sniffer();

        // Top Channel & Rate Panel
        Display.drawCyberPanel(3, by + 3, 129, 54, pal.panelBg, pal.primary, pal.secondary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 9);
        char line[28];
        snprintf(line, sizeof(line), "CH:%02u [%s] %lddBm",
                 sn.currentChannel, sn.hopping ? "HOP" : "LCK", (long)sn.lastRssi);
        c.print(line);

        snprintf(line, sizeof(line), "RATE: %lu PKT/s", (unsigned long)sn.pktPerSec);
        c.setTextColor(pal.highlight);
        c.setCursor(8, by + 24);
        c.print(line);

        snprintf(line, sizeof(line), "M:%lu D:%lu TOT:%lu",
                 (unsigned long)sn.mgmtPerSec, (unsigned long)sn.dataPerSec, (unsigned long)sn.totalCaptured);
        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 39);
        c.print(line);

        // Middle: 13-Channel 2.4GHz Spectrum Histogram
        Display.drawCyberPanel(3, by + 61, 129, 70, pal.panelBg, pal.secondary, pal.primary, 4);
        c.setTextColor(pal.primary);
        c.setCursor(8, by + 67);
        c.print("13-CH SPECTRUM LOAD");

        int16_t sx = 8, sy = by + 80, sw = 119, sh = 38;
        c.fillRect(sx, sy, sw, sh, pal.bg);
        c.drawRect(sx, sy, sw, sh, pal.gridDim);

        for (uint8_t ch = 0; ch < 13; ++ch) {
            int16_t bx = sx + 3 + ch * 9;
            uint16_t load = sn.channelPkts[ch];
            int16_t bh = constrain((int16_t)((load * (sh - 4)) / 60), 2, sh - 4);
            bool isCur = ((ch + 1) == sn.currentChannel);
            uint16_t col = isCur ? pal.highlight : ((load > 25) ? pal.warning : pal.primary);
            c.fillRect(bx, sy + sh - 1 - bh, 6, bh, col);
            if (isCur) {
                c.fillRect(bx + 1, sy + sh + 2, 4, 3, pal.secondary);
            }
        }

        // Bottom: Live Packet/s Oscilloscope
        Display.drawCyberPanel(3, by + 135, 129, 65, pal.panelBg, pal.gridDim, pal.secondary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 141);
        c.print("AIR TRAFFIC WAVE");

        int16_t wx = 8, wy = by + 153, ww = 119, wh = 41;
        c.fillRect(wx, wy, ww, wh, pal.bg);
        c.drawRect(wx, wy, ww, wh, pal.gridDim);

        int16_t prevX = -1, prevY = -1;
        for (uint8_t i = 0; i < 32; ++i) {
            uint8_t idx = (sn.historyIdx + i) % 32;
            uint16_t pps = sn.historyPps[idx];
            int16_t px = wx + 2 + (i * (ww - 4)) / 31;
            int16_t py = wy + wh - 2 - constrain((int16_t)((pps * (wh - 4)) / 120), 0, wh - 4);
            if (prevX >= 0) {
                c.drawLine(prevX, prevY, px, py, pal.primary);
            }
            prevX = px;
            prevY = py;
        }
    }
};

// ============================================================================
// 4. NETWORK TOOLS APPLICATION
// ============================================================================
class AppNetTools : public CyberApp {
    uint8_t  _presetIdx    = 0;
    char     _activeTarget[48] = {0};
    uint32_t _lastRenderMs = 0;

    void syncTargetName() {
        switch (_presetIdx) {
            case 0:
                std::strncpy(_activeTarget, Settings.get().pingHost, sizeof(_activeTarget) - 1);
                break;
            case 1: {
                String gw = Net.gatewayIp();
                std::strncpy(_activeTarget, (gw == "0.0.0.0") ? "192.168.1.1" : gw.c_str(), sizeof(_activeTarget) - 1);
                break;
            }
            case 2:
                std::strncpy(_activeTarget, "1.1.1.1", sizeof(_activeTarget) - 1);
                break;
            case 3:
                std::strncpy(_activeTarget, "8.8.8.8", sizeof(_activeTarget) - 1);
                break;
        }
        _activeTarget[sizeof(_activeTarget) - 1] = '\0';
    }

public:
    void onEnter() override {
        _presetIdx = 0;
        syncTargetName();
        _lastRenderMs = 0;
    }

    void onExit() override {
        Net.setContinuousPing(false);
    }

    const char* hintA() const override { return "TARGET"; }
    const char* hintB() const override { return Net.isContinuousPing() ? "STOP" : "PING"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _presetIdx = (_presetIdx + 1) % 4;
            syncTargetName();
            Net.resetPingStats();
            _lastRenderMs = 0;
        } else if (ev == ButtonEvent::B_SHORT) {
            syncTargetName();
            Net.triggerPing(_activeTarget);
            Net.setContinuousPing(!Net.isContinuousPing());
            _lastRenderMs = 0;
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        if (yOffset == 0 && (now - _lastRenderMs) < 250) {
            return;
        }
        _lastRenderMs = now;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberPanel(4, by + 4, 127, 74, pal.panelBg, pal.primary, pal.secondary, 4);
        c.setTextColor(Net.isConnected() ? pal.ok : pal.warning);
        c.setCursor(8, by + 10);
        c.print(Net.isConnected() ? "LINK: CONNECTED" : "LINK: OFFLINE");

        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 24); c.print("IP :");
        c.setTextColor(pal.textMain);
        c.setCursor(34, by + 24); c.print(Net.localIp());

        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 38); c.print("GW :");
        c.setTextColor(pal.textMain);
        c.setCursor(34, by + 38); c.print(Net.gatewayIp());

        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 52); c.print("DNS:");
        c.setTextColor(pal.secondary);
        c.setCursor(34, by + 52); c.print(Net.dnsIp());

        Display.drawCyberPanel(4, by + 82, 127, 118, pal.panelBg, pal.secondary, pal.primary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 88);
        c.print(Net.isContinuousPing() ? "SONAR // AUTO PING" : "SONAR // HOST PROBE");

        c.setTextColor(pal.highlight);
        c.setCursor(8, by + 102);
        c.print("> ");
        c.print(_activeTarget);

        const PingResult& pr = Net.pingStats();
        uint16_t stCol = pr.inProgress ? pal.warning : (pr.lastSuccess ? pal.ok : pal.danger);
        c.setTextColor(stCol);
        c.setCursor(8, by + 116);
        c.print(pr.statusMsg);

        char statBuf[28];
        snprintf(statBuf, sizeof(statBuf), "TX:%u RX:%u AVG:%ums",
                 pr.sentCount, pr.recvCount, pr.avgRttMs);
        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 130);
        c.print(statBuf);

        int16_t gx = 8, gy = by + 144, gw = 119, gh = 48;
        c.fillRect(gx, gy, gw, gh, pal.bg);
        c.drawRect(gx, gy, gw, gh, pal.gridDim);
        for (uint8_t i = 0; i < 24; ++i) {
            uint8_t idx = (pr.historyIdx + i) % 24;
            uint16_t ms = pr.historyMs[idx];
            int16_t bx = gx + 3 + i * 4;
            if (ms > 0) {
                int16_t bh = constrain((int16_t)((ms * gh) / 200), 3, gh - 3);
                c.fillRect(bx, gy + gh - 1 - bh, 3, bh, pal.primary);
            } else if (i < pr.sentCount) {
                c.fillRect(bx, gy + gh - 4, 3, 3, pal.danger);
            }
        }
    }
};

// ============================================================================
// 5. WEB DECK — REMOTE HTTP CYBER CONSOLE (NEW!)
// ============================================================================
class AppWebDeck : public CyberApp {
    uint32_t _lastRenderMs = 0;
public:
    void onEnter() override {
        _lastRenderMs = 0;
        if (!Net.isConnected() && !Net.isConfigPortalActive()) {
            Net.startConfigPortal();
        }
    }

    const char* hintA() const override { return Net.isConfigPortalActive() ? "STOP AP" : "START AP"; }
    const char* hintB() const override { return "TEST ALERT"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            if (Net.isConfigPortalActive()) {
                Net.stopConfigPortal();
                UI.showToast("AP HOTSPOT OFF", Settings.palette().warning);
            } else {
                Net.startConfigPortal();
                UI.showToast("AP HOTSPOT ON", Settings.palette().ok);
            }
            _lastRenderMs = 0;
        } else if (ev == ButtonEvent::B_SHORT) {
            UI.showToast("WEB DECK READY!", Settings.palette().secondary, 1400);
            _lastRenderMs = 0;
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        if (yOffset == 0 && (now - _lastRenderMs) < 350) {
            return;
        }
        _lastRenderMs = now;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberPanel(4, by + 4, 127, 68, pal.panelBg, pal.primary, pal.secondary, 4);
        c.setTextColor(pal.primary);
        c.setCursor(8, by + 10);
        c.print("HTTP REMOTE CONSOLE");

        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 26);
        c.print("LAN URL:");
        c.setTextColor(Net.isConnected() ? pal.ok : pal.textDim);
        c.setCursor(8, by + 38);
        c.print("http://");
        c.print(Net.localIp());

        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 52);
        c.print("AP: ");
        c.setTextColor(Net.isConfigPortalActive() ? pal.warning : pal.textDim);
        c.print(Net.isConfigPortalActive() ? "192.168.4.1 [ON]" : "OFF [PRESS A]");

        Display.drawCyberPanel(4, by + 76, 127, 64, pal.panelBg, pal.secondary, pal.primary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 82);
        c.print("HOTSPOT SSID:");
        c.setTextColor(pal.highlight);
        c.setCursor(8, by + 96);
        c.print(Net.portalSsid());

        char stat[28];
        snprintf(stat, sizeof(stat), "CLIENTS:%u  REQS:%lu",
                 Net.portalClientCount(), (unsigned long)Net.webRequestCount());
        c.setTextColor(pal.textMain);
        c.setCursor(8, by + 114);
        c.print(stat);

        Display.drawCyberPanel(4, by + 144, 127, 56, pal.panelBg, pal.gridDim, pal.secondary, 4);
        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 150);
        c.print("LAST REMOTE EVENT:");
        c.setTextColor(pal.primary);
        c.setCursor(8, by + 166);
        c.print("> ");
        c.print(Net.lastRemoteCmd());
    }
};

// ============================================================================
// 6. CRYPTO LAB — HARDWARE RNG & SHA-256 ENGINE (NEW!)
// ============================================================================
class AppCryptoLab : public CyberApp {
    uint8_t  _mode = 0; // 0 = Passphrase, 1 = 128-bit HEX Key, 2 = PIN / Token
    char     _secret[22] = {0};
    char     _shaHex1[22] = {0};
    char     _shaHex2[22] = {0};
    uint32_t _hashRateKps = 0;
    bool     _dirty = true;

    void generateEntropy() {
        static const char kAlpha[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%&*";
        if (_mode == 0) {
            for (int i = 0; i < 16; ++i) {
                _secret[i] = kAlpha[esp_random() % (sizeof(kAlpha) - 1)];
            }
            _secret[16] = '\0';
        } else if (_mode == 1) {
            snprintf(_secret, sizeof(_secret), "%08lX-%08lX",
                     (unsigned long)esp_random(), (unsigned long)esp_random());
        } else {
            snprintf(_secret, sizeof(_secret), "PIN:%06lu-%04lu",
                     (unsigned long)(esp_random() % 1000000UL), (unsigned long)(esp_random() % 10000UL));
        }

        // Compute real SHA-256 using ESP32 mbedTLS engine + benchmark speed
        uint8_t digest[32];
        uint32_t t0 = micros();
        for (int iter = 0; iter < 200; ++iter) {
            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            mbedtls_sha256_starts(&ctx, 0);
            mbedtls_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(_secret), std::strlen(_secret));
            mbedtls_sha256_finish(&ctx, digest);
            mbedtls_sha256_free(&ctx);
        }
        uint32_t dtUs = max<uint32_t>(1, micros() - t0);
        _hashRateKps = (200UL * 1000000UL) / dtUs;

        for (int i = 0; i < 8; ++i) {
            snprintf(_shaHex1 + i * 2, 3, "%02X", digest[i]);
            snprintf(_shaHex2 + i * 2, 3, "%02X", digest[i + 8]);
        }
        _dirty = true;
    }

public:
    void onEnter() override {
        generateEntropy();
    }

    const char* hintA() const override { return "MODE"; }
    const char* hintB() const override { return "GEN KEY"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _mode = (_mode + 1) % 3;
            generateEntropy();
        } else if (ev == ButtonEvent::B_SHORT) {
            generateEntropy();
            UI.showToast("NEW ENTROPY GENERATED", Settings.palette().primary, 900);
        }
    }

    void updateAndRender(int16_t yOffset) override {
        if (yOffset == 0 && !_dirty) return;
        _dirty = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberPanel(4, by + 4, 127, 60, pal.panelBg, pal.primary, pal.secondary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 10);
        const char* modeTitle = (_mode == 0) ? "MODE: CYBER PASS-16" :
                                (_mode == 1) ? "MODE: 64-BIT HEX KEY" : "MODE: SECURE TOKEN";
        c.print(modeTitle);

        c.fillRect(8, by + 26, 119, 22, pal.bg);
        c.drawRect(8, by + 26, 119, 22, pal.primary);
        c.setTextColor(pal.highlight);
        c.setCursor(12, by + 33);
        c.print(_secret);

        Display.drawCyberPanel(4, by + 68, 127, 70, pal.panelBg, pal.secondary, pal.primary, 4);
        c.setTextColor(pal.primary);
        c.setCursor(8, by + 74);
        c.print("SHA-256 DIGEST (HW):");

        c.setTextColor(pal.textMain);
        c.setCursor(10, by + 92);
        c.print(_shaHex1);
        c.setTextColor(pal.secondary);
        c.setCursor(10, by + 106);
        c.print(_shaHex2);
        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 120);
        c.print("ENTROPY: ESP32 RF RNG");

        Display.drawCyberPanel(4, by + 142, 127, 56, pal.panelBg, pal.gridDim, pal.secondary, 4);
        c.setTextColor(pal.secondary);
        c.setCursor(8, by + 148);
        c.print("CRYPTO BENCHMARK:");
        char bench[28];
        snprintf(bench, sizeof(bench), "%lu SHA256/sec", (unsigned long)_hashRateKps);
        c.setTextColor(pal.ok);
        c.setCursor(8, by + 164);
        c.print(bench);
        c.setTextColor(pal.textDim);
        c.setCursor(8, by + 178);
        c.print("PRESS [B] RE-SEED");
    }
};

// ============================================================================
// 7. TERMINAL APPLICATION
// ============================================================================
class AppTerminal : public CyberApp {
    static constexpr size_t kMaxLines = 12;
    char    _lines[kMaxLines][22];
    uint8_t _lineCount = 0;
    uint8_t _cmdIdx    = 0;
    bool    _dirty     = true;

    static constexpr uint8_t kCmdCount = 9;
    const char* const _cmds[kCmdCount] = {
        "help", "sysinfo", "wifi", "scan", "ping", "keygen", "uptime", "theme", "reboot"
    };

    void appendLine(const char* text) {
        if (_lineCount < kMaxLines) {
            std::strncpy(_lines[_lineCount], text, 21);
            _lines[_lineCount][21] = '\0';
            _lineCount++;
        } else {
            for (size_t i = 1; i < kMaxLines; ++i) {
                std::memcpy(_lines[i - 1], _lines[i], 22);
            }
            std::strncpy(_lines[kMaxLines - 1], text, 21);
            _lines[kMaxLines - 1][21] = '\0';
        }
        _dirty = true;
    }

    void executeCommand(const char* cmd) {
        char prompt[24];
        snprintf(prompt, sizeof(prompt), "deck:~$ %s", cmd);
        appendLine(prompt);

        if (std::strcmp(cmd, "help") == 0) {
            appendLine("sysinfo wifi scan");
            appendLine("ping keygen uptime");
            appendLine("theme reboot");
        } else if (std::strcmp(cmd, "sysinfo") == 0) {
            char b[22];
            snprintf(b, sizeof(b), "CPU:%luMHz %.0fFPS",
                     (unsigned long)ESP.getCpuFreqMHz(), Anim.fps());
            appendLine(b);
            snprintf(b, sizeof(b), "RAM:%luKB FREE",
                     (unsigned long)(ESP.getFreeHeap() / 1024));
            appendLine(b);
        } else if (std::strcmp(cmd, "wifi") == 0) {
            if (Net.isConnected()) {
                appendLine(Net.connectedSsid().c_str());
                appendLine(Net.localIp().c_str());
            } else {
                appendLine("WIFI: OFFLINE");
                appendLine("OPEN WEB DECK APP");
            }
        } else if (std::strcmp(cmd, "scan") == 0) {
            Net.startAsyncScan();
            char b[22];
            snprintf(b, sizeof(b), "LAST SCAN: %u APs", (unsigned)Net.scannedCount());
            appendLine(b);
        } else if (std::strcmp(cmd, "ping") == 0) {
            appendLine("PROBING HOST...");
            Net.triggerPing(Settings.get().pingHost);
            appendLine(Settings.get().pingHost);
        } else if (std::strcmp(cmd, "keygen") == 0) {
            char b[22];
            snprintf(b, sizeof(b), "KEY:%08lX%08lX",
                     (unsigned long)esp_random(), (unsigned long)esp_random());
            appendLine(b);
        } else if (std::strcmp(cmd, "uptime") == 0) {
            uint32_t s = millis() / 1000UL;
            char b[22];
            snprintf(b, sizeof(b), "UP: %lum %lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
            appendLine(b);
        } else if (std::strcmp(cmd, "theme") == 0) {
            Settings.get().themeId = (Settings.get().themeId + 1) % THEME_COUNT;
            Settings.markDirty();
            appendLine(Settings.palette().name);
        } else if (std::strcmp(cmd, "reboot") == 0) {
            UI.showConfirmModal("SYSTEM REBOOT", "RESTART NEON DECK", "FIRMWARE NOW?", []() {
                ESP.restart();
            });
        }
    }

public:
    AppTerminal() {
        appendLine("NEON DECK SHELL v1.1");
        appendLine("USE [A/B] FOR CMDS");
        appendLine("--------------------");
    }

    void onEnter() override { _dirty = true; }

    const char* hintA() const override { return "CMD"; }
    const char* hintB() const override { return "RUN"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _cmdIdx = (_cmdIdx + 1) % kCmdCount;
            _dirty  = true;
        } else if (ev == ButtonEvent::B_SHORT) {
            executeCommand(_cmds[_cmdIdx]);
        }
    }

    void updateAndRender(int16_t yOffset) override {
        if (yOffset == 0 && !_dirty) return;
        _dirty = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberPanel(3, by + 3, 129, 168, pal.panelBg, pal.gridDim, pal.primary, 3);
        for (uint8_t i = 0; i < _lineCount; ++i) {
            bool isCmd = (std::strncmp(_lines[i], "deck:", 5) == 0);
            c.setTextColor(isCmd ? pal.secondary : pal.primary);
            c.setCursor(7, by + 9 + i * 13);
            c.print(_lines[i]);
        }

        int16_t cmdY = by + 175;
        Display.drawCyberPanel(3, cmdY, 129, 25, pal.bg, pal.primary, pal.secondary, 3);
        c.setTextColor(pal.secondary);
        c.setCursor(8, cmdY + 8);
        c.print("$>");
        c.setTextColor(pal.highlight);
        c.setCursor(24, cmdY + 8);
        c.print(_cmds[_cmdIdx]);

        int16_t curX = 24 + std::strlen(_cmds[_cmdIdx]) * 6 + 2;
        c.fillRect(curX, cmdY + 7, 5, 9, pal.primary);
    }
};

// ============================================================================
// 8. MATRIX DIGITAL RAIN APPLICATION
// ============================================================================
class AppMatrix : public CyberApp {
    static constexpr uint8_t kCols = 22;
    float    _headY[kCols];
    float    _speed[kCols];
    uint8_t  _len[kCols];
    uint32_t _hudShowUntilMs = 0;
    bool     _initialized    = false;

    void initColumns() {
        for (uint8_t i = 0; i < kCols; ++i) {
            _headY[i] = -(float)(esp_random() % 180);
            _speed[i] = 35.0f + (esp_random() % 55);
            _len[i]   = 6 + (esp_random() % 12);
        }
        _initialized = true;
    }

public:
    void onEnter() override {
        if (!_initialized) initColumns();
        _hudShowUntilMs = millis() + 1800;
    }

    const char* hintA() const override { return "SPEED"; }
    const char* hintB() const override { return "DENSITY"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Settings.get().matrixSpeed = (Settings.get().matrixSpeed % 5) + 1;
            Settings.markDirty();
            _hudShowUntilMs = millis() + 1600;
        } else if (ev == ButtonEvent::B_SHORT) {
            Settings.get().matrixDensity = (Settings.get().matrixDensity % 5) + 1;
            Settings.markDirty();
            _hudShowUntilMs = millis() + 1600;
        }
    }

    void updateAndRender(int16_t yOffset) override {
        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        float dt = Anim.dt();
        float spdMult = 0.45f + Settings.get().matrixSpeed * 0.35f;
        uint8_t activeCols = 6 + Settings.get().matrixDensity * 3;

        for (uint8_t col = 0; col < kCols; ++col) {
            if (col >= activeCols) continue;
            _headY[col] += _speed[col] * spdMult * dt;
            int16_t headRow = (int16_t)(_headY[col] / 8.0f);
            int16_t trailRows = _len[col];

            if ((headRow - trailRows) * 8 > DisplayManager::kBodyH) {
                _headY[col] = -(float)(esp_random() % 80);
                _speed[col] = 35.0f + (esp_random() % 65);
                _len[col]   = 5 + Settings.get().matrixDensity * 2 + (esp_random() % 6);
            }

            int16_t x = 2 + col * 6;
            for (int16_t r = 0; r < trailRows; ++r) {
                int16_t charRow = headRow - r;
                int16_t py = by + charRow * 8;
                if (py < DisplayManager::kBodyY || py > (DisplayManager::kBodyY + DisplayManager::kBodyH - 8)) {
                    continue;
                }
                char glyph = (char)(0x30 + ((col * 17 + charRow * 31 + (Anim.frameCount() >> 3)) % 42));
                uint16_t col565 = (r == 0)
                    ? pal.highlight
                    : AnimationEngine::lerpColor565(pal.bg, pal.primary,
                          (uint16_t)constrain(256 - (r * 256 / trailRows), 25, 240));
                c.setTextColor(col565);
                c.drawChar(glyph, x, py);
            }
        }

        if (millis() < _hudShowUntilMs) {
            Display.drawCyberPanel(14, by + 160, 107, 34, pal.panelBg, pal.primary, pal.secondary, 3);
            char buf[24];
            snprintf(buf, sizeof(buf), "SPD:%u  DEN:%u",
                     Settings.get().matrixSpeed, Settings.get().matrixDensity);
            c.setTextColor(pal.highlight);
            c.setCursor(26, by + 173);
            c.print(buf);
        }
    }
};

// ============================================================================
// 9. STARFIELD 3D WARP APPLICATION
// ============================================================================
class AppStarfield : public CyberApp {
    struct Star {
        float x, y, z, prevZ;
    };
    static constexpr size_t kNumStars = 65;
    Star _stars[kNumStars];
    bool _warpMode = false;
    bool _init     = false;

    void resetStar(Star& s, bool randomZ) {
        s.x = (float)((int32_t)(esp_random() % 260) - 130);
        s.y = (float)((int32_t)(esp_random() % 400) - 200);
        s.z = randomZ ? (float)(10 + (esp_random() % 190)) : 200.0f;
        s.prevZ = s.z;
    }

public:
    void onEnter() override {
        if (!_init) {
            for (auto& s : _stars) resetStar(s, true);
            _init = true;
        }
    }

    const char* hintA() const override { return "SPEED"; }
    const char* hintB() const override { return "WARP FX"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Settings.get().starfieldSpeed = (Settings.get().starfieldSpeed % 5) + 1;
            Settings.markDirty();
            char b[20];
            snprintf(b, sizeof(b), "WARP FACTOR %u", Settings.get().starfieldSpeed);
            UI.showToast(b, Settings.palette().secondary, 1000);
        } else if (ev == ButtonEvent::B_SHORT) {
            _warpMode = !_warpMode;
            UI.showToast(_warpMode ? "HYPERWARP: ON" : "CRUISE MODE", Settings.palette().primary, 1000);
        }
    }

    void updateAndRender(int16_t yOffset) override {
        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);

        int16_t cx = DisplayManager::kWidth / 2;
        int16_t cy = DisplayManager::kBodyY + DisplayManager::kBodyH / 2 + yOffset;
        float spd = (28.0f + Settings.get().starfieldSpeed * 28.0f) * (_warpMode ? 1.8f : 1.0f);
        float dt  = Anim.dt();

        for (auto& s : _stars) {
            s.prevZ = s.z;
            s.z -= spd * dt;
            if (s.z <= 4.0f) {
                resetStar(s, false);
                continue;
            }

            int16_t sx = cx + (int16_t)((s.x / s.z) * 95.0f);
            int16_t sy = cy + (int16_t)((s.y / s.z) * 95.0f);

            if (sx < 0 || sx >= DisplayManager::kWidth ||
                sy < DisplayManager::kBodyY || sy >= DisplayManager::kBodyY + DisplayManager::kBodyH) {
                resetStar(s, false);
                continue;
            }

            uint16_t bright = (uint16_t)constrain(255 - (int)(s.z * 1.2f), 40, 255);
            uint16_t col = AnimationEngine::lerpColor565(pal.gridDim, pal.highlight, bright);

            if (_warpMode || Settings.get().starfieldSpeed >= 4) {
                int16_t px = cx + (int16_t)((s.x / s.prevZ) * 95.0f);
                int16_t py = cy + (int16_t)((s.y / s.prevZ) * 95.0f);
                c.drawLine(px, py, sx, sy, col);
            } else {
                if (s.z < 65.0f) {
                    c.fillRect(sx, sy, 2, 2, col);
                } else {
                    c.drawPixel(sx, sy, col);
                }
            }
        }

        c.drawCircle(cx, cy, 14, pal.gridDim);
        c.drawFastHLine(cx - 20, cy, 5, pal.secondary);
        c.drawFastHLine(cx + 16, cy, 5, pal.secondary);
        c.drawFastVLine(cx, cy - 20, 5, pal.secondary);
        c.drawFastVLine(cx, cy + 16, 5, pal.secondary);
    }
};

// ============================================================================
// 10. DIGITAL CYBERPUNK CLOCK APPLICATION
// ============================================================================
class AppClock : public CyberApp {
    uint32_t _lastEpoch = 0;
    bool     _dirty     = true;
public:
    void onEnter() override { _dirty = true; }

    const char* hintA() const override { return "UTC TZ"; }
    const char* hintB() const override { return "NTP SYNC"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            int8_t tz = Settings.get().timezoneHours + 1;
            if (tz > 14) tz = -12;
            Settings.get().timezoneHours = tz;
            Settings.markDirty();
            _dirty = true;
            char buf[20];
            snprintf(buf, sizeof(buf), "TIMEZONE UTC%+d", tz);
            UI.showToast(buf, Settings.palette().secondary, 1200);
        } else if (ev == ButtonEvent::B_SHORT) {
            _dirty = true;
            if (Net.isConnected()) {
                long gmtOffset = (long)Settings.get().timezoneHours * 3600L;
                configTime(gmtOffset, 0, "pool.ntp.org", "time.cloudflare.com");
                UI.showToast("NTP SYNC REQUESTED", Settings.palette().ok, 1400);
            } else {
                UI.showToast("NO WIFI FOR NTP", Settings.palette().warning, 1400);
            }
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t ep = Net.currentEpoch();
        if (yOffset == 0 && !_dirty && ep == _lastEpoch) {
            return; // Redraw only when the second ticks -> 0% flicker!
        }
        _lastEpoch = ep;
        _dirty     = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        Display.drawCyberGrid(0, 0, pal.gridDim);

        char hms[16], dateStr[24];
        Net.getFormattedTime(hms, sizeof(hms), dateStr, sizeof(dateStr));

        Display.drawCyberPanel(6, by + 8, 123, 24, pal.panelBg,
                               Net.isNtpSynced() ? pal.ok : pal.warning, pal.secondary, 4);
        c.setTextColor(Net.isNtpSynced() ? pal.ok : pal.warning);
        c.setCursor(12, by + 16);
        c.print(Net.isNtpSynced() ? "SYNC: NTP LOCKED" : "SYNC: UNSYNCED EST");

        int16_t cx = DisplayManager::kWidth / 2;
        int16_t cy = by + 98;
        c.drawCircle(cx, cy, 54, pal.gridDim);
        c.drawCircle(cx, cy, 48, pal.secondary);

        uint32_t sec = ep % 60;
        float secAng = (sec / 60.0f) * 6.283185f - 1.570796f;
        int16_t sx = cx + (int16_t)(std::cos(secAng) * 51.0f);
        int16_t sy = cy + (int16_t)(std::sin(secAng) * 51.0f);
        c.fillCircle(sx, sy, 3, pal.primary);

        Display.drawCyberPanel(8, cy - 22, 119, 44, pal.panelBg, pal.primary, pal.highlight, 5);
        c.setTextSize(2);
        c.setTextColor(pal.highlight);
        c.setCursor(19, cy - 10);
        c.print(hms);
        c.setTextSize(1);

        Display.drawCyberPanel(6, by + 162, 123, 36, pal.panelBg, pal.gridDim, pal.primary, 4);
        c.setTextColor(pal.textMain);
        c.setCursor(16, by + 169);
        c.print(dateStr);

        char tzBuf[24];
        snprintf(tzBuf, sizeof(tzBuf), "ZONE: UTC%+d", Settings.get().timezoneHours);
        c.setTextColor(pal.secondary);
        c.setCursor(28, by + 183);
        c.print(tzBuf);
    }
};

// ============================================================================
// 11. SETTINGS & HARDWARE CONFIGURATION APPLICATION
// ============================================================================
class AppSettings : public CyberApp {
    static constexpr uint8_t kItemCount = 10;
    uint8_t _selIdx  = 0;
    bool    _infoBox = false;
    bool    _dirty   = true;
public:
    void onEnter() override {
        _selIdx  = 0;
        _infoBox = false;
        _dirty   = true;
    }

    const char* hintA() const override { return _infoBox ? "BACK" : "NEXT"; }
    const char* hintB() const override { return _infoBox ? "CLOSE" : "CHANGE"; }

    void handleInput(ButtonEvent ev) override {
        _dirty = true;
        if (_infoBox) {
            if (ev == ButtonEvent::A_SHORT || ev == ButtonEvent::B_SHORT) {
                _infoBox = false;
            }
            return;
        }

        if (ev == ButtonEvent::A_SHORT) {
            _selIdx = (_selIdx + 1) % kItemCount;
        } else if (ev == ButtonEvent::B_SHORT) {
            SystemSettings& s = Settings.get();
            switch (_selIdx) {
                case 0: // THEME
                    s.themeId = (s.themeId + 1) % THEME_COUNT;
                    Settings.markDirty();
                    UI.invalidateAll();
                    break;
                case 1: // BRIGHTNESS
                    if (s.brightness >= 240) s.brightness = 50;
                    else s.brightness = min<int>(255, s.brightness + 50);
                    Display.setBrightness(s.brightness);
                    Settings.markDirty();
                    break;
                case 2: // GLITCH FX
                    s.animIntensity = (s.animIntensity == 0) ? 2 : 0;
                    Settings.markDirty();
                    break;
                case 3: // UI SPEED
                    s.animSpeed = (s.animSpeed + 1) % 3;
                    Settings.markDirty();
                    break;
                case 4: // FPS CAP
                    if (s.targetFps == 30) s.targetFps = 60;
                    else if (s.targetFps == 60) s.targetFps = 90;
                    else if (s.targetFps == 90) s.targetFps = 120;
                    else s.targetFps = 30;
                    Anim.setTargetFps(s.targetFps);
                    Settings.markDirty();
                    break;
                case 5: // SOUND FX
                    s.soundEnabled = !s.soundEnabled;
                    Settings.markDirty();
                    break;
                case 6: // WIFI AP
                    if (Net.isConfigPortalActive()) {
                        Net.stopConfigPortal();
                        UI.showToast("WIFI AP STOPPED", Settings.palette().warning);
                    } else {
                        Net.startConfigPortal();
                        UI.showToast("AP: 192.168.4.1", Settings.palette().ok, 2200);
                    }
                    break;
                case 7: // WIFI RESET
                    UI.showConfirmModal("WIPE WIFI NVS", "DELETE SAVED WIFI", "CREDENTIALS?", []() {
                        Settings.clearWifiCredentials();
                        Net.disconnectWifi();
                        UI.showToast("WIFI CREDENTIALS WIPED", Settings.palette().danger);
                    });
                    break;
                case 8: // HW INFO
                    _infoBox = true;
                    break;
                case 9: // FACTORY RESET
                    UI.showConfirmModal("FACTORY RESET", "RESTORE ALL DEFAULT", "DECK SETTINGS?", []() {
                        Settings.resetToDefaults();
                        Display.setBrightness(Settings.get().brightness);
                        Anim.setTargetFps(Settings.get().targetFps);
                        UI.showToast("DEFAULTS RESTORED", Settings.palette().ok);
                    });
                    break;
            }
        }
    }

    void updateAndRender(int16_t yOffset) override {
        if (yOffset == 0 && !_dirty) return;
        _dirty = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        if (_infoBox) {
            Display.drawCyberPanel(4, by + 6, 127, 190, pal.panelBg, pal.primary, pal.secondary, 5);
            c.setTextColor(pal.primary);
            c.setCursor(8, by + 14);
            c.print("// HW PINOUT & FW");

            c.setTextColor(pal.textMain);
            c.setCursor(8, by + 32); c.print("PANEL: ST7789 135x240");
            char b[28];
            snprintf(b, sizeof(b), "SPI: MOSI=%d CLK=%d", PIN_TFT_MOSI, PIN_TFT_SCLK);
            c.setCursor(8, by + 46); c.print(b);
            snprintf(b, sizeof(b), "CS=%d DC=%d RST=%d BL=%d", PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL);
            c.setCursor(8, by + 60); c.print(b);
            snprintf(b, sizeof(b), "OFFSET: X=%d Y=%d", TFT_OFFSET_X, TFT_OFFSET_Y);
            c.setCursor(8, by + 74); c.print(b);
            snprintf(b, sizeof(b), "BUTTONS: A=%d B=%d", PIN_BTN_A, PIN_BTN_B);
            c.setCursor(8, by + 88); c.print(b);

            c.setTextColor(pal.secondary);
            c.setCursor(8, by + 108); c.print("WEB CONSOLE AP:");
            c.setTextColor(pal.highlight);
            c.setCursor(8, by + 122); c.print(Net.portalSsid());
            c.setCursor(8, by + 136); c.print("HTTP://192.168.4.1");

            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 162); c.print("FW: " NEON_DECK_VERSION);
            return;
        }

        int16_t listTop = by + 6;
        uint8_t visRows = 8;
        if (Net.isConfigPortalActive()) {
            Display.drawCyberPanel(3, by + 4, 129, 36, pal.panelBg, pal.warning, pal.primary, 3);
            c.setTextColor(pal.warning);
            c.setCursor(7, by + 10);
            c.print("AP: "); c.print(Net.portalSsid());
            c.setTextColor(pal.highlight);
            c.setCursor(7, by + 24);
            c.print("HTTP://192.168.4.1");
            listTop = by + 44;
            visRows = 6;
        }

        uint8_t startIdx = 0;
        if (_selIdx >= visRows) {
            startIdx = _selIdx - (visRows - 1);
        }

        const SystemSettings& s = Settings.get();
        for (uint8_t r = 0; r < visRows && (startIdx + r) < kItemCount; ++r) {
            uint8_t idx = startIdx + r;
            int16_t ry  = listTop + r * 24;
            const char* label = "";
            char val[16] = "";

            switch (idx) {
                case 0:
                    label = "THEME";
                    std::strncpy(val, Settings.palette().name, 8);
                    val[8] = '\0';
                    break;
                case 1:
                    label = "BRIGHT";
                    snprintf(val, sizeof(val), "%u%%", (unsigned)(s.brightness * 100 / 255));
                    break;
                case 2:
                    label = "GLITCH FX";
                    std::strcpy(val, (s.animIntensity > 0) ? "ON" : "OFF");
                    break;
                case 3:
                    label = "UI SPEED";
                    std::strcpy(val, (s.animSpeed == 0) ? "FAST" : (s.animSpeed == 1) ? "NORM" : "SLOW");
                    break;
                case 4:
                    label = "FPS CAP";
                    snprintf(val, sizeof(val), "%uHz", s.targetFps);
                    break;
                case 5:
                    label = "SOUND";
                    std::strcpy(val, (PIN_BUZZER < 0) ? "NO HW" : (s.soundEnabled ? "ON" : "OFF"));
                    break;
                case 6:
                    label = "WIFI AP";
                    std::strcpy(val, Net.isConfigPortalActive() ? "ON" : "START");
                    break;
                case 7:
                    label = "WIFI RST";
                    std::strcpy(val, "WIPE");
                    break;
                case 8:
                    label = "HW INFO";
                    std::strcpy(val, "VIEW");
                    break;
                case 9:
                    label = "DEFAULTS";
                    std::strcpy(val, "RESET");
                    break;
            }
            UI.drawMenuRow(ry, label, val, idx == _selIdx, idx == 6 && Net.isConfigPortalActive());
        }

        Display.drawScrollbar(listTop, visRows * 24 - 4, kItemCount, visRows, _selIdx);
    }
};

// Static instances (zero dynamic heap allocation)
static AppSystem        s_appSystem;
static AppWifiRadar     s_appWifiRadar;
static AppPacketSniffer s_appPktSniffer;
static AppNetTools      s_appNetTools;
static AppWebDeck       s_appWebDeck;
static AppCryptoLab     s_appCryptoLab;
static AppTerminal      s_appTerminal;
static AppMatrix        s_appMatrix;
static AppStarfield     s_appStarfield;
static AppClock         s_appClock;
static AppSettings      s_appSettings;

CyberApp* getAppInstance(AppId id) {
    switch (id) {
        case APP_SYSTEM:      return &s_appSystem;
        case APP_WIFI_RADAR:  return &s_appWifiRadar;
        case APP_PKT_SNIFFER: return &s_appPktSniffer;
        case APP_NET_TOOLS:   return &s_appNetTools;
        case APP_WEB_DECK:    return &s_appWebDeck;
        case APP_CRYPTO_LAB:  return &s_appCryptoLab;
        case APP_TERMINAL:    return &s_appTerminal;
        case APP_MATRIX:      return &s_appMatrix;
        case APP_STARFIELD:   return &s_appStarfield;
        case APP_CLOCK:       return &s_appClock;
        case APP_SETTINGS:    return &s_appSettings;
        default:              return &s_appSystem;
    }
}
