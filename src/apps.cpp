#include "apps.h"
#include "display_manager.h"
#include "animation_engine.h"
#include "settings_manager.h"
#include "wifi_manager.h"
#include <cmath>
#include <cstring>
#include <mbedtls/sha256.h>

static const AppMetadata kAppMetaEN[APP_COUNT] = {
    { APP_SYSTEM,      "SYSTEM",      "DUAL-CORE & RAM HUD",   "MOD-01 // SYS" },
    { APP_WIFI_RADAR,  "WIFI RADAR",  "2.4GHZ AP INSPECTOR",   "MOD-02 // RAD" },
    { APP_PKT_SNIFFER, "PKT SNIFFER", "802.11 TRAFFIC & CH",   "MOD-03 // SNF" },
    { APP_NET_TOOLS,   "NET TOOLS",   "IP SONAR & HOST PING",  "MOD-04 // NET" },
    { APP_WEB_DECK,    "WEB DECK",    "REMOTE HTTP CONSOLE",   "MOD-05 // WEB" },
    { APP_CYBER_EYE,   "CYBER EYE",   "VIRTUAL PET KEYCHAIN",  "MOD-06 // EYE" },
    { APP_CUBE_3D,     "3D CUBE",     "WIREFRAME TESSERACT",   "MOD-07 // 3DC" },
    { APP_TERMINAL,    "TERMINAL",    "LOCAL DECK SHELL",      "MOD-08 // SHL" },
    { APP_MATRIX,      "MATRIX",      "DIGITAL RAIN STREAM",   "MOD-09 // MTX" },
    { APP_STARFIELD,   "STARFIELD",   "3D WARP VECTOR SIM",    "MOD-10 // STR" },
    { APP_CLOCK,       "CLOCK",       "CUSTOM & WIFI TIME",    "MOD-11 // CLK" },
    { APP_SETTINGS,    "SETTINGS",    "CONFIG & TUNING",       "MOD-12 // CFG" }
};

static const AppMetadata kAppMetaUA[APP_COUNT] = {
    { APP_SYSTEM,      "СИСТЕМА",     "ПРОЦЕСОР ТА ПАМ'ЯТЬ",   "МОД-01 // СИС" },
    { APP_WIFI_RADAR,  "WI-FI РАДАР", "АНАЛІЗАТОР 2.4 ГГЦ",    "МОД-02 // РАД" },
    { APP_PKT_SNIFFER, "СНІФФЕР",     "ТРАФІК ПАКЕТІВ 802.11", "МОД-03 // СНФ" },
    { APP_NET_TOOLS,   "МЕРЕЖА",      "ПІНГ ТА СОНАР ХОСТА",   "МОД-04 // МЕР" },
    { APP_WEB_DECK,    "ВЕБ-ДЕК",     "ДИСТАНЦІЙНА КОНСОЛЬ",   "МОД-05 // ВЕБ" },
    { APP_CYBER_EYE,   "КІБЕР-ОКО",   "ВИХОВАНЕЦЬ-БРЕЛОК",     "МОД-06 // ОКО" },
    { APP_CUBE_3D,     "3D КУБ",      "КАРКАСНИЙ ТЕСЕРАКТ",    "МОД-07 // КУБ" },
    { APP_TERMINAL,    "ТЕРМІНАЛ",    "ЛОКАЛЬНИЙ ШЕЛЛ",        "МОД-08 // ШЕЛ" },
    { APP_MATRIX,      "МАТРИЦЯ",     "ПОТІК ЦИФРОВОГО ДОЩУ",  "МОД-09 // МТХ" },
    { APP_STARFIELD,   "ЗОРІ 3D",     "КОСМІЧНИЙ ГІПЕРПРОСТІР", "МОД-10 // ЗОР" },
    { APP_CLOCK,       "ГОДИННИК",    "ЧАС З ТЕЛЕФОНУ ТА NTP", "МОД-11 // ЧАС" },
    { APP_SETTINGS,    "НАЛАШТУВ.",   "LITTLEFS ТА СИСТЕМА",   "МОД-12 // НАЛ" }
};

const AppMetadata& getAppMeta(uint8_t idx) {
    bool ua = (Settings.get().language == 1);
    return ua ? kAppMetaUA[idx % APP_COUNT] : kAppMetaEN[idx % APP_COUNT];
}

// ============================================================================
// 1. SYSTEM APPLICATION (MINIMALIST)
// ============================================================================
class AppSystem : public CyberApp {
    uint32_t _lastRenderMs = 0;
    bool     _forceRedraw  = true;
public:
    void onEnter() override {
        _forceRedraw = true;
    }

