/*
 * SPDX-License-Identifier: MIT
 */
#include "face.h"

#include <algorithm>
#include <cmath>

namespace pet::face {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float clampUnit(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float lerp(float from, float to, float progress)
{
    return from + (to - from) * progress;
}

Color blend(Color from, Color to, float progress)
{
    return {
        static_cast<uint8_t>(std::lround(lerp(from.red, to.red, progress))),
        static_cast<uint8_t>(std::lround(lerp(from.green, to.green, progress))),
        static_cast<uint8_t>(std::lround(lerp(from.blue, to.blue, progress))),
    };
}

FacePose neutralPose()
{
    return {
        .left_eye  = {{94.0f, 103.0f}, {51.0f, 12.0f}, 0.0f, 1.0f, EyeGlyph::Stroke, EyeGlyph::Stroke, 1.0f},
        .right_eye = {{226.0f, 103.0f}, {51.0f, 12.0f}, 0.0f, 1.0f, EyeGlyph::Stroke, EyeGlyph::Stroke, 1.0f},
        .mouth     = {{160.0f, 158.0f}, 35.0f, 5.0f, 5.0f, 0.0f, MouthGlyph::TinyArc, MouthGlyph::TinyArc, 1.0f},
        .palette   = {{0, 0, 0}, {246, 244, 236}, {246, 244, 236}},
        .accent_mark      = AccentMark::None,
        .accent_intensity = 0.0f,
        .visual_tilt      = 0.0f,
    };
}

void clampPose(FacePose& pose)
{
    for (EyePose* eye : {&pose.left_eye, &pose.right_eye}) {
        eye->center.x         = std::clamp(eye->center.x, 0.0f, kDesignWidth);
        eye->center.y         = std::clamp(eye->center.y, 0.0f, kDesignHeight);
        eye->size.x           = std::clamp(eye->size.x, 6.0f, 140.0f);
        eye->size.y           = std::clamp(eye->size.y, 2.0f, 100.0f);
        eye->rotation_degrees = std::clamp(eye->rotation_degrees, -45.0f, 45.0f);
        eye->openness         = clampUnit(eye->openness);
        eye->glyph_blend      = clampUnit(eye->glyph_blend);
    }
    pose.mouth.center.x    = std::clamp(pose.mouth.center.x, 0.0f, kDesignWidth);
    pose.mouth.center.y    = std::clamp(pose.mouth.center.y, 0.0f, kDesignHeight);
    pose.mouth.width       = std::clamp(pose.mouth.width, 6.0f, 220.0f);
    pose.mouth.curve_depth = std::clamp(pose.mouth.curve_depth, -60.0f, 60.0f);
    pose.mouth.thickness   = std::clamp(pose.mouth.thickness, 1.0f, 24.0f);
    pose.mouth.openness    = clampUnit(pose.mouth.openness);
    pose.mouth.glyph_blend = clampUnit(pose.mouth.glyph_blend);
    pose.accent_intensity  = clampUnit(pose.accent_intensity);
    pose.visual_tilt       = std::clamp(pose.visual_tilt, -20.0f, 20.0f);
}

void setBothEyes(FacePose& pose, EyeGlyph glyph, Vec2 size)
{
    pose.left_eye.glyph           = glyph;
    pose.right_eye.glyph          = glyph;
    pose.left_eye.previous_glyph  = glyph;
    pose.right_eye.previous_glyph = glyph;
    pose.left_eye.glyph_blend     = 1.0f;
    pose.right_eye.glyph_blend    = 1.0f;
    pose.left_eye.size            = size;
    pose.right_eye.size           = size;
}

float smoothstep(float value)
{
    value = clampUnit(value);
    return value * value * (3.0f - 2.0f * value);
}

}  // namespace

FacePose preset(ExpressionId expression, float intensity)
{
    const FacePose neutral = neutralPose();
    FacePose target        = neutral;

    switch (expression) {
        case ExpressionId::Idle:
            target.mouth.curve_depth = 8.0f;
            break;
        case ExpressionId::Curious:
            setBothEyes(target, EyeGlyph::Circle, {34.0f, 34.0f});
            target.mouth.glyph = MouthGlyph::OpenOval;
            target.mouth.width = 21.0f;
            target.mouth.openness = 0.38f;
            target.visual_tilt = -4.0f;
            break;
        case ExpressionId::Happy:
            setBothEyes(target, EyeGlyph::Arc, {42.0f, 20.0f});
            target.left_eye.rotation_degrees = 5.0f;
            target.right_eye.rotation_degrees = -5.0f;
            target.mouth.glyph = MouthGlyph::Curve;
            target.mouth.width = 48.0f;
            target.mouth.curve_depth = 18.0f;
            break;
        case ExpressionId::Excited:
            setBothEyes(target, EyeGlyph::Circle, {39.0f, 39.0f});
            target.mouth.glyph = MouthGlyph::OpenOval;
            target.mouth.width = 27.0f;
            target.mouth.openness = 0.72f;
            target.accent_mark = AccentMark::Blush;
            target.accent_intensity = 0.50f;
            break;
        case ExpressionId::Hungry:
            setBothEyes(target, EyeGlyph::Circle, {33.0f, 33.0f});
            target.mouth.glyph = MouthGlyph::Three;
            target.mouth.width = 29.0f;
            target.mouth.curve_depth = 11.0f;
            target.visual_tilt = 3.0f;
            break;
        case ExpressionId::Lonely:
            setBothEyes(target, EyeGlyph::Circle, {35.0f, 35.0f});
            target.mouth.glyph = MouthGlyph::TinyArc;
            target.mouth.curve_depth = -7.0f;
            target.accent_mark = AccentMark::Tears;
            target.accent_intensity = 0.72f;
            break;
        case ExpressionId::Sleepy:
            setBothEyes(target, EyeGlyph::HalfLid, {46.0f, 20.0f});
            target.left_eye.openness = 0.45f;
            target.right_eye.openness = 0.45f;
            target.mouth.curve_depth = 1.0f;
            target.palette.feature = {182, 181, 174};
            break;
        case ExpressionId::Sleeping:
            setBothEyes(target, EyeGlyph::Stroke, {34.0f, 5.0f});
            target.left_eye.openness = 0.0f;
            target.right_eye.openness = 0.0f;
            target.mouth.glyph = MouthGlyph::TinyArc;
            target.mouth.width = 25.0f;
            target.mouth.curve_depth = 5.0f;
            target.palette.feature = {145, 144, 138};
            break;
        case ExpressionId::Waking:
            setBothEyes(target, EyeGlyph::Circle, {31.0f, 31.0f});
            target.left_eye.openness = 0.55f;
            target.right_eye.openness = 0.55f;
            target.mouth.glyph = MouthGlyph::TinyArc;
            target.mouth.curve_depth = 8.0f;
            break;
        case ExpressionId::Petted:
            setBothEyes(target, EyeGlyph::Arc, {43.0f, 20.0f});
            target.mouth.glyph = MouthGlyph::Curve;
            target.mouth.width = 43.0f;
            target.mouth.curve_depth = 17.0f;
            target.accent_mark = AccentMark::Blush;
            target.accent_intensity = 0.65f;
            break;
        case ExpressionId::Fed:
            setBothEyes(target, EyeGlyph::Arc, {41.0f, 20.0f});
            target.mouth.glyph = MouthGlyph::OpenOval;
            target.mouth.width = 25.0f;
            target.mouth.openness = 0.30f;
            target.accent_mark = AccentMark::Blush;
            target.accent_intensity = 0.35f;
            break;
        case ExpressionId::Startled:
            setBothEyes(target, EyeGlyph::Circle, {42.0f, 42.0f});
            target.mouth.glyph = MouthGlyph::OpenOval;
            target.mouth.width = 21.0f;
            target.mouth.openness = 0.85f;
            target.accent_mark = AccentMark::Sweat;
            target.accent_intensity = 0.80f;
            break;
        case ExpressionId::Paired:
            setBothEyes(target, EyeGlyph::Arc, {42.0f, 20.0f});
            target.mouth.glyph = MouthGlyph::Curve;
            target.mouth.width = 43.0f;
            target.mouth.curve_depth = 15.0f;
            target.palette.accent = {222, 191, 255};
            target.accent_mark = AccentMark::Blush;
            target.accent_intensity = 0.48f;
            break;
    }

    target.left_eye.previous_glyph  = target.left_eye.glyph;
    target.right_eye.previous_glyph = target.right_eye.glyph;
    target.left_eye.glyph_blend     = 1.0f;
    target.right_eye.glyph_blend    = 1.0f;
    target.mouth.previous_glyph     = target.mouth.glyph;
    target.mouth.glyph_blend        = 1.0f;

    FacePose result = interpolate(neutral, target, clampUnit(intensity));
    clampPose(result);
    return result;
}

FacePose interpolate(const FacePose& from, const FacePose& to, float progress)
{
    progress = clampUnit(progress);
    const auto blendVec = [progress](Vec2 a, Vec2 b) { return Vec2{lerp(a.x, b.x, progress), lerp(a.y, b.y, progress)}; };
    const auto blendEye = [&blendVec, progress](const EyePose& a, const EyePose& b) {
        return EyePose{blendVec(a.center, b.center), blendVec(a.size, b.size),
                       lerp(a.rotation_degrees, b.rotation_degrees, progress), lerp(a.openness, b.openness, progress),
                       b.glyph, a.glyph, progress};
    };

    FacePose result = {
        .left_eye  = blendEye(from.left_eye, to.left_eye),
        .right_eye = blendEye(from.right_eye, to.right_eye),
        .mouth     = {blendVec(from.mouth.center, to.mouth.center), lerp(from.mouth.width, to.mouth.width, progress),
                      lerp(from.mouth.curve_depth, to.mouth.curve_depth, progress),
                      lerp(from.mouth.thickness, to.mouth.thickness, progress), lerp(from.mouth.openness, to.mouth.openness, progress),
                      to.mouth.glyph, from.mouth.glyph, progress},
        .palette   = {blend(from.palette.background, to.palette.background, progress),
                      blend(from.palette.feature, to.palette.feature, progress),
                      blend(from.palette.accent, to.palette.accent, progress)},
        .accent_mark      = progress < 0.5f ? from.accent_mark : to.accent_mark,
        .accent_intensity = lerp(from.accent_intensity, to.accent_intensity, progress),
        .visual_tilt      = lerp(from.visual_tilt, to.visual_tilt, progress),
    };
    clampPose(result);
    return result;
}

bool isValid(const FacePose& pose)
{
    const float values[] = {
        pose.left_eye.center.x, pose.left_eye.center.y, pose.left_eye.size.x, pose.left_eye.size.y,
        pose.left_eye.rotation_degrees, pose.left_eye.openness, pose.right_eye.center.x, pose.right_eye.center.y,
        pose.right_eye.size.x, pose.right_eye.size.y, pose.right_eye.rotation_degrees, pose.right_eye.openness,
        pose.mouth.center.x, pose.mouth.center.y, pose.mouth.width, pose.mouth.curve_depth, pose.mouth.thickness,
        pose.mouth.openness, pose.accent_intensity, pose.visual_tilt,
    };
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return pose.left_eye.center.x >= 0.0f && pose.left_eye.center.x <= kDesignWidth &&
           pose.right_eye.center.x >= 0.0f && pose.right_eye.center.x <= kDesignWidth &&
           pose.left_eye.center.y >= 0.0f && pose.left_eye.center.y <= kDesignHeight &&
           pose.right_eye.center.y >= 0.0f && pose.right_eye.center.y <= kDesignHeight &&
           pose.left_eye.openness >= 0.0f && pose.left_eye.openness <= 1.0f &&
           pose.right_eye.openness >= 0.0f && pose.right_eye.openness <= 1.0f &&
           pose.mouth.openness >= 0.0f && pose.mouth.openness <= 1.0f && pose.accent_intensity >= 0.0f &&
           pose.accent_intensity <= 1.0f;
}

ExpressionController::ExpressionController(uint32_t seed)
    : transition_start_(preset(ExpressionId::Idle)), transition_target_(transition_start_), random_state_(seed ? seed : 1)
{
    next_blink_ms_ = 1600 + nextRandom() % 1800;
}

void ExpressionController::request(ExpressionId expression, float intensity, Transition transition, uint32_t now_ms)
{
    transition_start_       = basePoseAt(now_ms);
    transition_target_      = preset(expression, intensity);
    expression_             = expression;
    transition_started_ms_  = now_ms;
    transition_duration_ms_ = transition.duration_ms;
}

FacePose ExpressionController::basePoseAt(uint32_t now_ms) const
{
    if (transition_duration_ms_ == 0) {
        return transition_target_;
    }
    const uint32_t elapsed = now_ms - transition_started_ms_;
    const float progress   = static_cast<float>(elapsed) / static_cast<float>(transition_duration_ms_);
    return interpolate(transition_start_, transition_target_, smoothstep(progress));
}

void ExpressionController::advanceBlink(uint32_t now_ms)
{
    if (blink_active_ && (now_ms - blink_started_ms_) >= 140) {
        blink_active_ = false;
        next_blink_ms_ = now_ms + 1700 + nextRandom() % 2600;
    }
    if (!blink_active_ && now_ms >= next_blink_ms_) {
        blink_active_ = true;
        blink_started_ms_ = now_ms;
    }
}

void ExpressionController::applyLife(FacePose& pose, uint32_t now_ms) const
{
    const float phase = static_cast<float>((now_ms + random_state_) % 2200) / 2200.0f;
    const float drift = phase < 0.5f ? phase * 2.0f - 0.5f : 1.5f - phase * 2.0f;
    if (pose.left_eye.openness > 0.08f || pose.right_eye.openness > 0.08f) {
        pose.left_eye.center.x += drift * 1.5f;
        pose.right_eye.center.x += drift * 1.5f;
    }

    if (blink_active_) {
        const float blink_progress = static_cast<float>(now_ms - blink_started_ms_) / 140.0f;
        const float closure = std::sin(clampUnit(blink_progress) * kPi);
        pose.left_eye.openness *= 1.0f - closure;
        pose.right_eye.openness *= 1.0f - closure;
    }
    clampPose(pose);
}

FacePose ExpressionController::sample(uint32_t now_ms)
{
    advanceBlink(now_ms);
    FacePose pose = basePoseAt(now_ms);
    applyLife(pose, now_ms);
    return pose;
}

ExpressionId ExpressionController::expression() const
{
    return expression_;
}

uint32_t ExpressionController::nextRandom()
{
    random_state_ = random_state_ * 1664525u + 1013904223u;
    return random_state_;
}

}  // namespace pet::face
