#pragma once
#include <Arduino.h>
#include "button_manager.h"
#include "ui_manager.h"

struct AppMetadata {
    AppId       id;
    const char* title;
    const char* subtitle;
    const char* codeTag;
};

class CyberApp {
public:
    virtual ~CyberApp() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void handleInput(ButtonEvent ev) = 0;
    virtual void updateAndRender(int16_t yOffset) = 0;
    virtual const char* hintA() const { return "NEXT/BACK"; }
    virtual const char* hintB() const { return "ACT/MENU"; }
};

const AppMetadata& getAppMeta(uint8_t idx);
CyberApp* getAppInstance(AppId id);
