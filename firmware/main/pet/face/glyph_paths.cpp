/*
 * SPDX-License-Identifier: MIT
 */
#include "glyph_paths.h"

#include <algorithm>
#include <cmath>

namespace pet::face {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kCircleControl = 0.5522847498307936f;
constexpr Vec2 kFaceCenter{160.0f, 120.0f};

float opacity(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

Vec2 rotate(Vec2 point, float degrees, Vec2 center)
{
    if (degrees == 0.0f) {
        return point;
    }
    const float radians = degrees * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    return {center.x + x * cosine - y * sine, center.y + x * sine + y * cosine};
}

void transform(FacePath& path, float local_rotation, Vec2 local_center, float face_rotation)
{
    for (uint8_t index = 0; index < path.command_count; ++index) {
        PathCommand& command = path.commands[index];
        if (command.type == PathCommandType::Close) {
            continue;
        }
        command.first = rotate(rotate(command.first, local_rotation, local_center), face_rotation, kFaceCenter);
        command.second = rotate(rotate(command.second, local_rotation, local_center), face_rotation, kFaceCenter);
        command.end = rotate(rotate(command.end, local_rotation, local_center), face_rotation, kFaceCenter);
    }
}

class PathBuilder {
public:
    explicit PathBuilder(PathStyle style) : path_{.style = style} {}

    void moveTo(Vec2 point) { append({PathCommandType::Move, point, {}, {}}); }
    void lineTo(Vec2 point) { append({PathCommandType::Line, point, {}, {}}); }
    void cubicTo(Vec2 first, Vec2 second, Vec2 end) { append({PathCommandType::Cubic, first, second, end}); }
    void close() { append({PathCommandType::Close, {}, {}, {}}); }

    void ellipse(Vec2 center, float radius_x, float radius_y)
    {
        const float control_x = radius_x * kCircleControl;
        const float control_y = radius_y * kCircleControl;
        moveTo({center.x + radius_x, center.y});
        cubicTo({center.x + radius_x, center.y + control_y}, {center.x + control_x, center.y + radius_y},
                {center.x, center.y + radius_y});
        cubicTo({center.x - control_x, center.y + radius_y}, {center.x - radius_x, center.y + control_y},
                {center.x - radius_x, center.y});
        cubicTo({center.x - radius_x, center.y - control_y}, {center.x - control_x, center.y - radius_y},
                {center.x, center.y - radius_y});
        cubicTo({center.x + control_x, center.y - radius_y}, {center.x + radius_x, center.y - control_y},
                {center.x + radius_x, center.y});
        close();
    }

    void roundedRect(float left, float top, float width, float height, float radius)
    {
        const float right = left + width;
        const float bottom = top + height;
        const float control = radius * kCircleControl;
        moveTo({left + radius, top});
        lineTo({right - radius, top});
        cubicTo({right - radius + control, top}, {right, top + radius - control}, {right, top + radius});
        lineTo({right, bottom - radius});
        cubicTo({right, bottom - radius + control}, {right - radius + control, bottom}, {right - radius, bottom});
        lineTo({left + radius, bottom});
        cubicTo({left + radius - control, bottom}, {left, bottom - radius + control}, {left, bottom - radius});
        lineTo({left, top + radius});
        cubicTo({left, top + radius - control}, {left + radius - control, top}, {left + radius, top});
        close();
    }

    FacePath finish(float local_rotation, Vec2 local_center, float face_rotation)
    {
        transform(path_, local_rotation, local_center, face_rotation);
        return path_;
    }

private:
    void append(PathCommand command)
    {
        if (path_.command_count < path_.commands.size()) {
            path_.commands[path_.command_count++] = command;
        }
    }

    FacePath path_;
};

void append(FaceDrawing& drawing, FacePath path)
{
    if (path.command_count > 0 && drawing.path_count < drawing.paths.size()) {
        drawing.paths[drawing.path_count++] = path;
    }
}

PathStyle stroke(Color color, float width, float alpha)
{
    return {.color = color, .opacity = opacity(alpha), .stroke_width = width, .fill = false, .stroke = true};
}

PathStyle fill(Color color, float alpha)
{
    return {.color = color, .opacity = opacity(alpha), .stroke_width = 0.0f, .fill = true, .stroke = false};
}

void appendEye(FaceDrawing& drawing, EyeGlyph glyph, const EyePose& eye, Color color, float alpha, float face_rotation)
{
    if (alpha <= 0.01f) {
        return;
    }
    const float width = eye.size.x;
    const float height = std::max(3.0f, eye.size.y * std::max(0.12f, eye.openness));
    const float left = eye.center.x - width * 0.5f;
    const float right = eye.center.x + width * 0.5f;
    const float top = eye.center.y - height * 0.5f;
    const float bottom = eye.center.y + height * 0.5f;
    const float line_width = std::max(3.0f, std::min(8.0f, height * 0.32f));

    if (eye.openness < 0.08f || glyph == EyeGlyph::Stroke) {
        PathBuilder path(stroke(color, line_width, alpha));
        path.moveTo({left, eye.center.y});
        path.lineTo({right, eye.center.y});
        append(drawing, path.finish(eye.rotation_degrees, eye.center, face_rotation));
    } else if (glyph == EyeGlyph::Circle || glyph == EyeGlyph::Dot) {
        PathBuilder path(fill(color, alpha));
        const float radius_x = glyph == EyeGlyph::Dot ? width * 0.18f : width * 0.5f;
        const float radius_y = glyph == EyeGlyph::Dot ? width * 0.18f : height * 0.5f;
        path.ellipse(eye.center, radius_x, radius_y);
        append(drawing, path.finish(eye.rotation_degrees, eye.center, face_rotation));
    } else if (glyph == EyeGlyph::Chevron) {
        PathBuilder path(fill(color, alpha));
        path.moveTo({left, bottom});
        path.lineTo({eye.center.x, top});
        path.lineTo({right, bottom});
        path.close();
        append(drawing, path.finish(eye.rotation_degrees, eye.center, face_rotation));
    } else if (glyph == EyeGlyph::HalfLid) {
        PathBuilder path(fill(color, alpha));
        path.moveTo({left, top});
        path.cubicTo({left + width * 0.25f, bottom}, {right - width * 0.25f, bottom}, {right, top});
        path.lineTo({right, top + line_width});
        path.cubicTo({right - width * 0.25f, bottom + line_width}, {left + width * 0.25f, bottom + line_width},
                     {left, top + line_width});
        path.close();
        append(drawing, path.finish(eye.rotation_degrees, eye.center, face_rotation));
    } else {
        PathBuilder path(stroke(color, line_width, alpha));
        path.moveTo({left, top});
        path.cubicTo({left + width * 0.20f, bottom}, {right - width * 0.20f, bottom}, {right, top});
        append(drawing, path.finish(eye.rotation_degrees, eye.center, face_rotation));
    }
}

void appendMouth(FaceDrawing& drawing, MouthGlyph glyph, const MouthPose& mouth, Color color, float alpha, float face_rotation)
{
    if (alpha <= 0.01f) {
        return;
    }
    const float left = mouth.center.x - mouth.width * 0.5f;
    const float right = mouth.center.x + mouth.width * 0.5f;
    const float upper = mouth.center.y - mouth.curve_depth;
    const float lower = mouth.center.y + mouth.curve_depth;
    PathBuilder path(stroke(color, mouth.thickness, alpha));

    switch (glyph) {
        case MouthGlyph::Curve:
        case MouthGlyph::TinyArc:
            path.moveTo({left, upper});
            path.cubicTo({left + mouth.width * 0.22f, lower}, {right - mouth.width * 0.22f, lower}, {right, upper});
            break;
        case MouthGlyph::OpenOval:
            path.ellipse(mouth.center, mouth.width * 0.32f, 4.0f + mouth.openness * 16.0f);
            break;
        case MouthGlyph::Three:
            path.moveTo({left, mouth.center.y - 9.0f});
            path.cubicTo({mouth.center.x + 15.0f, mouth.center.y - 12.0f},
                         {mouth.center.x + 15.0f, mouth.center.y - 2.0f}, mouth.center);
            path.cubicTo({mouth.center.x + 15.0f, mouth.center.y + 2.0f},
                         {mouth.center.x + 15.0f, mouth.center.y + 12.0f}, {left, mouth.center.y + 9.0f});
            break;
        case MouthGlyph::ZigZag:
            path.moveTo({left, mouth.center.y});
            path.lineTo({left + mouth.width * 0.25f, mouth.center.y - 7.0f});
            path.lineTo({mouth.center.x, mouth.center.y + 7.0f});
            path.lineTo({left + mouth.width * 0.75f, mouth.center.y - 7.0f});
            path.lineTo({right, mouth.center.y});
            break;
        case MouthGlyph::W:
            // Matches the user-authored R1 C1 base SVG. The intentionally
            // asymmetric handles keep the compact hand-drawn kaomoji feel.
            path.moveTo({left, mouth.center.y});
            path.cubicTo({left + mouth.width * 0.25f, mouth.center.y + 14.0f},
                         {mouth.center.x - mouth.width * 0.125f, mouth.center.y + 12.0f}, mouth.center);
            path.cubicTo({mouth.center.x + mouth.width * 0.2083333f, mouth.center.y + 5.0f},
                         {right - mouth.width * 0.25f, mouth.center.y + 14.0f}, {right, mouth.center.y});
            break;
    }
    append(drawing, path.finish(0.0f, mouth.center, face_rotation));
}

void appendAccent(FaceDrawing& drawing, const FacePose& pose)
{
    const float alpha = opacity(pose.accent_intensity);
    if (pose.accent_mark == AccentMark::None || alpha <= 0.01f) {
        return;
    }

    if (pose.accent_mark == AccentMark::Blush) {
        PathBuilder left(fill(pose.palette.accent, alpha));
        left.roundedRect(39.0f, 151.0f, 25.0f, 6.0f, 3.0f);
        append(drawing, left.finish(0.0f, kFaceCenter, pose.visual_tilt));
        PathBuilder right(fill(pose.palette.accent, alpha));
        right.roundedRect(256.0f, 151.0f, 25.0f, 6.0f, 3.0f);
        append(drawing, right.finish(0.0f, kFaceCenter, pose.visual_tilt));
    } else if (pose.accent_mark == AccentMark::Anger) {
        PathBuilder path(stroke(pose.palette.accent, 4.0f, alpha));
        path.moveTo({270.0f, 74.0f});
        path.lineTo({281.0f, 65.0f});
        path.moveTo({278.0f, 82.0f});
        path.lineTo({291.0f, 80.0f});
        path.moveTo({274.0f, 91.0f});
        path.lineTo({282.0f, 101.0f});
        append(drawing, path.finish(0.0f, kFaceCenter, pose.visual_tilt));
    } else {
        PathBuilder path(fill(pose.palette.accent, alpha));
        const float x = pose.accent_mark == AccentMark::Sweat ? 276.0f : 57.0f;
        const float y = pose.accent_mark == AccentMark::Sweat ? 76.0f : 130.0f;
        path.moveTo({x, y});
        path.cubicTo({x - 17.0f, y + 22.0f}, {x - 11.0f, y + 31.0f}, {x, y + 31.0f});
        path.cubicTo({x + 11.0f, y + 31.0f}, {x + 17.0f, y + 22.0f}, {x, y});
        path.close();
        if (pose.accent_mark == AccentMark::Tears) {
            path.moveTo({263.0f, y});
            path.cubicTo({246.0f, y + 22.0f}, {252.0f, y + 31.0f}, {263.0f, y + 31.0f});
            path.cubicTo({274.0f, y + 31.0f}, {280.0f, y + 22.0f}, {263.0f, y});
            path.close();
        }
        append(drawing, path.finish(0.0f, kFaceCenter, pose.visual_tilt));
    }
}

}  // namespace

FaceDrawing buildFaceDrawing(const FacePose& pose)
{
    FaceDrawing drawing{.background = pose.palette.background};
    appendEye(drawing, pose.left_eye.previous_glyph, pose.left_eye, pose.palette.feature,
              1.0f - pose.left_eye.glyph_blend, pose.visual_tilt);
    appendEye(drawing, pose.left_eye.glyph, pose.left_eye, pose.palette.feature, pose.left_eye.glyph_blend,
              pose.visual_tilt);
    appendEye(drawing, pose.right_eye.previous_glyph, pose.right_eye, pose.palette.feature,
              1.0f - pose.right_eye.glyph_blend, pose.visual_tilt);
    appendEye(drawing, pose.right_eye.glyph, pose.right_eye, pose.palette.feature, pose.right_eye.glyph_blend,
              pose.visual_tilt);
    appendMouth(drawing, pose.mouth.previous_glyph, pose.mouth, pose.palette.feature, 1.0f - pose.mouth.glyph_blend,
                pose.visual_tilt);
    appendMouth(drawing, pose.mouth.glyph, pose.mouth, pose.palette.feature, pose.mouth.glyph_blend,
                pose.visual_tilt);
    appendAccent(drawing, pose);
    return drawing;
}

}  // namespace pet::face
