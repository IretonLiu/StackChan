/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_preset.h"

#include <pet/face_schema.generated.h>

#include <array>
#include <cstdint>

namespace pet::preset {

/**
 * Caller-owned evaluated preset state. Future motion definitions modify this
 * value before the renderer samples paths; immutable generated definitions are
 * never mutated at runtime.
 */
struct FacePresetEvaluation {
    shape::Color background{};
    std::array<FacePresetNodeDefinition, kMaxFacePresetNodes> nodes{};
    uint8_t node_count = 0;
};

enum class FacePresetEvaluationError : uint8_t {
    None,
    EmptyPreset,
    TooManyNodes,
    InvalidNodeKind,
    InvalidParent,
    NonFiniteValue,
    InvalidTransform,
    InvalidStyle,
    InvalidCircle,
    InvalidPolygon,
};

/**
 * Copies and validates a generated preset into caller-owned mutable state.
 *
 * The result is the static base pose; `elapsed_ms` is accepted so the host and
 * device share one call signature but is not consumed here. Time-dependent motion
 * is applied afterwards by advanceFacePose() using a frame delta. On failure
 * `evaluation` is left unchanged.
 */
[[nodiscard]] bool evaluateFacePreset(const FacePresetDefinition& preset, uint32_t elapsed_ms,
                                      FacePresetEvaluation& evaluation, FacePresetEvaluationError* error = nullptr);

}  // namespace pet::preset
