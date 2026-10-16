#include "ui_manager.h"
#include "apps.h"
#include "wifi_manager.h"
#include "settings_manager.h"
#include <cmath>
#include <cstring>

UIManager UI;

static constexpr uint8_t kQuickMenuCount = 6;

void UIManager::begin() {
    _state           = SystemState::BOOT_SEQUENCE;
    _bootStartMs     = millis();
    _selectedApp     = 0;
    _nextGlitchMs    = millis() + 1800;
    _glitchPattern   = 0;
    _lastActivityMs  = millis();
    _lastCatPurrMs   = millis();
    invalidateAll();
}

void UIManager::openApp(AppId id) {
    if (_state == SystemState::IN_APP && _activeApp != id) {
        CyberApp* prev = getAppInstance(_activeApp);
        if (prev) prev->onExit();
    }
    _activeApp = id;
    _state     = SystemState::IN_APP;
    CyberApp* app = getAppInstance(_activeApp);
    if (app) {
        app->onEnter();
    }
    _transitionTween.start(36.0f, 0.0f, 190, EasingType::OUT_EXPO);
    invalidateAll();
}

void UIManager::returnToLauncher() {
    if (_state == SystemState::IN_APP) {
        CyberApp* app = getAppInstance(_activeApp);
        if (app) {
            app->onExit();
        }
    }
    Settings.saveIfDirty();
    _state = SystemState::LAUNCHER;
    _transitionTween.start(-28.0f, 0.0f, 190, EasingType::OUT_EXPO);
    _nextGlitchMs = millis() + 1400;
    invalidateAll();
}

void UIManager::showToast(const char* message, uint16_t borderCol, uint16_t durationMs) {
    std::strncpy(_toastMsg, message, sizeof(_toastMsg) - 1);
    _toastMsg[sizeof(_toastMsg) - 1] = '\0';
    _toastCol    = (borderCol == 0) ? Settings.palette().primary : borderCol;
    _toastEndMs  = millis() + durationMs;
    _toastActive = true;
    invalidateBody();
}

void UIManager::showConfirmModal(const char* title, const char* line1, const char* line2, ModalCallback onConfirm) {
    std::strncpy(_modalTitle, title, sizeof(_modalTitle) - 1);
    std::strncpy(_modalLine1, line1, sizeof(_modalLine1) - 1);
    std::strncpy(_modalLine2, line2, sizeof(_modalLine2) - 1);
    _modalConfirmCb = onConfirm;
    _modalActive    = true;
    invalidateAll();
}

void UIManager::handleQuickMenuInput(ButtonEvent ev) {
    switch (ev) {
        case ButtonEvent::A_SHORT:
            _quickMenuIndex = (_quickMenuIndex + 1) % kQuickMenuCount;
            invalidateBody();
            break;
        case ButtonEvent::A_LONG:
        case ButtonEvent::B_LONG:
            _quickMenuOpen = false;
            invalidateAll();
            break;
        case ButtonEvent::B_SHORT: {
            switch (_quickMenuIndex) {
                case 0: { // THEME
                    Settings.get().themeId = (Settings.get().themeId + 1) % THEME_COUNT;
                    Settings.markDirty();
                    showToast(Settings.palette().name, Settings.palette().primary, 1300);
                    break;
                }
                case 1: { // BRIGHTNESS
                    uint8_t b = Settings.get().brightness;
                    if (b >= 230) b = 65;
                    else if (b < 110) b = 130;
                    else if (b < 185) b = 195;
                    else b = 255;
                    Settings.get().brightness = b;
                    Display.setBrightness(b);
                    Settings.markDirty();
                    break;
                }
                case 2: { // FPS HUD
                    Settings.get().showFpsOverlay = !Settings.get().showFpsOverlay;
                    Settings.markDirty();
                    break;
                }
                case 3: { // WIFI AP SETUP
                    if (Net.isConfigPortalActive()) {
                        Net.stopConfigPortal();
                        showToast("AP STOPPED", Settings.palette().warning);
                    } else {
                        Net.startConfigPortal();
                        showToast("AP: 192.168.4.1", Settings.palette().secondary, 2200);
                    }
                    break;
                }
                case 4: { // GO LAUNCHER
                    _quickMenuOpen = false;
                    returnToLauncher();
                    return;
                }
                case 5: { // CLOSE
                    _quickMenuOpen = false;
                    break;
                }
            }
            Settings.saveIfDirty();
            invalidateAll();
            break;
        }
        default:
            break;
    }
}

