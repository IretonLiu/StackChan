/*
 * SPDX-License-Identifier: MIT
 */
#include "face_svg_renderer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace pet::face::test {
namespace {

std::string color(Color color)
{
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(color.red) << std::setw(2)
           << static_cast<int>(color.green) << std::setw(2) << static_cast<int>(color.blue);
    return stream.str();
}

void renderEyeGlyph(std::ostringstream& svg, EyeGlyph glyph, const EyePose& eye, Color feature, float opacity)
{
    if (opacity <= 0.01f) {
        return;
    }
    const float width = eye.size.x;
    const float height = std::max(3.0f, eye.size.y * std::max(0.12f, eye.openness));
    const float left = eye.center.x - width * 0.5f;
    const float right = eye.center.x + width * 0.5f;
    const float top = eye.center.y - height * 0.5f;
    const float bottom = eye.center.y + height * 0.5f;
    const float stroke = std::max(3.0f, std::min(6.0f, height * 0.28f));

    if (eye.openness < 0.08f) {
        glyph = EyeGlyph::Stroke;
    }
    svg << "<g opacity=\"" << opacity << "\">";
    switch (glyph) {
        case EyeGlyph::Stroke:
            svg << "<path d=\"M " << left << ' ' << eye.center.y << " L " << right << ' ' << eye.center.y;
            break;
        case EyeGlyph::Arc:
            svg << "<path d=\"M " << left << ' ' << top << " Q " << eye.center.x << ' ' << bottom << ' ' << right
                << ' ' << top;
            break;
        case EyeGlyph::Circle:
            svg << "<ellipse cx=\"" << eye.center.x << "\" cy=\"" << eye.center.y << "\" rx=\"" << width * 0.5f
                << "\" ry=\"" << height * 0.5f << "\" fill=\"none\" stroke=\"" << color(feature)
                << "\" stroke-width=\"" << stroke << "\"/>";
            svg << "</g>";
            return;
        case EyeGlyph::Chevron:
            svg << "<path d=\"M " << left << ' ' << bottom << " L " << eye.center.x << ' ' << top << " L " << right
                << ' ' << bottom;
            break;
        case EyeGlyph::HalfLid:
            svg << "<path d=\"M " << left << ' ' << top << " Q " << eye.center.x << ' ' << bottom << ' ' << right
                << ' ' << top;
            break;
        case EyeGlyph::Dot:
            svg << "<circle cx=\"" << eye.center.x << "\" cy=\"" << eye.center.y << "\" r=\"" << width * 0.18f
                << "\" fill=\"" << color(feature) << "\"/>";
            svg << "</g>";
            return;
    }
    svg << "\" fill=\"none\" stroke=\"" << color(feature) << "\" stroke-width=\"" << stroke
        << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
    svg << "</g>";
}

void renderEye(std::ostringstream& svg, const EyePose& eye, Color feature)
{
    svg << "<g transform=\"rotate(" << eye.rotation_degrees << ' ' << eye.center.x << ' ' << eye.center.y << ")\">";
    renderEyeGlyph(svg, eye.previous_glyph, eye, feature, 1.0f - eye.glyph_blend);
    renderEyeGlyph(svg, eye.glyph, eye, feature, eye.glyph_blend);
    svg << "</g>\n";
}

void renderMouthGlyph(std::ostringstream& svg, MouthGlyph glyph, const MouthPose& mouth, Color feature, float opacity)
{
    if (opacity <= 0.01f) {
        return;
    }
    const float left = mouth.center.x - mouth.width * 0.5f;
    const float right = mouth.center.x + mouth.width * 0.5f;
    const float upper = mouth.center.y - mouth.curve_depth;
    const float lower = mouth.center.y + mouth.curve_depth;
    svg << "<g opacity=\"" << opacity << "\" fill=\"none\" stroke=\"" << color(feature) << "\" stroke-width=\""
        << mouth.thickness << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    switch (glyph) {
        case MouthGlyph::Curve:
        case MouthGlyph::TinyArc:
            svg << "<path d=\"M " << left << ' ' << upper << " Q " << mouth.center.x << ' ' << lower << ' ' << right
                << ' ' << upper << "\"/>";
            break;
        case MouthGlyph::OpenOval:
            svg << "<ellipse cx=\"" << mouth.center.x << "\" cy=\"" << mouth.center.y << "\" rx=\""
                << mouth.width * 0.32f << "\" ry=\"" << 4.0f + mouth.openness * 16.0f << "\"/>";
            break;
        case MouthGlyph::Three:
            svg << "<path d=\"M " << left << ' ' << mouth.center.y - 9.0f << " Q " << mouth.center.x + 8.0f << ' '
                << mouth.center.y - 8.0f << ' ' << mouth.center.x << ' ' << mouth.center.y << " Q "
                << mouth.center.x + 8.0f << ' ' << mouth.center.y + 8.0f << ' ' << left << ' '
                << mouth.center.y + 9.0f << "\"/>";
            break;
        case MouthGlyph::ZigZag:
            svg << "<path d=\"M " << left << ' ' << mouth.center.y << " L " << left + mouth.width * 0.25f << ' '
                << mouth.center.y - 7.0f << " L " << mouth.center.x << ' ' << mouth.center.y + 7.0f << " L "
                << left + mouth.width * 0.75f << ' ' << mouth.center.y - 7.0f << " L " << right << ' ' << mouth.center.y
                << "\"/>";
            break;
    }
    svg << "</g>\n";
}

void renderAccent(std::ostringstream& svg, const FacePose& pose)
{
    const float opacity = std::clamp(pose.accent_intensity, 0.0f, 1.0f);
    if (pose.accent_mark == AccentMark::None || opacity <= 0.01f) {
        return;
    }
    const std::string accent = color(pose.palette.accent);
    svg << "<g opacity=\"" << opacity << "\" fill=\"none\" stroke=\"" << accent
        << "\" stroke-width=\"4\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    switch (pose.accent_mark) {
        case AccentMark::Blush:
            svg << "<path d=\"M 38 153 L 58 153 M 262 153 L 282 153\"/>";
            break;
        case AccentMark::Tears:
            svg << "<path d=\"M 57 130 Q 46 147 57 153 Q 68 147 57 130 M 263 130 Q 252 147 263 153 Q 274 147 263 130\"/>";
            break;
        case AccentMark::Sweat:
            svg << "<path d=\"M 275 76 Q 263 94 275 102 Q 287 94 275 76\"/>";
            break;
        case AccentMark::Anger:
            svg << "<path d=\"M 270 75 L 279 66 M 279 84 L 292 82 M 274 91 L 282 101\"/>";
            break;
        case AccentMark::None:
            break;
    }
    svg << "</g>\n";
}

}  // namespace

std::string renderSvg(const FacePose& pose)
{
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << kDesignWidth << ' ' << kDesignHeight
        << "\" width=\"" << kDesignWidth << "\" height=\"" << kDesignHeight << "\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"" << color(pose.palette.background) << "\"/>\n";
    svg << "<g transform=\"rotate(" << pose.visual_tilt << " 160 120)\">\n";
    renderEye(svg, pose.left_eye, pose.palette.feature);
    renderEye(svg, pose.right_eye, pose.palette.feature);
    renderMouthGlyph(svg, pose.mouth.previous_glyph, pose.mouth, pose.palette.feature, 1.0f - pose.mouth.glyph_blend);
    renderMouthGlyph(svg, pose.mouth.glyph, pose.mouth, pose.palette.feature, pose.mouth.glyph_blend);
    renderAccent(svg, pose);
    svg << "</g>\n</svg>\n";
    return svg.str();
}

}  // namespace pet::face::test
