#include "settings_manager.h"
#include <cstring>

SettingsManager Settings;

// Helper constexpr for RGB888 -> RGB565
static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const ColorPalette kPalettes[THEME_COUNT] = {
    // 0: NEON GREEN (Matrix / Terminal Hacker)
    {
        "NEON GREEN",
        rgb565(4, 8, 6),       // bg: nearly black
        rgb565(8, 22, 14),     // panelBg
        rgb565(12, 34, 20),    // gridDim
        rgb565(0, 255, 110),   // primary: neon green
        rgb565(0, 225, 255),   // secondary: cyber cyan
        rgb565(180, 255, 210), // highlight: bright mint glow
        rgb565(220, 255, 230), // textMain
        rgb565(90, 145, 110),  // textDim
        rgb565(255, 185, 0),   // warning: amber
        rgb565(255, 50, 70),   // danger: neon crimson
        rgb565(0, 255, 120)    // ok
    },
    // 1: CYBER CYAN (High-Tech Sci-Fi OS)
    {
        "CYBER CYAN",
        rgb565(4, 7, 12),      // bg: nearly black
        rgb565(8, 18, 30),     // panelBg
        rgb565(12, 30, 48),    // gridDim
        rgb565(0, 235, 255),   // primary: electric cyan
        rgb565(255, 190, 0),   // secondary: amber accent
        rgb565(190, 250, 255), // highlight: ice cyan glow
        rgb565(225, 248, 255), // textMain
        rgb565(90, 135, 160),  // textDim
        rgb565(255, 185, 0),   // warning
        rgb565(255, 55, 80),   // danger
        rgb565(0, 255, 140)    // ok
    },
    // 2: AMBER GOLD (Retro-Futuristic Industrial Deck)
    {
        "AMBER GOLD",
        rgb565(10, 6, 2),      // bg: nearly black
        rgb565(26, 16, 4),     // panelBg
        rgb565(42, 26, 6),     // gridDim
        rgb565(255, 180, 0),   // primary: phosphor amber
        rgb565(0, 230, 255),   // secondary: cyan contrast
        rgb565(255, 235, 160), // highlight: bright gold
        rgb565(255, 235, 195), // textMain
        rgb565(160, 115, 50),  // textDim
        rgb565(255, 130, 0),   // warning
        rgb565(255, 50, 50),   // danger
        rgb565(80, 255, 120)   // ok
    },
    // 3: SYNTH PINK (Night City Outrun)
    {
        "SYNTH PINK",
        rgb565(10, 4, 12),     // bg: nearly black
        rgb565(24, 8, 30),     // panelBg
        rgb565(40, 14, 52),    // gridDim
        rgb565(255, 40, 180),  // primary: neon magenta
        rgb565(0, 240, 255),   // secondary: cyber cyan
        rgb565(255, 185, 235), // highlight: neon pink glow
        rgb565(250, 230, 255), // textMain
        rgb565(145, 95, 160),  // textDim
        rgb565(255, 190, 0),   // warning
        rgb565(255, 50, 70),   // danger
        rgb565(0, 255, 150)    // ok
    }
};

void SettingsManager::applyDefaults() {
    _settings.themeId         = THEME_NEON_GREEN;
    _settings.brightness      = 220;
    _settings.animIntensity   = 2; // Full Cyber FX
    _settings.animSpeed       = 1; // Normal (1.0x)
    _settings.targetFps       = DEFAULT_TARGET_FPS;
    _settings.soundEnabled    = (PIN_BUZZER >= 0);
    _settings.showFpsOverlay  = false;
    _settings.displayRotation = TFT_DEFAULT_ROTATION;
    _settings.matrixSpeed     = 3;
    _settings.matrixDensity   = 3;
    _settings.starfieldSpeed  = 3;
    _settings.timezoneHours   = 3; // UTC+3 default
    _settings.sleepTimeoutSec = 20; // 20 seconds idle timeout by default
    std::strncpy(_settings.pingHost, "192.168.1.1", sizeof(_settings.pingHost) - 1);
    _settings.pingHost[sizeof(_settings.pingHost) - 1] = '\0';
    _settings.lastSavedEpoch  = 0; // 0 = unsynchronized (no fake hardcoded date!)
    _settings.language        = 0; // 0 = EN by default, 1 = UA
}