    const char* hintA() const override { return "REFRESH"; }
    const char* hintB() const override { return "FPS HUD"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
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
        if (yOffset == 0 && !_forceRedraw && (now - _lastRenderMs) < 300) {
            return;
        }
        _lastRenderMs = now;
        _forceRedraw  = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        char buf[32];
        uint32_t freeHeap  = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        float usedRatio = (totalHeap > 0) ? (1.0f - ((float)freeHeap / (float)totalHeap)) : 0.4f;
        uint32_t upSec = now / 1000UL;

        auto drawRow = [&](int16_t y, const char* k, const char* v, uint16_t vCol) {
            c.setTextColor(pal.textDim);
            c.setCursor(10, y);
            c.print(k);
            int16_t vw = std::strlen(v) * 6;
            c.setTextColor(vCol);
            c.setCursor(DisplayManager::kWidth - 10 - vw, y);
            c.print(v);
        };

        snprintf(buf, sizeof(buf), "%lu MHz", (unsigned long)ESP.getCpuFreqMHz());
        drawRow(by + 20, "CPU", buf, pal.highlight);

        snprintf(buf, sizeof(buf), "%lu KB", (unsigned long)(freeHeap / 1024));
        drawRow(by + 48, "FREE RAM", buf, pal.primary);
        Display.drawProgressBar(10, by + 62, DisplayManager::kWidth - 20, 3, usedRatio, pal.primary, pal.gridDim, pal.panelBg);

        snprintf(buf, sizeof(buf), "%.0f FPS", Anim.fps());
        drawRow(by + 84, "DISPLAY", buf, pal.secondary);

        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                 (unsigned long)(upSec / 3600), (unsigned long)((upSec % 3600) / 60), (unsigned long)(upSec % 60));
        drawRow(by + 114, "UPTIME", buf, pal.textMain);

        if (Net.isConnected()) {
            snprintf(buf, sizeof(buf), "%ld dBm", (long)Net.currentRssi());
            drawRow(by + 144, "WIFI", buf, pal.ok);
        } else {
            drawRow(by + 144, "WIFI", Net.isConfigPortalActive() ? "AP ON" : "OFFLINE",
                    Net.isConfigPortalActive() ? pal.secondary : pal.textDim);
        }

        if (Net.hasValidBattery()) {
            snprintf(buf, sizeof(buf), "%u%% %.2fV %s",
                     Net.batteryPercent(), Net.batteryVoltage(),
                     Net.isCharging() ? "CHRG" : "BAT");
            drawRow(by + 174, "BATTERY", buf, Net.isCharging() ? pal.highlight : pal.ok);
        } else {
            drawRow(by + 174, "POWER", "USB 5V (NO BAT)", pal.secondary);
        }
    }
};

// ============================================================================
// 2. WIFI RADAR APPLICATION (MINIMALIST)
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

    const char* hintA() const override { return "NEXT"; }
    const char* hintB() const override { return "SCAN"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            if (Net.scannedCount() > 0) {
                _selectedIdx = (_selectedIdx + 1) % Net.scannedCount();
                _dirty = true;
            }
        } else if (ev == ButtonEvent::B_SHORT) {
            if (Net.startAsyncScan()) {
                _dirty = true;
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

        size_t count = Net.scannedCount();
        if (_selectedIdx >= count && count > 0) {
            _selectedIdx = 0;
        }

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 10);
        if (Net.isScanning()) {
            c.setTextColor(pal.primary);
            c.print("SCANNING...");
        } else {
            char hdr[24];
            snprintf(hdr, sizeof(hdr), "NETWORKS: %u", (unsigned)count);
            c.print(hdr);
        }

        if (count == 0 && !Net.isScanning()) {
            c.setTextColor(pal.textDim);
            c.setCursor(24, by + 94);
            c.print("NO NETWORKS");
            return;
        }

        constexpr size_t kVisRows = 7;
        size_t startIdx = 0;
        if (_selectedIdx >= kVisRows) {
            startIdx = _selectedIdx - (kVisRows - 1);
        }

        for (size_t row = 0; row < kVisRows && (startIdx + row) < count; ++row) {
            size_t idx = startIdx + row;
            const ScannedApInfo& ap = Net.scannedAp(idx);
            bool sel = (idx == _selectedIdx);
            int16_t ry = by + 28 + row * 24;

            if (sel) {
                c.fillRect(6, ry, DisplayManager::kWidth - 12, 20, pal.panelBg);
                c.fillRect(6, ry + 2, 2, 16, pal.primary);
            }

            char ssidShort[14];
            std::strncpy(ssidShort, ap.ssid, 13);
            ssidShort[13] = '\0';
            c.setTextColor(ap.isConnectedAp ? pal.ok : (sel ? pal.highlight : pal.textMain));
            c.setCursor(14, ry + 6);
            c.print(ssidShort);

            char rssiBuf[8];
            snprintf(rssiBuf, sizeof(rssiBuf), "%ld", (long)ap.rssi);
            int16_t rw = std::strlen(rssiBuf) * 6;
            c.setTextColor(sel ? pal.secondary : pal.textDim);
            c.setCursor(DisplayManager::kWidth - 10 - rw, ry + 6);
            c.print(rssiBuf);
        }
    }
};

