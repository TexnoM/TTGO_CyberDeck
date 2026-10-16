#include "display_manager.h"
#include <cmath>
#include <cstring>

DisplayManager Display;

LGFX_NeonDeck::LGFX_NeonDeck() {
    {
        auto cfg = _bus.config();
        cfg.spi_host    = VSPI_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = TFT_SPI_FREQ_HZ;
        cfg.freq_read   = 16000000;
        cfg.spi_3wire  = true;
        cfg.use_lock    = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk    = PIN_TFT_SCLK;
        cfg.pin_mosi    = PIN_TFT_MOSI;
        cfg.pin_miso    = PIN_TFT_MISO;
        cfg.pin_dc      = PIN_TFT_DC;
        _bus.config(cfg);
        _panel.setBus(&_bus);
    }
    {
        auto cfg = _panel.config();
        cfg.pin_cs           = PIN_TFT_CS;
        cfg.pin_rst          = PIN_TFT_RST;
        cfg.pin_busy         = -1;
        cfg.panel_width      = TFT_WIDTH_PX;
        cfg.panel_height     = TFT_HEIGHT_PX;
        cfg.offset_x         = TFT_OFFSET_X;
        cfg.offset_y         = TFT_OFFSET_Y;
        cfg.offset_rotation  = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = false;
        cfg.invert           = TFT_INVERT_COLORS;
        cfg.rgb_order        = TFT_RGB_ORDER;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = false;
        _panel.config(cfg);
    }
    {
        auto cfg = _light.config();
        cfg.pin_bl      = PIN_TFT_BL;
        cfg.invert      = !TFT_BL_ACTIVE_HIGH;
        cfg.freq        = 25000; // 25 kHz high-frequency PWM eliminates backlight flicker
        cfg.pwm_channel = 7;
        _light.config(cfg);
        _panel.setLight(&_light);
    }
    setPanel(&_panel);
}

bool DisplayManager::begin() {
    _tft.init();
    _tft.initDMA();
    _tft.setRotation(Settings.get().displayRotation);
    _tft.setBrightness(Settings.get().brightness);
    _tft.fillScreen(TFT_BLACK);

    _canvas.setColorDepth(16);
    void* buf = _canvas.createSprite(kWidth, kHeight);
    _spriteReady = (buf != nullptr);
    if (_spriteReady) {
        _canvas.fillSprite(TFT_BLACK);
        _canvas.setTextWrap(false);
    }
    markAllDirty();
    return _spriteReady;
}

void DisplayManager::setBrightness(uint8_t brightness) {
    _tft.setBrightness(brightness);
}

void DisplayManager::setRotation(uint8_t rotation) {
    _tft.setRotation(rotation & 3);
    markAllDirty();
}

void DisplayManager::markDirty(int16_t x, int16_t y, int16_t w, int16_t h) {
    _dirty.mark(x, y, w, h, kWidth, kHeight);
}

void DisplayManager::flush() {
    if (!_spriteReady || !_dirty.dirty) return;

    int16_t y = _dirty.y1;
    int16_t h = _dirty.height();

    if (h > 0) {
        // Use native swap565_t pointer from LGFX_Sprite for zero-tear, zero-flicker DMA push
        const auto* base = static_cast<const lgfx::swap565_t*>(_canvas.getBuffer());
        _tft.startWrite();
        _tft.pushImageDMA(0, y, kWidth, h, base + (y * kWidth));
        _tft.waitDMA();
        _tft.endWrite();
    }
    _dirty.clear();
}

void DisplayManager::clearBody(uint16_t bgColor) {
    _canvas.fillRect(0, kBodyY, kWidth, kBodyH, bgColor);
    markBodyDirty();
}

void DisplayManager::drawCyberGrid(int16_t shiftX, int16_t shiftY, uint16_t gridColor) {
    constexpr int16_t step = 18;
    int16_t ox = ((shiftX % step) + step) % step;
    int16_t oy = ((shiftY % step) + step) % step;

    for (int16_t x = ox; x < kWidth; x += step) {
        _canvas.drawFastVLine(x, kBodyY, kBodyH, gridColor);
    }
    for (int16_t y = kBodyY + oy; y < kBodyY + kBodyH; y += step) {
        _canvas.drawFastHLine(0, y, kWidth, gridColor);
    }
}

void DisplayManager::drawScanlines(int16_t yStart, int16_t h, uint8_t phase) {
    // Moving scanlines over text caused visible screen strobing/flicker on ST7789;
    // intentionally kept no-op so panels and typography stay rock-solid.
    (void)yStart;
    (void)h;
    (void)phase;
}

void DisplayManager::drawSubtleNoise(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint8_t count) {
    (void)x; (void)y; (void)w; (void)h; (void)color; (void)count;
}

