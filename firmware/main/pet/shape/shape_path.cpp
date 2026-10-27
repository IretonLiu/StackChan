/*
 * SPDX-License-Identifier: MIT
 */
#include "shape_path.h"

#include <algorithm>
#include <cmath>

namespace pet::shape {

Affine2D makeAffine(const Transform2D& transform)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = transform.rotation_degrees * kPi / 180.0f;
    const float cosine  = std::cos(radians);
    const float sine    = std::sin(radians);

    Affine2D result{
        cosine * transform.scale.x,
        -sine * transform.scale.y,
        0.0f,
        sine * transform.scale.x,
        cosine * transform.scale.y,
        0.0f,
    };
    // The pivot is the local point placed at translation. This makes an ear
    // base attachment stable while its rotation and scale change.
    result.tx = transform.translation.x - result.m00 * transform.pivot.x - result.m01 * transform.pivot.y;
    result.ty = transform.translation.y - result.m10 * transform.pivot.x - result.m11 * transform.pivot.y;
    return result;
}

bool invert(const Affine2D& transform, Affine2D& inverse)
{
    const float determinant = transform.m00 * transform.m11 - transform.m01 * transform.m10;
    if (std::abs(determinant) < 0.000001f) {
        return false;
    }

    const float reciprocal = 1.0f / determinant;
    inverse.m00 = transform.m11 * reciprocal;
    inverse.m01 = -transform.m01 * reciprocal;
    inverse.m10 = -transform.m10 * reciprocal;
    inverse.m11 = transform.m00 * reciprocal;
    inverse.tx = -(inverse.m00 * transform.tx + inverse.m01 * transform.ty);
    inverse.ty = -(inverse.m10 * transform.tx + inverse.m11 * transform.ty);
    return true;
}

void include(Bounds2D& bounds, Vec2 point)
{
    if (!bounds.valid) {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.valid   = true;
        return;
    }
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
}

}  // namespace pet::shape
