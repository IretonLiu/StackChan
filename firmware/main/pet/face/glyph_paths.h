/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace pet::face {

enum class PathCommandType : uint8_t {
    Move,
    Line,
    Cubic,
    Close,
};

struct PathCommand {
    PathCommandType type;
    Vec2 first{};
    Vec2 second{};
    Vec2 end{};
};

struct PathStyle {
    Color color{};
    float opacity = 1.0f;
    float stroke_width = 0.0f;
    bool fill = false;
    bool stroke = false;
};

constexpr size_t kMaxPathCommands = 12;
constexpr size_t kMaxFacePaths = 16;

struct FacePath {
    PathStyle style{};
    std::array<PathCommand, kMaxPathCommands> commands{};
    uint8_t command_count = 0;
};

struct FaceDrawing {
    Color background{};
    std::array<FacePath, kMaxFacePaths> paths{};
    uint8_t path_count = 0;
};

/**
 * Generates the complete, renderer-neutral face drawing for a pose.
 *
 * Both the SVG preview and the Pet Base LVGL adapter consume these exact
 * cubic Bézier commands. The fixed-size output is allocation-free and bounded
 * for 30 fps firmware rendering.
 */
FaceDrawing buildFaceDrawing(const FacePose& pose);

}  // namespace pet::face