void UIManager::handleInput(ButtonEvent ev) {
    if (ev == ButtonEvent::NONE) return;

    _lastActivityMs = millis();

    // Wake up from screensaver without executing triggering button action
    if (_screensaverActive) {
        _screensaverActive = false;
        Display.setBrightness(Settings.get().brightness);
        invalidateAll();
        return;
    }

    if (_state == SystemState::BOOT_SEQUENCE) {
        _state = SystemState::LAUNCHER;
        _transitionTween.start(28.0f, 0.0f, 180, EasingType::OUT_CUBIC);
        invalidateAll();
        return;
    }

    if (ev == ButtonEvent::CHORD_AB) {
        Settings.get().themeId = (Settings.get().themeId + 1) % THEME_COUNT;
        Settings.markDirty();
        Settings.saveIfDirty();
        showToast(Settings.palette().name, Settings.palette().primary, 1200);
        invalidateAll();
        return;
    }

    if (_modalActive) {
        if (ev == ButtonEvent::A_SHORT || ev == ButtonEvent::A_LONG) {
            _modalActive = false;
            invalidateAll();
        } else if (ev == ButtonEvent::B_SHORT) {
            _modalActive = false;
            if (_modalConfirmCb) _modalConfirmCb();
            invalidateAll();
        }
        return;
    }

    if (_quickMenuOpen) {
        handleQuickMenuInput(ev);
        return;
    }

    if (ev == ButtonEvent::B_LONG) {
        _quickMenuOpen = true;
        _quickMenuTween.start(24.0f, 0.0f, 150, EasingType::OUT_CUBIC);
        invalidateAll();
        return;
    }

    if (_state == SystemState::LAUNCHER) {
        if (ev == ButtonEvent::A_SHORT) {
            _selectedApp = (_selectedApp + 1) % APP_COUNT;
            _cardSlideTween.start(44.0f, 0.0f, 170, EasingType::OUT_QUINT);
            invalidateBody();
        } else if (ev == ButtonEvent::A_LONG) {
            _selectedApp = (_selectedApp + APP_COUNT - 1) % APP_COUNT;
            _cardSlideTween.start(-44.0f, 0.0f, 170, EasingType::OUT_QUINT);
            invalidateBody();
        } else if (ev == ButtonEvent::B_SHORT) {
            openApp((AppId)_selectedApp);
        }
    } else if (_state == SystemState::IN_APP) {
        if (ev == ButtonEvent::A_LONG) {
            returnToLauncher();
        } else {
            CyberApp* app = getAppInstance(_activeApp);
            if (app) {
                app->handleInput(ev);
                invalidateBody();
                invalidateFooter();
            }
        }
    }
}

