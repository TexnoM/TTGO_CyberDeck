#include <Arduino.h>
#include <esp_timer.h>
#include "hardware_config.h"
#include "settings_manager.h"
#include "button_manager.h"
#include "display_manager.h"
#include "animation_engine.h"
#include "wifi_manager.h"
#include "ui_manager.h"

void setup() {
    Serial.begin(115200);
    delay(40);

#if HW_SERIAL_DIAGNOSTICS
    Serial.println();
    Serial.println("====================================================");
    Serial.println("  NEON DECK // POCKET CYBERDECK FIRMWARE (" NEON_DECK_VERSION ")");
    Serial.println("====================================================");
    Serial.printf("  MCU         : %s Rev %d (%lu MHz)\n",
                  ESP.getChipModel(), ESP.getChipRevision(), (unsigned long)ESP.getCpuFreqMHz());
    Serial.printf("  TFT ST7789  : %dx%d | SPI=%d MHz | INV=%d\n",
                  TFT_WIDTH_PX, TFT_HEIGHT_PX, (int)(TFT_SPI_FREQ_HZ / 1000000UL), (int)TFT_INVERT_COLORS);
    Serial.printf("  TFT PINS    : MOSI=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d\n",
                  PIN_TFT_MOSI, PIN_TFT_SCLK, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL);
    Serial.printf("  CGRAM OFFSET: X=%d Y=%d\n", TFT_OFFSET_X, TFT_OFFSET_Y);
    Serial.printf("  BUTTON PINS : BTN_A=GPIO %d | BTN_B=GPIO %d (Active %s)\n",
                  PIN_BTN_A, PIN_BTN_B, BTN_ACTIVE_LOW ? "LOW" : "HIGH");
    Serial.println("====================================================");
#endif

    // 1. Load persistent configuration from NVS
    Settings.begin();

    // 2. Initialize debounced two-button input controller
    Buttons.begin();

    // 3. Initialize ST7789 display & offscreen DMA sprite canvas
    bool dispOk = Display.begin();
#if HW_SERIAL_DIAGNOSTICS
    Serial.printf("[INIT] Display Canvas Allocation: %s (Free Heap: %lu bytes)\n",
                  dispOk ? "OK" : "FAILED", (unsigned long)ESP.getFreeHeap());
#else
    (void)dispOk;
#endif

    // 4. Initialize frame-time animation engine
    Anim.begin(Settings.get().targetFps);

    // 5. Initialize Wi-Fi, NTP & optional battery voltage sensing
    Net.begin();

    // 6. Start NEON DECK UI & Boot Sequence
    UI.begin();
}

void loop() {
    // Non-blocking input & network service ticks
    Buttons.update();
    ButtonEvent ev;
    while ((ev = Buttons.pollEvent()) != ButtonEvent::NONE) {
        UI.handleInput(ev);
    }

    Net.update();

    // High-resolution frame-paced rendering
    if (Anim.shouldRenderFrame()) {
        uint32_t renderStartUs = (uint32_t)esp_timer_get_time();
        UI.updateAndRender();
        uint32_t renderDurationUs = (uint32_t)esp_timer_get_time() - renderStartUs;
        Anim.endFrame(renderDurationUs);
    } else {
        // Yield briefly to FreeRTOS idle/Wi-Fi tasks without stalling button polling
        delay(1);
    }
}