void DisplayManager::applyGlitchSlices(int16_t yMin, int16_t yMax, uint8_t sliceCount, int16_t maxShiftPx, uint16_t accentCol) {
    if (!_spriteReady || Settings.get().animIntensity == 0) return;
    auto* buf = static_cast<uint16_t*>(_canvas.getBuffer());
    uint32_t seed = (Anim.nowMs() / 35) * 2654435761UL;

    for (uint8_t s = 0; s < sliceCount; ++s) {
        seed = seed * 1664525UL + 1013904223UL;
        int16_t sy = yMin + ((seed >> 16) % max<int16_t>(4, (yMax - yMin - 6)));
        int16_t sh = 2 + ((seed >> 12) & 0x03); // 2..5 px slice height
        int16_t shift = (int16_t)((seed >> 8) % (maxShiftPx * 2 + 1)) - maxShiftPx;
        if (shift == 0) shift = 3;

        for (int16_t row = sy; row < sy + sh && row < kHeight; ++row) {
            uint16_t* line = buf + row * kWidth;
            if (shift > 0) {
                std::memmove(line + shift, line, (kWidth - shift) * sizeof(uint16_t));
            } else if (shift < 0) {
                int16_t absShift = -shift;
                std::memmove(line, line + absShift, (kWidth - absShift) * sizeof(uint16_t));
            }
        }
        // Subtle cyber artifact line
        _canvas.drawFastHLine(10, sy, 28, accentCol);
    }
}

void DisplayManager::drawScrollbar(int16_t yTop, int16_t height, uint8_t totalItems, uint8_t visibleItems, uint8_t currentIndex) {
    if (totalItems <= 1) return;
    const ColorPalette& pal = Settings.palette();
    int16_t x = kWidth - 3;
    _canvas.drawFastVLine(x, yTop, height, pal.gridDim);

    int16_t thumbH = max<int16_t>(10, (height * min<uint8_t>(visibleItems, totalItems)) / totalItems);
    int16_t maxScroll = max<int16_t>(1, totalItems - 1);
    int16_t thumbY = yTop + ((height - thumbH) * currentIndex) / maxScroll;
    _canvas.fillRect(x - 1, thumbY, 3, thumbH, pal.primary);
}

void DisplayManager::drawCyberPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                                    uint16_t fillCol, uint16_t borderCol, uint16_t cornerCol,
                                    uint8_t cut) {
    if (w <= 2 || h <= 2) return;
    _canvas.fillRect(x + 1, y + 1, w - 2, h - 2, fillCol);

    // Outer chamfered border
    _canvas.drawFastHLine(x + cut, y, w - cut * 2, borderCol);
    _canvas.drawFastHLine(x + cut, y + h - 1, w - cut * 2, borderCol);
    _canvas.drawFastVLine(x, y + cut, h - cut * 2, borderCol);
    _canvas.drawFastVLine(x + w - 1, y + cut, h - cut * 2, borderCol);

    // Diagonal chamfers
    _canvas.drawLine(x, y + cut, x + cut, y, borderCol);
    _canvas.drawLine(x + w - 1 - cut, y, x + w - 1, y + cut, borderCol);
    _canvas.drawLine(x, y + h - 1 - cut, x + cut, y + h - 1, borderCol);
    _canvas.drawLine(x + w - 1 - cut, y + h - 1, x + w - 1, y + h - 1 - cut, borderCol);

    // Glowing corner brackets
    int16_t brk = min<int16_t>(10, w / 4);
    _canvas.drawFastHLine(x + cut, y, brk, cornerCol);
    _canvas.drawFastHLine(x + w - cut - brk, y + h - 1, brk, cornerCol);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                     float ratio01, uint16_t fillCol, uint16_t borderCol, uint16_t bgCol) {
    ratio01 = constrain(ratio01, 0.0f, 1.0f);
    _canvas.fillRect(x, y, w, h, bgCol);
    _canvas.drawRect(x, y, w, h, borderCol);
    int16_t innerW = w - 4;
    int16_t fillW  = (int16_t)(innerW * ratio01);
    if (fillW > 0) {
        _canvas.fillRect(x + 2, y + 2, fillW, h - 4, fillCol);
        for (int16_t sx = x + 6; sx < x + 2 + fillW; sx += 5) {
            _canvas.drawFastVLine(sx, y + 2, h - 4, bgCol);
        }
    }
}

