/*
 * SPDX-License-Identifier: MIT
 */
#include "lvgl_face_renderer.h"

#include "glyph_paths.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pet::face {
namespace {

#if LV_USE_VECTOR_GRAPHIC

constexpr int32_t kCanvasWidth  = static_cast<int32_t>(kDesignWidth);
constexpr int32_t kCanvasHeight = static_cast<int32_t>(kDesignHeight);

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
        lv_draw_vector_dsc_set_fill_color(descriptor,
                                          lv_color_make(style.color.red, style.color.green, style.color.blue));
    }
    if (style.stroke) {
        lv_draw_vector_dsc_set_stroke_color(descriptor,
                                            lv_color_make(style.color.red, style.color.green, style.color.blue));
        lv_draw_vector_dsc_set_stroke_width(descriptor, style.stroke_width);
        lv_draw_vector_dsc_set_stroke_cap(descriptor, LV_VECTOR_STROKE_CAP_ROUND);
        lv_draw_vector_dsc_set_stroke_join(descriptor, LV_VECTOR_STROKE_JOIN_ROUND);
    }
}

/// Bounding box of every control point, grown by the widest stroke half-width.
///
/// The control hull always contains the curve, so this box is a safe dirty
/// region even for animated (already transformed) path geometry.
bool bounds(const FaceDrawing& drawing, lv_area_t& area)
{
    float low_x    = 0.0f;
    float low_y    = 0.0f;
    float high_x   = 0.0f;
    float high_y   = 0.0f;
    float margin   = 0.0f;
    bool any_point = false;

    for (uint8_t path_index = 0; path_index < drawing.path_count; ++path_index) {
        const FacePath& path = drawing.paths[path_index];
        margin               = std::max(margin, (path.style.stroke ? path.style.stroke_width * 0.5f : 0.0f) + 2.0f);
        for (uint8_t index = 0; index < path.command_count; ++index) {
            const PathCommand& command = path.commands[index];
            // Only the control points that belong to this command define the
            // hull; unused fields are zero-initialised.
            const Vec2 points[3] = {command.first, command.second, command.end};
            const uint8_t count  = command.type == PathCommandType::Cubic ? 3 : 1;
            for (uint8_t point_index = 0; point_index < count; ++point_index) {
                const Vec2& value = points[point_index];
                if (!any_point) {
                    low_x     = value.x;
                    low_y     = value.y;
                    high_x    = value.x;
                    high_y    = value.y;
                    any_point = true;
                } else {
                    low_x  = std::min(low_x, value.x);
                    low_y  = std::min(low_y, value.y);
                    high_x = std::max(high_x, value.x);
                    high_y = std::max(high_y, value.y);
                }
            }
        }
    }

    if (!any_point) {
        return false;
    }

    area.x1 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(low_x - margin)));
    area.y1 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(low_y - margin)));
    area.x2 = std::min<int32_t>(kCanvasWidth - 1, static_cast<int32_t>(std::ceil(high_x + margin)));
    area.y2 = std::min<int32_t>(kCanvasHeight - 1, static_cast<int32_t>(std::ceil(high_y + margin)));
    return area.x1 <= area.x2 && area.y1 <= area.y2;
}

void unite(lv_area_t& target, const lv_area_t& other)
{
    target.x1 = std::min(target.x1, other.x1);
    target.y1 = std::min(target.y1, other.y1);
    target.x2 = std::max(target.x2, other.x2);
    target.y2 = std::max(target.y2, other.y2);
}

