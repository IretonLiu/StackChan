/* SPDX-License-Identifier: MIT */
#include "channels.h"

namespace kiwi::studio {

namespace {

using pet::preset::FacePoseChannel;

const ChannelInfo kChannels[] = {
    {"transform.translation_x", FacePoseChannel::TransformTranslationX, ChannelNodeKind::Any, false,
     pet::preset::kCoordinateMinimum, pet::preset::kCoordinateMaximum},
    {"transform.translation_y", FacePoseChannel::TransformTranslationY, ChannelNodeKind::Any, false,
     pet::preset::kCoordinateMinimum, pet::preset::kCoordinateMaximum},
    {"transform.rotation_degrees", FacePoseChannel::TransformRotationDegrees, ChannelNodeKind::Any, false,
     pet::preset::kRotationMinimum, pet::preset::kRotationMaximum},
    {"transform.scale_x", FacePoseChannel::TransformScaleX, ChannelNodeKind::Any, false, pet::preset::kScaleMinimum,
     pet::preset::kScaleMaximum},
    {"transform.scale_y", FacePoseChannel::TransformScaleY, ChannelNodeKind::Any, false, pet::preset::kScaleMinimum,
     pet::preset::kScaleMaximum},
    {"circle.center_x", FacePoseChannel::CircleCenterX, ChannelNodeKind::Circle, false, pet::preset::kCoordinateMinimum,
     pet::preset::kCoordinateMaximum},
    {"circle.center_y", FacePoseChannel::CircleCenterY, ChannelNodeKind::Circle, false, pet::preset::kCoordinateMinimum,
     pet::preset::kCoordinateMaximum},
    {"circle.radius", FacePoseChannel::CircleRadius, ChannelNodeKind::Circle, false, pet::preset::kRadiusMinimum,
     pet::preset::kRadiusMaximum},
    {"rounded_polygon.corner_x", FacePoseChannel::PolygonCornerX, ChannelNodeKind::Polygon, true,
     pet::preset::kCoordinateMinimum, pet::preset::kCoordinateMaximum},
    {"rounded_polygon.corner_y", FacePoseChannel::PolygonCornerY, ChannelNodeKind::Polygon, true,
     pet::preset::kCoordinateMinimum, pet::preset::kCoordinateMaximum},
    {"rounded_polygon.corner_roundness", FacePoseChannel::PolygonCornerRoundness, ChannelNodeKind::Polygon, true,
     pet::shape::kMinCornerRoundness, pet::shape::kMaxCornerRoundness},
    {"rounded_polygon.corner_cap_tension", FacePoseChannel::PolygonCornerCapTension, ChannelNodeKind::Polygon, true,
     pet::shape::kMinCapTension, pet::shape::kMaxCapTension},
    {"rounded_polygon.edge_bulge", FacePoseChannel::PolygonEdgeBulge, ChannelNodeKind::Polygon, true,
     pet::shape::kMinEdgeBulge, pet::shape::kMaxEdgeBulge},
    {"rounded_polygon.edge_start", FacePoseChannel::PolygonEdgeStart, ChannelNodeKind::Polygon, true,
     pet::shape::kMinEdgeHandle, pet::shape::kMaxEdgeHandle},
    {"rounded_polygon.edge_end", FacePoseChannel::PolygonEdgeEnd, ChannelNodeKind::Polygon, true,
     pet::shape::kMinEdgeHandle, pet::shape::kMaxEdgeHandle},
};

}  // namespace

const ChannelInfo* channelInfos()
{
    return kChannels;
}

size_t channelInfoCount()
{
    return sizeof(kChannels) / sizeof(kChannels[0]);
}

const ChannelInfo* findChannel(const char* id)
{
    if (id == nullptr) return nullptr;
    for (const ChannelInfo& info : kChannels) {
        const char* left  = info.id;
        const char* right = id;
        while (*left != '\0' && *left == *right) ++left, ++right;
        if (*left == '\0' && *right == '\0') return &info;
    }
    return nullptr;
}

const ChannelInfo* findChannel(pet::preset::FacePoseChannel channel)
{
    for (const ChannelInfo& info : kChannels) {
        if (info.channel == channel) return &info;
    }
    return nullptr;
}

bool channelAppliesTo(ChannelNodeKind required, bool node_is_polygon)
{
    switch (required) {
        case ChannelNodeKind::Any:
            return true;
        case ChannelNodeKind::Circle:
            return !node_is_polygon;
        case ChannelNodeKind::Polygon:
            return node_is_polygon;
    }
    return false;
}

}  // namespace kiwi::studio
