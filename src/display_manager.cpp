#include "display_manager.h"
#include <cmath>
#include <cstring>

DisplayManager Display;

LGFX_TTGO_CyberDeck::LGFX_TTGO_CyberDeck() {
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
    setPanel(&_panel);
}

#include <U8g2lib.h>

// High quality cyrillic + latin unicode font (6x12 px bitmap)
static lgfx::U8g2font s_u8g2_cyrillic(u8g2_font_6x12_t_cyrillic);

bool DisplayManager::begin() {
    _tft.init();
    _tft.initDMA();
    _tft.setRotation(Settings.get().displayRotation);
    _tft.fillScreen(TFT_BLACK);

    // Direct hardware PWM via analogWrite (LEDC) for reliable brightness & sleep dimming
    pinMode(PIN_TFT_BL, OUTPUT);
    setBrightness(Settings.get().brightness);

    _canvas.setColorDepth(16);
    void* buf = _canvas.createSprite(kWidth, kHeight);
    _spriteReady = (buf != nullptr);
    if (_spriteReady) {
        _canvas.fillSprite(TFT_BLACK);
        _canvas.setTextWrap(false);
        resetFont();
    }
    markAllDirty();
    return _spriteReady;
}

void DisplayManager::resetFont() {
    if (!_spriteReady) return;
    _canvas.setFont(&s_u8g2_cyrillic);
    _canvas.setTextSize(1);
}

void DisplayManager::setClockFont() {
    if (!_spriteReady) return;
    _canvas.setFont(&fonts::Font4);
    _canvas.setTextSize(2);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, brightness);
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
    if (!_spriteReady || Settings.get().animIntensity == 0) return;
    // Soft CRT scanline texture: subtly tint every 3rd row ONLY where background/panel pixels exist
    // so bright text and icons NEVER flicker or strobe!
    const ColorPalette& pal = Settings.palette();
    uint16_t tintBg    = AnimationEngine::lerpColor565(pal.bg, 0x0000, 65);
    uint16_t tintPanel = AnimationEngine::lerpColor565(pal.panelBg, pal.gridDim, 55);

    int16_t offset = phase % 3;
    for (int16_t y = yStart + offset; y < yStart + h && y < kHeight; y += 3) {
        for (int16_t x = 2; x < kWidth - 2; x += 2) {
            uint16_t px = _canvas.readPixel(x, y);
            if (px == pal.panelBg) {
                _canvas.drawPixel(x, y, tintPanel);
            } else if (px == pal.bg) {
                _canvas.drawPixel(x, y, tintBg);
            }
        }
    }
}

void DisplayManager::drawSubtleNoise(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, uint8_t count) {
    if (Settings.get().animIntensity == 0) return;
    const ColorPalette& pal = Settings.palette();
    uint16_t softSpark = AnimationEngine::lerpColor565(pal.panelBg, color, 85);
    uint32_t seed = (Anim.nowMs() / 120) * 1664525UL + 1013904223UL;
    for (uint8_t i = 0; i < count; ++i) {
        seed = seed * 1664525UL + 1013904223UL;
        int16_t px = x + ((seed >> 16) % max<int16_t>(1, w));
        seed = seed * 1664525UL + 1013904223UL;
        int16_t py = y + ((seed >> 16) % max<int16_t>(1, h));
        uint16_t cur = _canvas.readPixel(px, py);
        if (cur == pal.bg || cur == pal.panelBg) {
            _canvas.drawPixel(px, py, softSpark);
        }
    }
}

void DisplayManager::drawSoftScanBeam(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t animMs, uint16_t glowCol) {
    if (Settings.get().animIntensity == 0 || h <= 8) return;
    const ColorPalette& pal = Settings.palette();
    int16_t beamY = y + 3 + ((animMs / 28) % (h - 6));
    uint16_t softGlow1 = AnimationEngine::lerpColor565(pal.panelBg, glowCol, 65);
    uint16_t softGlow2 = AnimationEngine::lerpColor565(pal.panelBg, glowCol, 32);

    for (int16_t dx = x + 3; dx < x + w - 3; ++dx) {
        if (_canvas.readPixel(dx, beamY) == pal.panelBg) {
            _canvas.drawPixel(dx, beamY, softGlow1);
        }
        if (beamY > y + 2 && _canvas.readPixel(dx, beamY - 1) == pal.panelBg) {
            _canvas.drawPixel(dx, beamY - 1, softGlow2);
        }
    }
}

