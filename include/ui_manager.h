#pragma once
#include <Arduino.h>
#include "display_manager.h"
#include "button_manager.h"
#include "animation_engine.h"

enum AppId : uint8_t {
    APP_SYSTEM = 0,
    APP_WIFI_RADAR,
    APP_PKT_SNIFFER,
    APP_NET_TOOLS,
    APP_WEB_DECK,
    APP_CRYPTO_LAB,
    APP_TERMINAL,
    APP_MATRIX,
    APP_STARFIELD,
    APP_CLOCK,
    APP_SETTINGS,
    APP_COUNT
};

enum class SystemState : uint8_t {
    BOOT_SEQUENCE = 0,
    LAUNCHER,
    IN_APP
};

using ModalCallback = void (*)();

class UIManager {
public:
    void begin();
    void handleInput(ButtonEvent ev);
    void updateAndRender();

    // Navigation
    void openApp(AppId id);
    void returnToLauncher();
    SystemState state() const { return _state; }
    AppId currentApp() const { return _activeApp; }

    // Reusable UI Components
    void drawHeader(const char* title);
    void drawFooter(const char* hintA, const char* hintB);
    void drawMenuRow(int16_t y, const char* label, const char* value, bool selected, bool highlightValue = false);
    void drawLoadingSpinner(int16_t cx, int16_t cy, int16_t radius, uint16_t col);

    // Toast Notification
    void showToast(const char* message, uint16_t borderCol = 0, uint16_t durationMs = 1800);

    // Modal Dialog
    void showConfirmModal(const char* title, const char* line1, const char* line2, ModalCallback onConfirm);
    bool isModalActive() const { return _modalActive; }

    // Request redraw of body on next frame
    void invalidateBody()   { _bodyNeedsRedraw = true; }
    void invalidateFooter() { _footerNeedsRedraw = true; }
    void invalidateAll()    { _headerNeedsRedraw = _bodyNeedsRedraw = _footerNeedsRedraw = true; }

private:
    SystemState _state             = SystemState::BOOT_SEQUENCE;
    uint32_t    _bootStartMs       = 0;

    // Launcher state & multi-glitch engine
    uint8_t     _selectedApp       = 0;
    TweenValue  _cardSlideTween;
    TweenValue  _transitionTween;
    uint32_t    _nextGlitchMs      = 1500;
    uint32_t    _glitchEndMs       = 0;
    uint8_t     _glitchPattern     = 0;
    uint32_t    _lastLauncherFxMs  = 0;

    // Active app
    AppId       _activeApp         = APP_SYSTEM;

    // Quick Menu overlay (Long-press B anywhere)
    bool        _quickMenuOpen     = false;
    uint8_t     _quickMenuIndex    = 0;
    TweenValue  _quickMenuTween;

    // Modal Dialog state
    bool          _modalActive     = false;
    char          _modalTitle[22]  = {0};
    char          _modalLine1[26]  = {0};
    char          _modalLine2[26]  = {0};
    ModalCallback _modalConfirmCb  = nullptr;

    // Toast state
    bool        _toastActive       = false;
    char        _toastMsg[28]      = {0};
    uint16_t    _toastCol          = 0;
    uint32_t    _toastEndMs        = 0;

    // Dirty tracking flags
    bool        _headerNeedsRedraw = true;
    bool        _bodyNeedsRedraw   = true;
    bool        _footerNeedsRedraw = true;
    uint32_t    _lastHeaderTickMs  = 0;
    bool        _lastHoldActive    = false;

    void renderBootSequence();
    void renderLauncher(bool glitchActive);
    void renderQuickMenu();
    void renderModal();
    void renderToast();

    void handleQuickMenuInput(ButtonEvent ev);
};

extern UIManager UI;
