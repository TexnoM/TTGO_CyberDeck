#pragma once
#include <Arduino.h>

// ============================================================================
// NEON DECK — HARDWARE CONFIGURATION & CLONE COMPATIBILITY MATRIX
// ============================================================================
// Target: Chinese Clone of LilyGO TTGO T-Display 1.14" (ESP32 + ST7789 135x240)
//
// KNOWN HARDWARE VARIANTS IN THE WILD:
// ----------------------------------------------------------------------------
// 1. VARIANT A (Default - Standard T-Display v1.1 Clone, ~95% of boards):
//    - USB-UART: CH9102F / CP2104 / CH340K / CH340C
//    - TFT ST7789: MOSI=19, SCLK=18, CS=5, DC=16, RST=23, BL=4 (Active HIGH)
//    - CGRAM Offset: X=52, Y=40 (135x240 window inside 240x320 ST7789 RAM)
//    - Color Inversion: TRUE (IPS panel)
//    - Buttons: BTN_A = GPIO 0 (Active LOW), BTN_B = GPIO 35 (Active LOW)
//    - Battery ADC: GPIO 34 with ADC_EN on GPIO 14
//
// 2. VARIANT B (Alternative Clone - Shifted CGRAM / Non-IPS or Alt Button):
//    - Same SPI pins, or RST tied to EN (-1)
//    - CGRAM Offset: X=53, Y=40 (fixes 1px garbage line on right/bottom edge)
//    - Color Inversion: sometimes FALSE on cheaper TN panels
//    - Enable via -DHW_PROFILE_VARIANT_B=1 in platformio.ini or uncomment below
//
// HOW TO VERIFY YOUR BOARD'S PINOUT:
// ----------------------------------------------------------------------------
// - Boot with Serial Monitor at 115200 baud: NEON DECK prints active pins and
//   button GPIO state changes in real time when Hardware Diagnostics is enabled.
// - If you see a 1-pixel colored noise line along the edge: change TFT_OFFSET_X
//   from 52 to 53 (or 51) and TFT_OFFSET_Y from 40 to 41.
// - If colors look like a photo negative (white background instead of black):
//   toggle TFT_INVERT_COLORS between true and false.
// - If backlight stays dark: toggle TFT_BL_ACTIVE_HIGH or check PIN_TFT_BL.
// ============================================================================

// Uncomment to select Variant B directly in this header:
// #define HW_PROFILE_VARIANT_B 1

// ----------------------------------------------------------------------------
// 1. DISPLAY PINS & ST7789 PARAMETERS
// ----------------------------------------------------------------------------
#ifndef PIN_TFT_MOSI
#define PIN_TFT_MOSI        19
#endif

#ifndef PIN_TFT_SCLK
#define PIN_TFT_SCLK        18
#endif

#ifndef PIN_TFT_MISO
#define PIN_TFT_MISO        -1
#endif

#ifndef PIN_TFT_CS
#define PIN_TFT_CS          5
#endif

#ifndef PIN_TFT_DC
#define PIN_TFT_DC          16
#endif

#ifndef PIN_TFT_RST
#define PIN_TFT_RST         23
#endif

#ifndef PIN_TFT_BL
#define PIN_TFT_BL          4
#endif

#ifndef TFT_BL_ACTIVE_HIGH
#define TFT_BL_ACTIVE_HIGH  true
#endif

// Physical display resolution in Portrait orientation
#ifndef TFT_WIDTH_PX
#define TFT_WIDTH_PX        135
#endif

#ifndef TFT_HEIGHT_PX
#define TFT_HEIGHT_PX       240
#endif

// Display rotation: 0 = Portrait (USB at bottom), 2 = Portrait Flipped (USB at top)
#ifndef TFT_DEFAULT_ROTATION
#define TFT_DEFAULT_ROTATION 0
#endif

// ST7789 CGRAM window offsets for 135x240 panel
#if defined(HW_PROFILE_VARIANT_B)
    #ifndef TFT_OFFSET_X
    #define TFT_OFFSET_X    53
    #endif
    #ifndef TFT_OFFSET_Y
    #define TFT_OFFSET_Y    40
    #endif
    #ifndef TFT_INVERT_COLORS
    #define TFT_INVERT_COLORS false
    #endif