void DisplayManager::drawAppIcon(uint8_t appIndex, int16_t cx, int16_t cy,
                                 uint16_t primaryCol, uint16_t secondaryCol, uint32_t animMs) {
    (void)animMs;
    switch (appIndex) {
        case 0: { // SYSTEM — Dual-Core MCU
            _canvas.drawRect(cx - 8, cy - 8, 17, 17, primaryCol);
            _canvas.fillRect(cx - 5, cy - 5, 5, 11, secondaryCol);
            _canvas.fillRect(cx + 1, cy - 5, 5, 11, primaryCol);
            for (int i = -5; i <= 5; i += 5) {
                _canvas.drawFastHLine(cx - 12, cy + i, 4, primaryCol);
                _canvas.drawFastHLine(cx + 9,  cy + i, 4, primaryCol);
                _canvas.drawFastVLine(cx + i, cy - 12, 4, primaryCol);
                _canvas.drawFastVLine(cx + i, cy + 9,  4, primaryCol);
            }
            break;
        }
        case 1: { // WIFI RADAR — Radar sweep
            _canvas.drawCircle(cx, cy, 11, secondaryCol);
            _canvas.drawCircle(cx, cy, 6, secondaryCol);
            _canvas.drawLine(cx, cy, cx + 8, cy - 7, primaryCol);
            _canvas.fillCircle(cx + 5, cy - 4, 2, primaryCol);
            _canvas.fillCircle(cx, cy, 2, primaryCol);
            break;
        }
        case 2: { // PKT SNIFFER — Spectrum analyzer bars + waveform
            int16_t heights[7] = {5, 11, 16, 9, 18, 13, 7};
            for (int i = 0; i < 7; ++i) {
                int16_t bx = cx - 10 + i * 3;
                int16_t bh = heights[i];
                uint16_t col = (i == 2 || i == 4) ? primaryCol : secondaryCol;
                _canvas.fillRect(bx, cy + 9 - bh, 2, bh, col);
            }
            _canvas.drawFastHLine(cx - 11, cy + 10, 22, primaryCol);
            break;
        }
        case 3: { // NET TOOLS — Network nodes
            _canvas.drawCircle(cx - 7, cy - 5, 3, primaryCol);
            _canvas.drawCircle(cx + 7, cy - 5, 3, secondaryCol);
            _canvas.drawCircle(cx,     cy + 6, 3, primaryCol);
            _canvas.drawLine(cx - 4, cy - 5, cx + 4, cy - 5, secondaryCol);
            _canvas.drawLine(cx - 5, cy - 2, cx - 1, cy + 4, primaryCol);
            _canvas.drawLine(cx + 5, cy - 2, cx + 1, cy + 4, primaryCol);
            break;
        }
        case 4: { // WEB DECK — Cyber globe / remote console
            _canvas.drawCircle(cx, cy, 10, primaryCol);
            _canvas.drawEllipse(cx, cy, 5, 10, secondaryCol);
            _canvas.drawFastHLine(cx - 10, cy, 21, secondaryCol);
            _canvas.drawFastHLine(cx - 8, cy - 5, 17, primaryCol);
            _canvas.drawFastHLine(cx - 8, cy + 5, 17, primaryCol);
            break;
        }
        case 5: { // CRYPTO LAB — Cyber shield / key lock
            _canvas.drawRect(cx - 7, cy - 2, 15, 11, primaryCol);
            _canvas.drawCircle(cx, cy - 4, 5, secondaryCol);
            _canvas.fillRect(cx - 1, cy + 1, 3, 5, secondaryCol);
            break;
        }
        case 6: { // TERMINAL — [ >_ ] prompt
            _canvas.drawRect(cx - 11, cy - 9, 22, 18, secondaryCol);
            _canvas.drawFastHLine(cx - 11, cy - 5, 22, secondaryCol);
            _canvas.drawLine(cx - 7, cy - 1, cx - 3, cy + 2, primaryCol);
            _canvas.drawLine(cx - 7, cy + 5, cx - 3, cy + 2, primaryCol);
            _canvas.fillRect(cx, cy + 4, 6, 2, primaryCol);
            break;
        }
        case 7: { // MATRIX — Cascading digital rain
            for (int i = -8; i <= 8; i += 4) {
                int16_t drop = ((i + 8) * 2) % 12;
                _canvas.drawFastVLine(cx + i, cy - 8, 16, secondaryCol);
                _canvas.fillRect(cx + i - 1, cy - 6 + drop, 3, 3, primaryCol);
            }
            break;
        }
        case 8: { // STARFIELD — 4-point star
            _canvas.drawLine(cx - 10, cy, cx + 10, cy, primaryCol);
            _canvas.drawLine(cx, cy - 10, cx, cy + 10, primaryCol);
            _canvas.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, secondaryCol);
            _canvas.drawLine(cx - 5, cy + 5, cx + 5, cy - 5, secondaryCol);
            _canvas.fillCircle(cx, cy, 2, primaryCol);
            break;
        }
        case 9: { // CLOCK — Chronometer
            _canvas.drawCircle(cx, cy, 10, primaryCol);
            _canvas.drawCircle(cx, cy, 8, secondaryCol);
            _canvas.drawFastVLine(cx, cy - 6, 6, primaryCol);
            _canvas.drawFastHLine(cx, cy, 5, secondaryCol);
            break;
        }
        default: { // SETTINGS — Sliders
            for (int i = -6; i <= 6; i += 6) {
                _canvas.drawFastHLine(cx - 9, cy + i, 18, secondaryCol);
                int16_t knobX = cx + ((i == 0) ? 4 : -4);
                _canvas.fillRect(knobX - 2, cy + i - 2, 5, 5, primaryCol);
            }
            break;
        }
    }
}
