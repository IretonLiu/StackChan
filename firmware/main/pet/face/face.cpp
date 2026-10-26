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

float clampSigned(float value)
{
    return std::clamp(value, -1.0f, 1.0f);
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
    FacePose pose = {
        .left_eye  = {{92.0f, 102.0f}, {58.0f, 76.0f}, 0.0f, 1.0f, {0.0f, 0.0f}, 0.33f},
        .right_eye = {{228.0f, 102.0f}, {58.0f, 76.0f}, 0.0f, 1.0f, {0.0f, 0.0f}, 0.33f},
        .mouth     = {{160.0f, 172.0f}, 76.0f, 0.0f, 6.0f, 0.0f},
        .palette   = {{7, 16, 24}, {217, 241, 255}, {7, 16, 24}, {244, 132, 162}},
        .blush_intensity = 0.0f,
        .visual_tilt     = 0.0f,
    };
    return pose;
}

void clampPose(FacePose& pose)
{
    for (EyePose* eye : {&pose.left_eye, &pose.right_eye}) {
        eye->center.x          = std::clamp(eye->center.x, 0.0f, kDesignWidth);
        eye->center.y          = std::clamp(eye->center.y, 0.0f, kDesignHeight);
        eye->size.x            = std::clamp(eye->size.x, 8.0f, 140.0f);
        eye->size.y            = std::clamp(eye->size.y, 8.0f, 140.0f);
        eye->rotation_degrees  = std::clamp(eye->rotation_degrees, -45.0f, 45.0f);
        eye->lid_open          = clampUnit(eye->lid_open);
        eye->pupil_offset.x    = clampSigned(eye->pupil_offset.x);
        eye->pupil_offset.y    = clampSigned(eye->pupil_offset.y);
        eye->pupil_scale       = std::clamp(eye->pupil_scale, 0.08f, 0.85f);
    }
    pose.mouth.center.x   = std::clamp(pose.mouth.center.x, 0.0f, kDesignWidth);
    pose.mouth.center.y   = std::clamp(pose.mouth.center.y, 0.0f, kDesignHeight);
    pose.mouth.width      = std::clamp(pose.mouth.width, 8.0f, 220.0f);
    pose.mouth.curve_depth = std::clamp(pose.mouth.curve_depth, -60.0f, 60.0f);
    pose.mouth.thickness  = std::clamp(pose.mouth.thickness, 1.0f, 30.0f);
    pose.mouth.openness   = clampUnit(pose.mouth.openness);
    pose.blush_intensity  = clampUnit(pose.blush_intensity);
    pose.visual_tilt      = std::clamp(pose.visual_tilt, -20.0f, 20.0f);
}