#include <LittleFS.h>
#include <FS.h>

static const char* kConfigFilePath = "/config.json";

static bool extractJsonInt(const String& json, const char* key, int& outVal) {
    String searchKey = String("\"") + key + "\"";
    int idx = json.indexOf(searchKey);
    if (idx < 0) return false;
    int colon = json.indexOf(':', idx + searchKey.length());
    if (colon < 0) return false;
    outVal = json.substring(colon + 1).toInt();
    return true;
}

static bool extractJsonBool(const String& json, const char* key, bool& outVal) {
    String searchKey = String("\"") + key + "\"";
    int idx = json.indexOf(searchKey);
    if (idx < 0) return false;
    int colon = json.indexOf(':', idx + searchKey.length());
    if (colon < 0) return false;
    String sub = json.substring(colon + 1);
    sub.trim();
    outVal = sub.startsWith("true") || sub.startsWith("1");
    return true;
}

static bool extractJsonString(const String& json, const char* key, char* outVal, size_t maxLen) {
    String searchKey = String("\"") + key + "\"";
    int idx = json.indexOf(searchKey);
    if (idx < 0) return false;
    int colon = json.indexOf(':', idx + searchKey.length());
    if (colon < 0) return false;
    int q1 = json.indexOf('"', colon);
    if (q1 < 0) return false;
    int q2 = json.indexOf('"', q1 + 1);
    if (q2 < 0) return false;
    String val = json.substring(q1 + 1, q2);
    std::strncpy(outVal, val.c_str(), maxLen - 1);
    outVal[maxLen - 1] = '\0';
    return true;
}

bool SettingsManager::loadFromLittleFS() {
    if (!_littleFsOk) return false;
    if (!LittleFS.exists(kConfigFilePath)) return false;

    File f = LittleFS.open(kConfigFilePath, "r");
    if (!f) return false;

    String json = f.readString();
    f.close();

    int intVal = 0;
    bool boolVal = false;

    if (extractJsonInt(json, "theme", intVal)) _settings.themeId = (uint8_t)(intVal % THEME_COUNT);
    if (extractJsonInt(json, "bright", intVal)) _settings.brightness = (uint8_t)constrain(intVal, 25, 255);
    if (extractJsonInt(json, "animInt", intVal)) _settings.animIntensity = (uint8_t)constrain(intVal, 0, 2);
    if (extractJsonInt(json, "animSpd", intVal)) _settings.animSpeed = (uint8_t)constrain(intVal, 0, 2);
    if (extractJsonInt(json, "tgtFps", intVal)) {
        if (intVal == 30 || intVal == 60 || intVal == 90 || intVal == 120) {
            _settings.targetFps = (uint8_t)intVal;
        }
    }
    if (extractJsonBool(json, "sound", boolVal)) _settings.soundEnabled = boolVal;
    if (extractJsonBool(json, "fpsOvr", boolVal)) _settings.showFpsOverlay = boolVal;
    if (extractJsonInt(json, "rot", intVal)) _settings.displayRotation = (uint8_t)(intVal & 3);
    if (extractJsonInt(json, "mtxSpd", intVal)) _settings.matrixSpeed = (uint8_t)constrain(intVal, 1, 5);
    if (extractJsonInt(json, "mtxDen", intVal)) _settings.matrixDensity = (uint8_t)constrain(intVal, 1, 5);
    if (extractJsonInt(json, "strSpd", intVal)) _settings.starfieldSpeed = (uint8_t)constrain(intVal, 1, 5);
    if (extractJsonInt(json, "tzHrs", intVal)) _settings.timezoneHours = (int8_t)constrain(intVal, -12, 14);
    if (extractJsonInt(json, "sleepTo", intVal)) _settings.sleepTimeoutSec = (uint8_t)constrain(intVal, 0, 120);
    if (extractJsonInt(json, "lang", intVal)) _settings.language = (uint8_t)(intVal & 1);

    char hostBuf[48];
    if (extractJsonString(json, "pingHost", hostBuf, sizeof(hostBuf))) {
        std::strncpy(_settings.pingHost, hostBuf, sizeof(_settings.pingHost) - 1);
        _settings.pingHost[sizeof(_settings.pingHost) - 1] = '\0';
    }

    if (extractJsonInt(json, "epoch", intVal)) {
        _settings.lastSavedEpoch = (uint32_t)intVal;
    }

    return true;
}

