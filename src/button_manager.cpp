#include "button_manager.h"
#include "settings_manager.h"

ButtonManager Buttons;

bool ButtonManager::readPinA() const {
    int val = digitalRead(PIN_BTN_A);
    return BTN_ACTIVE_LOW ? (val == LOW) : (val == HIGH);
}

bool ButtonManager::readPinB() const {
    int val = digitalRead(PIN_BTN_B);
    return BTN_ACTIVE_LOW ? (val == LOW) : (val == HIGH);
}

void ButtonManager::begin() {
    // GPIO 34..39 on ESP32 are input-only without internal pull-ups
    if (PIN_BTN_A >= 34) {
        pinMode(PIN_BTN_A, INPUT);
    } else {
        pinMode(PIN_BTN_A, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    }

    if (PIN_BTN_B >= 34) {
        pinMode(PIN_BTN_B, INPUT);
    } else {
        pinMode(PIN_BTN_B, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    }

#if PIN_BUZZER >= 0
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
#endif

    _rawA = _stableA = readPinA();
    _rawB = _stableB = readPinB();
    _qHead = _qTail = 0;
}

void ButtonManager::pushEvent(ButtonEvent ev) {
    uint8_t nextHead = (_qHead + 1) % kQueueCap;
    if (nextHead != _qTail) {
        _queue[_qHead] = ev;
        _qHead = nextHead;
    }
#if HW_SERIAL_DIAGNOSTICS
    const char* name = "NONE";
    switch (ev) {
        case ButtonEvent::A_SHORT:  name = "A_SHORT (Next)"; break;
        case ButtonEvent::B_SHORT:  name = "B_SHORT (Confirm)"; break;
        case ButtonEvent::A_LONG:   name = "A_LONG (Back)"; break;
        case ButtonEvent::B_LONG:   name = "B_LONG (QuickMenu)"; break;
        case ButtonEvent::CHORD_AB: name = "CHORD_AB (Shortcut)"; break;
        default: break;
    }
    Serial.printf("[BTN] Event: %s\n", name);
#endif
}

ButtonEvent ButtonManager::pollEvent() {
    if (_qHead == _qTail) {
        return ButtonEvent::NONE;
    }
    ButtonEvent ev = _queue[_qTail];
    _qTail = (_qTail + 1) % kQueueCap;
    return ev;
}

void ButtonManager::playClickTone(uint16_t freqHz, uint16_t durationMs) {
#if PIN_BUZZER >= 0
    if (!Settings.get().soundEnabled) return;
    tone(PIN_BUZZER, freqHz, durationMs);
#else
    (void)freqHz;
    (void)durationMs;
#endif
}

void ButtonManager::update() {
    uint32_t now = millis();

    bool curA = readPinA();
    bool curB = readPinB();

    if (curA != _rawA) {
        _rawA = curA;
        _lastChangeA = now;
    }
    if (curB != _rawB) {
        _rawB = curB;
        _lastChangeB = now;
    }

    // Debounce Button A
    if ((now - _lastChangeA) >= BTN_DEBOUNCE_MS && _rawA != _stableA) {
        _stableA = _rawA;
        if (_stableA) {
            _pressStartA = now;
            _longFiredA  = false;
            // Check chord with B
            if (_stableB && !_chordConsumed && (now - _pressStartB) <= BTN_CHORD_WINDOW_MS) {
                _chordConsumed = true;
                _longFiredA = true;
                _longFiredB = true;
                playClickTone(2600, 14);
                pushEvent(ButtonEvent::CHORD_AB);
            }
        } else {
            if (!_longFiredA && !_chordConsumed) {
                playClickTone(1900, 6);
                pushEvent(ButtonEvent::A_SHORT);
            }
            if (!_stableB) {
                _chordConsumed = false;
            }
        }
    }

    // Debounce Button B
    if ((now - _lastChangeB) >= BTN_DEBOUNCE_MS && _rawB != _stableB) {
        _stableB = _rawB;
        if (_stableB) {
            _pressStartB = now;
            _longFiredB  = false;
            // Check chord with A
            if (_stableA && !_chordConsumed && (now - _pressStartA) <= BTN_CHORD_WINDOW_MS) {
                _chordConsumed = true;
                _longFiredA = true;
                _longFiredB = true;
                playClickTone(2600, 14);
                pushEvent(ButtonEvent::CHORD_AB);
            }
        } else {
            if (!_longFiredB && !_chordConsumed) {
                playClickTone(2200, 8);
                pushEvent(ButtonEvent::B_SHORT);
            }
            if (!_stableA) {
                _chordConsumed = false;
            }
        }
    }

    // Long press detection while held
    if (_stableA && !_longFiredA && !_chordConsumed) {
        if ((now - _pressStartA) >= BTN_LONG_PRESS_MS) {
            _longFiredA = true;
            playClickTone(1300, 16);
            pushEvent(ButtonEvent::A_LONG);
        }
    }
    if (_stableB && !_longFiredB && !_chordConsumed) {
        if ((now - _pressStartB) >= BTN_LONG_PRESS_MS) {
            _longFiredB = true;
            playClickTone(2500, 16);
            pushEvent(ButtonEvent::B_LONG);
        }
    }
}

float ButtonManager::holdProgressA() const {
    if (!_stableA || _longFiredA || _chordConsumed) return 0.0f;
    uint32_t elapsed = millis() - _pressStartA;
    if (elapsed < 90) return 0.0f;
    return constrain((float)(elapsed - 90) / (float)(BTN_LONG_PRESS_MS - 90), 0.0f, 1.0f);
}

float ButtonManager::holdProgressB() const {
    if (!_stableB || _longFiredB || _chordConsumed) return 0.0f;
    uint32_t elapsed = millis() - _pressStartB;
    if (elapsed < 90) return 0.0f;
    return constrain((float)(elapsed - 90) / (float)(BTN_LONG_PRESS_MS - 90), 0.0f, 1.0f);
}
