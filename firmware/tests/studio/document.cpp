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
    if (count > node.polygon.corner_count) {
        // Grow the ring by splitting the closing edge. The new joint is pushed away from
        // the centroid so it is a real corner, not a collinear (degenerate) point that the
        // validator and sampler would reject.
        Vec2 centroid{};
        for (uint8_t index = 0; index < node.polygon.corner_count; ++index) {
            centroid.x += node.polygon.corner_positions[index].x;
            centroid.y += node.polygon.corner_positions[index].y;
        }
        centroid.x /= float(node.polygon.corner_count);
        centroid.y /= float(node.polygon.corner_count);
        for (uint8_t index = node.polygon.corner_count; index < count; ++index) {
            const Vec2& end   = node.polygon.corner_positions[index - 1];
            const Vec2& start = node.polygon.corner_positions[0];
            Vec2 middle{(end.x + start.x) * 0.5f, (end.y + start.y) * 0.5f};
            Vec2 away{middle.x - centroid.x, middle.y - centroid.y};
            const float away_length = std::sqrt(away.x * away.x + away.y * away.y);
            const float edge_length =
                std::sqrt((start.x - end.x) * (start.x - end.x) + (start.y - end.y) * (start.y - end.y));
            if (away_length > 1e-4f) {
                middle.x += away.x / away_length * edge_length * 0.15f;
                middle.y += away.y / away_length * edge_length * 0.15f;
            }
            node.polygon.corner_positions[index] = middle;
            node.polygon.corners[index] = {pet::shape::kDefaultCornerRoundness, pet::shape::kDefaultCapTension};
            node.polygon.edges[index]   = {pet::shape::kDefaultEdgeBulge, pet::shape::kDefaultEdgeHandle,
                                           pet::shape::kDefaultEdgeHandle};
        }
    }
    node.polygon.corner_count = count;
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