bool SettingsManager::saveToLittleFS() {
    if (!_littleFsOk) return false;
    File f = LittleFS.open(kConfigFilePath, "w");
    if (!f) return false;

    String json = "{\n";
    json += "  \"theme\": " + String(_settings.themeId) + ",\n";
    json += "  \"bright\": " + String(_settings.brightness) + ",\n";
    json += "  \"animInt\": " + String(_settings.animIntensity) + ",\n";
    json += "  \"animSpd\": " + String(_settings.animSpeed) + ",\n";
    json += "  \"tgtFps\": " + String(_settings.targetFps) + ",\n";
    json += "  \"sound\": " + String(_settings.soundEnabled ? "true" : "false") + ",\n";
    json += "  \"fpsOvr\": " + String(_settings.showFpsOverlay ? "true" : "false") + ",\n";
    json += "  \"rot\": " + String(_settings.displayRotation) + ",\n";
    json += "  \"mtxSpd\": " + String(_settings.matrixSpeed) + ",\n";
    json += "  \"mtxDen\": " + String(_settings.matrixDensity) + ",\n";
    json += "  \"strSpd\": " + String(_settings.starfieldSpeed) + ",\n";
    json += "  \"tzHrs\": " + String(_settings.timezoneHours) + ",\n";
    json += "  \"sleepTo\": " + String(_settings.sleepTimeoutSec) + ",\n";
    json += "  \"lang\": " + String(_settings.language) + ",\n";
    json += "  \"pingHost\": \"" + String(_settings.pingHost) + "\",\n";
    json += "  \"epoch\": " + String(_settings.lastSavedEpoch) + "\n";
    json += "}\n";

    f.print(json);
    f.close();
    return true;
}

void SettingsManager::begin() {
    _littleFsOk = LittleFS.begin(true /* formatOnFail */);
    applyDefaults();
    load();
}

void SettingsManager::load() {
    // 1. First try loading from LittleFS (/config.json) as in Bruce firmware
    if (loadFromLittleFS()) {
        _dirty = false;
        return;
    }

    // 2. Fallback to NVS if LittleFS config not yet initialized
    if (_prefs.begin("neondeck_cfg", true)) {
        uint32_t magic = _prefs.getUInt("magic", 0);
        if (magic == 0x4E454F31UL) { // "NEO1"
            _settings.themeId         = _prefs.getUChar("theme", _settings.themeId) % THEME_COUNT;
            _settings.brightness      = constrain(_prefs.getUChar("bright", _settings.brightness), 25, 255);
            _settings.animIntensity   = constrain(_prefs.getUChar("animInt", _settings.animIntensity), 0, 2);
            _settings.animSpeed       = constrain(_prefs.getUChar("animSpd", _settings.animSpeed), 0, 2);
            _settings.targetFps       = _prefs.getUChar("tgtFps", _settings.targetFps);
            if (_settings.targetFps != 30 && _settings.targetFps != 60 &&
                _settings.targetFps != 90 && _settings.targetFps != 120) {
                _settings.targetFps = DEFAULT_TARGET_FPS;
            }
            _settings.soundEnabled    = _prefs.getBool("sound", _settings.soundEnabled);
            _settings.showFpsOverlay  = _prefs.getBool("fpsOvr", _settings.showFpsOverlay);
            _settings.displayRotation = _prefs.getUChar("rot", _settings.displayRotation) & 3;
            _settings.matrixSpeed     = constrain(_prefs.getUChar("mtxSpd", _settings.matrixSpeed), 1, 5);
            _settings.matrixDensity   = constrain(_prefs.getUChar("mtxDen", _settings.matrixDensity), 1, 5);
            _settings.starfieldSpeed  = constrain(_prefs.getUChar("strSpd", _settings.starfieldSpeed), 1, 5);
            _settings.timezoneHours   = constrain(_prefs.getChar("tzHrs", _settings.timezoneHours), -12, 14);
            String host = _prefs.getString("pingHost", _settings.pingHost);
            if (host.length() > 0) {
                std::strncpy(_settings.pingHost, host.c_str(), sizeof(_settings.pingHost) - 1);
                _settings.pingHost[sizeof(_settings.pingHost) - 1] = '\0';
            }
            _settings.lastSavedEpoch  = _prefs.getUInt("epoch", _settings.lastSavedEpoch);
            _settings.language        = _prefs.getUChar("lang", _settings.language);
        }
        _prefs.end();
    }

    // Save initial config to LittleFS for subsequent boots
    saveToLittleFS();
    _dirty = false;
}

