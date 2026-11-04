/* SPDX-License-Identifier: MIT */
#pragma once

#include "sdf_face_renderer.h"

#include <lvgl.h>

namespace pet::sdf {

inline constexpr uint16_t kDesignWidth  = 320;
inline constexpr uint16_t kDesignHeight = 240;

/**
 * LVGL adapter for the standalone SDF face module.
 *
 * It owns a persistent PSRAM ARGB8888 canvas. Unlike the vector face renderer,
 * this class receives an SDF Scene directly and asks the field rasterizer to
 * composite pixels into that buffer; it never constructs FaceDrawing paths.
 */
class LvglSdfFaceRenderer {
public:
    explicit LvglSdfFaceRenderer(lv_obj_t* parent);
    ~LvglSdfFaceRenderer();

    LvglSdfFaceRenderer(const LvglSdfFaceRenderer&)            = delete;
    LvglSdfFaceRenderer& operator=(const LvglSdfFaceRenderer&) = delete;

    [[nodiscard]] bool render(const Scene& scene);
    [[nodiscard]] bool ready() const { return canvas_ != nullptr && draw_buffer_ != nullptr; }

private:
    lv_obj_t* canvas_           = nullptr;
    lv_draw_buf_t* draw_buffer_ = nullptr;
};

}  // namespace pet::sdf
