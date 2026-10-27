/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "shape_path.h"

#include <cstdint>

namespace pet::shape {

struct CircleParameters {
    Vec2 center{};
    float radius = 1.0f;
};

enum class CircleSampleError : uint8_t {
    None,
    NonFiniteParameters,
    NonFiniteTransform,
    NonFiniteStyle,
    InvalidRadius,
    NonFiniteGeometry,
};

struct CircleSample {
    ShapePath path{};
    CircleParameters parameters{};
    CircleSampleError error = CircleSampleError::None;
};

/** Samples a circle as a closed four-cubic loop. Non-uniform transforms yield an ellipse. */
[[nodiscard]] bool sampleCircle(const CircleParameters& parameters, const Affine2D& world, CircleSample& sample,
                                PathStyle style = {});

}  // namespace pet::shape
