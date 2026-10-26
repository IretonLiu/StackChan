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
    void renderEye(const EyePose& eye, lv_obj_t* eye_object, lv_obj_t* pupil_object, lv_obj_t* lid_object,
                   const FacePalette& palette);
    static lv_color_t toLvColor(Color color);

    lv_obj_t* root_ = nullptr;
    std::array<lv_obj_t*, 2> eyes_{};
    std::array<lv_obj_t*, 2> pupils_{};
    std::array<lv_obj_t*, 2> lids_{};
    std::array<lv_obj_t*, 2> blushes_{};
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* open_mouth_ = nullptr;
    std::array<lv_point_precise_t, 9> mouth_points_{};
};

}  // namespace pet::face
