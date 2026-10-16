#include "animation_engine.h"
#include "settings_manager.h"
#include <cmath>
#include <esp_timer.h>

AnimationEngine Anim;

void TweenValue::start(float from, float to, uint32_t durationMs, EasingType ease) {
    startVal = from;
    endVal   = to;
    current  = from;
    startUs  = (uint32_t)esp_timer_get_time();
    float mult = Settings.getAnimSpeedMultiplier();
    uint32_t adjMs = (uint32_t)max(15.0f, durationMs / mult);
    durationUs = adjMs * 1000UL;
    easing   = ease;
    active   = true;
}

bool TweenValue::update(uint32_t nowUs) {
    if (!active) return false;
    uint32_t elapsed = nowUs - startUs;
    if (elapsed >= durationUs) {
        current = endVal;
        active  = false;
        return true;
    }
    float t = (float)elapsed / (float)durationUs;
    float e = AnimationEngine::ease(t, easing);
    current = startVal + (endVal - startVal) * e;
    return true;
}

void AnimationEngine::begin(uint8_t targetFps) {
    setTargetFps(targetFps);
    _lastFrameUs = (uint32_t)esp_timer_get_time();
    _nowUs       = _lastFrameUs;
    _nowMs       = millis();
}

void AnimationEngine::setTargetFps(uint8_t targetFps) {
    if (targetFps < 15) targetFps = 15;
    if (targetFps > 120) targetFps = 120;
    _targetFps     = targetFps;
    _targetFrameUs = 1000000UL / _targetFps;
}

bool AnimationEngine::shouldRenderFrame() {
    uint32_t now = (uint32_t)esp_timer_get_time();
    uint32_t elapsed = now - _lastFrameUs;
    if (elapsed < _targetFrameUs) {
        return false;
    }
    _lastFrameUs = now;
    _nowUs       = now;
    _nowMs       = millis();

    // Clamp dt to avoid jumps if a blocking Wi-Fi call occurred
    _dtSec = (float)elapsed * 1e-6f;
    if (_dtSec > 0.1f) _dtSec = 0.1f;

    _frameTimeMs = (float)elapsed * 0.001f;
    _historyUs[_historyIdx] = (uint16_t)min<uint32_t>(elapsed, 65535UL);
    _historyIdx = (_historyIdx + 1) % kHistorySize;

    _fpsAccumUs += elapsed;
    _fpsFrames++;
    if (_fpsAccumUs >= 250000UL) { // Update FPS readout every 250 ms
        _currentFps = ((float)_fpsFrames * 1000000.0f) / (float)_fpsAccumUs;
        _fpsAccumUs = 0;
        _fpsFrames  = 0;
    }

    _frameCount++;
    return true;
}

void AnimationEngine::endFrame(uint32_t renderDurationUs) {
    _renderTimeMs = _renderTimeMs * 0.8f + ((float)renderDurationUs * 0.001f) * 0.2f;
}

float AnimationEngine::ease(float t, EasingType type) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    switch (type) {
        case EasingType::OUT_CUBIC:    return easeOutCubic(t);
        case EasingType::IN_OUT_CUBIC: return easeInOutCubic(t);
        case EasingType::OUT_QUINT:    return easeOutQuint(t);
        case EasingType::IN_OUT_QUINT: return easeInOutQuint(t);
        case EasingType::OUT_EXPO:     return easeOutExpo(t);
        default:                       return t;
    }
}

float AnimationEngine::easeOutCubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

float AnimationEngine::easeInOutCubic(float t) {
    return (t < 0.5f)
        ? (4.0f * t * t * t)
        : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f);
}

float AnimationEngine::easeOutQuint(float t) {
    float inv = 1.0f - t;
    float inv2 = inv * inv;
    return 1.0f - (inv2 * inv2 * inv);
}

float AnimationEngine::easeInOutQuint(float t) {
    return (t < 0.5f)
        ? (16.0f * t * t * t * t * t)
        : (1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) * 0.5f);
}

float AnimationEngine::easeOutExpo(float t) {
    return (t >= 1.0f) ? 1.0f : (1.0f - std::pow(2.0f, -10.0f * t));
}

uint16_t AnimationEngine::lerpColor565(uint16_t c1, uint16_t c2, uint16_t t256) {
    if (t256 == 0) return c1;
    if (t256 >= 256) return c2;

    uint16_t r1 = (c1 >> 11) & 0x1F;
    uint16_t g1 = (c1 >> 5)  & 0x3F;
    uint16_t b1 =  c1        & 0x1F;

    uint16_t r2 = (c2 >> 11) & 0x1F;
    uint16_t g2 = (c2 >> 5)  & 0x3F;
    uint16_t b2 =  c2        & 0x1F;

    uint16_t r = r1 + (((int16_t)(r2 - r1) * (int16_t)t256) >> 8);
    uint16_t g = g1 + (((int16_t)(g2 - g1) * (int16_t)t256) >> 8);
    uint16_t b = b1 + (((int16_t)(b2 - b1) * (int16_t)t256) >> 8);

    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

uint16_t AnimationEngine::pulse256(uint32_t periodMs) const {
    if (periodMs == 0) return 128;
    uint32_t phase = (_nowMs % periodMs) * 512UL / periodMs;
    return (phase <= 256) ? (uint16_t)phase : (uint16_t)(512 - phase);
}
