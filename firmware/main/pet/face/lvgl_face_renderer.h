/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face.h"

#include <array>
#include <lvgl.h>

namespace pet::face {

/**
 * Fixed-object LVGL adapter for FacePose.
 *
 * Construct it once under the display lock, then call render() from the Pet UI
 * tick under that same lock. render() performs no allocation.
 */
class LvglFaceRenderer {
public:
    explicit LvglFaceRenderer(lv_obj_t* parent);
    ~LvglFaceRenderer();

    LvglFaceRenderer(const LvglFaceRenderer&)            = delete;
    LvglFaceRenderer& operator=(const LvglFaceRenderer&) = delete;

    void render(const FacePose& pose);

private:
    void renderEyeGlyph(EyeGlyph glyph, const EyePose& eye, Color feature, float opacity, size_t slot);
    void renderMouthGlyph(MouthGlyph glyph, const MouthPose& mouth, Color feature, float opacity, size_t slot);
    void renderAccent(const FacePose& pose);
    static lv_color_t toLvColor(Color color);

    lv_obj_t* root_ = nullptr;
    std::array<lv_obj_t*, 4> eye_lines_{};
    std::array<lv_obj_t*, 4> eye_shapes_{};
    std::array<std::array<lv_point_precise_t, 5>, 4> eye_points_{};
    std::array<lv_obj_t*, 2> mouth_lines_{};
    std::array<lv_obj_t*, 2> mouth_shapes_{};
    std::array<std::array<lv_point_precise_t, 5>, 2> mouth_points_{};
    std::array<lv_obj_t*, 2> accent_shapes_{};
    lv_obj_t* accent_line_ = nullptr;
    std::array<lv_point_precise_t, 6> accent_points_{};
};

}  // namespace pet::face
