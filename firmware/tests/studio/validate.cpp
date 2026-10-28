/* SPDX-License-Identifier: MIT */
#include "validate.h"

#include "geometry.h"
#include "motion_model.h"

#include <array>
#include <cmath>
#include <utility>

namespace kiwi::studio {
namespace {

using namespace pet::shape;

bool fail(Problem& problem, std::string path, std::string message)
{
    problem.path    = std::move(path);
    problem.message = std::move(message);
    return false;
}

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

bool validateScene(const SceneDoc& document, Problem& problem)
{
    problem = {};
    if (!validId(document.id.c_str())) return fail(problem, {}, "Scene id must be a lowercase kebab-case id");
    if (document.count == 0 || document.count > kMaxNodes)
        return fail(problem, {}, "Scene must contain 1.." + std::to_string(kMaxNodes) + " nodes");

    for (size_t index = 0; index < document.count; ++index) {
        const Node& node        = document.nodes[index];
        const std::string where = "nodes[" + std::to_string(index) + "]";
        if (!validId(node.id.c_str())) return fail(problem, where, "id must be a lowercase kebab-case id");
        for (size_t earlier = 0; earlier < index; ++earlier)
            if (document.nodes[earlier].id == node.id)
                return fail(problem, where, "duplicates node id '" + node.id + "'");
        if (node.parent_index < -1 || node.parent_index >= static_cast<int>(index))
            return fail(problem, where, "parent must name an earlier node or be omitted");

        if (!std::isfinite(node.style.opacity) || node.style.opacity < 0.0f || node.style.opacity > 1.0f)
            return fail(problem, where + ".style.opacity", "must be a finite number in [0, 1]");
        if (!std::isfinite(node.style.stroke_width) || node.style.stroke_width < 0.0f)
            return fail(problem, where + ".style.stroke_width", "must be a finite non-negative number");

        if (!std::isfinite(node.transform.rotation_degrees) || !std::isfinite(node.transform.translation.x) ||
            !std::isfinite(node.transform.translation.y) || !std::isfinite(node.transform.scale.x) ||
            !std::isfinite(node.transform.scale.y) || !std::isfinite(node.transform.pivot.x) ||
            !std::isfinite(node.transform.pivot.y))
            return fail(problem, where + ".transform", "values must be finite");
        if (node.transform.scale.x == 0.0f || node.transform.scale.y == 0.0f)
            return fail(problem, where + ".transform.scale", "components must be non-zero");

        if (!motionPropertyApplies(node.motion.property, node.kind))
            return fail(problem, where + ".motion.property", "does not apply to this node kind");

        if (node.kind == Kind::Circle) {
            if (!std::isfinite(node.circle.center.x) || !std::isfinite(node.circle.center.y) ||
                !std::isfinite(node.circle.radius) || node.circle.radius <= 0.0f)
                return fail(problem, where + ".circle", "center and positive radius must be finite");
        } else {
            const uint8_t count = node.polygon.vertex_count;
            if (count < 3 || count > pet::shape::kMaxRoundedPolygonVertices)
                return fail(problem, where + ".rounded_polygon.vertices",
                            "must contain 3.." + std::to_string(pet::shape::kMaxRoundedPolygonVertices) + " points");
            float first_turn = 0.0f;
            for (uint8_t vertex = 0; vertex < count; ++vertex) {
                const Vec2& point = node.polygon.vertices[vertex];
                if (!std::isfinite(point.x) || !std::isfinite(point.y))
                    return fail(problem, where + ".rounded_polygon.vertices", "points must be finite");
                const float turn = cross(node.polygon.vertices[(vertex + count - 1) % count], point,
                                         node.polygon.vertices[(vertex + 1) % count]);
                if (std::fabs(turn) < .0001f)
                    return fail(problem, where + ".rounded_polygon.vertices", "must be non-degenerate");
                if (vertex == 0)
                    first_turn = turn;
                else if (first_turn * turn <= 0.0f)
                    return fail(problem, where + ".rounded_polygon.vertices", "must be convex with consistent winding");
                if (!std::isfinite(node.polygon.corners[vertex].roundness) ||
                    node.polygon.corners[vertex].roundness < 0.0f || node.polygon.corners[vertex].roundness > .35f ||
                    !std::isfinite(node.polygon.corners[vertex].cap_tension) ||
                    node.polygon.corners[vertex].cap_tension < .10f || node.polygon.corners[vertex].cap_tension > .90f)
                    return fail(problem, where + ".rounded_polygon.corners[" + std::to_string(vertex) + "]",
                                "outside safe range");
                if (!std::isfinite(node.polygon.edges[vertex].bulge) || node.polygon.edges[vertex].bulge < -.35f ||
                    node.polygon.edges[vertex].bulge > .35f || !std::isfinite(node.polygon.edges[vertex].start) ||
                    node.polygon.edges[vertex].start < .08f || node.polygon.edges[vertex].start > .45f ||
                    !std::isfinite(node.polygon.edges[vertex].end) || node.polygon.edges[vertex].end < .08f ||
                    node.polygon.edges[vertex].end > .45f)
                    return fail(problem, where + ".rounded_polygon.edges[" + std::to_string(vertex) + "]",
                                "outside safe range");
            }
        }
    }

    std::array<pet::shape::Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(document, worlds)) return fail(problem, {}, "Invalid parent hierarchy");
    for (size_t index = 0; index < document.count; ++index) {
        const char* problem_text = shapeProblem(document.nodes[index], worlds[index]);
        if (problem_text != nullptr) return fail(problem, "nodes[" + std::to_string(index) + "]", problem_text);
    }
    return true;
}

bool validateScene(const SceneDoc& document, std::string& error)
{
    Problem problem;
    if (validateScene(document, problem)) {
        error.clear();
        return true;
    }
    error = problem.formatted();
    return false;
}

}  // namespace kiwi::studio