void DisplayManager::applyMultiGlitch(int16_t yMin, int16_t yMax, uint8_t pattern, uint32_t animMs) {
    if (!_spriteReady || Settings.get().animIntensity == 0) return;
    const ColorPalette& pal = Settings.palette();
    auto* buf = static_cast<uint16_t*>(_canvas.getBuffer());
    uint32_t seed = (animMs / 55) * 2654435761UL;

    uint8_t pat = pattern % GLITCH_PATTERN_COUNT;

    switch (pat) {
        case GLITCH_RGB_SPLIT: {
            // Pattern 0: Soft RGB Chromatic Split + twin micro-slices
            for (uint8_t s = 0; s < 2; ++s) {
                seed = seed * 1664525UL + 1013904223UL;
                int16_t sy = yMin + ((seed >> 16) % max<int16_t>(6, (yMax - yMin - 8)));
                int16_t shift = (s == 0) ? 2 : -2;
                for (int16_t r = sy; r < sy + 3 && r < kHeight; ++r) {
                    uint16_t* line = buf + r * kWidth;
                    if (shift > 0) std::memmove(line + shift, line, (kWidth - shift) * sizeof(uint16_t));
                    else std::memmove(line, line - shift, (kWidth + shift) * sizeof(uint16_t));
                }
                _canvas.drawFastHLine(12, sy, 22, (s == 0) ? pal.secondary : pal.danger);
            }
            break;
        }
        case GLITCH_WAVE_SLICE: {
            // Pattern 1: Smooth Sine Wave Horizontal Ripple across the icon/title area
            int16_t waveCenter = (yMin + yMax) / 2;
            for (int16_t r = waveCenter - 12; r <= waveCenter + 12; r += 2) {
                if (r < yMin || r >= yMax || r >= kHeight) continue;
                int16_t shift = (int16_t)(std::sin((r * 0.35f) + (animMs * 0.02f)) * 2.5f);
                uint16_t* line = buf + r * kWidth;
                if (shift > 0) std::memmove(line + shift, line, (kWidth - shift) * sizeof(uint16_t));
                else if (shift < 0) std::memmove(line, line - shift, (kWidth + shift) * sizeof(uint16_t));
            }
            break;
        }
        case GLITCH_SCAN_BEAM: {
            // Pattern 2: Double Cyber Laser Sync Bars with phosphor trail
            int16_t by1 = yMin + ((animMs / 12) % max<int16_t>(8, yMax - yMin));
            uint16_t barCol = AnimationEngine::lerpColor565(pal.panelBg, pal.secondary, 110);
            _canvas.drawFastHLine(10, by1, kWidth - 20, barCol);
            _canvas.drawFastHLine(18, by1 + 2, kWidth - 36, AnimationEngine::lerpColor565(pal.panelBg, pal.primary, 90));
            break;
        }
        case GLITCH_HOLO_NOISE: {
            // Pattern 3: Holographic Data Burst (dashed binary telemetry lines + soft slice)
            seed = seed * 1664525UL + 1013904223UL;
            int16_t sy = yMin + ((seed >> 16) % max<int16_t>(8, yMax - yMin - 10));
            uint16_t dashCol = AnimationEngine::lerpColor565(pal.panelBg, pal.highlight, 120);
            for (int16_t x = 12; x < kWidth - 14; x += 6) {
                if (((x + (seed & 0xF)) & 4) == 0) {
                    _canvas.drawFastHLine(x, sy, 4, dashCol);
                }
            }
            uint16_t* line = buf + (sy + 1) * kWidth;
            std::memmove(line + 2, line, (kWidth - 2) * sizeof(uint16_t));
            break;
        }
        case GLITCH_CLASSIC_SOFT: {
            // Pattern 4: Softened Classic Glitch (gentle scanlines + subtle noise + 2px jitter)
            drawScanlines(yMin, yMax - yMin, (uint8_t)((animMs / 90) % 3));
            drawSubtleNoise(10, yMin, kWidth - 20, yMax - yMin, pal.secondary, 8);
            int16_t sy = yMin + 14 + ((seed >> 15) % max<int16_t>(6, yMax - yMin - 20));
            uint16_t* line = buf + sy * kWidth;
            std::memmove(line + 2, line, (kWidth - 2) * sizeof(uint16_t));
            break;
        }
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
    (void)cut;
    if (w <= 2 || h <= 2) return;
    _canvas.fillRect(x, y, w, h, fillCol);
    // Minimalist top accent bar + tiny corner ticks
    _canvas.drawFastHLine(x, y, w, borderCol);
    _canvas.drawFastHLine(x, y + h - 1, w, Settings.palette().gridDim);
    _canvas.fillRect(x, y, 3, 2, cornerCol);
    _canvas.fillRect(x + w - 3, y, 3, 2, cornerCol);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                     float ratio01, uint16_t fillCol, uint16_t borderCol, uint16_t bgCol) {
    (void)borderCol;
    ratio01 = constrain(ratio01, 0.0f, 1.0f);
    _canvas.fillRect(x, y, w, h, bgCol);
    int16_t fillW = (int16_t)(w * ratio01);
    if (fillW > 0) {
        _canvas.fillRect(x, y, fillW, h, fillCol);
    }
}

void DisplayManager::drawAppIcon(uint8_t appIndex, int16_t cx, int16_t cy,
                                 uint16_t primaryCol, uint16_t secondaryCol, uint32_t animMs) {
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
            float ang = (animMs % 2400) * (6.283185f / 2400.0f);
            int16_t rx = cx + (int16_t)(std::cos(ang) * 9.0f);
            int16_t ry = cy + (int16_t)(std::sin(ang) * 9.0f);
            _canvas.drawLine(cx, cy, rx, ry, primaryCol);
            _canvas.fillCircle(cx, cy, 2, primaryCol);
            break;
        }
        case 2: { // PKT SNIFFER — Spectrum analyzer bars
            int16_t heights[7] = {6, 12, 16, 10, 18, 13, 8};
            for (int i = 0; i < 7; ++i) {
                int16_t bx = cx - 10 + i * 3;
                int16_t bh = heights[i] - ((animMs / 180 + i) % 4);
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
        case 5: { // CYBER EYE — Cybernetic Eye / Virtual Pet
            _canvas.drawCircle(cx, cy, 10, primaryCol);
            _canvas.drawCircle(cx, cy, 4, secondaryCol);
            _canvas.fillCircle(cx, cy, 2, primaryCol);
            _canvas.drawFastHLine(cx - 12, cy, 3, secondaryCol);
            _canvas.drawFastHLine(cx + 10, cy, 3, secondaryCol);
            break;
        }
        case 6: { // 3D CUBE — Wireframe Isometric Cube
            int16_t s = 6;
            _canvas.drawRect(cx - s, cy - s, s * 2, s * 2, primaryCol);
            _canvas.drawLine(cx - s, cy - s, cx - s - 3, cy - s - 3, secondaryCol);
            _canvas.drawLine(cx + s, cy - s, cx + s - 3, cy - s - 3, secondaryCol);
            _canvas.drawLine(cx + s, cy + s, cx + s - 3, cy + s - 3, secondaryCol);
            _canvas.drawLine(cx - s, cy + s, cx - s - 3, cy + s - 3, secondaryCol);
            _canvas.drawRect(cx - s - 3, cy - s - 3, s * 2, s * 2, secondaryCol);
            break;
        }
        case 7: { // TERMINAL — [ >_ ] prompt
            _canvas.drawRect(cx - 11, cy - 9, 22, 18, secondaryCol);
            _canvas.drawFastHLine(cx - 11, cy - 5, 22, secondaryCol);
            _canvas.drawLine(cx - 7, cy - 1, cx - 3, cy + 2, primaryCol);
            _canvas.drawLine(cx - 7, cy + 5, cx - 3, cy + 2, primaryCol);
            if ((animMs / 400) % 2 == 0) {
                _canvas.fillRect(cx, cy + 4, 6, 2, primaryCol);
            }
            break;
        }
        case 8: { // MATRIX — Cascading digital rain
            for (int i = -8; i <= 8; i += 4) {
                int16_t drop = ((animMs / 110) + (i + 8) * 2) % 12;
                _canvas.drawFastVLine(cx + i, cy - 8, 16, secondaryCol);
                _canvas.fillRect(cx + i - 1, cy - 6 + drop, 3, 3, primaryCol);
            }
            break;
        }
        case 9: { // STARFIELD — 4-point star
            _canvas.drawLine(cx - 10, cy, cx + 10, cy, primaryCol);
            _canvas.drawLine(cx, cy - 10, cx, cy + 10, primaryCol);
            _canvas.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, secondaryCol);
            _canvas.drawLine(cx - 5, cy + 5, cx + 5, cy - 5, secondaryCol);
            _canvas.fillCircle(cx, cy, 2, primaryCol);
            break;
        }
        case 10: { // CLOCK — Chronometer
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
