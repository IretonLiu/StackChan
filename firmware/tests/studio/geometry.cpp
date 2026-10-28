/* SPDX-License-Identifier: MIT */
#include "geometry.h"

namespace kiwi::studio {
namespace {

using namespace pet::shape;

}  // namespace

bool sampleNode(const Node& node, const Affine2D& world, ShapePath& path)
{
    if (node.kind == Kind::Circle) {
        CircleSample sample;
        if (!sampleCircle(node.circle, world, sample, node.style)) return false;
        path = sample.path;
        return true;
    }
    RoundedPolygonSample sample;
    if (!sampleRoundedPolygon(node.polygon, world, sample, node.style)) return false;
    path = sample.path;
    return true;
}

bool resolveWorlds(const SceneDoc& document, std::array<Affine2D, kMaxNodes>& worlds)
{
    for (size_t index = 0; index < document.count; ++index) {
        const Node& node = document.nodes[index];
        if (node.parent_index < -1 || node.parent_index >= static_cast<int>(index)) return false;
        const Affine2D local = makeAffine(node.transform);
        worlds[index]        = node.parent_index < 0 ? local : compose(worlds[node.parent_index], local);
    }
    return true;
}

const char* shapeProblem(const Node& node, const Affine2D& world)
{
    if (node.kind == Kind::Circle) {
        CircleSample sample;
        if (sampleCircle(node.circle, world, sample, node.style)) return nullptr;
        switch (sample.error) {
            case CircleSampleError::InvalidRadius:
                return "radius must be greater than zero";
            case CircleSampleError::NonFiniteParameters:
                return "circle center or radius is not finite";
            case CircleSampleError::NonFiniteTransform:
                return "transform produces a singular shape";
            case CircleSampleError::NonFiniteStyle:
                return "style values are not finite";
            default:
                return "circle geometry is invalid";
        }
    }
    RoundedPolygonSample sample;
    if (sampleRoundedPolygon(node.polygon, world, sample, node.style)) return nullptr;
    switch (sample.error) {
        case RoundedPolygonSampleError::InvalidVertexCount:
            return "vertex count must be between 3 and 5";
        case RoundedPolygonSampleError::DegenerateOrNonConvex:
            return "vertices must form a convex polygon";
        case RoundedPolygonSampleError::NonFiniteParameters:
            return "a vertex or edge value is not finite";
        case RoundedPolygonSampleError::NonFiniteTransform:
            return "transform produces a singular shape";
        case RoundedPolygonSampleError::NonFiniteStyle:
            return "style values are not finite";
        default:
            return "polygon geometry is invalid";
    }
}

}  // namespace kiwi::studio
