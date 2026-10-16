// ============================================================================
// TTGO_CyberDeck — Minimal Display & Two-Button Hardware Verifier
// ============================================================================
// Purpose: Standalone minimal diagnostic test to verify ST7789 SPI pins,
// backlight polarity, CGRAM offsets, and GPIO 0 / GPIO 35 buttons on a
// Chinese TTGO T-Display 1.14" ESP32 clone before or independently of main OS.
// ============================================================================

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define TEST_PIN_MOSI 19
#define TEST_PIN_SCLK 18
#define TEST_PIN_CS   5
#define TEST_PIN_DC   16
#define TEST_PIN_RST  23
#define TEST_PIN_BL   4
#define TEST_BTN_A    0
#define TEST_BTN_B    35

class LGFX_MinimalTest : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
public:
    LGFX_MinimalTest() {
        auto bcfg = _bus.config();
        bcfg.spi_host   = VSPI_HOST;
        bcfg.spi_mode   = 0;
        bcfg.freq_write = 40000000;
        bcfg.pin_sclk   = TEST_PIN_SCLK;
        bcfg.pin_mosi   = TEST_PIN_MOSI;
        bcfg.pin_miso   = -1;
        bcfg.pin_dc     = TEST_PIN_DC;
        _bus.config(bcfg);
        _panel.setBus(&_bus);

        auto pcfg = _panel.config();
        pcfg.pin_cs       = TEST_PIN_CS;
        pcfg.pin_rst      = TEST_PIN_RST;
        pcfg.panel_width  = 135;
        pcfg.panel_height = 240;
        pcfg.offset_x     = 52;
        pcfg.offset_y     = 40;
        pcfg.invert       = true;
        _panel.config(pcfg);
        setPanel(&_panel);
    }
};

static LGFX_MinimalTest tft;

void setup() {
    Serial.begin(115200);
    pinMode(TEST_PIN_BL, OUTPUT);
    digitalWrite(TEST_PIN_BL, HIGH);
    pinMode(TEST_BTN_A, INPUT_PULLUP);
    pinMode(TEST_BTN_B, INPUT);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 135, 240, TFT_GREEN);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("HW VERIFIER OK", 12, 20);
    Serial.println("[VERIFY] ST7789 135x240 & Buttons initialized.");
}

void loop() {
    bool a = (digitalRead(TEST_BTN_A) == LOW);
    bool b = (digitalRead(TEST_BTN_B) == LOW);
    tft.setTextColor(a ? TFT_CYAN : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(a ? "BTN A (GPIO 0): DOWN " : "BTN A (GPIO 0): UP   ", 8, 60);
    tft.setTextColor(b ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
    tft.drawString(b ? "BTN B (GPIO35): DOWN " : "BTN B (GPIO35): UP   ", 8, 80);
    delay(25);
}