// ============================================================================
// 3. PACKET SNIFFER (MINIMALIST)
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

    const char* hintA() const override { return "CH"; }
    const char* hintB() const override { return Net.sniffer().hopping ? "LOCK" : "HOP"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Net.stepSnifferChannel();
            _lastRenderMs = 0;
        } else if (ev == ButtonEvent::B_SHORT) {
            Net.toggleSnifferHopping();
            _lastRenderMs = 0;
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

        char chBuf[24];
        snprintf(chBuf, sizeof(chBuf), "CH %02u %s", sn.currentChannel, sn.hopping ? "[HOP]" : "[LOCK]");
        c.setTextColor(pal.secondary);
        c.setCursor(10, by + 16);
        c.print(chBuf);

        char rateBuf[16];
        snprintf(rateBuf, sizeof(rateBuf), "%lu", (unsigned long)sn.pktPerSec);
        c.setTextSize(2);
        c.setTextColor(pal.highlight);
        c.setCursor(10, by + 42);
        c.print(rateBuf);
        c.setTextSize(1);
        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 64);
        c.print("PACKETS / SEC");

        // Single clean waveform at the bottom
        int16_t wx = 10, wy = by + 104, ww = DisplayManager::kWidth - 20, wh = 76;
        c.drawFastHLine(wx, wy + wh, ww, pal.gridDim);

        int16_t prevX = -1, prevY = -1;
        for (uint8_t i = 0; i < 32; ++i) {
            uint8_t idx = (sn.historyIdx + i) % 32;
            uint16_t pps = sn.historyPps[idx];
            int16_t px = wx + (i * ww) / 31;
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
// 4. NETWORK TOOLS APPLICATION (MINIMALIST)
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

    const char* hintA() const override { return "HOST"; }
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

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 16);
        c.print("IP  ");
        c.setTextColor(Net.isConnected() ? pal.textMain : pal.warning);
        c.print(Net.isConnected() ? Net.localIp() : String("OFFLINE"));

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 44);
        c.print("TARGET");
        c.setTextColor(pal.highlight);
        c.setCursor(10, by + 58);
        c.print(_activeTarget);

        const PingResult& pr = Net.pingStats();
        uint16_t stCol = pr.inProgress ? pal.warning : (pr.lastSuccess ? pal.ok : pal.danger);
        c.setTextColor(stCol);
        c.setCursor(10, by + 88);
        c.print(pr.statusMsg);

        int16_t gx = 10, gy = by + 126, gw = DisplayManager::kWidth - 20, gh = 54;
        c.drawFastHLine(gx, gy + gh, gw, pal.gridDim);
        for (uint8_t i = 0; i < 24; ++i) {
            uint8_t idx = (pr.historyIdx + i) % 24;
            uint16_t ms = pr.historyMs[idx];
            int16_t bx = gx + 2 + i * 4;
            if (ms > 0) {
                int16_t bh = constrain((int16_t)((ms * gh) / 200), 3, gh - 2);
                c.fillRect(bx, gy + gh - bh, 3, bh, pal.primary);
            } else if (i < pr.sentCount) {
                c.fillRect(bx, gy + gh - 3, 3, 3, pal.danger);
            }
        }
    }
};

// ============================================================================
// 5. WEB DECK — REMOTE HTTP CYBER CONSOLE (MINIMALIST)
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
    const char* hintB() const override { return "PING UI"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            if (Net.isConfigPortalActive()) {
                Net.stopConfigPortal();
            } else {
                Net.startConfigPortal();
            }
            _lastRenderMs = 0;
        } else if (ev == ButtonEvent::B_SHORT) {
            UI.showToast("WEB CONSOLE ACTIVE", Settings.palette().secondary, 1200);
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

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 24);
        c.print("WIFI HOTSPOT");
        c.setTextColor(Net.isConfigPortalActive() ? pal.ok : pal.warning);
        c.setCursor(10, by + 40);
        c.print(Net.isConfigPortalActive() ? Net.portalSsid() : "STOPPED (PRESS A)");

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 76);
        c.print("WEB URL");
        c.setTextColor(pal.highlight);
        c.setCursor(10, by + 92);
        c.print(Net.isConnected() ? Net.localIp() : String("192.168.4.1"));

        c.setTextColor(pal.textDim);
        c.setCursor(10, by + 130);
        c.print("OPEN IN PHONE BROWSER");
        c.setCursor(10, by + 144);
        c.print("TO SYNC WIFI & CLOCK");
    }
};

// ============================================================================
// 6. CYBER EYE — VIRTUAL CYBER PET KEYCHAIN (GRAPHICAL DEMO)
// ============================================================================
class AppCyberEye : public CyberApp {
    uint8_t  _mood = 0; // 0=CYBER, 1=SQUINT, 2=HAPPY, 3=TARGET
    uint32_t _nextBlinkMs = 0;
    uint32_t _blinkStartMs = 0;
    bool     _blinking = false;
    float    _lookX = 0.0f;
    float    _lookY = 0.0f;
    float    _targetLookX = 0.0f;
    float    _targetLookY = 0.0f;
    uint32_t _nextLookChangeMs = 0;

public:
    void onEnter() override {
        _nextBlinkMs = millis() + 2000;
        _nextLookChangeMs = millis() + 1000;
    }

