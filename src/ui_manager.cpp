#include "ui_manager.h"
#include "apps.h"
#include "wifi_manager.h"
#include "settings_manager.h"
#include <cmath>
#include <cstring>

UIManager UI;

static constexpr uint8_t kQuickMenuCount = 6;

void UIManager::begin() {
    _state        = SystemState::BOOT_SEQUENCE;
    _bootStartMs  = millis();
    _selectedApp  = 0;
    _nextGlitchMs = millis() + 3200;
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
    _nextGlitchMs = millis() + 2400;
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

    if (_state == SystemState::BOOT_SEQUENCE) {
        _state = SystemState::LAUNCHER;
        _transitionTween.start(28.0f, 0.0f, 180, EasingType::OUT_CUBIC);
        invalidateAll();
        return;
    }

    // Simultaneous A+B chord shortcut: instant theme cycle
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

    // Long press on Button B opens the Quick Menu from anywhere
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

    c.fillRect(0, 0, DisplayManager::kWidth, DisplayManager::kHeaderH, pal.panelBg);
    c.drawFastHLine(0, DisplayManager::kHeaderH - 1, DisplayManager::kWidth, pal.primary);
    c.drawFastHLine(0, DisplayManager::kHeaderH - 2, 26, pal.secondary);

    c.setTextSize(1);
    c.setTextColor(pal.primary, pal.panelBg);
    c.setCursor(4, 5);
    c.print(title);

    int16_t rightCursor = DisplayManager::kWidth - 3;

    // 1. Battery Indicator ONLY when a real valid battery measurement is present
    if (Net.hasValidBattery()) {
        rightCursor -= 18;
        uint8_t pct = Net.batteryPercent();
        uint16_t batCol = (pct > 25) ? pal.ok : pal.danger;
        c.drawRect(rightCursor, 5, 14, 8, pal.textDim);
        c.fillRect(rightCursor + 14, 7, 2, 4, pal.textDim);
        int16_t fillW = (10 * pct) / 100;
        if (fillW > 0) {
            c.fillRect(rightCursor + 2, 7, fillW, 4, batCol);
        }
        rightCursor -= 3;
    }

    // 2. Network / Sniffer / AP Status Indicator
    if (Net.sniffer().active) {
        rightCursor -= 22;
        c.fillRect(rightCursor, 4, 21, 10, pal.secondary);
        c.setTextColor(pal.bg, pal.secondary);
        c.setCursor(rightCursor + 2, 5);
        c.print("SNF");
        rightCursor -= 3;
    } else if (Net.isConfigPortalActive()) {
        rightCursor -= 16;
        c.fillRect(rightCursor, 4, 15, 10, pal.warning);
        c.setTextColor(pal.bg, pal.warning);
        c.setCursor(rightCursor + 2, 5);
        c.print("AP");
        rightCursor -= 3;
    } else if (Net.isConnected()) {
        rightCursor -= 15;
        int32_t rssi = Net.currentRssi();
        int bars = (rssi > -60) ? 4 : (rssi > -72) ? 3 : (rssi > -82) ? 2 : 1;
        for (int b = 0; b < 4; ++b) {
            int16_t bh = 2 + b * 2;
            uint16_t col = (b < bars) ? pal.ok : pal.gridDim;
            c.fillRect(rightCursor + b * 4, 13 - bh, 2, bh, col);
        }
        rightCursor -= 3;
    } else {
        rightCursor -= 14;
        c.setTextColor(pal.textDim, pal.panelBg);
        c.setCursor(rightCursor, 5);
        c.print("--");
        rightCursor -= 2;
    }

    // 3. FPS Overlay Badge if enabled
    if (Settings.get().showFpsOverlay) {
        char fpsBuf[10];
        snprintf(fpsBuf, sizeof(fpsBuf), "%2uF", (unsigned)(Anim.fps() + 0.5f));
        rightCursor -= 20;
        c.setTextColor(pal.secondary, pal.panelBg);
        c.setCursor(rightCursor, 5);
        c.print(fpsBuf);
    }

    Display.markHeaderDirty();
}

