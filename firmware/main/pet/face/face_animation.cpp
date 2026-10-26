/*
 * SPDX-License-Identifier: MIT
 */
#include "face_animation.h"

#include <cmath>
#include <initializer_list>

namespace pet::face {
namespace {

constexpr Color kInk{248, 245, 237};
constexpr float kPi = 3.14159265358979323846f;

PathCommand move(float x, float y)
{
    return {PathCommandType::Move, {x, y}, {}, {}};
}

PathCommand line(float x, float y)
{
    return {PathCommandType::Line, {x, y}, {}, {}};
}

PathCommand cubic(float first_x, float first_y, float second_x, float second_y, float end_x, float end_y)
{
    return {PathCommandType::Cubic, {first_x, first_y}, {second_x, second_y}, {end_x, end_y}};
}

PathCommand close()
{
    return {PathCommandType::Close, {}, {}, {}};
}

FacePath path(bool fill, std::initializer_list<PathCommand> commands)
{
    FacePath result;
    result.style = {.color = kInk, .opacity = 1.0f, .stroke_width = 6.0f, .fill = fill, .stroke = true};
    for (const PathCommand& command : commands) {
        if (result.command_count < result.commands.size()) {
            result.commands[result.command_count++] = command;
        }
    }
    return result;
}

void append(FaceDrawing& drawing, FacePath face_path)
{
    if (drawing.path_count < drawing.paths.size()) {
        drawing.paths[drawing.path_count++] = face_path;
    }
}

void clear(FaceDrawing& drawing)
{
    drawing.background = {0, 0, 0};
    drawing.path_count = 0;
}

void idleFace(FaceDrawing& drawing)
{
    clear(drawing);
    append(drawing, path(false, {move(69.0f, 110.9f), cubic(85.0f, 103.0f, 103.0f, 103.0f, 120.0f, 103.0f)}));
    append(drawing, path(false, {move(200.0f, 103.0f), cubic(217.0f, 103.0f, 235.0f, 103.0f, 249.3f, 109.2f)}));
    append(drawing, path(false, {move(136.0f, 125.0f), cubic(148.0f, 139.0f, 154.0f, 137.0f, 160.0f, 125.0f),
                                cubic(170.0f, 130.0f, 172.0f, 139.0f, 184.0f, 125.0f)}));
}

void mischievousFace(FaceDrawing& drawing)
{
    clear(drawing);
    append(drawing, path(true, {move(85.1f, 89.3f), cubic(72.0f, 119.7f, 93.9f, 134.9f, 120.9f, 125.2f), close()}));
    append(drawing, path(true, {move(201.2f, 126.1f), cubic(234.9f, 133.7f, 245.9f, 112.1f, 235.4f, 88.9f), close()}));
    append(drawing, path(false, {move(136.0f, 125.0f), cubic(148.0f, 139.0f, 154.0f, 137.0f, 160.0f, 125.0f),
                                cubic(170.0f, 130.0f, 172.0f, 139.0f, 184.0f, 125.0f)}));
}

void happyFace(FaceDrawing& drawing)
{
    clear(drawing);
    append(drawing, path(false, {move(136.0f, 125.0f), cubic(148.0f, 139.0f, 154.0f, 137.0f, 160.0f, 125.0f),
                                cubic(170.0f, 130.0f, 172.0f, 139.0f, 184.0f, 125.0f)}));
    append(drawing, path(false, {move(227.3f, 116.3f), line(196.5f, 104.9f)}));
    append(drawing, path(false, {move(90.0f, 116.9f), line(120.8f, 105.6f)}));
    append(drawing, path(false, {move(230.3f, 102.4f), line(199.9f, 104.1f)}));
    append(drawing, path(false, {move(89.5f, 103.0f), line(119.9f, 104.7f)}));
    append(drawing, path(false, {move(228.2f, 87.6f), line(196.1f, 104.1f)}));
    append(drawing, path(false, {move(88.3f, 88.7f), line(120.3f, 105.1f)}));
    append(drawing, path(false, {move(51.3f, 130.7f), line(66.9f, 118.9f)}));
    append(drawing, path(false, {move(266.6f, 131.5f), line(255.4f, 121.2f)}));
    append(drawing, path(false, {move(64.0f, 133.7f), line(79.6f, 121.0f)}));
    append(drawing, path(false, {move(255.2f, 132.0f), line(243.6f, 122.9f)}));
    append(drawing, path(false, {move(81.0f, 135.5f), line(85.9f, 124.8f)}));
    append(drawing, path(false, {move(240.9f, 134.0f), line(236.0f, 123.3f)}));
    append(drawing, path(true, {move(227.3f, 53.0f), cubic(213.8f, 29.4f, 209.2f, 60.6f, 220.6f, 71.6f),
                               cubic(246.8f, 63.6f, 250.6f, 49.6f, 226.9f, 53.0f), close()}));
}

void dizzyFace(FaceDrawing& drawing)
{
    clear(drawing);
    append(drawing, path(false, {move(81.7f, 99.5f), cubic(65.6f, 137.5f, 112.1f, 140.4f, 118.0f, 113.0f),
                                cubic(120.5f, 98.2f, 106.6f, 86.4f, 94.4f, 95.7f),
                                cubic(87.2f, 104.9f, 86.3f, 115.9f, 102.4f, 115.5f)}));
    append(drawing, path(false, {move(196.7f, 98.8f), cubic(180.7f, 136.8f, 227.1f, 139.7f, 233.0f, 112.3f),
                                cubic(235.5f, 97.5f, 221.6f, 85.7f, 209.4f, 95.0f),
                                cubic(202.2f, 104.3f, 201.3f, 115.3f, 217.4f, 114.8f)}));
    append(drawing, path(false, {move(131.1f, 122.3f), cubic(137.0f, 142.1f, 136.6f, 142.5f, 144.2f, 123.1f),
                                cubic(150.1f, 135.3f, 150.1f, 137.9f, 155.6f, 123.9f),
                                cubic(164.0f, 135.8f, 164.4f, 136.2f, 167.8f, 123.1f),
                                cubic(179.2f, 134.1f, 178.8f, 133.2f, 185.5f, 123.1f)}));
}

void transformPoint(Vec2& point, float rotation_degrees, Vec2 center, float translate_x, float translate_y)
{
    const float radians = rotation_degrees * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    point.x = center.x + x * cosine - y * sine + translate_x;
    point.y = center.y + x * sine + y * cosine + translate_y;
}

void transform(FacePath& path, float rotation_degrees, Vec2 center, float translate_x = 0.0f, float translate_y = 0.0f)
{
    for (uint8_t index = 0; index < path.command_count; ++index) {
        PathCommand& command = path.commands[index];
        if (command.type == PathCommandType::Close) {
            continue;
        }
        transformPoint(command.first, rotation_degrees, center, translate_x, translate_y);
        if (command.type == PathCommandType::Cubic) {
            transformPoint(command.second, rotation_degrees, center, translate_x, translate_y);
            transformPoint(command.end, rotation_degrees, center, translate_x, translate_y);
        }
    }
}

void drawingFor(PetFaceState state, FaceDrawing& drawing)
{
    switch (state) {
        case PetFaceState::Idle: idleFace(drawing); return;
        case PetFaceState::Mischievous: mischievousFace(drawing); return;
        case PetFaceState::Happy: happyFace(drawing); return;
        case PetFaceState::Dizzy: dizzyFace(drawing); return;
    }
    idleFace(drawing);
}

}  // namespace

PetFaceAnimator::PetFaceAnimator(uint32_t seed) : random_state_(seed ? seed : 1)
{
}

void PetFaceAnimator::reset(uint32_t now_ms)
{
    state_ = PetFaceState::Idle;
    state_started_ms_ = now_ms;
    state_ends_ms_ = 0;
    next_idle_change_ms_ = now_ms + nextIdleIntervalMs();
}

void PetFaceAnimator::onPetted(uint32_t now_ms)
{
    if (state_ == PetFaceState::Dizzy && now_ms < state_ends_ms_) {
        return;
    }
    setState(PetFaceState::Happy, now_ms);
    state_ends_ms_ = now_ms + 3200;
}

void PetFaceAnimator::onShaken(uint32_t now_ms)
{
    setState(PetFaceState::Dizzy, now_ms);
    state_ends_ms_ = now_ms + 4000;
}

const FaceDrawing& PetFaceAnimator::sample(uint32_t now_ms)
{
    if ((state_ == PetFaceState::Happy || state_ == PetFaceState::Dizzy) && now_ms >= state_ends_ms_) {
        setState(PetFaceState::Idle, now_ms);
        next_idle_change_ms_ = now_ms + nextIdleIntervalMs();
    }
    if ((state_ == PetFaceState::Idle || state_ == PetFaceState::Mischievous) && now_ms >= next_idle_change_ms_) {
        setState(state_ == PetFaceState::Idle ? PetFaceState::Mischievous : PetFaceState::Idle, now_ms);
        next_idle_change_ms_ = now_ms + nextIdleIntervalMs();
    }

    drawingFor(state_, drawing_);
    const float elapsed_ms = static_cast<float>(now_ms - state_started_ms_);
    if (state_ == PetFaceState::Idle || state_ == PetFaceState::Mischievous) {
        const float breath = std::sin(elapsed_ms * (2.0f * kPi / 2600.0f)) * 1.35f;
        transform(drawing_.paths[2], 0.0f, {0.0f, 0.0f}, 0.0f, breath);
    } else if (state_ == PetFaceState::Happy) {
        const float phase = std::sin(elapsed_ms * (2.0f * kPi / 720.0f));
        for (uint8_t index = 7; index <= 12; ++index) {
            const float direction = (index % 2 == 1) ? -1.0f : 1.0f;
            transform(drawing_.paths[index], 0.0f, {0.0f, 0.0f}, direction * phase * 1.6f, phase * 0.8f);
        }
        transform(drawing_.paths[13], std::sin(elapsed_ms * (2.0f * kPi / 1100.0f)) * 10.0f, {229.0f, 53.0f});
    } else if (state_ == PetFaceState::Dizzy) {
        const float angle = std::fmod(elapsed_ms * 0.40f, 360.0f);
        transform(drawing_.paths[0], angle, {96.0f, 112.0f});
        transform(drawing_.paths[1], angle, {211.0f, 112.0f});
    }
    return drawing_;
}

PetFaceState PetFaceAnimator::state() const
{
    return state_;
}

uint32_t PetFaceAnimator::nextRandom()
{
    random_state_ = random_state_ * 1664525u + 1013904223u;
    return random_state_;
}

uint32_t PetFaceAnimator::nextIdleIntervalMs()
{
    return 4000u + nextRandom() % 6001u;
}

void PetFaceAnimator::setState(PetFaceState state, uint32_t now_ms)
{
    state_ = state;
    state_started_ms_ = now_ms;
}

}  // namespace pet::face
