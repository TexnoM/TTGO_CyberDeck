#pragma once
#include <Arduino.h>

enum class EasingType : uint8_t {
    LINEAR = 0,
    OUT_CUBIC,
    IN_OUT_CUBIC,
    OUT_QUINT,
    IN_OUT_QUINT,
    OUT_EXPO
};

struct DirtyRegion {
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
    bool    dirty = false;

    void clear() {
        dirty = false;
        x1 = y1 = x2 = y2 = 0;
    }

    void mark(int16_t x, int16_t y, int16_t w, int16_t h, int16_t maxW, int16_t maxH) {
        if (w <= 0 || h <= 0) return;
        int16_t nx1 = max<int16_t>(0, x);
        int16_t ny1 = max<int16_t>(0, y);
        int16_t nx2 = min<int16_t>(maxW, x + w);
        int16_t ny2 = min<int16_t>(maxH, y + h);
        if (nx2 <= nx1 || ny2 <= ny1) return;
        if (!dirty) {
            x1 = nx1; y1 = ny1; x2 = nx2; y2 = ny2;
            dirty = true;
        } else {
            if (nx1 < x1) x1 = nx1;
            if (ny1 < y1) y1 = ny1;
            if (nx2 > x2) x2 = nx2;
            if (ny2 > y2) y2 = ny2;
        }
    }

    int16_t width() const { return dirty ? (x2 - x1) : 0; }
    int16_t height() const { return dirty ? (y2 - y1) : 0; }
};

struct TweenValue {
    float      startVal = 0.0f;
    float      endVal   = 0.0f;
    float      current  = 0.0f;
    uint32_t   startUs  = 0;
    uint32_t   durationUs = 1;
    EasingType easing   = EasingType::OUT_CUBIC;
    bool       active   = false;

    void start(float from, float to, uint32_t durationMs, EasingType ease = EasingType::OUT_CUBIC);
    bool update(uint32_t nowUs);
};

class AnimationEngine {
public:
    static constexpr size_t kHistorySize = 40;

    void begin(uint8_t targetFps);
    void setTargetFps(uint8_t targetFps);
    uint8_t getTargetFps() const { return _targetFps; }

    // Returns true when it is time to render the next frame
    bool shouldRenderFrame();
    void endFrame(uint32_t renderDurationUs);

    // Frame timing & metrics
    float    dt() const { return _dtSec; }
    uint32_t nowMs() const { return _nowMs; }
    uint32_t nowUs() const { return _nowUs; }
    float    fps() const { return _currentFps; }
    float    frameTimeMs() const { return _frameTimeMs; }
    float    renderTimeMs() const { return _renderTimeMs; }
    uint32_t frameCount() const { return _frameCount; }

    const uint16_t* frameTimeHistoryUs() const { return _historyUs; }
    size_t          historyIndex() const { return _historyIdx; }

    // Easing functions (t in [0.0, 1.0])
    static float ease(float t, EasingType type);
    static float easeOutCubic(float t);
    static float easeInOutCubic(float t);
    static float easeOutQuint(float t);
    static float easeInOutQuint(float t);
    static float easeOutExpo(float t);

    // Fast RGB565 color interpolation (t in [0..256])
    static uint16_t lerpColor565(uint16_t c1, uint16_t c2, uint16_t t256);

    // Subtle procedural pulse [0..256] based on current time
    uint16_t pulse256(uint32_t periodMs) const;

private:
    uint8_t  _targetFps     = 60;
    uint32_t _targetFrameUs = 16666;
    uint32_t _lastFrameUs   = 0;
    uint32_t _nowUs         = 0;
    uint32_t _nowMs         = 0;
    float    _dtSec         = 0.016f;
    float    _currentFps    = 60.0f;
    float    _frameTimeMs   = 16.6f;
    float    _renderTimeMs  = 2.0f;
    uint32_t _frameCount    = 0;

    uint32_t _fpsAccumUs    = 0;
    uint16_t _fpsFrames     = 0;

    uint16_t _historyUs[kHistorySize] = {0};
    size_t   _historyIdx    = 0;
};

extern AnimationEngine Anim;
