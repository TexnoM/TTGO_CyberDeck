#pragma once
#include <Arduino.h>
#include "settings_manager.h"

// Inline localization helper
inline const char* tr(const char* en, const char* ua) {
    return (Settings.get().language == 1) ? ua : en;
}
