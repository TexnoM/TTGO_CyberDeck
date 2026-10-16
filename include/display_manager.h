#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "hardware_config.h"
#include "animation_engine.h"
#include "settings_manager.h"

class LGFX_NeonDeck : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;

public:
    LGFX_NeonDeck();
};

class DisplayManager {
public:
    static constexpr int16_t kWidth  = TFT_WIDTH_PX;
    static constexpr int16_t kHeight = TFT_HEIGHT_PX;

    static constexpr int16_t kHeaderH = 18;
    static constexpr int16_t kFooterH = 18;
    static constexpr int16_t kBodyY   = kHeaderH;
    static constexpr int16_t kBodyH   = kHeight - kHeaderH - kFooterH; // 204 px

    bool begin();
    void setBrightness(uint8_t brightness);
    void setRotation(uint8_t rotation);

    LGFX_Sprite& canvas() { return _canvas; }
    LGFX_NeonDeck& tft() { return _tft; }

    // Dirty-region tracking & flicker-free SPI push
    void markDirty(int16_t x, int16_t y, int16_t w, int16_t h);
    void markHeaderDirty() { markDirty(0, 0, kWidth, kHeaderH); }
    void markBodyDirty()   { markDirty(0, kBodyY, kWidth, kBodyH); }
    void markFooterDirty() { markDirty(0, kHeight - kFooterH, kWidth, kFooterH); }
    void markAllDirty()    { markDirty(0, 0, kWidth, kHeight); }
    bool isDirty() const   { return _dirty.dirty; }

    // Push dirty region to ST7789 cleanly without flicker or byte-order artifacts
    void flush();

    // Reusable Cyberpunk Graphics Primitives
    void clearBody(uint16_t bgColor);
    void drawCyberGrid(int16_t shiftX, int16_t shiftY, uint16_t gridColor);
    void drawScanlines(int16_t yStart, int16_t h, uint8_t phase);
    void drawSubtleNoise(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint8_t count);

    // Subtle horizontal slice glitch effect (copies & shifts horizontal strips within canvas)
    void applyGlitchSlices(int16_t yMin, int16_t yMax, uint8_t sliceCount, int16_t maxShiftPx, uint16_t accentCol);

    // Vertical scrollbar for menus and lists
    void drawScrollbar(int16_t yTop, int16_t height, uint8_t totalItems, uint8_t visibleItems, uint8_t currentIndex);

    // Chamfered futuristic panel with glowing corner brackets
    void drawCyberPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t fillCol, uint16_t borderCol, uint16_t cornerCol,
                        uint8_t cut = 4);

    // Neon progress bar with segmented cyber ticks
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                         float ratio01, uint16_t fillCol, uint16_t borderCol, uint16_t bgCol);

    // Pixel-art / vector app icons (24x24) for all 11 apps
    void drawAppIcon(uint8_t appIndex, int16_t cx, int16_t cy, uint16_t primaryCol, uint16_t secondaryCol, uint32_t animMs);

private:
    LGFX_NeonDeck _tft;
    LGFX_Sprite   _canvas{&_tft};
    DirtyRegion   _dirty;
    bool          _spriteReady = false;
};

extern DisplayManager Display;
