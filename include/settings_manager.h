#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "hardware_config.h"

enum ThemeId : uint8_t {
    THEME_NEON_GREEN = 0,
    THEME_CYBER_CYAN = 1,
    THEME_AMBER_GOLD = 2,
    THEME_SYNTH_PINK = 3,
    THEME_COUNT      = 4
};

struct ColorPalette {
    const char* name;
    uint16_t bg;           // Nearly black background
    uint16_t panelBg;      // Dark panel fill
    uint16_t gridDim;      // Subtle background grid / scanline tint
    uint16_t primary;      // Main neon accent
    uint16_t secondary;    // Secondary accent (cyan/amber contrast)
    uint16_t highlight;    // Bright glowing highlight
    uint16_t textMain;     // Readable primary text
    uint16_t textDim;      // Muted labels
    uint16_t warning;      // Amber warning
    uint16_t danger;       // Red alert/error
    uint16_t ok;           // Connected / OK green
};

struct SystemSettings {
    uint8_t  themeId;          // ThemeId (0..3)
    uint8_t  brightness;       // Backlight PWM (25..255)
    uint8_t  animIntensity;    // 0 = Minimal, 1 = Balanced, 2 = Full Cyber FX
    uint8_t  animSpeed;        // 0 = Snappy (1.5x), 1 = Normal (1.0x), 2 = Cinematic (0.7x)
    uint8_t  targetFps;        // 30, 60, 90, 120
    bool     soundEnabled;     // Buzzer feedback if PIN_BUZZER >= 0
    bool     showFpsOverlay;   // Tiny FPS/ms badge in header or status
    uint8_t  displayRotation;  // 0 or 2 (Portrait or Flipped Portrait)
    uint8_t  matrixSpeed;      // 1..5
    uint8_t  matrixDensity;    // 1..5
    uint8_t  starfieldSpeed;   // 1..5
    int8_t   timezoneHours;    // UTC offset in hours (-12..+14)
    char     pingHost[48];     // Configurable host/IP for Network Tools ping test
    uint32_t lastSavedEpoch;   // Last valid epoch timestamp
};

class SettingsManager {
public:
    void begin();
    void load();
    void saveIfDirty();
    void forceSave();
    void resetToDefaults();

    SystemSettings& get() { return _settings; }
    const SystemSettings& get() const { return _settings; }
    void markDirty() { _dirty = true; }
    bool isDirty() const { return _dirty; }

    const ColorPalette& palette() const;
    const ColorPalette& paletteById(uint8_t id) const;

    // Wi-Fi credentials stored securely in dedicated NVS namespace
    bool hasWifiCredentials();
    bool loadWifiCredentials(String& ssidOut, String& passOut);
    void saveWifiCredentials(const String& ssid, const String& pass);
    void clearWifiCredentials();

    // Persist last known NTP epoch occasionally (not on every frame)
    void saveLastEpoch(uint32_t epoch);

    float getAnimSpeedMultiplier() const;

private:
    Preferences    _prefs;
    SystemSettings _settings;
    bool           _dirty = false;

    void applyDefaults();
};

extern SettingsManager Settings;