void SettingsManager::saveIfDirty() {
    if (!_dirty) return;
    forceSave();
}

void SettingsManager::forceSave() {
    // 1. Save config to LittleFS (/config.json)
    saveToLittleFS();

    // 2. Mirror to NVS as backup
    if (_prefs.begin("neondeck_cfg", false)) {
        _prefs.putUInt("magic", 0x4E454F31UL);
        _prefs.putUChar("theme", _settings.themeId);
        _prefs.putUChar("bright", _settings.brightness);
        _prefs.putUChar("animInt", _settings.animIntensity);
        _prefs.putUChar("animSpd", _settings.animSpeed);
        _prefs.putUChar("tgtFps", _settings.targetFps);
        _prefs.putBool("sound", _settings.soundEnabled);
        _prefs.putBool("fpsOvr", _settings.showFpsOverlay);
        _prefs.putUChar("rot", _settings.displayRotation);
        _prefs.putUChar("mtxSpd", _settings.matrixSpeed);
        _prefs.putUChar("mtxDen", _settings.matrixDensity);
        _prefs.putUChar("strSpd", _settings.starfieldSpeed);
        _prefs.putChar("tzHrs", _settings.timezoneHours);
        _prefs.putString("pingHost", _settings.pingHost);
        _prefs.putUInt("epoch", _settings.lastSavedEpoch);
        _prefs.putUChar("lang", _settings.language);
        _prefs.end();
    }
    _dirty = false;
}

void SettingsManager::resetToDefaults() {
    applyDefaults();
    forceSave();
}

const ColorPalette& SettingsManager::palette() const {
    return kPalettes[_settings.themeId % THEME_COUNT];
}

const ColorPalette& SettingsManager::paletteById(uint8_t id) const {
    return kPalettes[id % THEME_COUNT];
}

bool SettingsManager::hasWifiCredentials() {
    if (!_prefs.begin("neondeck_net", true)) return false;
    String ssid = _prefs.getString("ssid", "");
    _prefs.end();
    return ssid.length() > 0;
}

bool SettingsManager::loadWifiCredentials(String& ssidOut, String& passOut) {
    if (!_prefs.begin("neondeck_net", true)) return false;
    ssidOut = _prefs.getString("ssid", "");
    passOut = _prefs.getString("pass", "");
    _prefs.end();
    return ssidOut.length() > 0;
}

void SettingsManager::saveWifiCredentials(const String& ssid, const String& pass) {
    if (!_prefs.begin("neondeck_net", false)) return;
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.end();
}

void SettingsManager::clearWifiCredentials() {
    if (!_prefs.begin("neondeck_net", false)) return;
    _prefs.clear();
    _prefs.end();
}

void SettingsManager::saveLastEpoch(uint32_t epoch) {
    _settings.lastSavedEpoch = epoch;
    if (!_prefs.begin("neondeck_cfg", false)) return;
    _prefs.putUInt("epoch", epoch);
    _prefs.end();
}

float SettingsManager::getAnimSpeedMultiplier() const {
    switch (_settings.animSpeed) {
        case 0:  return 1.55f; // Snappy
        case 2:  return 0.65f; // Cinematic
        default: return 1.00f; // Normal
    }
}
