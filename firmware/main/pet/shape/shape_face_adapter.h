/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "shape_path.h"

#include <pet/face/glyph_paths.h>

namespace pet::shape {

/**
 * Converts generated shape geometry to the fixed-capacity vector model consumed
 * by LvglFaceRenderer. The conversion is lossless for ShapePath's Move, Cubic,
 * and Close command subset.
 */
[[nodiscard]] bool toFacePath(const ShapePath& source, face::FacePath& destination);

/**
 * Appends a converted shape to the exact FaceDrawing submitted to the firmware
 * renderer. Returns false without modifying `drawing` when it has no path slot,
 * cannot fit in a FacePath, or contains invalid numeric geometry.
 */
[[nodiscard]] bool appendToFaceDrawing(const ShapePath& source, face::FaceDrawing& drawing);

}  // namespace pet::shape