void UIManager::drawHeader(const char* title) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    c.fillRect(0, 0, DisplayManager::kWidth, DisplayManager::kHeaderH, pal.bg);
    c.drawFastHLine(0, DisplayManager::kHeaderH - 1, DisplayManager::kWidth, pal.gridDim);

    c.setTextSize(1);
    c.setTextColor(pal.primary, pal.bg);
    c.setCursor(6, 5);
    c.print(title);

    int16_t rightCursor = DisplayManager::kWidth - 5;

    // Draw Battery Icon & Charge/Percentage indicator
    rightCursor -= 16;
    int16_t bx = rightCursor;
    int16_t by = 6;

    c.drawRect(bx, by, 14, 7, pal.gridDim);
    c.drawFastVLine(bx + 14, by + 2, 3, pal.gridDim);

    if (Net.hasValidBattery()) {
        uint8_t pct = Net.batteryPercent();
        int16_t fillW = (pct * 12) / 100;
        uint16_t bCol = Net.isCharging() ? pal.highlight : (pct < 20 ? pal.danger : pal.ok);
        if (fillW > 0) {
            c.fillRect(bx + 1, by + 1, fillW, 5, bCol);
        }
        if (Net.isCharging()) {
            c.drawPixel(bx + 6, by + 2, pal.warning);
            c.drawPixel(bx + 7, by + 3, pal.warning);
            c.drawPixel(bx + 6, by + 4, pal.warning);
        }
    } else {
        // USB Powered symbol
        c.setTextColor(pal.secondary, pal.bg);
        c.setCursor(bx + 2, by);
        c.print("U");
    }
    rightCursor -= 4;

    if (Net.isConfigPortalActive()) {
        rightCursor -= 14;
        c.setTextColor(pal.warning, pal.bg);
        c.setCursor(rightCursor, 5);
        c.print("AP");
        rightCursor -= 4;
    } else if (Net.isConnected()) {
        rightCursor -= 14;
        int32_t rssi = Net.currentRssi();
        int bars = (rssi > -60) ? 4 : (rssi > -72) ? 3 : (rssi > -82) ? 2 : 1;
        for (int b = 0; b < 4; ++b) {
            int16_t bh = 2 + b * 2;
            uint16_t col = (b < bars) ? pal.ok : pal.gridDim;
            c.fillRect(rightCursor + b * 4, 13 - bh, 2, bh, col);
        }
        rightCursor -= 4;
    }

    if (Settings.get().showFpsOverlay) {
        char fpsBuf[10];
        snprintf(fpsBuf, sizeof(fpsBuf), "%2uF", (unsigned)(Anim.fps() + 0.5f));
        rightCursor -= 20;
        c.setTextColor(pal.secondary, pal.bg);
        c.setCursor(rightCursor, 5);
        c.print(fpsBuf);
    }

    Display.markHeaderDirty();
}

void UIManager::drawFooter(const char* hintA, const char* hintB) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    int16_t y = DisplayManager::kHeight - DisplayManager::kFooterH;

    c.fillRect(0, y, DisplayManager::kWidth, DisplayManager::kFooterH, pal.bg);
    c.drawFastHLine(0, y, DisplayManager::kWidth, pal.gridDim);

    float holdA = Buttons.holdProgressA();
    float holdB = Buttons.holdProgressB();
    if (holdA > 0.01f) {
        c.fillRect(0, y, (int16_t)(65.0f * holdA), 2, pal.primary);
    }
    if (holdB > 0.01f) {
        int16_t wB = (int16_t)(65.0f * holdB);
        c.fillRect(DisplayManager::kWidth - wB, y, wB, 2, pal.secondary);
    }

    c.setTextColor(Buttons.isPressedA() ? pal.highlight : pal.primary, pal.bg);
    c.setCursor(6, y + 6);
    c.print("A:");
    c.setTextColor(pal.textDim, pal.bg);
    c.setCursor(18, y + 6);
    c.print(hintA);

    int16_t bTextW = (std::strlen(hintB) + 2) * 6;
    int16_t bx = max<int16_t>(68, DisplayManager::kWidth - 6 - bTextW);
    c.setTextColor(Buttons.isPressedB() ? pal.highlight : pal.secondary, pal.bg);
    c.setCursor(bx, y + 6);
    c.print("B:");
    c.setTextColor(pal.textDim, pal.bg);
    c.setCursor(bx + 12, y + 6);
    c.print(hintB);

    Display.markFooterDirty();
}

void UIManager::drawMenuRow(int16_t y, const char* label, const char* value, bool selected, bool highlightValue) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    int16_t rowW = DisplayManager::kWidth - 12;
    uint16_t rowBg = selected ? pal.panelBg : pal.bg;

    c.fillRect(6, y, rowW, 20, rowBg);
    if (selected) {
        c.fillRect(6, y + 2, 2, 16, pal.primary);
    }

    char cleanLabel[12];
    std::strncpy(cleanLabel, label, 10);
    cleanLabel[10] = '\0';

    c.setTextColor(selected ? pal.highlight : pal.textDim, rowBg);
    c.setCursor(14, y + 6);
    c.print(cleanLabel);

    if (value && value[0] != '\0') {
        char cleanVal[10];
        std::strncpy(cleanVal, value, 9);
        cleanVal[9] = '\0';
        int16_t vw = std::strlen(cleanVal) * 6;
        int16_t vx = 6 + rowW - 4 - vw;
        uint16_t vCol = highlightValue ? pal.ok : (selected ? pal.primary : pal.textMain);
        c.setTextColor(vCol, rowBg);
        c.setCursor(vx, y + 6);
        c.print(cleanVal);
    }
}

