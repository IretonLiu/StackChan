/*
 * SPDX-License-Identifier: MIT
 */
#include "face_svg_renderer.h"

#include <pet/face/glyph_paths.h>

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

void renderPath(std::ostringstream& svg, const FacePath& path)
{
    svg << "<path d=\"";
    for (uint8_t index = 0; index < path.command_count; ++index) {
        const PathCommand& command = path.commands[index];
        switch (command.type) {
            case PathCommandType::Move:
                svg << "M " << command.first.x << ' ' << command.first.y << ' ';
                break;
            case PathCommandType::Line:
                svg << "L " << command.first.x << ' ' << command.first.y << ' ';
                break;
            case PathCommandType::Cubic:
                svg << "C " << command.first.x << ' ' << command.first.y << ' ' << command.second.x << ' '
                    << command.second.y << ' ' << command.end.x << ' ' << command.end.y << ' ';
                break;
            case PathCommandType::Close:
                svg << "Z ";
                break;
        }
    }
    svg << "\" fill=\"" << (path.style.fill ? color(path.style.color) : "none") << "\" stroke=\""
        << (path.style.stroke ? color(path.style.color) : "none") << "\" stroke-width=\"" << path.style.stroke_width
        << "\" opacity=\"" << path.style.opacity << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n";
}

}  // namespace

std::string renderSvg(const FacePose& pose)
{
    const FaceDrawing drawing = buildFaceDrawing(pose);
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << kDesignWidth << ' ' << kDesignHeight
        << "\" width=\"" << kDesignWidth << "\" height=\"" << kDesignHeight << "\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"" << color(drawing.background) << "\"/>\n";
    for (uint8_t index = 0; index < drawing.path_count; ++index) {
        renderPath(svg, drawing.paths[index]);
    }
    return svg.str() + "</svg>\n";
}

}  // namespace pet::face::test
