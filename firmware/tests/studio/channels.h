/* SPDX-License-Identifier: MIT */
#pragma once

// Catalog of the animatable channels the preset compiler accepts.
//
// The ids/kinds/elements mirror pet_faces/face_schema.json, and the limits come from the
// generated header so the studio and firmware agree on ranges. The round-trip test runs
// the compiler over the studio's output, which is the guard against this table drifting.

#include <pet/face_schema.generated.h>

#include <cstddef>
#include <cstdint>

namespace kiwi::studio {

enum class ChannelNodeKind : uint8_t { Any, Circle, Polygon };

struct ChannelInfo {
    const char* id;
    pet::preset::FacePoseChannel channel;
    ChannelNodeKind node_kind;
    bool needs_element;
    float minimum;
    float maximum;
};

const ChannelInfo* channelInfos();
size_t channelInfoCount();
const ChannelInfo* findChannel(const char* id);
const ChannelInfo* findChannel(pet::preset::FacePoseChannel channel);

/** True when `info` can drive a node of the given primitive kind. */
bool channelAppliesTo(ChannelNodeKind required, bool node_is_polygon);

}  // namespace kiwi::studio