void expand(lv_area_t& area)
{
    area.x1 = std::max<int32_t>(0, area.x1 - 1);
    area.y1 = std::max<int32_t>(0, area.y1 - 1);
    area.x2 = std::min<int32_t>(kCanvasWidth - 1, area.x2 + 1);
    area.y2 = std::min<int32_t>(kCanvasHeight - 1, area.y2 + 1);
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
    // An ARGB8888 canvas lets the vector engine rasterize straight into the
    // persistent buffer, so a frame never allocates a temporary surface. The
    // 300 KiB buffer lives in PSRAM on the Pet Base board.
    draw_buffer_ = lv_draw_buf_create(static_cast<uint32_t>(kDesignWidth), static_cast<uint32_t>(kDesignHeight),
                                      LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    lv_canvas_set_draw_buf(canvas_, draw_buffer_);
    clearArea({0, 0, kCanvasWidth - 1, kCanvasHeight - 1}, drawing_background_);
    lv_obj_invalidate(canvas_);
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
    render(buildFaceDrawing(pose));
}

void LvglFaceRenderer::render(const FaceDrawing& drawing)
{
#if LV_USE_VECTOR_GRAPHIC
    lv_area_t ink_area{};
    const bool has_ink = drawing.path_count > 0 && bounds(drawing, ink_area);

    // Repainting only the ink region keeps the PSRAM write traffic low enough
    // for 30 fps; a full 320 x 240 canvas clear alone costs about 20 ms.
    lv_area_t dirty{};
    bool has_dirty                = false;
    const bool background_changed = drawing.background.red != drawing_background_.red ||
                                    drawing.background.green != drawing_background_.green ||
                                    drawing.background.blue != drawing_background_.blue;
    if (background_changed) {
        // The persistent canvas was initially cleared with its default colour.
        // A new preset background therefore has to repaint every device pixel,
        // not only the current face ink region.
        dirty     = {0, 0, kCanvasWidth - 1, kCanvasHeight - 1};
        has_dirty = true;
    } else if (has_ink) {
        dirty     = ink_area;
        has_dirty = true;
        if (previous_ink_valid_) {
            unite(dirty, previous_ink_);
        }
    } else if (previous_ink_valid_) {
        dirty     = previous_ink_;
        has_dirty = true;
    }

    if (has_dirty) {
        expand(dirty);
        clearArea(dirty, drawing.background);
        if (has_ink) {
            drawInk(drawing, dirty);
        }
        lv_obj_invalidate_area(canvas_, &dirty);
    }

    previous_ink_       = ink_area;
    previous_ink_valid_ = has_ink;
    drawing_background_ = drawing.background;
#else
    lv_canvas_fill_bg(canvas_, toLvColor(drawing.background), LV_OPA_COVER);
    lv_obj_invalidate(canvas_);
#endif
}

#if LV_USE_VECTOR_GRAPHIC

void LvglFaceRenderer::clearArea(const lv_area_t& area, Color color)
{
    const uint32_t pixel  = (0xFFu << 24) | (static_cast<uint32_t>(color.red) << 16) |
                            (static_cast<uint32_t>(color.green) << 8) | static_cast<uint32_t>(color.blue);
    const uint32_t stride = draw_buffer_->header.stride / 4;
    auto* pixels          = reinterpret_cast<uint32_t*>(draw_buffer_->data);

    for (int32_t y = area.y1; y <= area.y2; ++y) {
        uint32_t* row = pixels + static_cast<size_t>(y) * stride + static_cast<size_t>(area.x1);
        std::fill(row, row + (area.x2 - area.x1 + 1), pixel);
    }
}

void LvglFaceRenderer::drawInk(const FaceDrawing& drawing, const lv_area_t& area)
{
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    layer._clip_area    = area;
    layer.phy_clip_area = area;

    lv_draw_vector_dsc_t* descriptor = lv_draw_vector_dsc_create(&layer);
    for (uint8_t index = 0; index < drawing.path_count; ++index) {
        submitPath(descriptor, drawing.paths[index]);
    }
    // One vector task per frame: the software rasterizer then runs once no
    // matter how many paths the face is made of.
    lv_draw_vector(descriptor);
    lv_canvas_finish_layer(canvas_, &layer);
    lv_draw_vector_dsc_delete(descriptor);
}

void LvglFaceRenderer::submitPath(lv_draw_vector_dsc_t* descriptor, const FacePath& path)
{
    lv_vector_path_t* vector_path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);

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
                const lv_fpoint_t first  = point(command.first);
                const lv_fpoint_t second = point(command.second);
                const lv_fpoint_t end    = point(command.end);
                lv_vector_path_cubic_to(vector_path, &first, &second, &end);
                break;
            }
            case PathCommandType::Close:
                lv_vector_path_close(vector_path);
                break;
        }
    }

    configure(descriptor, path.style);
    lv_draw_vector_dsc_add_path(descriptor, vector_path);
    lv_vector_path_delete(vector_path);
}

#endif

lv_color_t LvglFaceRenderer::toLvColor(Color color)
{
    return lv_color_make(color.red, color.green, color.blue);
}

}  // namespace pet::face
