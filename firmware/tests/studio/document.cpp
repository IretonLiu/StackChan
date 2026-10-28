/* SPDX-License-Identifier: MIT */
#include "document.h"

#include "geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace kiwi::studio {
namespace {

using namespace pet::shape;

}  // namespace

int uniqueNodeNumber(const SceneDoc& document, Kind kind)
{
    const std::string prefix = kind == Kind::Circle ? "circle-" : "polygon-";
    for (int number = 1;; ++number) {
        const std::string candidate = prefix + std::to_string(number);
        bool used                   = false;
        for (size_t index = 0; index < document.count; ++index) used |= document.nodes[index].id == candidate;
        if (!used) return number;
    }
}

Node makeNode(const SceneDoc& document, Kind kind)
{
    Node node;
    node.id   = (kind == Kind::Circle ? "circle-" : "polygon-") + std::to_string(uniqueNodeNumber(document, kind));
    node.kind = kind;
    node.polygon.corner_count     = 3;
    node.polygon.corner_positions = {{{-30, 20}, {0, -35}, {30, 20}}};
    return node;
}

bool setCornerCount(SceneDoc& document, size_t node_index, uint8_t count, std::string& error)
{
    error.clear();
    if (node_index >= document.count) {
        error = "Node index is out of range";
        return false;
    }
    Node& node = document.nodes[node_index];
    if (node.kind != Kind::Polygon) {
        error = "Only rounded polygons have corners";
        return false;
    }
    if (count < 3 || count > pet::shape::kMaxRoundedPolygonCorners) {
        error = "Corner count must be 3.." + std::to_string(pet::shape::kMaxRoundedPolygonCorners);
        return false;
    }

    const auto distance = [](Vec2 a, Vec2 b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    // Grow one corner at a time by splitting the longest edge. Splitting the longest edge
    // is what keeps a skewed polygon growable: the feasible outward displacement (the
    // convexity window) is widest there. Appending on a fixed edge collapses two corners
    // onto the same short edge and can leave no valid, non-degenerate position at all.
    while (node.polygon.corner_count < count) {
        pet::shape::RoundedPolygonParameters& polygon = node.polygon;
        const uint8_t ring                            = polygon.corner_count;
        uint8_t edge                                  = 0;
        float longest                                 = 0.0f;
        for (uint8_t index = 0; index < ring; ++index) {
            const float span = distance(polygon.corner_positions[index], polygon.corner_positions[(index + 1) % ring]);
            if (span > longest) {
                longest = span;
                edge    = index;
            }
        }
        if (longest < 1e-4f) {
            error = "Polygon has a zero-length edge";
            return false;
        }

        const Vec2 start = polygon.corner_positions[edge];
        const Vec2 end   = polygon.corner_positions[(edge + 1) % ring];
        const Vec2 middle{(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        const Vec2 direction{(end.x - start.x) / longest, (end.y - start.y) / longest};
        float winding = 0.0f;
        for (uint8_t index = 0; index < ring && winding == 0.0f; ++index)
            winding = cross(polygon.corner_positions[(index + ring - 1) % ring], polygon.corner_positions[index],
                            polygon.corner_positions[(index + 1) % ring]);
        const Vec2 outward = winding >= 0.0f ? Vec2{direction.y, -direction.x} : Vec2{-direction.y, direction.x};
        const float reach =
            0.35f * std::min({longest, distance(polygon.corner_positions[(edge + ring - 1) % ring], start),
                              distance(end, polygon.corner_positions[(edge + 2) % ring])});
        const uint8_t insert = uint8_t(edge + 1);

        // Largest outward offset first (a pronounced, readable corner), then shrink until
        // the engine's own sampler accepts the ring. That makes the result exactly what the
        // canvas and the preset compiler will later see.
        bool placed = false;
        for (const float factor : {1.0f, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f, 0.12f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f}) {
            pet::shape::RoundedPolygonParameters trial = polygon;
            for (uint8_t index = ring; index > insert; --index) {
                trial.corner_positions[index] = trial.corner_positions[index - 1];
                trial.corners[index]          = trial.corners[index - 1];
                trial.edges[index]            = trial.edges[index - 1];
            }
            trial.corner_positions[insert] = {middle.x + outward.x * reach * factor,
                                              middle.y + outward.y * reach * factor};
            trial.corners[insert]          = {pet::shape::kDefaultCornerRoundness, pet::shape::kDefaultCapTension};
            trial.edges[insert]            = polygon.edges[edge];
            trial.edges[edge]              = polygon.edges[edge];
            trial.corner_count             = uint8_t(ring + 1);
            pet::shape::RoundedPolygonSample sample;
            if (pet::shape::sampleRoundedPolygon(trial, {}, sample)) {
                polygon = trial;
                placed  = true;
                break;
            }
        }
        if (!placed) {
            error = "Could not grow the polygon without breaking convexity";
            return false;
        }
    }
    return true;
}

void resetScene(SceneDoc& document)
{
    document                                = {};
    document.nodes[0]                       = makeNode(document, Kind::Polygon);
    document.nodes[0].transform.translation = {108, 78};
    ++document.count;
    document.nodes[1]                       = makeNode(document, Kind::Polygon);
    document.nodes[1].transform.translation = {212, 78};
    ++document.count;
    document.nodes[2]        = makeNode(document, Kind::Circle);
    document.nodes[2].circle = {{118, 138}, 12};
    ++document.count;
    document.nodes[3]        = makeNode(document, Kind::Circle);
    document.nodes[3].circle = {{202, 138}, 12};
    ++document.count;
}

void clearScene(SceneDoc& document)
{
    document = {};
}

int nodeDepth(const SceneDoc& document, int index)
{
    int depth  = 0;
    int cursor = index;
    while (cursor >= 0 && cursor < static_cast<int>(document.count) && document.nodes[cursor].parent_index >= 0) {
        cursor = document.nodes[cursor].parent_index;
        ++depth;
    }
    return depth;
}

std::string uniqueCopyId(const SceneDoc& document, const std::string& id)
{
    for (int suffix = 1;; ++suffix) {
        const std::string candidate = id + "-copy" + (suffix == 1 ? "" : "-" + std::to_string(suffix));
        bool exists                 = false;
        for (size_t index = 0; index < document.count; ++index) exists |= document.nodes[index].id == candidate;
        if (!exists) return candidate;
    }
}

bool duplicateNode(SceneDoc& document, int selected, int& new_selection, std::string& error)
{
    if (selected < 0 || selected >= static_cast<int>(document.count)) {
        error = "Select a node to duplicate";
        return false;
    }
    if (document.count == kMaxNodes) {
        error = "Node limit reached";
        return false;
    }
    Node copy = document.nodes[selected];
    copy.id   = uniqueCopyId(document, copy.id);
    copy.transform.translation.x += 8.0f;
    copy.transform.translation.y += 8.0f;
    document.nodes[document.count] = copy;
    new_selection                  = static_cast<int>(document.count++);
    return true;
}

bool moveNode(SceneDoc& document, int source, int target, int& new_selection, std::string& error)
{
    if (source < 0 || source >= static_cast<int>(document.count) || target < 0 ||
        target >= static_cast<int>(document.count)) {
        error = "Move target is out of range";
        return false;
    }
    SceneDoc before = document;
    std::swap(document.nodes[source], document.nodes[target]);
    for (size_t index = 0; index < document.count; ++index) {
        int& parent = document.nodes[index].parent_index;
        if (parent == source)
            parent = target;
        else if (parent == target)
            parent = source;
    }
    std::array<Affine2D, kMaxNodes> worlds{};
    new_selection = target;
    if (resolveWorlds(document, worlds)) return true;
    document = before;
    error    = "Move would break the parent hierarchy";
    return false;
}

bool deleteNode(SceneDoc& document, int index, int& new_selection, std::string& error)
{
    if (index < 0 || index >= static_cast<int>(document.count)) {
        error = "Select a node to delete";
        return false;
    }
    for (size_t child = 0; child < document.count; ++child)
        if (document.nodes[child].parent_index == index) {
            error = "Cannot delete a node while it has children";
            return false;
        }
    for (size_t cursor = index; cursor + 1 < document.count; ++cursor)
        document.nodes[cursor] = document.nodes[cursor + 1];
    --document.count;
    for (size_t cursor = 0; cursor < document.count; ++cursor)
        if (document.nodes[cursor].parent_index > index) --document.nodes[cursor].parent_index;
    new_selection = std::min<int>(index, int(document.count) - 1);
    return true;
}

bool deleteSelected(SceneDoc& document, int selected, int& new_selection, std::string& error)
{
    if (document.count <= 1) {
        error = "Cannot delete the last node";
        return false;
    }
    return deleteNode(document, selected, new_selection, error);
}

}  // namespace kiwi::studio