    const char* hintA() const override { return "MOOD"; }
    const char* hintB() const override { return "BLINK"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _mood = (_mood + 1) % 4;
            Buttons.playClickTone(1800, 20);
        } else if (ev == ButtonEvent::B_SHORT) {
            _blinking = true;
            _blinkStartMs = millis();
            Buttons.playClickTone(2400, 30);
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t now = millis();
        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        // Auto look direction change
        if (now >= _nextLookChangeMs) {
            _nextLookChangeMs = now + 1200 + (esp_random() % 2000);
            _targetLookX = ((int)(esp_random() % 31) - 15) * 1.0f;
            _targetLookY = ((int)(esp_random() % 21) - 10) * 1.0f;
        }
        _lookX += (_targetLookX - _lookX) * 0.15f;
        _lookY += (_targetLookY - _lookY) * 0.15f;

        // Auto blink logic
        if (!_blinking && now >= _nextBlinkMs) {
            _blinking = true;
            _blinkStartMs = now;
            _nextBlinkMs = now + 2500 + (esp_random() % 3500);
        }

        float blinkFactor = 1.0f; // 1.0 = fully open, 0.0 = fully closed
        if (_blinking) {
            uint32_t dt = now - _blinkStartMs;
            if (dt < 90) {
                blinkFactor = 1.0f - (dt / 90.0f);
            } else if (dt < 180) {
                blinkFactor = (dt - 90) / 90.0f;
            } else {
                _blinking = false;
                blinkFactor = 1.0f;
            }
        }

        int16_t cx = DisplayManager::kWidth / 2;
        int16_t cy = by + 95;

        // Title & Mood status
        const char* moodNames[] = { "CYBER PET", "HUNTER SQUINT", "HAPPY ARC", "RED OVERCLOCK" };
        c.setTextColor(pal.secondary);
        int16_t tw = std::strlen(moodNames[_mood]) * 6;
        c.setCursor((DisplayManager::kWidth - tw) / 2, by + 16);
        c.print(moodNames[_mood]);

        // Eye geometry
        int16_t eyeW = 50;
        int16_t eyeH = (int16_t)(32.0f * blinkFactor);

        uint16_t eyeCol = (_mood == 3) ? pal.danger : pal.primary;
        uint16_t irisCol = (_mood == 3) ? pal.warning : pal.highlight;

        if (eyeH < 3) {
            // Fully closed eye slit
            c.drawFastHLine(cx - eyeW, cy, eyeW * 2, eyeCol);
        } else {
            if (_mood == 2) {
                // Happy anime curve (^ _ ^)
                for (int16_t dx = -eyeW; dx <= eyeW; dx += 2) {
                    float norm = (float)dx / (float)eyeW;
                    int16_t dy = (int16_t)((1.0f - norm * norm) * eyeH);
                    c.drawPixel(cx + dx, cy - dy, eyeCol);
                    c.drawPixel(cx + dx, cy - dy + 1, eyeCol);
                }
            } else {
                // Cyber Outer Eye Sclera Outline
                c.drawEllipse(cx, cy, eyeW, eyeH, eyeCol);
                c.drawEllipse(cx, cy, eyeW - 1, eyeH - 1, AnimationEngine::lerpColor565(pal.bg, eyeCol, 120));

                // Pupil / Iris with animated look position
                int16_t px = cx + (int16_t)_lookX;
                int16_t py = cy + (int16_t)(_lookY * blinkFactor);

                int16_t irisR = (_mood == 1) ? 14 : 18;
                c.drawCircle(px, py, (int16_t)(irisR * blinkFactor), eyeCol);
                c.fillCircle(px, py, (int16_t)(8 * blinkFactor), irisCol);

                // Cyber glint highlight
                if (blinkFactor > 0.6f) {
                    c.fillCircle(px - 3, py - 3, 2, pal.highlight);
                }

                // Decorative cyber corner brackets
                c.drawFastHLine(cx - eyeW - 4, cy - eyeH - 2, 6, pal.secondary);
                c.drawFastVLine(cx - eyeW - 4, cy - eyeH - 2, 6, pal.secondary);
                c.drawFastHLine(cx + eyeW - 2, cy - eyeH - 2, 6, pal.secondary);
                c.drawFastVLine(cx + eyeW + 4, cy - eyeH - 2, 6, pal.secondary);
                c.drawFastHLine(cx - eyeW - 4, cy + eyeH + 2, 6, pal.secondary);
                c.drawFastVLine(cx - eyeW - 4, cy + eyeH - 4, 6, pal.secondary);
                c.drawFastHLine(cx + eyeW - 2, cy + eyeH + 2, 6, pal.secondary);
                c.drawFastVLine(cx + eyeW + 4, cy + eyeH - 4, 6, pal.secondary);
            }
        }

        // Bottom heartbeat / keychain pulse
        int16_t barY = by + 170;
        c.drawFastHLine(14, barY, DisplayManager::kWidth - 28, pal.gridDim);
        int16_t pulseX = 14 + ((now / 20) % (DisplayManager::kWidth - 28));
        c.fillRect(pulseX - 3, barY - 2, 6, 5, eyeCol);
    }
};

// ============================================================================
// 7. 3D WIRE CUBE / TESSERACT (REAL-TIME 3D GRAPHICAL DEMO)
// ============================================================================
class AppCube3D : public CyberApp {
    float _angleX = 0.0f;
    float _angleY = 0.0f;
    float _angleZ = 0.0f;
    uint8_t _mode = 0; // 0 = CUBE, 1 = TESSERACT
    uint8_t _speed = 2;

    struct Vec3 { float x, y, z; };
    struct Point2D { int16_t x, y; };