void setBothLids(FacePose& pose, float openness)
{
    pose.left_eye.lid_open  = openness;
    pose.right_eye.lid_open = openness;
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
            target.mouth.curve_depth = 4.0f;
            break;
        case ExpressionId::Curious:
            target.left_eye.size = {64.0f, 84.0f};
            target.right_eye.size = {64.0f, 84.0f};
            target.left_eye.pupil_scale = 0.45f;
            target.right_eye.pupil_scale = 0.45f;
            target.mouth.width = 42.0f;
            target.mouth.openness = 0.25f;
            target.visual_tilt = -5.0f;
            break;
        case ExpressionId::Happy:
            target.left_eye.lid_open = 0.70f;
            target.right_eye.lid_open = 0.70f;
            target.left_eye.rotation_degrees = 9.0f;
            target.right_eye.rotation_degrees = -9.0f;
            target.mouth.curve_depth = 28.0f;
            target.mouth.thickness = 8.0f;
            target.blush_intensity = 0.45f;
            break;
        case ExpressionId::Excited:
            target.left_eye.size = {68.0f, 90.0f};
            target.right_eye.size = {68.0f, 90.0f};
            target.left_eye.pupil_scale = 0.50f;
            target.right_eye.pupil_scale = 0.50f;
            target.mouth.curve_depth = 30.0f;
            target.mouth.openness = 0.72f;
            target.blush_intensity = 0.75f;
            break;
        case ExpressionId::Hungry:
            target.left_eye.pupil_offset = {0.0f, -0.35f};
            target.right_eye.pupil_offset = {0.0f, -0.35f};
            target.mouth.width = 34.0f;
            target.mouth.openness = 0.48f;
            target.visual_tilt = 4.0f;
            break;
        case ExpressionId::Lonely:
            setBothLids(target, 0.52f);
            target.left_eye.pupil_offset = {-0.25f, 0.20f};
            target.right_eye.pupil_offset = {-0.25f, 0.20f};
            target.mouth.curve_depth = -15.0f;
            target.mouth.width = 54.0f;
            target.palette.feature = {142, 177, 196};
            break;
        case ExpressionId::Sleepy:
            setBothLids(target, 0.27f);
            target.mouth.width = 52.0f;
            target.mouth.curve_depth = -4.0f;
            target.palette.feature = {132, 166, 186};
            break;
        case ExpressionId::Sleeping:
            setBothLids(target, 0.0f);
            target.mouth.width = 36.0f;
            target.mouth.curve_depth = 5.0f;
            target.palette.feature = {95, 126, 145};
            break;
        case ExpressionId::Waking:
            target.left_eye.size = {62.0f, 86.0f};
            target.right_eye.size = {62.0f, 86.0f};
            target.left_eye.lid_open = 0.42f;
            target.right_eye.lid_open = 0.42f;
            target.mouth.curve_depth = 12.0f;
            break;
        case ExpressionId::Petted:
            setBothLids(target, 0.42f);
            target.left_eye.rotation_degrees = 8.0f;
            target.right_eye.rotation_degrees = -8.0f;
            target.mouth.curve_depth = 25.0f;
            target.blush_intensity = 0.85f;
            break;
        case ExpressionId::Fed:
            target.mouth.curve_depth = 32.0f;
            target.mouth.openness = 0.18f;
            target.blush_intensity = 0.55f;
            break;
        case ExpressionId::Startled:
            target.left_eye.size = {70.0f, 96.0f};
            target.right_eye.size = {70.0f, 96.0f};
            target.left_eye.pupil_scale = 0.20f;
            target.right_eye.pupil_scale = 0.20f;
            target.mouth.width = 30.0f;
            target.mouth.openness = 0.82f;
            break;
        case ExpressionId::Paired:
            target.mouth.curve_depth = 22.0f;
            target.blush_intensity = 0.68f;
            target.palette.feature = {234, 221, 255};
            target.palette.blush = {189, 137, 255};
            break;
    }

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
                       lerp(a.rotation_degrees, b.rotation_degrees, progress), lerp(a.lid_open, b.lid_open, progress),
                       blendVec(a.pupil_offset, b.pupil_offset), lerp(a.pupil_scale, b.pupil_scale, progress)};
    };

    FacePose result = {
        .left_eye  = blendEye(from.left_eye, to.left_eye),
        .right_eye = blendEye(from.right_eye, to.right_eye),
        .mouth     = {blendVec(from.mouth.center, to.mouth.center), lerp(from.mouth.width, to.mouth.width, progress),
                      lerp(from.mouth.curve_depth, to.mouth.curve_depth, progress),
                      lerp(from.mouth.thickness, to.mouth.thickness, progress),
                      lerp(from.mouth.openness, to.mouth.openness, progress)},
        .palette   = {blend(from.palette.background, to.palette.background, progress),
                      blend(from.palette.feature, to.palette.feature, progress),
                      blend(from.palette.pupil, to.palette.pupil, progress),
                      blend(from.palette.blush, to.palette.blush, progress)},
        .blush_intensity = lerp(from.blush_intensity, to.blush_intensity, progress),
        .visual_tilt     = lerp(from.visual_tilt, to.visual_tilt, progress),
    };
    clampPose(result);
    return result;
}

bool isValid(const FacePose& pose)
{
    const float values[] = {
        pose.left_eye.center.x, pose.left_eye.center.y, pose.left_eye.size.x, pose.left_eye.size.y,
        pose.left_eye.rotation_degrees, pose.left_eye.lid_open, pose.left_eye.pupil_offset.x,
        pose.left_eye.pupil_offset.y, pose.left_eye.pupil_scale, pose.right_eye.center.x,
        pose.right_eye.center.y, pose.right_eye.size.x, pose.right_eye.size.y, pose.right_eye.rotation_degrees,
        pose.right_eye.lid_open, pose.right_eye.pupil_offset.x, pose.right_eye.pupil_offset.y,
        pose.right_eye.pupil_scale, pose.mouth.center.x, pose.mouth.center.y, pose.mouth.width,
        pose.mouth.curve_depth, pose.mouth.thickness, pose.mouth.openness, pose.blush_intensity, pose.visual_tilt,
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
           pose.left_eye.lid_open >= 0.0f && pose.left_eye.lid_open <= 1.0f &&
           pose.right_eye.lid_open >= 0.0f && pose.right_eye.lid_open <= 1.0f &&
           pose.mouth.openness >= 0.0f && pose.mouth.openness <= 1.0f && pose.blush_intensity >= 0.0f &&
           pose.blush_intensity <= 1.0f;
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
        blink_active_    = true;
        blink_started_ms_ = now_ms;
    }
}

void ExpressionController::applyLife(FacePose& pose, uint32_t now_ms) const
{
    const float phase = static_cast<float>((now_ms + random_state_) % 2200) / 2200.0f;
    const float drift = phase < 0.5f ? phase * 2.0f - 0.5f : 1.5f - phase * 2.0f;
    if (pose.left_eye.lid_open > 0.08f || pose.right_eye.lid_open > 0.08f) {
        pose.left_eye.pupil_offset.x  = clampSigned(pose.left_eye.pupil_offset.x + drift * 0.12f);
        pose.right_eye.pupil_offset.x = clampSigned(pose.right_eye.pupil_offset.x + drift * 0.12f);
    }

    if (blink_active_) {
        const float blink_progress = static_cast<float>(now_ms - blink_started_ms_) / 140.0f;
        const float closure        = std::sin(clampUnit(blink_progress) * kPi);
        pose.left_eye.lid_open *= 1.0f - closure;
        pose.right_eye.lid_open *= 1.0f - closure;
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
