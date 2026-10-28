/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <pet/shape/circle.h>
#include <pet/shape/rounded_polygon.h>

#include <cstdint>

namespace pet::preset {

enum class ShapeKind : uint8_t {
    Circle,
    RoundedPolygon,
};

struct FacePoseDefinition;
struct FaceAnimationGoalDefinition;

/** Host-compiled data only: firmware never parses JSON scene source. */
struct FacePresetNodeDefinition {
    ShapeKind kind = ShapeKind::Circle;
    shape::PathStyle style{};
    shape::Transform2D transform{};
    // Parent must precede this node in a compiled scene; -1 denotes world space.
    int8_t parent_index = -1;
    shape::CircleParameters circle{};
    shape::RoundedPolygonParameters rounded_polygon{};
};

struct FacePresetDefinition {
    const char* id = nullptr;
    shape::Color background{};
    const FacePresetNodeDefinition* nodes = nullptr;
    uint8_t node_count                    = 0;
    // Optional typed pose table; definitions live in face_pose_engine.h.
    const FacePoseDefinition* poses                    = nullptr;
    uint8_t pose_count                                 = 0;
    const FaceAnimationGoalDefinition* animation_goals = nullptr;
    uint8_t animation_goal_count                       = 0;
    const char* default_animation_goal_id              = nullptr;
};

}  // namespace pet::preset
