/* SPDX-License-Identifier: MIT */
#include "document.h"

#include "geometry.h"

#include <algorithm>
#include <array>
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
    node.polygon.vertex_count = 3;
    node.polygon.vertices     = {{{-30, 20}, {0, -35}, {30, 20}}};
    return node;
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
