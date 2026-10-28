/* SPDX-License-Identifier: MIT */
#include "motion_model.h"

#include <array>
#include <cstdint>

namespace kiwi::studio {
namespace {

constexpr std::array<MotionDescriptor, 9> kMotionDescriptors{{
    {MotionProperty::TranslateX, "Translate X", {-4096.0f, 4096.0f}, false, false},
    {MotionProperty::TranslateY, "Translate Y", {-4096.0f, 4096.0f}, false, false},
    {MotionProperty::Rotation, "Rotation", {-180.0f, 180.0f}, false, false},
    {MotionProperty::ScaleX, "Scale X", {0.05f, 240.0f}, false, false},
    {MotionProperty::ScaleY, "Scale Y", {0.05f, 240.0f}, false, false},
    {MotionProperty::Opacity, "Opacity", {0.0f, 1.0f}, false, false},
    {MotionProperty::CircleRadius, "Circle R", {0.01f, 240.0f}, true, false},
    {MotionProperty::CornerRoundness, "Corner R", {0.0f, 0.35f}, false, true},
    {MotionProperty::EdgeBulge, "Edge bulge", {-0.35f, 0.35f}, false, true},
}};

}  // namespace

const MotionDescriptor* motionDescriptors()
{
    return kMotionDescriptors.data();
}

size_t motionDescriptorCount()
{
    return kMotionDescriptors.size();
}

const MotionDescriptor* findMotionDescriptor(MotionProperty property)
{
    const int index = static_cast<int>(property);
    if (index < 0 || static_cast<size_t>(index) >= kMotionDescriptors.size()) return nullptr;
    const MotionDescriptor& descriptor = kMotionDescriptors[static_cast<size_t>(index)];
    return descriptor.property == property ? &descriptor : nullptr;
}

const char* motionPropertyLabel(MotionProperty property)
{
    const MotionDescriptor* descriptor = findMotionDescriptor(property);
    return descriptor != nullptr ? descriptor->label : "unknown";
}

bool motionPropertyApplies(MotionProperty property, Kind kind)
{
    const MotionDescriptor* descriptor = findMotionDescriptor(property);
    if (descriptor == nullptr) return false;
    if (descriptor->needs_circle && kind != Kind::Circle) return false;
    if (descriptor->needs_polygon && kind != Kind::Polygon) return false;
    return true;
}

bool motionPropertyValid(int index)
{
    return index >= 0 && static_cast<size_t>(index) < kMotionDescriptors.size();
}

float* resolveMotionTarget(Node& node, MotionProperty property, pet::preset::MotionRange& range)
{
    if (!motionPropertyApplies(property, node.kind)) return nullptr;
    const MotionDescriptor* descriptor = findMotionDescriptor(property);
    if (descriptor == nullptr) return nullptr;
    range = descriptor->range;
    switch (property) {
        case MotionProperty::TranslateX:
            return &node.transform.translation.x;
        case MotionProperty::TranslateY:
            return &node.transform.translation.y;
        case MotionProperty::Rotation:
            return &node.transform.rotation_degrees;
        case MotionProperty::ScaleX:
            return &node.transform.scale.x;
        case MotionProperty::ScaleY:
            return &node.transform.scale.y;
        case MotionProperty::Opacity:
            return &node.style.opacity;
        case MotionProperty::CircleRadius:
            return &node.circle.radius;
        case MotionProperty::CornerRoundness:
            if (node.polygon.vertex_count == 0) return nullptr;
            return &node.polygon.corners[0].roundness;
        case MotionProperty::EdgeBulge:
            if (node.polygon.vertex_count == 0) return nullptr;
            return &node.polygon.edges[0].bulge;
    }
    return nullptr;
}

bool applyMotion(Node& node, const MotionPreview& motion, uint32_t elapsed_ms)
{
    if (!motion.enabled) return false;
    pet::preset::MotionRange range{};
    float* target = resolveMotionTarget(node, motion.property, range);
    if (target == nullptr) return false;
    if (motion.use_keyframes)
        return pet::preset::evaluateKeyframeMotion(motion.keyframes, elapsed_ms, range, motion.easing, *target);
    pet::preset::LoopMotion loop;
    loop.amplitude      = motion.amplitude;
    loop.period_seconds = motion.period_seconds;
    loop.phase_radians  = motion.phase_radians;
    return pet::preset::evaluateLoopMotion(*target, loop, elapsed_ms, range, *target);
}

}  // namespace kiwi::studio
