/*
 * SPDX-License-Identifier: MIT
 */
#include "lvgl_face_renderer.h"

#include "glyph_paths.h"

#include <algorithm>
#include <cmath>

namespace pet::face {
namespace {

#if LV_USE_VECTOR_GRAPHIC

lv_opa_t toOpacity(float value)
{
    return static_cast<lv_opa_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(LV_OPA_COVER)));
}

lv_fpoint_t point(Vec2 value)
{
    return {value.x, value.y};
}

void configure(lv_draw_vector_dsc_t* descriptor, const PathStyle& style)
{
    lv_draw_vector_dsc_set_fill_opa(descriptor,
                                    style.fill ? toOpacity(style.opacity) : static_cast<lv_opa_t>(LV_OPA_TRANSP));
    lv_draw_vector_dsc_set_stroke_opa(descriptor,
                                      style.stroke ? toOpacity(style.opacity) : static_cast<lv_opa_t>(LV_OPA_TRANSP));
    if (style.fill) {
        lv_draw_vector_dsc_set_fill_color(descriptor, lv_color_make(style.color.red, style.color.green, style.color.blue));
    }
    if (style.stroke) {
        lv_draw_vector_dsc_set_stroke_color(descriptor,
                                            lv_color_make(style.color.red, style.color.green, style.color.blue));
        lv_draw_vector_dsc_set_stroke_width(descriptor, style.stroke_width);
        lv_draw_vector_dsc_set_stroke_cap(descriptor, LV_VECTOR_STROKE_CAP_ROUND);
        lv_draw_vector_dsc_set_stroke_join(descriptor, LV_VECTOR_STROKE_JOIN_ROUND);
    }
}

#endif

}  // namespace

LvglFaceRenderer::LvglFaceRenderer(lv_obj_t* parent)
{
    canvas_ = lv_canvas_create(parent);
    lv_obj_set_size(canvas_, static_cast<lv_coord_t>(kDesignWidth), static_cast<lv_coord_t>(kDesignHeight));
    lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);

#if LV_USE_VECTOR_GRAPHIC
    // The persistent 320 x 240 RGB565 canvas needs 150 KiB and keeps the
    // renderer asset-free. The Pet Base board provides PSRAM.
    draw_buffer_ = lv_draw_buf_create(static_cast<uint32_t>(kDesignWidth), static_cast<uint32_t>(kDesignHeight),
                                      LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    lv_canvas_set_draw_buf(canvas_, draw_buffer_);
#endif
}

LvglFaceRenderer::~LvglFaceRenderer()
{
#if LV_USE_VECTOR_GRAPHIC
    if (draw_buffer_) {
        lv_draw_buf_destroy(draw_buffer_);
    }
#endif
    if (canvas_) {
        lv_obj_delete(canvas_);
    }
}

void LvglFaceRenderer::render(const FacePose& pose)
{
    const FaceDrawing drawing = buildFaceDrawing(pose);
    lv_canvas_fill_bg(canvas_, toLvColor(drawing.background), LV_OPA_COVER);

#if LV_USE_VECTOR_GRAPHIC
    for (uint8_t index = 0; index < drawing.path_count; ++index) {
        drawPath(drawing.paths[index]);
    }
#endif
    lv_obj_invalidate(canvas_);
}

#if LV_USE_VECTOR_GRAPHIC

void LvglFaceRenderer::drawPath(const FacePath& path)
{
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    lv_draw_vector_dsc_t* descriptor = lv_draw_vector_dsc_create(&layer);
    lv_vector_path_t* vector_path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
    configure(descriptor, path.style);

    for (uint8_t index = 0; index < path.command_count; ++index) {
        const PathCommand& command = path.commands[index];
        switch (command.type) {
            case PathCommandType::Move: {
                const lv_fpoint_t value = point(command.first);
                lv_vector_path_move_to(vector_path, &value);
                break;
            }
            case PathCommandType::Line: {
                const lv_fpoint_t value = point(command.first);
                lv_vector_path_line_to(vector_path, &value);
                break;
            }
            case PathCommandType::Cubic: {
                const lv_fpoint_t first = point(command.first);
                const lv_fpoint_t second = point(command.second);
                const lv_fpoint_t end = point(command.end);
                lv_vector_path_cubic_to(vector_path, &first, &second, &end);
                break;
            }
            case PathCommandType::Close:
                lv_vector_path_close(vector_path);
                break;
        }
    }

    lv_draw_vector_dsc_add_path(descriptor, vector_path);
    lv_vector_path_delete(vector_path);
    lv_draw_vector(descriptor);
    lv_draw_vector_dsc_delete(descriptor);
    lv_canvas_finish_layer(canvas_, &layer);
}

#endif

lv_color_t LvglFaceRenderer::toLvColor(Color color)
{
    return lv_color_make(color.red, color.green, color.blue);
}

}  // namespace pet::face