    Point2D project(Vec3 v, int16_t cx, int16_t cy, float scale, float distance) {
        float radX = _angleX;
        float y1 = v.y * std::cos(radX) - v.z * std::sin(radX);
        float z1 = v.y * std::sin(radX) + v.z * std::cos(radX);

        float radY = _angleY;
        float x2 = v.x * std::cos(radY) + z1 * std::sin(radY);
        float z2 = -v.x * std::sin(radY) + z1 * std::cos(radY);

        float radZ = _angleZ;
        float x3 = x2 * std::cos(radZ) - y1 * std::sin(radZ);
        float y3 = x2 * std::sin(radZ) + y1 * std::cos(radZ);

        float zPersp = z2 + distance;
        float f = (zPersp > 0.1f) ? (scale / zPersp) : scale;
        return Point2D{ (int16_t)(cx + x3 * f), (int16_t)(cy + y3 * f) };
    }

public:
    const char* hintA() const override { return "SPEED"; }
    const char* hintB() const override { return (_mode == 0) ? "4D TESS" : "3D CUBE"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            _speed = (_speed % 4) + 1;
        } else if (ev == ButtonEvent::B_SHORT) {
            _mode = (_mode + 1) % 2;
        }
    }

    void updateAndRender(int16_t yOffset) override {
        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        float dt = Anim.dt();
        float spd = (0.6f + _speed * 0.7f);
        _angleX += 0.8f * spd * dt;
        _angleY += 1.2f * spd * dt;
        _angleZ += 0.5f * spd * dt;

        int16_t cx = DisplayManager::kWidth / 2;
        int16_t cy = by + 98;

        c.setTextColor(pal.secondary);
        c.setCursor(10, by + 14);
        c.print((_mode == 0) ? "3D WIRE CUBE" : "4D TESSERACT");

        static const Vec3 kCubeVerts[8] = {
            {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
            {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
        };

        static const uint8_t kEdges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        Point2D pOuter[8];
        for (int i = 0; i < 8; ++i) {
            pOuter[i] = project(kCubeVerts[i], cx, cy, 140.0f, 3.8f);
        }

        // Draw Outer Cube
        for (int i = 0; i < 12; ++i) {
            c.drawLine(pOuter[kEdges[i][0]].x, pOuter[kEdges[i][0]].y,
                       pOuter[kEdges[i][1]].x, pOuter[kEdges[i][1]].y, pal.primary);
        }

        if (_mode == 1) {
            // Draw Inner Tesseract Cube & connection struts
            Point2D pInner[8];
            for (int i = 0; i < 8; ++i) {
                Vec3 vInner = { kCubeVerts[i].x * 0.5f, kCubeVerts[i].y * 0.5f, kCubeVerts[i].z * 0.5f };
                pInner[i] = project(vInner, cx, cy, 140.0f, 3.8f);
            }
            for (int i = 0; i < 12; ++i) {
                c.drawLine(pInner[kEdges[i][0]].x, pInner[kEdges[i][0]].y,
                           pInner[kEdges[i][1]].x, pInner[kEdges[i][1]].y, pal.secondary);
            }
            for (int i = 0; i < 8; ++i) {
                c.drawLine(pOuter[i].x, pOuter[i].y, pInner[i].x, pInner[i].y, pal.highlight);
            }
        }

        // Draw Vertices
        for (int i = 0; i < 8; ++i) {
            c.fillRect(pOuter[i].x - 1, pOuter[i].y - 1, 3, 3, pal.highlight);
        }

        char spdBuf[16];
        snprintf(spdBuf, sizeof(spdBuf), "SPD %ux", _speed);
        c.setTextColor(pal.textDim);
        c.setCursor(DisplayManager::kWidth - 46, by + 14);
        c.print(spdBuf);

        // Real-time FPS & Frame time Diagnostics for 3D demo
        char fpsBuf[16];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", Anim.fps());
        c.setTextColor(pal.ok);
        c.setCursor(10, by + 172);
        c.print(fpsBuf);

        char frameTimeBuf[16];
        snprintf(frameTimeBuf, sizeof(frameTimeBuf), "%.1f ms", Anim.frameTimeMs());
        c.setTextColor(pal.textDim);
        c.setCursor(DisplayManager::kWidth - 48, by + 172);
        c.print(frameTimeBuf);
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

    static constexpr uint8_t kCmdCount = 10;
    const char* const _cmds[kCmdCount] = {
        "help", "sysinfo", "wifi", "scan", "ping", "keygen", "uptime", "theme", "cat", "reboot"
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
            appendLine("theme cat reboot");
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
        } else if (std::strcmp(cmd, "cat") == 0) {
            UI.triggerCyberCat();
            appendLine("(=^･ω･^=) PURR!");
        } else if (std::strcmp(cmd, "reboot") == 0) {
            UI.showConfirmModal("SYSTEM REBOOT", "RESTART TTGO_CyberDeck", "FIRMWARE NOW?", []() {
                ESP.restart();
            });
        }
    }

public:
    AppTerminal() {
        appendLine("TTGO_CyberDeck v1.2");
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

        for (uint8_t i = 0; i < _lineCount; ++i) {
            bool isCmd = (std::strncmp(_lines[i], "deck:", 5) == 0);
            c.setTextColor(isCmd ? pal.secondary : pal.primary);
            c.setCursor(8, by + 8 + i * 13);
            c.print(_lines[i]);
        }

        int16_t cmdY = by + 176;
        c.drawFastHLine(6, cmdY, DisplayManager::kWidth - 12, pal.gridDim);
        c.setTextColor(pal.secondary);
        c.setCursor(8, cmdY + 8);
        c.print("$>");
        c.setTextColor(pal.highlight);
        c.setCursor(24, cmdY + 8);
        c.print(_cmds[_cmdIdx]);

        int16_t curX = 24 + std::strlen(_cmds[_cmdIdx]) * 6 + 2;
        c.fillRect(curX, cmdY + 7, 4, 8, pal.primary);
    }
};

// ============================================================================
// 8. MATRIX DIGITAL RAIN APPLICATION (MINIMALIST)
// ============================================================================
class AppMatrix : public CyberApp {
    static constexpr uint8_t kCols = 22;
    float   _headY[kCols];
    float   _speed[kCols];
    uint8_t _len[kCols];
    bool    _initialized = false;

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
    }

    const char* hintA() const override { return "SPEED"; }
    const char* hintB() const override { return "DENSITY"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Settings.get().matrixSpeed = (Settings.get().matrixSpeed % 5) + 1;
            Settings.markDirty();
        } else if (ev == ButtonEvent::B_SHORT) {
            Settings.get().matrixDensity = (Settings.get().matrixDensity % 5) + 1;
            Settings.markDirty();
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
    }
};

