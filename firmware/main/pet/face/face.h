/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace pet::face {

constexpr float kDesignWidth  = 320.0f;
constexpr float kDesignHeight = 240.0f;

struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct Vec2 {
    float x;
    float y;
};

struct EyePose {
    Vec2 center;
    Vec2 size;
    float rotation_degrees;
    float lid_open;      // [0, 1], fully closed to fully open
    Vec2 pupil_offset;   // [-1, 1] inside the eye
    float pupil_scale;   // Relative to the smaller eye dimension
};

struct MouthPose {
    Vec2 center;
    float width;
    float curve_depth;  // Positive values form a smile.
    float thickness;
    float openness;     // [0, 1], adds a filled mouth below the curve.
};

struct FacePalette {
    Color background;
    Color feature;
    Color pupil;
    Color blush;
};

struct FacePose {
    EyePose left_eye;
    EyePose right_eye;
    MouthPose mouth;
    FacePalette palette;
    float blush_intensity;  // [0, 1]
    float visual_tilt;      // Degrees; display-only, never servo motion.
};

enum class ExpressionId {
    Idle,
    Curious,
    Happy,
    Excited,
    Hungry,
    Lonely,
    Sleepy,
    Sleeping,
    Waking,
    Petted,
    Fed,
    Startled,
    Paired,
};

struct Transition {
    uint32_t duration_ms = 200;
};

/// Returns a bounded, asset-free pose for a semantic expression.
FacePose preset(ExpressionId expression, float intensity = 1.0f);

/// Blends every continuous geometry and palette channel. Progress is clamped.
FacePose interpolate(const FacePose& from, const FacePose& to, float progress);

/// Checks that a renderer can safely consume the pose in the 320 x 240 space.
bool isValid(const FacePose& pose);

/**
 * Deterministic expression transition and life-overlay controller.
 *
 * The caller owns time: sample() takes elapsed milliseconds and never reads a
 * clock or creates a task. A fixed seed makes host previews reproducible.
 */
class ExpressionController {
public:
    explicit ExpressionController(uint32_t seed = 0x5EED1234);

    void request(ExpressionId expression, float intensity, Transition transition, uint32_t now_ms);
    FacePose sample(uint32_t now_ms);

    ExpressionId expression() const;

private:
    FacePose basePoseAt(uint32_t now_ms) const;
    void advanceBlink(uint32_t now_ms);
    void applyLife(FacePose& pose, uint32_t now_ms) const;
    uint32_t nextRandom();

    FacePose transition_start_;
    FacePose transition_target_;
    ExpressionId expression_ = ExpressionId::Idle;
    uint32_t transition_started_ms_ = 0;
    uint32_t transition_duration_ms_ = 0;
    uint32_t random_state_;
    uint32_t next_blink_ms_ = 1800;
    uint32_t blink_started_ms_ = 0;
    bool blink_active_ = false;
};

}  // namespace pet::face
