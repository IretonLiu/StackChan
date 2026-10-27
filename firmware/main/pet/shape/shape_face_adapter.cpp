/*
 * SPDX-License-Identifier: MIT
 */
#include "shape_face_adapter.h"

#include <cmath>

namespace pet::shape {
namespace {

bool finite(Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool finite(const PathStyle& value)
{
    return std::isfinite(value.opacity) && std::isfinite(value.stroke_width);
}

face::Color toFaceColor(Color source)
{
    return {source.red, source.green, source.blue};
}

bool toFaceCommandType(PathCommandType source, face::PathCommandType& destination)
{
    switch (source) {
        case PathCommandType::Move:
            destination = face::PathCommandType::Move;
            return true;
        case PathCommandType::Cubic:
            destination = face::PathCommandType::Cubic;
            return true;
        case PathCommandType::Close:
            destination = face::PathCommandType::Close;
            return true;
    }

    return false;
}

}  // namespace

bool toFacePath(const ShapePath& source, face::FacePath& destination)
{
    if (source.command_count > source.commands.size() || source.command_count > destination.commands.size() ||
        !finite(source.style)) {
        return false;
    }

    face::FacePath converted{};
    converted.style = {
        toFaceColor(source.style.color),
        source.style.opacity,
        source.style.stroke_width,
        source.style.fill,
        source.style.stroke,
    };
    converted.command_count = source.command_count;
    for (uint8_t index = 0; index < source.command_count; ++index) {
        const PathCommand& command = source.commands[index];
        if (!finite(command.first) || !finite(command.second) || !finite(command.end)) {
            return false;
        }
        face::PathCommandType command_type;
        if (!toFaceCommandType(command.type, command_type)) {
            return false;
        }
        converted.commands[index] = {
            command_type,
            {command.first.x, command.first.y},
            {command.second.x, command.second.y},
            {command.end.x, command.end.y},
        };
    }

    destination = converted;
    return true;
}

bool appendToFaceDrawing(const ShapePath& source, face::FaceDrawing& drawing)
{
    if (drawing.path_count >= drawing.paths.size()) {
        return false;
    }

    face::FacePath converted;
    if (!toFacePath(source, converted)) {
        return false;
    }

    drawing.paths[drawing.path_count++] = converted;
    return true;
}

}  // namespace pet::shape