#else
    #ifndef TFT_OFFSET_X
    #define TFT_OFFSET_X    52
    #endif
    #ifndef TFT_OFFSET_Y
    #define TFT_OFFSET_Y    40
    #endif
    #ifndef TFT_INVERT_COLORS
    #define TFT_INVERT_COLORS true
    #endif
#endif

// RGB/BGR color order (false = RGB for standard T-Display ST7789, true = BGR)
#ifndef TFT_RGB_ORDER
#define TFT_RGB_ORDER       false
#endif

// SPI Clock Frequency in Hz:
// 40 MHz is rock-solid on all clones; 80 MHz works on most short-trace PCBs.
#ifndef TFT_SPI_FREQ_HZ
#define TFT_SPI_FREQ_HZ     40000000
#endif

// Backlight PWM Configuration
#ifndef TFT_BL_PWM_FREQ
#define TFT_BL_PWM_FREQ     12000
#endif

#ifndef TFT_BL_PWM_BITS
#define TFT_BL_PWM_BITS     8
#endif

// ----------------------------------------------------------------------------
// 2. TWO-BUTTON INPUT CONFIGURATION
// ----------------------------------------------------------------------------
// Button A: Navigation / Next (Short) | Back / Exit (Long)
#ifndef PIN_BTN_A
#define PIN_BTN_A           0
#endif

// Button B: Select / Confirm (Short) | Quick Menu (Long)
// Note: GPIO 35 is input-only on ESP32 and uses an onboard external pull-up.
#ifndef PIN_BTN_B
#define PIN_BTN_B           35
#endif

#ifndef BTN_ACTIVE_LOW
#define BTN_ACTIVE_LOW      true
#endif

// Debounce and gesture timing in milliseconds
#ifndef BTN_DEBOUNCE_MS
#define BTN_DEBOUNCE_MS     28
#endif

#ifndef BTN_LONG_PRESS_MS
#define BTN_LONG_PRESS_MS   520
#endif

#ifndef BTN_CHORD_WINDOW_MS
#define BTN_CHORD_WINDOW_MS 90
#endif

// ----------------------------------------------------------------------------
// 3. OPTIONAL BATTERY VOLTAGE SENSING & AUDIO
// ----------------------------------------------------------------------------
// TTGO T-Display has a voltage divider on GPIO 34 enabled by GPIO 14.
// Set BATTERY_SENSE_ENABLED to true to measure real LiPo voltage.
// NEON DECK automatically validates the ADC reading and hides the battery
// indicator if no plausible battery/USB voltage is detected.
#ifndef BATTERY_SENSE_ENABLED
#define BATTERY_SENSE_ENABLED true
#endif

#ifndef PIN_BAT_ADC
#define PIN_BAT_ADC         34
#endif

#ifndef PIN_BAT_ADC_EN
#define PIN_BAT_ADC_EN      14
#endif

// Optional passive piezo buzzer pin (-1 = no buzzer hardware connected)
#ifndef PIN_BUZZER
#define PIN_BUZZER          -1
#endif

// ----------------------------------------------------------------------------
// 4. PERFORMANCE & ENGINE TARGETS
// ----------------------------------------------------------------------------
// NOTE: 120 Hz is an engine timing option, NOT an assumed physical capability
// of the ST7789 panel. At 40 MHz SPI, a full 135x240x16bpp frame transfer takes
// ~13 ms (~75 FPS theoretical bus max), while partial/dirty-region updates can
// run faster. Default target is 60 FPS for rock-solid frame pacing.
#ifndef DEFAULT_TARGET_FPS
#define DEFAULT_TARGET_FPS  60
#endif

// Enable Serial Hardware Diagnostic Logging on boot and button events
#ifndef HW_SERIAL_DIAGNOSTICS
#define HW_SERIAL_DIAGNOSTICS true
#endif