void UIManager::drawLoadingSpinner(int16_t cx, int16_t cy, int16_t radius, uint16_t col) {
    auto& c = Display.canvas();
    float baseAng = (Anim.nowMs() % 1000) * (6.283185f / 1000.0f);
    for (int i = 0; i < 4; ++i) {
        float a = baseAng + i * 1.57f;
        int16_t x = cx + (int16_t)(std::cos(a) * radius);
        int16_t y = cy + (int16_t)(std::sin(a) * radius);
        c.drawPixel(x, y, col);
    }
}

void UIManager::renderBootSequence() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    uint32_t elapsed = millis() - _bootStartMs;

    c.fillSprite(pal.bg);

    if (elapsed < 650) {
        c.setTextColor(pal.primary);
        c.setTextSize(2);
        c.setCursor(14, 92);
        c.print("CYBERDECK");
        c.setTextSize(1);
        c.setTextColor(pal.textDim);
        c.setCursor(24, 118);
        c.print("TTGO_CyberDeck");
    } else if (elapsed < 1300) {
        uint32_t stepMs = elapsed - 650;
        char line0[22], line1[22], line2[22];
        snprintf(line0, sizeof(line0), "CPU  %lu MHz", (unsigned long)ESP.getCpuFreqMHz());
        snprintf(line1, sizeof(line1), "RAM  %lu KB FREE", (unsigned long)(ESP.getFreeHeap() / 1024));
        snprintf(line2, sizeof(line2), "ROM  %lu MB FLASH", (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)));

        c.setTextColor(pal.primary);
        c.setCursor(16, 66);
        c.print("TTGO_CyberDeck");

        const char* lines[3] = { line0, line1, line2 };
        for (size_t i = 0; i < 3; ++i) {
            if (stepMs >= i * 140) {
                c.setTextColor(pal.textDim);
                c.setCursor(16, 96 + i * 18);
                c.print(lines[i]);
            }
        }
        Display.drawProgressBar(16, 168, 103, 3, constrain(stepMs / 600.0f, 0.0f, 1.0f),
                                pal.primary, pal.gridDim, pal.panelBg);
    } else {
        _state = SystemState::LAUNCHER;
        _transitionTween.start(24.0f, 0.0f, 180, EasingType::OUT_EXPO);
        invalidateAll();
        return;
    }

    Display.markAllDirty();
}

void UIManager::renderLauncher(bool glitchActive) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    uint32_t now = millis();

    int16_t slideX = (int16_t)_cardSlideTween.current;
    int16_t transY = (int16_t)_transitionTween.current;

    Display.clearBody(pal.bg);

    const AppMetadata& meta = getAppMeta(_selectedApp);

    // Minimalist Central Application Card (clean, uncluttered space)
    int16_t cardX = 10 + slideX;
    int16_t cardY = DisplayManager::kBodyY + 26 + transY;
    int16_t cardW = DisplayManager::kWidth - 20;
    int16_t cardH = 128;

    Display.drawCyberPanel(cardX, cardY, cardW, cardH, pal.panelBg, pal.primary, pal.secondary, 0);

    // Minimalist App Icon
    int16_t iconCx = cardX + cardW / 2;
    int16_t iconCy = cardY + 40;
    Display.drawAppIcon(_selectedApp, iconCx, iconCy, pal.primary, pal.secondary, now);

    // App Title (with subtle chromatic split only during glitch bursts)
    int16_t titleLen = std::strlen(meta.title) * 6;
    int16_t titleX   = cardX + (cardW - titleLen) / 2;
    int16_t titleY   = cardY + 76;
    if (glitchActive) {
        c.setTextColor(pal.secondary);
        c.setCursor(titleX - 1, titleY);
        c.print(meta.title);
        c.setTextColor(pal.danger);
        c.setCursor(titleX + 1, titleY);
        c.print(meta.title);
    }
    c.setTextColor(pal.highlight);
    c.setCursor(titleX, titleY);
    c.print(meta.title);

    // Clean 1-line subtitle
    int16_t subLen = std::strlen(meta.subtitle) * 6;
    c.setTextColor(pal.textDim);
    c.setCursor(cardX + max<int16_t>(4, (cardW - subLen) / 2), cardY + 96);
    c.print(meta.subtitle);

    // Minimalist Pagination Dots at bottom
    int16_t barY = DisplayManager::kBodyY + 178;
    for (uint8_t i = 0; i < APP_COUNT; ++i) {
        int16_t bx = 18 + i * 9;
        if (i == _selectedApp) {
            c.fillRect(bx - 1, barY, 6, 3, pal.primary);
        } else {
            c.fillRect(bx + 1, barY + 1, 2, 2, pal.gridDim);
        }
    }

    // Soft background scanline texture inside card only
    if (Settings.get().animIntensity > 0) {
        Display.drawScanlines(cardY + 2, cardH - 4, (uint8_t)((now / 160) % 3));
    }

    // Multi-pattern soft glitch effect during active glitch window
    if (glitchActive) {
        uint8_t activePat = (Settings.get().animIntensity == 1)
            ? static_cast<uint8_t>(GLITCH_CLASSIC_SOFT)
            : _glitchPattern;
        Display.applyMultiGlitch(cardY + 10, cardY + 110, activePat, now);
    }
}