// ============================================================================
// 9. STARFIELD 3D WARP APPLICATION (MINIMALIST)
// ============================================================================
class AppStarfield : public CyberApp {
    struct Star {
        float x, y, z, prevZ;
    };
    static constexpr size_t kNumStars = 55;
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
    const char* hintB() const override { return "WARP"; }

    void handleInput(ButtonEvent ev) override {
        if (ev == ButtonEvent::A_SHORT) {
            Settings.get().starfieldSpeed = (Settings.get().starfieldSpeed % 5) + 1;
            Settings.markDirty();
        } else if (ev == ButtonEvent::B_SHORT) {
            _warpMode = !_warpMode;
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
                c.drawPixel(sx, sy, col);
            }
        }
    }
};

// ============================================================================
// 10. DIGITAL CYBERPUNK CLOCK APPLICATION (MANUAL EDIT + WI-FI SYNC)
// ============================================================================
class AppClock : public CyberApp {
    uint32_t _lastEpoch = 0;
    bool     _dirty     = true;
    bool     _editMode  = false;
    uint8_t  _editField = 0; // 0=Hour, 1=Min, 2=Day, 3=Mon, 4=Year
    int      _setHour   = 12;
    int      _setMin    = 0;
    int      _setDay    = 1;
    int      _setMon    = 1;
    int      _setYear   = 2026;

    void loadCurrentIntoEditor() {
        time_t local = (time_t)(Net.currentEpoch() + (Settings.get().timezoneHours * 3600));
        struct tm tmStruct;
        gmtime_r(&local, &tmStruct);
        _setHour = tmStruct.tm_hour;
        _setMin  = tmStruct.tm_min;
        _setDay  = tmStruct.tm_mday;
        _setMon  = tmStruct.tm_mon + 1;
        _setYear = tmStruct.tm_year + 1900;
        if (_setYear < 2024) _setYear = 2026;
    }

    void saveEditorTime() {
        struct tm t = {};
        t.tm_year = _setYear - 1900;
        t.tm_mon  = _setMon - 1;
        t.tm_mday = _setDay;
        t.tm_hour = _setHour;
        t.tm_min  = _setMin;
        t.tm_sec  = 0;
        t.tm_isdst = -1;
        time_t localEpoch = mktime(&t);
        time_t utcEpoch = localEpoch - (Settings.get().timezoneHours * 3600);
        Net.setWifiEpoch((uint32_t)utcEpoch);
    }

public:
    void onEnter() override {
        _dirty = true;
        _editMode = false;
    }

    const char* hintA() const override {
        return _editMode ? "+1 VALUE" : "UTC TZ";
    }

    const char* hintB() const override {
        return _editMode ? "NEXT/HOLD SAVE" : "WIFI / HOLD EDIT";
    }

    void handleInput(ButtonEvent ev) override {
        if (!_editMode) {
            if (ev == ButtonEvent::A_SHORT) {
                int8_t tz = Settings.get().timezoneHours + 1;
                if (tz > 14) tz = -12;
                Settings.get().timezoneHours = tz;
                Settings.markDirty();
                _dirty = true;
                char buf[20];
                snprintf(buf, sizeof(buf), "UTC%+d", tz);
                UI.showToast(buf, Settings.palette().secondary, 1000);
            } else if (ev == ButtonEvent::B_SHORT) {
                _dirty = true;
                if (Net.isConnected()) {
                    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
                    UI.showToast("WIFI NTP SYNC...", Settings.palette().ok, 1500);
                } else {
                    if (Settings.hasWifiCredentials()) {
                        Net.connectStoredWifi();
                    }
                    if (!Net.isConfigPortalActive()) {
                        Net.startConfigPortal();
                    }
                    UI.showToast("AP: 192.168.4.1", Settings.palette().secondary, 2200);
                }
            } else if (ev == ButtonEvent::B_LONG) {
                _editMode = true;
                _editField = 0;
                loadCurrentIntoEditor();
                _dirty = true;
                UI.showToast("TIME EDIT MODE", Settings.palette().highlight, 1200);
            }
        } else {
            // In manual edit mode
            if (ev == ButtonEvent::A_SHORT) {
                switch (_editField) {
                    case 0: _setHour = (_setHour + 1) % 24; break;
                    case 1: _setMin  = (_setMin + 1) % 60; break;
                    case 2: _setDay  = (_setDay % 31) + 1; break;
                    case 3: _setMon  = (_setMon % 12) + 1; break;
                    case 4: _setYear = (_setYear >= 2035) ? 2024 : (_setYear + 1); break;
                }
                _dirty = true;
            } else if (ev == ButtonEvent::B_SHORT) {
                _editField = (_editField + 1) % 5;
                _dirty = true;
            } else if (ev == ButtonEvent::B_LONG) {
                saveEditorTime();
                _editMode = false;
                _dirty = true;
                UI.showToast("TIME SET SAVED!", Settings.palette().ok, 1500);
            } else if (ev == ButtonEvent::A_LONG) {
                _editMode = false;
                _dirty = true;
                UI.showToast("EDIT CANCELLED", Settings.palette().warning, 1000);
            }
        }
    }

