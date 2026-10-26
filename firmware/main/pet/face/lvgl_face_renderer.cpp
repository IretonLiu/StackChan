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

void setHidden(lv_obj_t* object, bool hidden)
{
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace

LvglFaceRenderer::LvglFaceRenderer(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, coordinate(kDesignWidth), coordinate(kDesignHeight));
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    styleShape(root_);

    for (size_t index = 0; index < eye_lines_.size(); ++index) {
        eye_lines_[index] = lv_line_create(root_);
        lv_obj_set_style_line_rounded(eye_lines_[index], true, LV_PART_MAIN);
        lv_line_set_points_mutable(eye_lines_[index], eye_points_[index].data(), eye_points_[index].size());

        eye_shapes_[index] = lv_obj_create(root_);
        styleShape(eye_shapes_[index]);
        lv_obj_set_style_radius(eye_shapes_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
    for (size_t index = 0; index < mouth_lines_.size(); ++index) {
        mouth_lines_[index] = lv_line_create(root_);
        lv_obj_set_style_line_rounded(mouth_lines_[index], true, LV_PART_MAIN);
        lv_line_set_points_mutable(mouth_lines_[index], mouth_points_[index].data(), mouth_points_[index].size());

        mouth_shapes_[index] = lv_obj_create(root_);
        styleShape(mouth_shapes_[index]);
        lv_obj_set_style_radius(mouth_shapes_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
    for (lv_obj_t*& shape : accent_shapes_) {
        shape = lv_obj_create(root_);
        styleShape(shape);
        lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
    accent_line_ = lv_line_create(root_);
    lv_obj_set_style_line_rounded(accent_line_, true, LV_PART_MAIN);
    lv_line_set_points_mutable(accent_line_, accent_points_.data(), accent_points_.size());
}

LvglFaceRenderer::~LvglFaceRenderer()
{
    if (root_) {
        lv_obj_delete(root_);
    }
}

void LvglFaceRenderer::renderEyeGlyph(EyeGlyph glyph, const EyePose& eye, Color feature, float alpha, size_t slot)
{
    lv_obj_t* line = eye_lines_[slot];
    lv_obj_t* shape = eye_shapes_[slot];
    if (alpha <= 0.01f) {
        setHidden(line, true);
        setHidden(shape, true);
        return;
    }

    if (eye.openness < 0.08f) {
        glyph = EyeGlyph::Stroke;
    }
    const float width = eye.size.x;
    const float height = std::max(3.0f, eye.size.y * std::max(0.12f, eye.openness));
    const float left = eye.center.x - width * 0.5f;
    const float right = eye.center.x + width * 0.5f;
    const float top = eye.center.y - height * 0.5f;
    const float bottom = eye.center.y + height * 0.5f;
    const lv_opa_t alpha_opa = opacity(alpha);

    if (glyph == EyeGlyph::Circle || glyph == EyeGlyph::Dot) {
        setHidden(line, true);
        setHidden(shape, false);
        lv_obj_set_size(shape, coordinate(width), coordinate(height));
        lv_obj_set_pos(shape, coordinate(left), coordinate(top));
        lv_obj_set_style_opa(shape, alpha_opa, LV_PART_MAIN);
        lv_obj_set_style_border_color(shape, toLvColor(feature), LV_PART_MAIN);
        lv_obj_set_style_bg_color(shape, toLvColor(feature), LV_PART_MAIN);
        if (glyph == EyeGlyph::Circle) {
            lv_obj_set_style_bg_opa(shape, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(shape, coordinate(std::max(3.0f, height * 0.28f)), LV_PART_MAIN);
        } else {
            const float dot = width * 0.36f;
            lv_obj_set_size(shape, coordinate(dot), coordinate(dot));
            lv_obj_set_pos(shape, coordinate(eye.center.x - dot * 0.5f), coordinate(eye.center.y - dot * 0.5f));
            lv_obj_set_style_border_width(shape, 0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, LV_PART_MAIN);
        }
        return;
    }

    setHidden(shape, true);
    setHidden(line, false);
    auto& points = eye_points_[slot];
    uint32_t count = 2;
    switch (glyph) {
        case EyeGlyph::Stroke:
            points[0] = {coordinate(left), coordinate(eye.center.y)};
            points[1] = {coordinate(right), coordinate(eye.center.y)};
            break;
        case EyeGlyph::Arc:
            points[0] = {coordinate(left), coordinate(top)};
            points[1] = {coordinate(left + width * 0.25f), coordinate(top + height * 0.75f)};
            points[2] = {coordinate(eye.center.x), coordinate(bottom)};
            points[3] = {coordinate(right - width * 0.25f), coordinate(top + height * 0.75f)};
            points[4] = {coordinate(right), coordinate(top)};
            count = 5;
            break;
        case EyeGlyph::Chevron:
            points[0] = {coordinate(left), coordinate(bottom)};
            points[1] = {coordinate(eye.center.x), coordinate(top)};
            points[2] = {coordinate(right), coordinate(bottom)};
            count = 3;
            break;
        case EyeGlyph::HalfLid:
            points[0] = {coordinate(left), coordinate(top)};
            points[1] = {coordinate(eye.center.x), coordinate(bottom)};
            points[2] = {coordinate(right), coordinate(top)};
            count = 3;
            break;
        case EyeGlyph::Circle:
        case EyeGlyph::Dot:
            break;
    }
    lv_obj_set_style_line_color(line, toLvColor(feature), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, coordinate(std::max(3.0f, height * 0.28f)), LV_PART_MAIN);
    lv_obj_set_style_line_opa(line, alpha_opa, LV_PART_MAIN);
    lv_line_set_points_mutable(line, points.data(), count);
}

void LvglFaceRenderer::renderMouthGlyph(MouthGlyph glyph, const MouthPose& mouth, Color feature, float alpha, size_t slot)
{
    lv_obj_t* line = mouth_lines_[slot];
    lv_obj_t* shape = mouth_shapes_[slot];
    if (alpha <= 0.01f) {
        setHidden(line, true);
        setHidden(shape, true);
        return;
    }

    const lv_opa_t alpha_opa = opacity(alpha);
    if (glyph == MouthGlyph::OpenOval) {
        setHidden(line, true);
        setHidden(shape, false);
        const float width = mouth.width * 0.64f;
        const float height = 6.0f + mouth.openness * 32.0f;
        lv_obj_set_size(shape, coordinate(width), coordinate(height));
        lv_obj_set_pos(shape, coordinate(mouth.center.x - width * 0.5f), coordinate(mouth.center.y - height * 0.5f));
        lv_obj_set_style_bg_opa(shape, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(shape, coordinate(mouth.thickness), LV_PART_MAIN);
        lv_obj_set_style_border_color(shape, toLvColor(feature), LV_PART_MAIN);
        lv_obj_set_style_opa(shape, alpha_opa, LV_PART_MAIN);
        return;
    }

    setHidden(shape, true);
    setHidden(line, false);
    auto& points = mouth_points_[slot];
    const float left = mouth.center.x - mouth.width * 0.5f;
    const float right = mouth.center.x + mouth.width * 0.5f;
    uint32_t count = 3;
    switch (glyph) {
        case MouthGlyph::Curve:
        case MouthGlyph::TinyArc:
            points[0] = {coordinate(left), coordinate(mouth.center.y - mouth.curve_depth)};
            points[1] = {coordinate(mouth.center.x), coordinate(mouth.center.y + mouth.curve_depth)};
            points[2] = {coordinate(right), coordinate(mouth.center.y - mouth.curve_depth)};
            break;
        case MouthGlyph::Three:
            points[0] = {coordinate(left), coordinate(mouth.center.y - 9.0f)};
            points[1] = {coordinate(mouth.center.x + 8.0f), coordinate(mouth.center.y - 7.0f)};
            points[2] = {coordinate(mouth.center.x), coordinate(mouth.center.y)};
            points[3] = {coordinate(mouth.center.x + 8.0f), coordinate(mouth.center.y + 7.0f)};
            points[4] = {coordinate(left), coordinate(mouth.center.y + 9.0f)};
            count = 5;
            break;
        case MouthGlyph::ZigZag:
            points[0] = {coordinate(left), coordinate(mouth.center.y)};
            points[1] = {coordinate(left + mouth.width * 0.25f), coordinate(mouth.center.y - 7.0f)};
            points[2] = {coordinate(mouth.center.x), coordinate(mouth.center.y + 7.0f)};
            points[3] = {coordinate(left + mouth.width * 0.75f), coordinate(mouth.center.y - 7.0f)};
            points[4] = {coordinate(right), coordinate(mouth.center.y)};
            count = 5;
            break;
        case MouthGlyph::OpenOval:
            break;
    }
    lv_obj_set_style_line_color(line, toLvColor(feature), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, coordinate(mouth.thickness), LV_PART_MAIN);
    lv_obj_set_style_line_opa(line, alpha_opa, LV_PART_MAIN);
    lv_line_set_points_mutable(line, points.data(), count);
}

void LvglFaceRenderer::renderAccent(const FacePose& pose)
{
    setHidden(accent_line_, true);
    for (lv_obj_t* shape : accent_shapes_) {
        setHidden(shape, true);
    }
    if (pose.accent_mark == AccentMark::None || pose.accent_intensity <= 0.01f) {
        return;
    }

    const lv_opa_t alpha = opacity(pose.accent_intensity);
    const lv_color_t color = toLvColor(pose.palette.accent);
    if (pose.accent_mark == AccentMark::Blush || pose.accent_mark == AccentMark::Tears || pose.accent_mark == AccentMark::Sweat) {
        const bool sweat = pose.accent_mark == AccentMark::Sweat;
        const float width = sweat ? 15.0f : (pose.accent_mark == AccentMark::Blush ? 24.0f : 15.0f);
        const float height = sweat ? 25.0f : (pose.accent_mark == AccentMark::Blush ? 5.0f : 22.0f);
        for (size_t index = 0; index < accent_shapes_.size(); ++index) {
            if (sweat && index == 1) {
                continue;
            }
            const float x = sweat ? 268.0f : (index == 0 ? 49.0f : 256.0f);
            const float y = sweat ? 76.0f : (pose.accent_mark == AccentMark::Blush ? 152.0f : 131.0f);
            lv_obj_t* shape = accent_shapes_[index];
            setHidden(shape, false);
            lv_obj_set_size(shape, coordinate(width), coordinate(height));
            lv_obj_set_pos(shape, coordinate(x), coordinate(y));
            lv_obj_set_style_opa(shape, alpha, LV_PART_MAIN);
            lv_obj_set_style_border_color(shape, color, LV_PART_MAIN);
            lv_obj_set_style_bg_color(shape, color, LV_PART_MAIN);
            if (pose.accent_mark == AccentMark::Blush) {
                lv_obj_set_style_border_width(shape, 0, LV_PART_MAIN);
                lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, LV_PART_MAIN);
            } else {
                lv_obj_set_style_border_width(shape, 3, LV_PART_MAIN);
                lv_obj_set_style_bg_opa(shape, LV_OPA_TRANSP, LV_PART_MAIN);
            }
        }
        return;
    }

    if (pose.accent_mark == AccentMark::Anger) {
        setHidden(accent_line_, false);
        accent_points_[0] = {270, 74};
        accent_points_[1] = {281, 65};
        accent_points_[2] = {278, 82};
        accent_points_[3] = {291, 80};
        lv_obj_set_style_line_color(accent_line_, color, LV_PART_MAIN);
        lv_obj_set_style_line_width(accent_line_, 4, LV_PART_MAIN);
        lv_obj_set_style_line_opa(accent_line_, alpha, LV_PART_MAIN);
        lv_line_set_points_mutable(accent_line_, accent_points_.data(), 4);
    }
}

void LvglFaceRenderer::render(const FacePose& pose)
{
    lv_obj_set_style_bg_color(root_, toLvColor(pose.palette.background), LV_PART_MAIN);
    renderEyeGlyph(pose.left_eye.previous_glyph, pose.left_eye, pose.palette.feature, 1.0f - pose.left_eye.glyph_blend, 0);
    renderEyeGlyph(pose.left_eye.glyph, pose.left_eye, pose.palette.feature, pose.left_eye.glyph_blend, 1);
    renderEyeGlyph(pose.right_eye.previous_glyph, pose.right_eye, pose.palette.feature, 1.0f - pose.right_eye.glyph_blend, 2);
    renderEyeGlyph(pose.right_eye.glyph, pose.right_eye, pose.palette.feature, pose.right_eye.glyph_blend, 3);
    renderMouthGlyph(pose.mouth.previous_glyph, pose.mouth, pose.palette.feature, 1.0f - pose.mouth.glyph_blend, 0);
    renderMouthGlyph(pose.mouth.glyph, pose.mouth, pose.palette.feature, pose.mouth.glyph_blend, 1);
    renderAccent(pose);
}

lv_color_t LvglFaceRenderer::toLvColor(Color color)
{
    return lv_color_make(color.red, color.green, color.blue);
}

}  // namespace pet::face
