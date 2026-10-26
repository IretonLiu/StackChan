/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "glyph_paths.h"

#include <lvgl.h>

namespace pet::face {

/**
 * Pet Base adapter for the shared, smooth face path model.
 *
 * The canvas is a persistent ARGB8888 buffer in PSRAM. Each frame repaints only
 * the union of the previous and current ink bounds, so the cost scales with the
 * face area instead of the whole screen.
 *
 * The LVGL/ThorVG vector pipeline is stack hungry: render() needs roughly 21 KiB
 * of stack, so it must not run on the default 8 KiB main task. Pet Base raises
 * CONFIG_ESP_MAIN_TASK_STACK_SIZE for exactly this reason.
 */
class LvglFaceRenderer {
public:
    explicit LvglFaceRenderer(lv_obj_t* parent);
    ~LvglFaceRenderer();

    LvglFaceRenderer(const LvglFaceRenderer&)            = delete;
    LvglFaceRenderer& operator=(const LvglFaceRenderer&) = delete;

    void render(const FacePose& pose);
    void render(const FaceDrawing& drawing);

private:
    static lv_color_t toLvColor(Color color);

#if LV_USE_VECTOR_GRAPHIC
    void clearArea(const lv_area_t& area, Color color);
    void drawInk(const FaceDrawing& drawing, const lv_area_t& area);
    static void submitPath(lv_draw_vector_dsc_t* descriptor, const FacePath& path);
#endif

    lv_obj_t* canvas_           = nullptr;
    lv_draw_buf_t* draw_buffer_ = nullptr;
    Color drawing_background_{};
    lv_area_t previous_ink_{};
    bool previous_ink_valid_ = false;
};

}  // namespace pet::face