    void updateAndRender(int16_t yOffset) override {
        uint32_t ep = Net.currentEpoch();
        if (yOffset == 0 && !_dirty && ep == _lastEpoch && !_editMode) {
            return;
        }
        _lastEpoch = ep;
        _dirty     = false;

        auto& c = Display.canvas();
        const ColorPalette& pal = Settings.palette();
        Display.clearBody(pal.bg);
        int16_t by = DisplayManager::kBodyY + yOffset;

        bool hasRealTime = (ep > 1700000000UL);

        if (_editMode) {
            // Edit mode header
            c.setTextSize(1);
            c.setTextColor(pal.warning);
            c.setCursor(14, by + 18);
            c.print("--- SET CLOCK ---");

            const char* fldNames[5] = { "EDIT: HOUR", "EDIT: MINUTE", "EDIT: DAY", "EDIT: MONTH", "EDIT: YEAR" };
            c.setTextColor(pal.highlight);
            c.setCursor(14, by + 34);
            c.print(fldNames[_editField]);

            // Formatted preview
            char editHms[16], editDate[24];
            snprintf(editHms, sizeof(editHms), "%02d:%02d", _setHour, _setMin);
            snprintf(editDate, sizeof(editDate), "%02d.%02d.%04d", _setDay, _setMon, _setYear);

            c.setTextSize(2);
            c.setTextColor((_editField <= 1) ? pal.highlight : pal.textMain);
            c.setCursor(35, by + 70);
            c.print(editHms);

            c.setTextSize(1);
            c.setTextColor((_editField >= 2) ? pal.highlight : pal.secondary);
            c.setCursor(28, by + 105);
            c.print(editDate);

            // Instructions
            c.setTextColor(pal.textDim);
            c.setCursor(8, by + 135);
            c.print("[A]: +1 Value");
            c.setCursor(8, by + 148);
            c.print("[B]: Next field");
            c.setCursor(8, by + 161);
            c.print("Hold [B]: Save time");
            c.setCursor(8, by + 174);
            c.print("Hold [A]: Cancel");
            return;
        }

        char hms[16], dateStr[24];
        Net.getFormattedTime(hms, sizeof(hms), dateStr, sizeof(dateStr));

        // Minimalist Wi-Fi Sync Status at top
        c.setTextSize(1);
        c.setTextColor(Net.isNtpSynced() ? pal.ok : (hasRealTime ? pal.secondary : pal.warning));
        c.setCursor(10, by + 28);
        if (Net.isNtpSynced()) {
            c.print("WIFI NTP SYNCED");
        } else if (hasRealTime) {
            c.print("TIME SET OK");
        } else {
            c.print("HOLD [B] TO SET TIME");
        }

        // Large clean digital time in center
        c.setTextSize(2);
        c.setTextColor(hasRealTime ? pal.highlight : pal.warning);
        c.setCursor(19, by + 82);
        c.print(hms);
        c.setTextSize(1);

        // Thin minimalist progress line for seconds
        uint32_t sec = ep % 60;
        Display.drawProgressBar(19, by + 106, 96, 2, sec / 59.0f, pal.primary, pal.gridDim, pal.panelBg);

        // Clean date and timezone below
        c.setTextColor(pal.textMain);
        int16_t dw = std::strlen(dateStr) * 6;
        c.setCursor((DisplayManager::kWidth - dw) / 2, by + 126);
        c.print(dateStr);

        char tzBuf[20];
        snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", Settings.get().timezoneHours);
        int16_t tzw = std::strlen(tzBuf) * 6;
        c.setTextColor(pal.textDim);
        c.setCursor((DisplayManager::kWidth - tzw) / 2, by + 146);
        c.print(tzBuf);

        c.setCursor(10, by + 170);
        c.setTextColor(pal.textDim);
        c.print("Hold B: Set clock");
    }
};

// ============================================================================
// 11. SETTINGS & HARDWARE CONFIGURATION APPLICATION (MINIMALIST)
// ============================================================================
class AppSettings : public CyberApp {
    static constexpr uint8_t kItemCount = 7;
    uint8_t _selIdx  = 0;
    bool    _dirty   = true;
public:
    void onEnter() override {
        _selIdx  = 0;
        _dirty   = true;
    }

    const char* hintA() const override {
        return (Settings.get().language == 1) ? "ДАЛІ" : "NEXT";
    }
    const char* hintB() const override {
        return (Settings.get().language == 1) ? "ЗМІНИТИ" : "CHANGE";
    }