void UIManager::renderQuickMenu() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    _quickMenuTween.update(Anim.nowUs());
    int16_t yOff = (int16_t)_quickMenuTween.current;

    int16_t mx = 4;
    int16_t my = DisplayManager::kBodyY + 6 + yOff;
    int16_t mw = DisplayManager::kWidth - 8;
    int16_t mh = 190;

    Display.drawCyberPanel(mx, my, mw, mh, pal.bg, pal.secondary, pal.primary, 5);
    c.fillRect(mx + 2, my + 2, mw - 4, 16, pal.panelBg);
    c.setTextColor(pal.secondary);
    c.setCursor(mx + 8, my + 6);
    c.print("// QUICK OVERRIDE");

    for (uint8_t i = 0; i < kQuickMenuCount; ++i) {
        int16_t ry = my + 24 + i * 26;
        const char* label = "";
        char val[12] = "";
        switch (i) {
            case 0:
                label = "THEME";
                std::strncpy(val, Settings.palette().name, 5);
                val[5] = '\0';
                break;
            case 1:
                label = "BRIGHT";
                snprintf(val, sizeof(val), "%u%%", (unsigned)(Settings.get().brightness * 100 / 255));
                break;
            case 2:
                label = "FPS HUD";
                std::strcpy(val, Settings.get().showFpsOverlay ? "ON" : "OFF");
                break;
            case 3:
                label = "WIFI AP";
                std::strcpy(val, Net.isConfigPortalActive() ? "ON" : "OFF");
                break;
            case 4:
                label = "LAUNCHER";
                std::strcpy(val, "GO");
                break;
            case 5:
                label = "CLOSE";
                std::strcpy(val, "EXIT");
                break;
        }
        drawMenuRow(ry, label, val, i == _quickMenuIndex, (i == 3 && Net.isConfigPortalActive()));
    }
    Display.markBodyDirty();
}

void UIManager::renderModal() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    int16_t mx = 6;
    int16_t my = 68;
    int16_t mw = DisplayManager::kWidth - 12;
    int16_t mh = 104;

    Display.drawCyberPanel(mx, my, mw, mh, pal.bg, pal.warning, pal.danger, 5);
    c.fillRect(mx + 2, my + 2, mw - 4, 16, pal.panelBg);
    c.setTextColor(pal.warning);
    c.setCursor(mx + 8, my + 6);
    c.print(_modalTitle);

    c.setTextColor(pal.textMain);
    c.setCursor(mx + 8, my + 30);
    c.print(_modalLine1);
    c.setCursor(mx + 8, my + 44);
    c.print(_modalLine2);

    Display.drawCyberPanel(mx + 8, my + 72, 50, 20, pal.panelBg, pal.textDim, pal.textDim, 2);
    c.setTextColor(pal.textMain);
    c.setCursor(mx + 14, my + 78);
    c.print("A:NO");

    Display.drawCyberPanel(mx + mw - 58, my + 72, 50, 20, pal.panelBg, pal.primary, pal.secondary, 2);
    c.setTextColor(pal.primary);
    c.setCursor(mx + mw - 52, my + 78);
    c.print("B:YES");

    Display.markBodyDirty();
}

