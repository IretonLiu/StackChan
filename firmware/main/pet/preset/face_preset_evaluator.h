/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_preset.h"

#include <array>
#include <cstdint>

namespace pet::preset {

/** Matches the compiler and FaceDrawing node capacity. */
constexpr uint8_t kMaxFacePresetNodes = 16;

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
 * `elapsed_ms` is supplied by the application instead of reading a clock. It
 * is intentionally unused until compiled motion definitions are added; this
 * establishes the host/device-shared playback boundary without changing the
 * static preset result. On failure `evaluation` is left unchanged.
 */
[[nodiscard]] bool evaluateFacePreset(const FacePresetDefinition& preset, uint32_t elapsed_ms,
                                      FacePresetEvaluation& evaluation, FacePresetEvaluationError* error = nullptr);

}  // namespace pet::preset