    void handleInput(ButtonEvent ev) override {
        _dirty = true;
        if (ev == ButtonEvent::A_SHORT) {
            _selIdx = (_selIdx + 1) % kItemCount;
        } else if (ev == ButtonEvent::B_SHORT) {
            SystemSettings& s = Settings.get();
            bool ua = (s.language == 1);
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
                case 2: // SLEEP TIMEOUT
                    if (s.sleepTimeoutSec == 20) s.sleepTimeoutSec = 30;
                    else if (s.sleepTimeoutSec == 30) s.sleepTimeoutSec = 60;
                    else if (s.sleepTimeoutSec == 60) s.sleepTimeoutSec = 0;
                    else if (s.sleepTimeoutSec == 0) s.sleepTimeoutSec = 10;
                    else s.sleepTimeoutSec = 20;
                    Settings.markDirty();
                    break;
                case 3: // LANGUAGE (EN / UA)
                    s.language = (s.language == 0) ? 1 : 0;
                    Settings.markDirty();
                    UI.showToast(s.language == 1 ? "МОВА: UA" : "LANG: EN", Settings.palette().ok, 1200);
                    UI.invalidateAll();
                    break;
                case 4: // WIFI AP
                    if (Net.isConfigPortalActive()) {
                        Net.stopConfigPortal();
                        UI.showToast(ua ? "ТОЧКУ ВИМК" : "AP OFF", Settings.palette().warning);
                    } else {
                        Net.startConfigPortal();
                        UI.showToast("AP: 192.168.4.1", Settings.palette().ok, 2000);
                    }
                    break;
                case 5: // WIFI WIPE
                    UI.showConfirmModal(ua ? "СКИД WIFI" : "WIPE WIFI",
                                        ua ? "ВИДАЛИТИ ПАРОЛІ" : "DELETE SAVED WIFI",
                                        ua ? "МЕРЕЖІ З ПАМ'ЯТІ?" : "CREDENTIALS?", []() {
                        Settings.clearWifiCredentials();
                        Net.disconnectWifi();
                        UI.showToast(Settings.get().language == 1 ? "WIFI ОЧИЩЕНО" : "WIFI WIPED", Settings.palette().danger);
                    });
                    break;
                case 6: // FACTORY RESET
                    UI.showConfirmModal(ua ? "СКИДАННЯ" : "RESET",
                                        ua ? "ВІДНОВИТИ ЗАВОДСЬКІ" : "RESTORE DEFAULT",
                                        ua ? "НАЛАШТУВАННЯ?" : "SETTINGS?", []() {
                        Settings.resetToDefaults();
                        Display.setBrightness(Settings.get().brightness);
                        Anim.setTargetFps(Settings.get().targetFps);
                        UI.showToast(Settings.get().language == 1 ? "СКИД ВИКОНАНО" : "DEFAULTS SET", Settings.palette().ok);
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
        bool ua = (Settings.get().language == 1);

        int16_t listTop = by + 12;
        const SystemSettings& s = Settings.get();
        for (uint8_t idx = 0; idx < kItemCount; ++idx) {
            int16_t ry = listTop + idx * 26;
            const char* label = "";
            char val[16] = "";

            switch (idx) {
                case 0:
                    label = ua ? "ТЕМА" : "THEME";
                    std::strncpy(val, Settings.palette().name, 8);
                    val[8] = '\0';
                    break;
                case 1:
                    label = ua ? "ЯСКРАВ" : "BRIGHT";
                    snprintf(val, sizeof(val), "%u%%", (unsigned)(s.brightness * 100 / 255));
                    break;
                case 2:
                    label = ua ? "АВТО-СОН" : "AUTO-DIM";
                    if (s.sleepTimeoutSec == 0) std::strcpy(val, ua ? "ВИМК" : "OFF");
                    else snprintf(val, sizeof(val), "%us", s.sleepTimeoutSec);
                    break;
                case 3:
                    label = ua ? "МОВА" : "LANG";
                    std::strcpy(val, ua ? "UA" : "EN");
                    break;
                case 4:
                    label = ua ? "ТОЧКА Д." : "WIFI AP";
                    std::strcpy(val, Net.isConfigPortalActive() ? (ua ? "АКТИВ" : "ON") : (ua ? "СТАРТ" : "START"));
                    break;
                case 5:
                    label = ua ? "СКИД WIFI" : "WIFI WIPE";
                    std::strcpy(val, ua ? "ОЧИСТ" : "WIPE");
                    break;
                case 6:
                    label = ua ? "СКИДАННЯ" : "DEFAULTS";
                    std::strcpy(val, ua ? "СКИД" : "RESET");
                    break;
            }
            UI.drawMenuRow(ry, label, val, idx == _selIdx, idx == 4 && Net.isConfigPortalActive());
        }
    }
};

// Static instances (zero dynamic heap allocation)
static AppSystem        s_appSystem;
static AppWifiRadar     s_appWifiRadar;
static AppPacketSniffer s_appPktSniffer;
static AppNetTools      s_appNetTools;
static AppWebDeck       s_appWebDeck;
static AppCyberEye      s_appCyberEye;
static AppCube3D        s_appCube3D;
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
        case APP_CYBER_EYE:   return &s_appCyberEye;
        case APP_CUBE_3D:     return &s_appCube3D;
        case APP_TERMINAL:    return &s_appTerminal;
        case APP_MATRIX:      return &s_appMatrix;
        case APP_STARFIELD:   return &s_appStarfield;
        case APP_CLOCK:       return &s_appClock;
        case APP_SETTINGS:    return &s_appSettings;
        default:              return &s_appSystem;
    }
}
