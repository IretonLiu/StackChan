/* SPDX-License-Identifier: MIT */
#include "lvgl_sdf_face_renderer.h"

namespace pet::sdf {

LvglSdfFaceRenderer::LvglSdfFaceRenderer(lv_obj_t* parent)
{
    canvas_ = lv_canvas_create(parent);
    lv_obj_set_size(canvas_, static_cast<lv_coord_t>(kDesignWidth), static_cast<lv_coord_t>(kDesignHeight));
    lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_EVENT_BUBBLE);

    draw_buffer_ = lv_draw_buf_create(kDesignWidth, kDesignHeight, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if (draw_buffer_ != nullptr) lv_canvas_set_draw_buf(canvas_, draw_buffer_);
}

LvglSdfFaceRenderer::~LvglSdfFaceRenderer()
{
    if (draw_buffer_ != nullptr) lv_draw_buf_destroy(draw_buffer_);
    if (canvas_ != nullptr) lv_obj_delete(canvas_);
}

bool LvglSdfFaceRenderer::render(const Scene& scene)
{
    if (!ready() || draw_buffer_->data == nullptr || draw_buffer_->header.stride < kDesignWidth * sizeof(uint32_t))
        return false;
    Argb8888Target target{};
    target.pixels        = static_cast<uint32_t*>(draw_buffer_->data);
    target.width         = kDesignWidth;
    target.height        = kDesignHeight;
    target.stride_pixels = static_cast<uint16_t>(draw_buffer_->header.stride / sizeof(uint32_t));
    if (!sdf::render(scene, target)) return false;
    lv_obj_invalidate(canvas_);
    return true;
}

}  // namespace pet::sdf