void UIManager::renderToast() {
    if (!_toastActive) return;
    if (millis() >= _toastEndMs) {
        _toastActive = false;
        invalidateBody();
        return;
    }
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    int16_t tw = DisplayManager::kWidth - 12;
    int16_t tx = 6;
    int16_t ty = DisplayManager::kHeight - DisplayManager::kFooterH - 24;

    Display.drawCyberPanel(tx, ty, tw, 20, pal.bg, _toastCol, pal.highlight, 3);
    c.setTextColor(pal.highlight);
    int16_t msgW = std::strlen(_toastMsg) * 6;
    c.setCursor(tx + max<int16_t>(4, (tw - msgW) / 2), ty + 6);
    c.print(_toastMsg);
    Display.markDirty(tx, ty, tw, 20);
}

void UIManager::triggerCyberCat() {
    _catActive = true;
    _catEndMs  = millis() + 4500;
    invalidateBody();
}

void UIManager::renderCyberCat() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    uint32_t now = millis();
    if (now >= _catEndMs) {
        _catActive = false;
        invalidateBody();
        return;
    }

    int16_t boxW = DisplayManager::kWidth - 10;
    int16_t boxH = 46;
    int16_t boxX = 5;
    int16_t boxY = DisplayManager::kHeight - DisplayManager::kFooterH - boxH - 4;

    Display.drawCyberPanel(boxX, boxY, boxW, boxH, pal.panelBg, pal.secondary, pal.highlight, 4);

    bool blink = ((now / 400) % 6 == 0);
    const char* catFace = blink ? " ( -.- ) " : " ( o.o ) ";

    c.setTextColor(pal.primary);
    c.setCursor(boxX + 6, boxY + 8);
    c.print("  /\\_/\\  ");

    c.setTextColor(pal.highlight);
    c.setCursor(boxX + 6, boxY + 20);
    c.print(catFace);

    c.setTextColor(pal.primary);
    c.setCursor(boxX + 6, boxY + 32);
    c.print("  > ^ <  ");

    c.setTextColor(pal.secondary);
    c.setCursor(boxX + 64, boxY + 10);
    c.print("ASTRO CAT");
    c.setTextColor(pal.ok);
    c.setCursor(boxX + 64, boxY + 24);
    c.print("PURR // =^.^=");

    Display.markDirty(boxX, boxY, boxW, boxH);
}

void UIManager::renderScreensaver() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    c.fillSprite(TFT_BLACK);

    char hms[16];
    Net.getFormattedTime(hms, sizeof(hms), nullptr, 0);

    // Pure Deep Sleep Clock: ONLY TIME AND NOTHING ELSE
    Display.setClockFont();
    int16_t tw = c.textWidth(hms);
    int16_t tx = (DisplayManager::kWidth - tw) / 2;
    int16_t ty = (DisplayManager::kHeight - 48) / 2;

    c.setTextColor(pal.highlight);
    c.setCursor(tx, ty);
    c.print(hms);

    // Restore standard UI font
    Display.resetFont();
    Display.markAllDirty();
}