void UIManager::drawFooter(const char* hintA, const char* hintB) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    int16_t y = DisplayManager::kHeight - DisplayManager::kFooterH;

    c.fillRect(0, y, DisplayManager::kWidth, DisplayManager::kFooterH, pal.panelBg);
    c.drawFastHLine(0, y, DisplayManager::kWidth, pal.gridDim);

    float holdA = Buttons.holdProgressA();
    float holdB = Buttons.holdProgressB();
    if (holdA > 0.01f) {
        int16_t wA = (int16_t)(65.0f * holdA);
        c.fillRect(0, y, wA, 2, pal.warning);
    }
    if (holdB > 0.01f) {
        int16_t wB = (int16_t)(65.0f * holdB);
        c.fillRect(DisplayManager::kWidth - wB, y, wB, 2, pal.secondary);
    }

    // Left badge [A]
    uint16_t bgA = Buttons.isPressedA() ? pal.primary : pal.gridDim;
    uint16_t fgA = Buttons.isPressedA() ? pal.bg : pal.primary;
    c.fillRect(2, y + 4, 11, 11, bgA);
    c.setTextColor(fgA, bgA);
    c.setCursor(4, y + 6);
    c.print("A");

    c.setTextColor(pal.textMain, pal.panelBg);
    c.setCursor(15, y + 6);
    c.print(hintA);

    // Right badge [B]
    int16_t bTextW = std::strlen(hintB) * 6;
    int16_t bBoxX  = DisplayManager::kWidth - 13;
    uint16_t bgB = Buttons.isPressedB() ? pal.secondary : pal.gridDim;
    uint16_t fgB = Buttons.isPressedB() ? pal.bg : pal.secondary;
    c.fillRect(bBoxX, y + 4, 11, 11, bgB);
    c.setTextColor(fgB, bgB);
    c.setCursor(bBoxX + 3, y + 6);
    c.print("B");

    c.setTextColor(pal.textMain, pal.panelBg);
    c.setCursor(max<int16_t>(66, bBoxX - bTextW - 3), y + 6);
    c.print(hintB);

    Display.markFooterDirty();
}

void UIManager::drawMenuRow(int16_t y, const char* label, const char* value, bool selected, bool highlightValue) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    int16_t rowW = DisplayManager::kWidth - 10; // Leave 4px on right for scrollbar
    uint16_t rowBg  = selected ? AnimationEngine::lerpColor565(pal.panelBg, pal.primary, 48) : pal.panelBg;
    uint16_t brdCol = selected ? pal.primary : pal.gridDim;

    c.fillRect(3, y, rowW, 20, rowBg);
    c.drawRect(3, y, rowW, 20, brdCol);
    if (selected) {
        c.fillRect(3, y, 3, 20, pal.primary);
        c.setTextColor(pal.primary, rowBg);
        c.setCursor(7, y + 6);
        c.print(">");
    }

    // Clamp label to max 10 chars so it never collides with value column
    char cleanLabel[12];
    std::strncpy(cleanLabel, label, 10);
    cleanLabel[10] = '\0';

    c.setTextColor(selected ? pal.highlight : pal.textMain, rowBg);
    c.setCursor(selected ? 14 : 8, y + 6);
    c.print(cleanLabel);

    if (value && value[0] != '\0') {
        char cleanVal[10];
        std::strncpy(cleanVal, value, 9);
        cleanVal[9] = '\0';
        int16_t vw = std::strlen(cleanVal) * 6;
        int16_t vx = 3 + rowW - 4 - vw;
        uint16_t vCol = highlightValue ? pal.ok : (selected ? pal.secondary : pal.textDim);
        c.setTextColor(vCol, rowBg);
        c.setCursor(vx, y + 6);
        c.print(cleanVal);
    }
}

void UIManager::drawLoadingSpinner(int16_t cx, int16_t cy, int16_t radius, uint16_t col) {
    auto& c = Display.canvas();
    float baseAng = (Anim.nowMs() % 1000) * (6.283185f / 1000.0f);
    for (int i = 0; i < 6; ++i) {
        float a = baseAng + i * 0.8f;
        int16_t x = cx + (int16_t)(std::cos(a) * radius);
        int16_t y = cy + (int16_t)(std::sin(a) * radius);
        c.fillCircle(x, y, (i == 5) ? 2 : 1, col);
    }
}

