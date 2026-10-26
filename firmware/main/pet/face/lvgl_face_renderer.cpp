/*
 * SPDX-License-Identifier: MIT
 */
#include "lvgl_face_renderer.h"

#include <algorithm>
#include <cmath>

namespace pet::face {
namespace {

lv_coord_t coordinate(float value)
{
    return static_cast<lv_coord_t>(std::lround(value));
}

lv_opa_t opacity(float value)
{
    return static_cast<lv_opa_t>(
        std::lround(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(LV_OPA_COVER)));
}

void styleShape(lv_obj_t* object)
{
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

}  // namespace

LvglFaceRenderer::LvglFaceRenderer(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, coordinate(kDesignWidth), coordinate(kDesignHeight));
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    styleShape(root_);

    for (size_t index = 0; index < eyes_.size(); ++index) {
        eyes_[index] = lv_obj_create(root_);
        styleShape(eyes_[index]);
        lv_obj_set_style_radius(eyes_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_clip_corner(eyes_[index], true, LV_PART_MAIN);

        pupils_[index] = lv_obj_create(eyes_[index]);
        styleShape(pupils_[index]);
        lv_obj_set_style_radius(pupils_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);

        lids_[index] = lv_obj_create(eyes_[index]);
        styleShape(lids_[index]);

        blushes_[index] = lv_obj_create(root_);
        styleShape(blushes_[index]);
        lv_obj_set_style_radius(blushes_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }

    open_mouth_ = lv_obj_create(root_);
    styleShape(open_mouth_);
    lv_obj_set_style_radius(open_mouth_, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    mouth_ = lv_line_create(root_);
    lv_obj_set_style_line_rounded(mouth_, true, LV_PART_MAIN);
    lv_line_set_points_mutable(mouth_, mouth_points_.data(), mouth_points_.size());
}

LvglFaceRenderer::~LvglFaceRenderer()
{
    if (root_) {
        lv_obj_delete(root_);
    }
}

void LvglFaceRenderer::renderEye(const EyePose& eye, lv_obj_t* eye_object, lv_obj_t* pupil_object, lv_obj_t* lid_object,
                                  const FacePalette& palette)
{
    const lv_coord_t width  = coordinate(eye.size.x);
    const lv_coord_t height = coordinate(eye.size.y);
    lv_obj_set_size(eye_object, width, height);
    lv_obj_set_pos(eye_object, coordinate(eye.center.x - eye.size.x * 0.5f), coordinate(eye.center.y - eye.size.y * 0.5f));
    lv_obj_set_style_bg_color(eye_object, toLvColor(palette.feature), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(eye_object, static_cast<int32_t>(std::lround(eye.rotation_degrees * 10.0f)),
                                        LV_PART_MAIN);

    const float pupil_diameter = std::min(eye.size.x, eye.size.y) * eye.pupil_scale;
    const float pupil_x = eye.size.x * 0.5f - pupil_diameter * 0.5f + eye.pupil_offset.x * eye.size.x * 0.22f;
    const float pupil_y = eye.size.y * 0.5f - pupil_diameter * 0.5f + eye.pupil_offset.y * eye.size.y * 0.22f;
    lv_obj_set_size(pupil_object, coordinate(pupil_diameter), coordinate(pupil_diameter));
    lv_obj_set_pos(pupil_object, coordinate(pupil_x), coordinate(pupil_y));
    lv_obj_set_style_bg_color(pupil_object, toLvColor(palette.pupil), LV_PART_MAIN);

    const float lid_height = eye.size.y * (1.0f - eye.lid_open);
    lv_obj_set_size(lid_object, width, coordinate(lid_height));
    lv_obj_set_pos(lid_object, 0, 0);
    lv_obj_set_style_bg_color(lid_object, toLvColor(palette.background), LV_PART_MAIN);
}

void LvglFaceRenderer::render(const FacePose& pose)
{
    lv_obj_set_style_bg_color(root_, toLvColor(pose.palette.background), LV_PART_MAIN);
    renderEye(pose.left_eye, eyes_[0], pupils_[0], lids_[0], pose.palette);
    renderEye(pose.right_eye, eyes_[1], pupils_[1], lids_[1], pose.palette);

    const float blush_width = 54.0f;
    const float blush_height = 22.0f;
    const float blush_y = 159.0f + pose.visual_tilt * 0.2f;
    for (size_t index = 0; index < blushes_.size(); ++index) {
        const float blush_x = index == 0 ? 21.0f : kDesignWidth - 75.0f;
        lv_obj_set_size(blushes_[index], coordinate(blush_width), coordinate(blush_height));
        lv_obj_set_pos(blushes_[index], coordinate(blush_x), coordinate(blush_y));
        lv_obj_set_style_bg_color(blushes_[index], toLvColor(pose.palette.blush), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blushes_[index], opacity(pose.blush_intensity), LV_PART_MAIN);
    }

    const float left_x = pose.mouth.center.x - pose.mouth.width * 0.5f;
    for (size_t index = 0; index < mouth_points_.size(); ++index) {
        const float progress = static_cast<float>(index) / static_cast<float>(mouth_points_.size() - 1);
        const float x = left_x + pose.mouth.width * progress;
        const float centered = progress * 2.0f - 1.0f;
        const float y = pose.mouth.center.y - pose.mouth.curve_depth +
                        2.0f * pose.mouth.curve_depth * (1.0f - centered * centered) + pose.visual_tilt * 0.25f;
        mouth_points_[index] = {coordinate(x), coordinate(y)};
    }
    lv_obj_set_style_line_color(mouth_, toLvColor(pose.palette.feature), LV_PART_MAIN);
    lv_obj_set_style_line_width(mouth_, coordinate(pose.mouth.thickness), LV_PART_MAIN);
    lv_line_set_points_mutable(mouth_, mouth_points_.data(), mouth_points_.size());

    const float open_height = pose.mouth.openness * 34.0f;
    lv_obj_set_size(open_mouth_, coordinate(pose.mouth.width * 0.54f), coordinate(open_height));
    lv_obj_set_pos(open_mouth_, coordinate(pose.mouth.center.x - pose.mouth.width * 0.27f),
                   coordinate(pose.mouth.center.y + pose.mouth.openness * 7.0f));
    lv_obj_set_style_bg_color(open_mouth_, toLvColor(pose.palette.feature), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(open_mouth_, opacity(pose.mouth.openness), LV_PART_MAIN);
}

lv_color_t LvglFaceRenderer::toLvColor(Color color)
{
    return lv_color_make(color.red, color.green, color.blue);
}

}  // namespace pet::face
