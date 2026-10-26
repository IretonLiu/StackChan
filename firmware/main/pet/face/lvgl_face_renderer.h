/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face.h"

#include <lvgl.h>

namespace pet::face {

/** Pet Base adapter for the shared, smooth face path model. */
class LvglFaceRenderer {
public:
    explicit LvglFaceRenderer(lv_obj_t* parent);
    ~LvglFaceRenderer();

    LvglFaceRenderer(const LvglFaceRenderer&)            = delete;
    LvglFaceRenderer& operator=(const LvglFaceRenderer&) = delete;

    void render(const FacePose& pose);

private:
    static lv_color_t toLvColor(Color color);

#if LV_USE_VECTOR_GRAPHIC
    void drawPath(const struct FacePath& path);
#endif

    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* draw_buffer_ = nullptr;
};

}  // namespace pet::face