void UIManager::renderBootSequence() {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();
    uint32_t elapsed = millis() - _bootStartMs;

    c.fillSprite(pal.bg);

    if (elapsed < 700) {
        float t = AnimationEngine::easeOutExpo(elapsed / 700.0f);
        int16_t boxW = (int16_t)(120 * t);
        int16_t boxX = (DisplayManager::kWidth - boxW) / 2;
        Display.drawCyberPanel(boxX, 55, max<int16_t>(16, boxW), 68, pal.panelBg, pal.primary, pal.secondary, 6);

        if (elapsed > 150) {
            c.setTextColor(pal.primary);
            c.setTextSize(2);
            c.setCursor(14, 70);
            c.print("NEON DECK");
            c.setTextSize(1);
            c.setTextColor(pal.secondary);
            c.setCursor(18, 96);
            c.print("CYBERDECK OS v1.1");
        }
    } else if (elapsed < 1450) {
        Display.drawCyberPanel(4, 12, 127, 216, pal.panelBg, pal.primary, pal.secondary, 5);
        c.setTextColor(pal.secondary);
        c.setCursor(10, 22);
        c.print("// HW INIT MATRIX");
        c.drawFastHLine(10, 33, 115, pal.gridDim);

        uint32_t stepMs = elapsed - 700;
        struct InitLine { uint16_t delayMs; const char* tag; const char* desc; };
        static const InitLine kLines[] = {
            {   0, "[OK]", "ESP32 240MHz 2C"  },
            { 110, "[OK]", "ST7789 135x240"   },
            { 220, "[OK]", "SPI DMA ENGINE"   },
            { 330, "[OK]", "802.11 SNIFFER"   },
            { 440, "[OK]", "CRYPTO SHA-256"   },
            { 550, "[OK]", "WEB DECK SERVER"  }
        };
        for (size_t i = 0; i < 6; ++i) {
            if (stepMs >= kLines[i].delayMs) {
                int16_t ly = 42 + i * 22;
                c.setTextColor(pal.ok);
                c.setCursor(10, ly);
                c.print(kLines[i].tag);
                c.setTextColor(pal.textMain);
                c.setCursor(38, ly);
                c.print(kLines[i].desc);
            }
        }
        Display.drawProgressBar(12, 195, 111, 10, constrain(stepMs / 700.0f, 0.0f, 1.0f),
                                pal.primary, pal.secondary, pal.bg);
    } else {
        _state = SystemState::LAUNCHER;
        _transitionTween.start(32.0f, 0.0f, 200, EasingType::OUT_EXPO);
        invalidateAll();
        return;
    }

    Display.markAllDirty();
}

