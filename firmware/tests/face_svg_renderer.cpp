/*
 * SPDX-License-Identifier: MIT
 */
#include "face_svg_renderer.h"

#include <algorithm>
#include <cmath>
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

void renderEye(std::ostringstream& svg, const EyePose& eye, const FacePalette& palette)
{
    const float pupil_radius = std::min(eye.size.x, eye.size.y) * eye.pupil_scale * 0.5f;
    const float pupil_x = eye.center.x + eye.pupil_offset.x * eye.size.x * 0.22f;
    const float pupil_y = eye.center.y + eye.pupil_offset.y * eye.size.y * 0.22f;
    const float lid_height = eye.size.y * (1.0f - eye.lid_open);

    svg << "<g transform=\"rotate(" << eye.rotation_degrees << ' ' << eye.center.x << ' ' << eye.center.y << ")\">\n";
    svg << "<ellipse cx=\"" << eye.center.x << "\" cy=\"" << eye.center.y << "\" rx=\"" << eye.size.x * 0.5f
        << "\" ry=\"" << eye.size.y * 0.5f << "\" fill=\"" << color(palette.feature) << "\"/>\n";
    svg << "<circle cx=\"" << pupil_x << "\" cy=\"" << pupil_y << "\" r=\"" << pupil_radius << "\" fill=\""
        << color(palette.pupil) << "\"/>\n";
    if (lid_height > 0.01f) {
        svg << "<rect x=\"" << eye.center.x - eye.size.x * 0.58f << "\" y=\"" << eye.center.y - eye.size.y * 0.6f
            << "\" width=\"" << eye.size.x * 1.16f << "\" height=\"" << lid_height << "\" fill=\""
            << color(palette.background) << "\"/>\n";
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

    const float blush_opacity = std::clamp(pose.blush_intensity, 0.0f, 1.0f);
    if (blush_opacity > 0.01f) {
        svg << "<ellipse cx=\"48\" cy=\"159\" rx=\"27\" ry=\"11\" fill=\"" << color(pose.palette.blush)
            << "\" fill-opacity=\"" << blush_opacity << "\"/>\n";
        svg << "<ellipse cx=\"272\" cy=\"159\" rx=\"27\" ry=\"11\" fill=\"" << color(pose.palette.blush)
            << "\" fill-opacity=\"" << blush_opacity << "\"/>\n";
    }

    renderEye(svg, pose.left_eye, pose.palette);
    renderEye(svg, pose.right_eye, pose.palette);

    const float left_x = pose.mouth.center.x - pose.mouth.width * 0.5f;
    const float right_x = pose.mouth.center.x + pose.mouth.width * 0.5f;
    const float edge_y = pose.mouth.center.y - pose.mouth.curve_depth;
    if (pose.mouth.openness > 0.01f) {
        svg << "<ellipse cx=\"" << pose.mouth.center.x << "\" cy=\""
            << pose.mouth.center.y + pose.mouth.openness * 11.0f << "\" rx=\"" << pose.mouth.width * 0.27f
            << "\" ry=\"" << 4.0f + pose.mouth.openness * 17.0f << "\" fill=\"" << color(pose.palette.feature)
            << "\"/>\n";
    }
    svg << "<path d=\"M " << left_x << ' ' << edge_y << " Q " << pose.mouth.center.x << ' '
        << pose.mouth.center.y + pose.mouth.curve_depth << ' ' << right_x << ' ' << edge_y << "\" fill=\"none\" stroke=\""
        << color(pose.palette.feature) << "\" stroke-width=\"" << pose.mouth.thickness
        << "\" stroke-linecap=\"round\"/>\n";
    svg << "</g>\n</svg>\n";
    return svg.str();
}

}  // namespace pet::face::test
