/* SPDX-License-Identifier: MIT */
#pragma once

// Document model contract for Kiwi Studio.
//
// This header is deliberately UI-free: it describes what a scene *is*, not how it is
// edited or drawn. Anything that only needs to understand the document can include this
// header alone. Editor-only state lives in editor_state.h.

#include "animation_model.h"

#include <pet/preset/face_motion.h>
#include <pet/shape/circle.h>
#include <pet/shape/rounded_polygon.h>
#include <pet/shape/shape_path.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace kiwi::studio {

inline constexpr size_t kMaxNodes         = 16;
inline constexpr float kDisplayWidth      = 320.0f;
inline constexpr float kDisplayHeight     = 240.0f;
inline constexpr float kMinNodeCoordinate = -4096.0f;
// The scene grammar nests deeper than ArduinoJson's default once compiler-owned
// animations (goals > programs > tracks > keyframes) are preserved.
inline constexpr uint8_t kMaxJsonNesting  = 32;
inline constexpr float kMaxNodeCoordinate = 4096.0f;

enum class Kind : uint8_t { Circle, Polygon };

/**
 * Animatable per-node channel. The explicit values are the on-disk `property`
 * indices used by `.petface.json`, so they must not be renumbered.
 */
enum class MotionProperty : int {
    TranslateX      = 0,
    TranslateY      = 1,
    Rotation        = 2,
    ScaleX          = 3,
    ScaleY          = 4,
    Opacity         = 5,
    CircleRadius    = 6,
    CornerRoundness = 7,
    EdgeBulge       = 8,
};

/** Playback/authoring settings for one node's procedural motion. */
struct MotionPreview {
    bool enabled            = false;
    bool playing            = false;
    bool loop               = true;
    bool use_keyframes      = false;
    MotionProperty property = MotionProperty::Rotation;
    float amplitude         = 8.0f;
    float period_seconds    = 2.0f;
    float phase_radians     = 0.0f;
    float time_seconds      = 0.0f;
    float duration_seconds  = 2.0f;
    pet::preset::KeyframeMotion keyframes{};
    pet::preset::MotionEasing easing = pet::preset::MotionEasing::Linear;
};

struct Node {
    std::string id;
    Kind kind = Kind::Circle;
    pet::shape::PathStyle style{};
    pet::shape::Transform2D transform{};
    pet::shape::CircleParameters circle{{160, 120}, 20};
    pet::shape::RoundedPolygonParameters polygon{};
    int parent_index = -1;
    bool visible     = true;
    MotionPreview motion{};
};

/**
 * The editable document. Invariants (ids, parent ordering, convex polygons, numeric
 * ranges) are owned by validateScene() in validate.h; mutation helpers must keep the
 * document valid before it is handed to persistence or the compiler.
 */
struct SceneDoc {
    std::string id{"kiwi-scene"};
    pet::shape::Color background{7, 16, 24};
    std::array<Node, kMaxNodes> nodes{};
    size_t count = 0;
    // Poses and animation goals are editable and serialized back to the same schema the
    // preset compiler consumes.
    AnimationDoc animations{};
};

}  // namespace kiwi::studio