void UIManager::renderLauncher(bool glitchActive) {
    auto& c = Display.canvas();
    const ColorPalette& pal = Settings.palette();

    int16_t slideX = (int16_t)_cardSlideTween.current;
    int16_t transY = (int16_t)_transitionTween.current;

    Display.clearBody(pal.bg);

    // Static parallax grid (moves only when sliding cards — zero idle flicker!)
    int16_t gridShiftX = -(_selectedApp * 6) + (slideX / 2);
    Display.drawCyberGrid(gridShiftX, 0, pal.gridDim);

    const AppMetadata& meta = getAppMeta(_selectedApp);

    // Top deck index banner
    int16_t bannerY = DisplayManager::kBodyY + 6 + transY;
    c.setTextColor(glitchActive ? pal.highlight : pal.secondary);
    c.setCursor(6, bannerY);
    if (glitchActive) {
        c.print("// GLITCH_SYNC #");
    } else {
        c.print(meta.codeTag);
    }

    char idxBuf[12];
    snprintf(idxBuf, sizeof(idxBuf), "[%02u/%02u]", (unsigned)(_selectedApp + 1), (unsigned)APP_COUNT);
    c.setTextColor(pal.textDim);
    c.setCursor(DisplayManager::kWidth - 46, bannerY);
    c.print(idxBuf);

    // Central Cyberpunk Application Card
    int16_t cardX = 6 + slideX;
    int16_t cardY = DisplayManager::kBodyY + 20 + transY;
    int16_t cardW = DisplayManager::kWidth - 12;
    int16_t cardH = 132;

    Display.drawCyberPanel(cardX, cardY, cardW, cardH, pal.panelBg, pal.primary, pal.secondary, 6);

    // Glowing icon frame inside card
    int16_t iconCx = cardX + cardW / 2;
    int16_t iconCy = cardY + 36;
    Display.drawCyberPanel(iconCx - 23, iconCy - 20, 46, 40, pal.bg, pal.secondary, pal.primary, 4);
    Display.drawAppIcon(_selectedApp, iconCx, iconCy, pal.primary, pal.secondary, 0);

    // App Title with chromatic aberration during glitch bursts
    int16_t titleLen = std::strlen(meta.title) * 6;
    int16_t titleX   = cardX + (cardW - titleLen) / 2;
    int16_t titleY   = cardY + 66;
    if (glitchActive) {
        c.setTextColor(pal.secondary);
        c.setCursor(titleX - 2, titleY);
        c.print(meta.title);
        c.setTextColor(pal.danger);
        c.setCursor(titleX + 2, titleY);
        c.print(meta.title);
    }
    c.setTextColor(pal.highlight);
    c.setCursor(titleX, titleY);
    c.print(meta.title);

    // Decorative divider
    c.drawFastHLine(cardX + 12, cardY + 80, cardW - 24, pal.gridDim);
    c.drawFastHLine(cardX + cardW / 2 - 12, cardY + 80, 24, pal.secondary);

    // Subtitle / description
    int16_t subLen = std::strlen(meta.subtitle) * 6;
    c.setTextColor(pal.textMain);
    c.setCursor(cardX + max<int16_t>(4, (cardW - subLen) / 2), cardY + 88);
    c.print(meta.subtitle);

    // Status strip inside card bottom
    c.drawFastHLine(cardX + 8, cardY + 104, cardW - 16, pal.gridDim);
    c.setTextColor(pal.primary);
    c.setCursor(cardX + 8, cardY + 112);
    char timeBuf[16];
    Net.getFormattedTime(timeBuf, sizeof(timeBuf), nullptr, 0);
    c.print(timeBuf);

    c.setTextColor(Net.isConnected() ? pal.ok : pal.warning);
    c.setCursor(cardX + cardW - 44, cardY + 112);
    c.print(Net.isConnected() ? "ONLINE" : "LOCAL");

    // Neighbor App Dock Preview (< PREV | NEXT >)
    uint8_t prevIdx = (_selectedApp + APP_COUNT - 1) % APP_COUNT;
    uint8_t nextIdx = (_selectedApp + 1) % APP_COUNT;
    int16_t dockY   = DisplayManager::kBodyY + 158;
    Display.drawCyberPanel(6, dockY, DisplayManager::kWidth - 12, 22, pal.panelBg, pal.gridDim, pal.secondary, 3);

    c.setTextColor(pal.textDim);
    c.setCursor(10, dockY + 7);
    c.print("<");
    char navShort[8];
    std::strncpy(navShort, getAppMeta(prevIdx).title, 6);
    navShort[6] = '\0';
    c.print(navShort);

    c.setCursor(65, dockY + 7);
    c.setTextColor(pal.secondary);
    c.print("|");

    std::strncpy(navShort, getAppMeta(nextIdx).title, 6);
    navShort[6] = '\0';
    int16_t nw = (std::strlen(navShort) + 1) * 6;
    c.setTextColor(pal.textDim);
    c.setCursor(DisplayManager::kWidth - 10 - nw, dockY + 7);
    c.print(navShort);
    c.print(">");

    // Carousel Pagination Progress Bar
    int16_t barY = DisplayManager::kBodyY + 186;
    for (uint8_t i = 0; i < APP_COUNT; ++i) {
        int16_t bx = 7 + i * 11;
        if (i == _selectedApp) {
            c.fillRect(bx, barY, 9, 5, pal.primary);
        } else {
            c.fillRect(bx + 1, barY + 1, 7, 3, pal.gridDim);
        }
    }

    // Apply brief horizontal slice glitch only during the controlled glitch window
    if (glitchActive) {
        Display.applyGlitchSlices(cardY + 10, cardY + 100, 3, 5, pal.secondary);
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

void UIManager::updateAndRender() {
    if (_state == SystemState::BOOT_SEQUENCE) {
        renderBootSequence();
        Display.flush();
        return;
    }

    uint32_t now = millis();

    // Update header once per second (or every 250ms if FPS overlay is enabled)
    uint32_t hdrInterval = Settings.get().showFpsOverlay ? 250 : 1000;
    if ((now - _lastHeaderTickMs) >= hdrInterval) {
        _lastHeaderTickMs = now;
        _headerNeedsRedraw = true;
    }

    if (_headerNeedsRedraw) {
        const char* headerTitle = "NEON//DECK";
        if (_state == SystemState::IN_APP) {
            headerTitle = getAppMeta((uint8_t)_activeApp).title;
        }
        drawHeader(headerTitle);
        _headerNeedsRedraw = false;
    }

    // Track button hold progress changes for footer bar without redrawing 60x/sec when idle
    bool holdActive = (Buttons.holdProgressA() > 0.01f) || (Buttons.holdProgressB() > 0.01f) ||
                      Buttons.isPressedA() || Buttons.isPressedB();
    if (holdActive || _lastHoldActive) {
        _footerNeedsRedraw = true;
    }
    _lastHoldActive = holdActive;

    // Body & Footer rendering
    if (_state == SystemState::LAUNCHER) {
        bool slideActive = _cardSlideTween.update(Anim.nowUs());
        bool transActive = _transitionTween.update(Anim.nowUs());

        // Controlled cyberpunk glitch burst on Main Screen (every ~3.2s for 130ms)
        bool glitchNow = false;
        if (Settings.get().animIntensity > 0 && !_quickMenuOpen && !_modalActive) {
            if (now >= _nextGlitchMs) {
                _glitchEndMs  = now + 130;
                _nextGlitchMs = now + 2800 + (esp_random() % 1800);
            }
            if (now < _glitchEndMs) {
                glitchNow = true;
            }
        }

        if ((now - _lastLauncherSecMs) >= 1000) {
            _lastLauncherSecMs = now;
            _bodyNeedsRedraw = true;
        }

        if (_bodyNeedsRedraw || slideActive || transActive || glitchNow || _wasGlitching) {
            renderLauncher(glitchNow);
            _bodyNeedsRedraw = false;
            _wasGlitching    = glitchNow;
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

    // Overlays
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

    Display.flush();
}
