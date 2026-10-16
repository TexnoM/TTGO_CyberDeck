#pragma once
#include <Arduino.h>
#include "hardware_config.h"

enum class ButtonEvent : uint8_t {
    NONE = 0,
    A_SHORT,      // Move selection / Next
    B_SHORT,      // Select / Confirm
    A_LONG,       // Go Back / Exit
    B_LONG,       // Open Quick Menu
    CHORD_AB      // Simultaneous press A + B shortcut
};

class ButtonManager {
public:
    void begin();
    void update();

    // Poll and consume the next queued event
    ButtonEvent pollEvent();

    // Live button state for diagnostics and UI indicators
    bool isPressedA() const { return _stableA; }
    bool isPressedB() const { return _stableB; }

    // 0.0 .. 1.0 hold progress toward long-press threshold
    float holdProgressA() const;
    float holdProgressB() const;

    // Optional piezo tone feedback (no-op if PIN_BUZZER < 0 or muted)
    void playClickTone(uint16_t freqHz = 1800, uint16_t durationMs = 8);

private:
    bool     _rawA          = false;
    bool     _rawB          = false;
    bool     _stableA       = false;
    bool     _stableB       = false;
    uint32_t _lastChangeA   = 0;
    uint32_t _lastChangeB   = 0;
    uint32_t _pressStartA   = 0;
    uint32_t _pressStartB   = 0;
    bool     _longFiredA    = false;
    bool     _longFiredB    = false;
    bool     _chordConsumed = false;

    static constexpr size_t kQueueCap = 8;
    ButtonEvent _queue[kQueueCap];
    uint8_t     _qHead = 0;
    uint8_t     _qTail = 0;

    void pushEvent(ButtonEvent ev);
    bool readPinA() const;
    bool readPinB() const;
};

extern ButtonManager Buttons;
