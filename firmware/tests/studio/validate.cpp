/* SPDX-License-Identifier: MIT */
#include "validate.h"

#include "geometry.h"

#include <cmath>

namespace kiwi::studio {
namespace {
using namespace pet::shape;
}  // namespace

bool validId(const char* text)
{
    if (text == nullptr || *text < 'a' || *text > 'z') return false;
    for (const char* character = text; *character != '\0'; ++character)
        if (!((*character >= 'a' && *character <= 'z') || (*character >= '0' && *character <= '9') ||
              *character == '-'))
            return false;
    return true;
}

bool validateScene(const SceneDoc& document, std::string& error)
{
    error.clear();
    if (!validId(document.id.c_str())) {
        error = "Scene id must be a lowercase kebab-case id";
        return false;
    }
    if (document.count == 0 || document.count > kMaxNodes) {
        error = "Scene must contain 1.." + std::to_string(kMaxNodes) + " nodes";
        return false;
    }
    for (size_t index = 0; index < document.count; ++index) {
        const Node& node        = document.nodes[index];
        const std::string where = "nodes[" + std::to_string(index) + "]";
        if (!validId(node.id.c_str())) {
            error = where + ": id must be a lowercase kebab-case id";
            return false;
        }
        for (size_t earlier = 0; earlier < index; ++earlier)
            if (document.nodes[earlier].id == node.id) {
                error = where + ": duplicates node id '" + node.id + "'";
                return false;
            }
        if (node.parent_index < -1 || node.parent_index >= static_cast<int>(index)) {
            error = where + ": parent must name an earlier node or be omitted";
            return false;
        }
        if (!std::isfinite(node.style.opacity) || node.style.opacity < 0.0f || node.style.opacity > 1.0f) {
            error = where + ".style.opacity: must be a finite number in [0, 1]";
            return false;
        }
        if (!std::isfinite(node.style.stroke_width) || node.style.stroke_width < 0.0f) {
            error = where + ".style.stroke_width: must be a finite non-negative number";
            return false;
        }
        if (!std::isfinite(node.transform.rotation_degrees) || !std::isfinite(node.transform.translation.x) ||
            !std::isfinite(node.transform.translation.y) || !std::isfinite(node.transform.scale.x) ||
            !std::isfinite(node.transform.scale.y) || !std::isfinite(node.transform.pivot.x) ||
            !std::isfinite(node.transform.pivot.y)) {
            error = where + ".transform: values must be finite";
            return false;
        }
        if (node.transform.scale.x == 0.0f || node.transform.scale.y == 0.0f) {
            error = where + ".transform.scale: components must be non-zero";
            return false;
        }
        if (node.kind == Kind::Circle) {
            if (!std::isfinite(node.circle.center.x) || !std::isfinite(node.circle.center.y) ||
                !std::isfinite(node.circle.radius) || node.circle.radius <= 0.0f) {
                error = where + ".circle: center and positive radius must be finite";
                return false;
            }
        } else {
            const uint8_t count = node.polygon.vertex_count;
            if (count < 3 || count > pet::shape::kMaxRoundedPolygonVertices) {
                error = where + ".rounded_polygon.vertices: must contain 3.." +
                        std::to_string(pet::shape::kMaxRoundedPolygonVertices) + " points";
                return false;
            }
            float first_turn = 0.0f;
            for (uint8_t vertex = 0; vertex < count; ++vertex) {
                const Vec2& point = node.polygon.vertices[vertex];
                if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                    error = where + ".rounded_polygon.vertices: points must be finite";
                    return false;
                }
                const float turn = cross(node.polygon.vertices[(vertex + count - 1) % count], point,
                                         node.polygon.vertices[(vertex + 1) % count]);
                if (std::fabs(turn) < .0001f) {
                    error = where + ".rounded_polygon.vertices: must be non-degenerate";
                    return false;
                }
                if (vertex == 0)
                    first_turn = turn;
                else if (first_turn * turn <= 0.0f) {
                    error = where + ".rounded_polygon.vertices: must be convex with consistent winding";
                    return false;
                }
                if (!std::isfinite(node.polygon.corners[vertex].roundness) ||
                    node.polygon.corners[vertex].roundness < 0.0f || node.polygon.corners[vertex].roundness > .35f ||
                    !std::isfinite(node.polygon.corners[vertex].cap_tension) ||
                    node.polygon.corners[vertex].cap_tension < .10f ||
                    node.polygon.corners[vertex].cap_tension > .90f) {
                    error = where + ".rounded_polygon.corners[" + std::to_string(vertex) + "]: outside safe range";
                    return false;
                }
                if (!std::isfinite(node.polygon.edges[vertex].bulge) || node.polygon.edges[vertex].bulge < -.35f ||
                    node.polygon.edges[vertex].bulge > .35f || !std::isfinite(node.polygon.edges[vertex].start) ||
                    node.polygon.edges[vertex].start < .08f || node.polygon.edges[vertex].start > .45f ||
                    !std::isfinite(node.polygon.edges[vertex].end) || node.polygon.edges[vertex].end < .08f ||
                    node.polygon.edges[vertex].end > .45f) {
                    error = where + ".rounded_polygon.edges[" + std::to_string(vertex) + "]: outside safe range";
                    return false;
                }
            }
        }
    }
    std::array<pet::shape::Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(document, worlds)) {
        error = "Invalid parent hierarchy";
        return false;
    }
    for (size_t index = 0; index < document.count; ++index) {
        const char* problem = shapeProblem(document.nodes[index], worlds[index]);
        if (problem != nullptr) {
            error = "nodes[" + std::to_string(index) + "]: " + problem;
            return false;
        }
    }
    return true;
}
}  // namespace kiwi::studio