void UIManager::updateAndRender() {
    if (_state == SystemState::BOOT_SEQUENCE) {
        renderBootSequence();
        Display.flush();
        return;
    }

    uint32_t now = millis();

    // Check Idle timeout (20s default) -> dim brightness and show clean clock
    uint8_t toSec = Settings.get().sleepTimeoutSec;
    if (toSec > 0 && !_screensaverActive && _state != SystemState::BOOT_SEQUENCE) {
        if ((now - _lastActivityMs) >= toSec * 1000UL) {
            _screensaverActive = true;
            uint8_t base = Settings.get().brightness;
            // Noticeable 50% sleep dimming (minimum floor 25)
            uint8_t dimmed = (base > 50) ? (uint8_t)(base * 50 / 100) : 25;
            Display.setBrightness(dimmed);
        }
    }

    // Hourly cyber cat easter egg check (every 3600 seconds)
    if (_lastCatPurrMs == 0) {
        _lastCatPurrMs = now;
    } else if ((now - _lastCatPurrMs) >= 3600000UL) {
        _lastCatPurrMs = now;
        triggerCyberCat();
    }

    if (_screensaverActive) {
        renderScreensaver();
        Display.flush();
        return;
    }

    uint32_t hdrInterval = Settings.get().showFpsOverlay ? 250 : 1000;
    if ((now - _lastHeaderTickMs) >= hdrInterval) {
        _lastHeaderTickMs = now;
        _headerNeedsRedraw = true;
    }

    if (_headerNeedsRedraw) {
        const char* headerTitle = "TTGO_CyberDeck";
        if (_state == SystemState::IN_APP) {
            headerTitle = getAppMeta((uint8_t)_activeApp).title;
        }
        drawHeader(headerTitle);
        _headerNeedsRedraw = false;
    }

    bool holdActive = (Buttons.holdProgressA() > 0.01f) || (Buttons.holdProgressB() > 0.01f) ||
                      Buttons.isPressedA() || Buttons.isPressedB();
    if (holdActive || _lastHoldActive) {
        _footerNeedsRedraw = true;
    }
    _lastHoldActive = holdActive;

    if (_state == SystemState::LAUNCHER) {
        bool slideActive = _cardSlideTween.update(Anim.nowUs());
        bool transActive = _transitionTween.update(Anim.nowUs());

        // Multi-pattern soft glitch scheduler
        bool glitchNow = false;
        uint8_t intensity = Settings.get().animIntensity;
        if (intensity > 0 && !_quickMenuOpen && !_modalActive) {
            if (now >= _nextGlitchMs) {
                _glitchPattern = (_glitchPattern + 1) % GLITCH_PATTERN_COUNT;
                _glitchEndMs   = now + 220; // 220ms smooth multi-frame glitch
                uint32_t gap   = (intensity >= 2) ? (1300 + (esp_random() % 1100)) : (2400 + (esp_random() % 1400));
                _nextGlitchMs  = now + gap;
            }
            if (now < _glitchEndMs) {
                glitchNow = true;
            }
        }

        // Gentle ambient refresh at ~12 FPS so scan beam & icon animation glide smoothly without strobing
        uint32_t idleInterval = (intensity > 0) ? 80 : 1000;
        if ((now - _lastLauncherFxMs) >= idleInterval) {
            _lastLauncherFxMs = now;
            _bodyNeedsRedraw  = true;
        }

        if (_bodyNeedsRedraw || slideActive || transActive || glitchNow) {
            renderLauncher(glitchNow);
            _bodyNeedsRedraw = false;
        }

        if (_footerNeedsRedraw) {
            drawFooter("NEXT", "OPEN");
            _footerNeedsRedraw = false;
        }
    } else if (_state == SystemState::IN_APP) {
        _transitionTween.update(Anim.nowUs());
        int16_t yOff = (int16_t)_transitionTween.current;
        CyberApp* app = getAppInstance(_activeApp);
        if (app && !_quickMenuOpen && !_modalActive) {
            app->updateAndRender(yOff);
            if (_footerNeedsRedraw) {
                drawFooter(app->hintA(), app->hintB());
                _footerNeedsRedraw = false;
            }
        }
    }

    if (_quickMenuOpen) {
        bool qAnim = _quickMenuTween.active;
        if (_bodyNeedsRedraw || qAnim) {
            renderQuickMenu();
            _bodyNeedsRedraw = false;
        }
        if (_footerNeedsRedraw) {
            drawFooter("NEXT", "EXEC");
            _footerNeedsRedraw = false;
        }
    }
    if (_modalActive) {
        if (_bodyNeedsRedraw) {
            renderModal();
            _bodyNeedsRedraw = false;
        }
        if (_footerNeedsRedraw) {
            drawFooter("CANCEL", "CONFIRM");
            _footerNeedsRedraw = false;
        }
    }
    if (_toastActive) {
        renderToast();
    }
    if (_catActive) {
        renderCyberCat();
    }

    Display.flush();
}
