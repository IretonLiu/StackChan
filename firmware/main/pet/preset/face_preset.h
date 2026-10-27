/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_motion.h"

#include <pet/shape/circle.h>
#include <pet/shape/rounded_polygon.h>

#include <cstdint>

namespace pet::preset {

enum class ShapeKind : uint8_t {
    Circle,
    RoundedPolygon,
};

/** Host-compiled data only: firmware never parses JSON scene source. */
struct FacePresetNodeDefinition {
    ShapeKind kind = ShapeKind::Circle;
    shape::PathStyle style{};
    shape::Transform2D transform{};
    // Parent must precede this node in a compiled scene; -1 denotes world space.
    int8_t parent_index = -1;
    shape::CircleParameters circle{};
    shape::RoundedPolygonParameters rounded_polygon{};
    KeyframeMotion motion{};
    MotionEasing motion_easing = MotionEasing::Linear;
};

struct FacePresetDefinition {
    const char* id = nullptr;
    shape::Color background{};
    const FacePresetNodeDefinition* nodes = nullptr;
    uint8_t node_count = 0;
};

}  // namespace pet::preset
