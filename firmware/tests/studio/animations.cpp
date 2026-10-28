/* SPDX-License-Identifier: MIT */
#include "animations.h"

#include "validate.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace kiwi::studio {

namespace {

using pet::preset::FacePoseChannel;

bool finite(float value)
{
    return std::isfinite(value);
}

bool fail(std::string& error, std::string path, std::string message)
{
    error = path.empty() ? std::move(message) : path + ": " + message;
    return false;
}

bool validateResponse(uint8_t fields, float frequency, float damping, float step, const std::string& path,
                      std::string& error)
{
    if ((fields & 1u) != 0 && (!finite(frequency) || frequency < 0.01f || frequency > 30.0f))
        return fail(error, path + ".response.frequency_hz", "must be from 0.01 to 30");
    if ((fields & 2u) != 0 && (!finite(damping) || damping < 0.0f || damping > 4.0f))
        return fail(error, path + ".response.damping_ratio", "must be from 0 to 4");
    if ((fields & 4u) != 0 && (!finite(step) || step < 0.001f || step > 0.1f))
        return fail(error, path + ".response.maximum_step_seconds", "must be from 0.001 to 0.1");
    return true;
}

}  // namespace

int AnimationDoc::findGoal(const std::string& id) const
{
    for (uint8_t index = 0; index < goal_count; ++index) {
        if (goals[index].id == id) return static_cast<int>(index);
    }
    return -1;
}

int AnimationDoc::findPose(const std::string& id) const
{
    for (uint8_t index = 0; index < pose_count; ++index) {
        if (poses[index].id == id) return static_cast<int>(index);
    }
    return -1;
}

int findNodeIndex(const SceneDoc& document, const std::string& id)
{
    for (size_t index = 0; index < document.count && index < kMaxNodes; ++index) {
        if (document.nodes[index].id == id) return static_cast<int>(index);
    }
    return -1;
}

float channelValue(const Node& node, const ChannelInfo& channel, uint8_t element)
{
    switch (channel.channel) {
        case FacePoseChannel::TransformTranslationX:
            return node.transform.translation.x;
        case FacePoseChannel::TransformTranslationY:
            return node.transform.translation.y;
        case FacePoseChannel::TransformRotationDegrees:
            return node.transform.rotation_degrees;
        case FacePoseChannel::TransformScaleX:
            return node.transform.scale.x;
        case FacePoseChannel::TransformScaleY:
            return node.transform.scale.y;
        case FacePoseChannel::CircleCenterX:
            return node.circle.center.x;
        case FacePoseChannel::CircleCenterY:
            return node.circle.center.y;
        case FacePoseChannel::CircleRadius:
            return node.circle.radius;
        case FacePoseChannel::PolygonCornerX:
            return node.polygon.corner_count > element ? node.polygon.corner_positions[element].x : 0.0f;
        case FacePoseChannel::PolygonCornerY:
            return node.polygon.corner_count > element ? node.polygon.corner_positions[element].y : 0.0f;
        case FacePoseChannel::PolygonCornerRoundness:
            return node.polygon.corner_count > element ? node.polygon.corners[element].roundness : 0.0f;
        case FacePoseChannel::PolygonCornerCapTension:
            return node.polygon.corner_count > element ? node.polygon.corners[element].cap_tension : 0.0f;
        case FacePoseChannel::PolygonEdgeBulge:
            return node.polygon.corner_count > element ? node.polygon.edges[element].bulge : 0.0f;
        case FacePoseChannel::PolygonEdgeStart:
            return node.polygon.corner_count > element ? node.polygon.edges[element].start : 0.0f;
        case FacePoseChannel::PolygonEdgeEnd:
            return node.polygon.corner_count > element ? node.polygon.edges[element].end : 0.0f;
    }
    return 0.0f;
}

bool setChannelValue(Node& node, const ChannelInfo& channel, uint8_t element, float value)
{
    if (!finite(value)) return false;
    const float clamped = std::clamp(value, channel.minimum, channel.maximum);
    switch (channel.channel) {
        case FacePoseChannel::TransformTranslationX:
            node.transform.translation.x = clamped;
            return true;
        case FacePoseChannel::TransformTranslationY:
            node.transform.translation.y = clamped;
            return true;
        case FacePoseChannel::TransformRotationDegrees:
            node.transform.rotation_degrees = clamped;
            return true;
        case FacePoseChannel::TransformScaleX:
            node.transform.scale.x = clamped;
            return true;
        case FacePoseChannel::TransformScaleY:
            node.transform.scale.y = clamped;
            return true;
        case FacePoseChannel::CircleCenterX:
            node.circle.center.x = clamped;
            return true;
        case FacePoseChannel::CircleCenterY:
            node.circle.center.y = clamped;
            return true;
        case FacePoseChannel::CircleRadius:
            node.circle.radius = clamped;
            return true;
        case FacePoseChannel::PolygonCornerX:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.corner_positions[element].x = clamped;
            return true;
        case FacePoseChannel::PolygonCornerY:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.corner_positions[element].y = clamped;
            return true;
        case FacePoseChannel::PolygonCornerRoundness:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.corners[element].roundness = clamped;
            return true;
        case FacePoseChannel::PolygonCornerCapTension:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.corners[element].cap_tension = clamped;
            return true;
        case FacePoseChannel::PolygonEdgeBulge:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.edges[element].bulge = clamped;
            return true;
        case FacePoseChannel::PolygonEdgeStart:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.edges[element].start = clamped;
            return true;
        case FacePoseChannel::PolygonEdgeEnd:
            if (element >= node.polygon.corner_count) return false;
            node.polygon.edges[element].end = clamped;
            return true;
    }
    return false;
}

int findPoseTarget(const Pose& pose, const std::string& node_id, const char* channel, uint8_t element)
{
    if (channel == nullptr) return -1;
    for (uint8_t index = 0; index < pose.target_count; ++index) {
        const PoseTarget& target = pose.targets[index];
        if (target.element == element && target.node_id == node_id && target.channel == channel) return index;
    }
    return -1;
}

bool ensurePoseTarget(Pose& pose, const Node& node, const ChannelInfo& channel, uint8_t element)
{
    if (findPoseTarget(pose, node.id, channel.id, element) >= 0) return true;
    if (pose.target_count >= kMaxPoseTargets) return false;
    PoseTarget& target = pose.targets[pose.target_count++];
    target.node_id     = node.id;
    target.channel     = channel.id;
    target.element     = element;
    target.value       = channelValue(node, channel, element);
    return true;
}

void makeDefaultAnimation(AnimationDoc& out)
{
    out                  = {};
    out.pose_count       = 1;
    out.poses[0].id      = "idle";
    out.goal_count       = 1;
    out.goals[0].id      = "idle";
    out.goals[0].pose_id = "idle";
    out.default_goal     = "idle";
}

std::string animationEventLabel(uint8_t event_mask)
{
    std::string label;
    if ((event_mask & 1u) != 0) label += "pet";
    if ((event_mask & 2u) != 0) {
        if (!label.empty()) label += ", ";
        label += "shake";
    }
    if (label.empty()) label = "none";
    return label;
}

bool validateAnimationDoc(const SceneDoc& document, const AnimationDoc& doc, std::string& error)
{
    if (doc.pose_count > kMaxPoses)
        return fail(error, "poses", "must contain at most " + std::to_string(kMaxPoses) + " entries");
    if (doc.goal_count > kMaxAnimationGoals)
        return fail(error, "animations.goals", "must contain at most " + std::to_string(kMaxAnimationGoals) + " goals");
    if (doc.goal_count == 0) return fail(error, "animations.goals", "must declare at least one goal");

    for (uint8_t pose_index = 0; pose_index < doc.pose_count; ++pose_index) {
        const Pose& pose        = doc.poses[pose_index];
        const std::string where = "poses[" + std::to_string(pose_index) + "]";
        if (!validId(pose.id.c_str())) return fail(error, where + ".id", "must be a lowercase kebab-case id");
        for (uint8_t earlier = 0; earlier < pose_index; ++earlier)
            if (doc.poses[earlier].id == pose.id)
                return fail(error, where + ".id", "duplicates pose id '" + pose.id + "'");
        if (pose.target_count > kMaxPoseTargets)
            return fail(error, where + ".targets",
                        "must contain at most " + std::to_string(kMaxPoseTargets) + " targets");
        for (uint8_t target_index = 0; target_index < pose.target_count; ++target_index) {
            const PoseTarget& target = pose.targets[target_index];
            const std::string path   = where + ".targets[" + std::to_string(target_index) + "]";
            const int node_index     = findNodeIndex(document, target.node_id);
            if (node_index < 0) return fail(error, path + ".node", "must name a scene node");
            const ChannelInfo* channel = findChannel(target.channel.c_str());
            if (channel == nullptr) return fail(error, path + ".channel", "must name a supported channel");
            const Node& node = document.nodes[node_index];
            if (!channelAppliesTo(channel->node_kind, node.kind == Kind::Polygon))
                return fail(error, path + ".channel", "requires a different node kind");
            if (channel->needs_element) {
                if (target.element >= node.polygon.corner_count)
                    return fail(error, path + ".element", "must name an existing polygon corner");
            } else if (target.element != 0) {
                return fail(error, path + ".element", "must be zero for this channel");
            }
            if (!finite(target.value) || target.value < channel->minimum || target.value > channel->maximum)
                return fail(error, path + ".value", "must be a finite number inside the channel range");
            if (!validateResponse(target.response_fields, target.response_frequency_hz, target.response_damping_ratio,
                                  target.response_maximum_step_seconds, path, error))
                return false;
            for (uint8_t other = 0; other < target_index; ++other) {
                const PoseTarget& previous = pose.targets[other];
                if (previous.element == target.element && previous.node_id == target.node_id &&
                    previous.channel == target.channel)
                    return fail(error, path, "duplicates another target channel in this pose");
            }
        }
    }

    for (uint8_t goal_index = 0; goal_index < doc.goal_count; ++goal_index) {
        const AnimationGoal& goal = doc.goals[goal_index];
        const std::string where   = "animations.goals[" + std::to_string(goal_index) + "]";
        if (!validId(goal.id.c_str())) return fail(error, where + ".id", "must be a lowercase kebab-case id");
        for (uint8_t earlier = 0; earlier < goal_index; ++earlier)
            if (doc.goals[earlier].id == goal.id)
                return fail(error, where + ".id", "duplicates goal id '" + goal.id + "'");
        const int pose_index = doc.findPose(goal.pose_id);
        if (pose_index < 0) return fail(error, where + ".pose", "must name a defined pose");
        if ((goal.event_mask & ~uint8_t(3)) != 0) return fail(error, where + ".events", "must be pet or shake");
        if (goal.event_mask != 0 && goal.duration_ms == 0)
            return fail(error, where + ".duration_ms", "must be positive for an event goal");
        if (goal.event_mask == 0 && goal.duration_ms != 0)
            return fail(error, where + ".duration_ms", "must be zero when no event activates the goal");
        if (goal.program_count > kMaxAnimationPrograms)
            return fail(error, where + ".programs",
                        "must contain at most " + std::to_string(kMaxAnimationPrograms) + " programs");
        for (uint8_t program_index = 0; program_index < goal.program_count; ++program_index) {
            const AnimationProgram& program = goal.programs[program_index];
            const std::string program_where = where + ".programs[" + std::to_string(program_index) + "]";
            if (!validId(program.id.c_str()))
                return fail(error, program_where + ".id", "must be a lowercase kebab-case id");
            for (uint8_t earlier = 0; earlier < program_index; ++earlier)
                if (goal.programs[earlier].id == program.id)
                    return fail(error, program_where + ".id", "duplicates program id '" + program.id + "'");
            if (!finite(program.duration_seconds) || program.duration_seconds < 0.001f ||
                program.duration_seconds > 600.0f)
                return fail(error, program_where + ".duration_seconds", "must be from 0.001 to 600");
            if (program.track_count == 0 || program.track_count > kMaxAnimationTracks)
                return fail(error, program_where + ".tracks",
                            "must contain 1.." + std::to_string(kMaxAnimationTracks) + " tracks");
            for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
                const AnimationTrack& track = program.tracks[track_index];
                const std::string path      = program_where + ".tracks[" + std::to_string(track_index) + "]";
                const int node_index        = findNodeIndex(document, track.node_id);
                if (node_index < 0) return fail(error, path + ".node", "must name a scene node");
                const ChannelInfo* channel = findChannel(track.channel.c_str());
                if (channel == nullptr) return fail(error, path + ".channel", "must name a supported channel");
                const Node& node = document.nodes[node_index];
                if (!channelAppliesTo(channel->node_kind, node.kind == Kind::Polygon))
                    return fail(error, path + ".channel", "requires a different node kind");
                if (channel->needs_element) {
                    if (track.element >= node.polygon.corner_count)
                        return fail(error, path + ".element", "must name an existing polygon corner");
                } else if (track.element != 0) {
                    return fail(error, path + ".element", "must be zero for this channel");
                }
                for (uint8_t other = 0; other < track_index; ++other) {
                    const AnimationTrack& previous = program.tracks[other];
                    if (previous.element == track.element && previous.node_id == track.node_id &&
                        previous.channel == track.channel)
                        return fail(error, path, "duplicates another track channel in this program");
                }
                if (!validateResponse(track.response_fields, track.response_frequency_hz, track.response_damping_ratio,
                                      track.response_maximum_step_seconds, path, error))
                    return false;
                const Pose& pose = doc.poses[pose_index];
                if (findPoseTarget(pose, track.node_id, track.channel.c_str(), track.element) < 0)
                    return fail(error, path, "must target a channel declared by the goal pose");
                if (track.sine) {
                    if (!finite(track.amplitude) || track.amplitude < 0.0f || !finite(track.period_seconds) ||
                        track.period_seconds <= 0.0f || !finite(track.center) || !finite(track.phase_radians))
                        return fail(error, path + ".source", "must be finite with a positive period");
                    if (track.center - track.amplitude < channel->minimum ||
                        track.center + track.amplitude > channel->maximum)
                        return fail(error, path + ".source", "must stay inside the channel range");
                } else {
                    if (track.keyframe_count < 2 || track.keyframe_count > pet::preset::kMaxMotionKeyframes)
                        return fail(
                            error, path + ".source.keyframes",
                            "must contain 2.." + std::to_string(pet::preset::kMaxMotionKeyframes) + " keyframes");
                    float previous_time = -1.0f;
                    for (uint8_t frame = 0; frame < track.keyframe_count; ++frame) {
                        const auto& keyframe = track.keyframes[frame];
                        if (!finite(keyframe.time_seconds) || !finite(keyframe.value))
                            return fail(error, path + ".source.keyframes", "values must be finite");
                        if (keyframe.time_seconds < 0.0f || keyframe.time_seconds <= previous_time ||
                            keyframe.time_seconds > program.duration_seconds)
                            return fail(error, path + ".source.keyframes",
                                        "must be ordered and inside the program duration");
                        if (frame == 0 && keyframe.time_seconds != 0.0f)
                            return fail(error, path + ".source.keyframes", "first keyframe must start at 0");
                        if (keyframe.value < channel->minimum || keyframe.value > channel->maximum)
                            return fail(error, path + ".source.keyframes", "must stay inside the channel range");
                        previous_time = keyframe.time_seconds;
                    }
                }
            }
        }
    }
    if (doc.findGoal(doc.default_goal) < 0) return fail(error, "animations.default_goal", "must name a defined goal");
    return true;
}

bool validateAnimations(const SceneDoc& document, std::string& error)
{
    return validateAnimationDoc(document, document.animations, error);
}

bool applyAnimationFrame(const SceneDoc& document, int goal_index, float time_seconds,
                         std::array<Node, kMaxNodes>& nodes, size_t node_count)
{
    const AnimationDoc& animations = document.animations;
    if (goal_index < 0 || goal_index >= static_cast<int>(animations.goal_count) || !finite(time_seconds) ||
        time_seconds < 0.0f)
        return false;
    const AnimationGoal& goal = animations.goals[goal_index];
    bool changed              = false;

    const int pose_index = animations.findPose(goal.pose_id);
    if (pose_index >= 0) {
        const Pose& pose = animations.poses[pose_index];
        for (uint8_t index = 0; index < pose.target_count; ++index) {
            const PoseTarget& target = pose.targets[index];
            const ChannelInfo* info  = findChannel(target.channel.c_str());
            const int node_index     = findNodeIndex(document, target.node_id);
            if (info == nullptr || node_index < 0 || static_cast<size_t>(node_index) >= node_count) continue;
            const float previous = channelValue(nodes[node_index], *info, target.element);
            setChannelValue(nodes[node_index], *info, target.element, target.value);
            const float current = channelValue(nodes[node_index], *info, target.element);
            changed             = changed || previous != current;
        }
    }

    struct Candidate {
        int node                    = -1;
        FacePoseChannel channel     = FacePoseChannel::TransformTranslationX;
        uint8_t element             = 0;
        const AnimationTrack* track = nullptr;
        float local_seconds         = 0.0f;
        uint16_t priority           = 0;
    };
    std::array<Candidate, kMaxAnimationCandidates> candidates{};
    size_t candidate_count = 0;

    for (uint8_t program_index = 0; program_index < goal.program_count; ++program_index) {
        const AnimationProgram& program = goal.programs[program_index];
        if (!program.loop && time_seconds > program.duration_seconds) continue;
        if (!finite(program.duration_seconds) || program.duration_seconds <= 0.0f) continue;
        const float local = program.loop ? std::fmod(time_seconds, program.duration_seconds) : time_seconds;
        for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
            const AnimationTrack& track = program.tracks[track_index];
            const ChannelInfo* info     = findChannel(track.channel.c_str());
            const int node_index        = findNodeIndex(document, track.node_id);
            if (info == nullptr || node_index < 0 || static_cast<size_t>(node_index) >= node_count) continue;
            if (candidate_count == kMaxAnimationCandidates) break;
            Candidate& candidate    = candidates[candidate_count++];
            candidate.node          = node_index;
            candidate.channel       = info->channel;
            candidate.element       = track.element;
            candidate.track         = &track;
            candidate.local_seconds = local;
            candidate.priority      = static_cast<uint16_t>(program.priority) * 256u + track.priority;
        }
    }

    const auto sameChannel = [](const Candidate& left, const Candidate& right) {
        return left.node == right.node && left.channel == right.channel && left.element == right.element;
    };
    for (size_t index = 0; index < candidate_count; ++index) {
        bool winner = true;
        for (size_t other = 0; other < candidate_count; ++other) {
            if (index == other || !sameChannel(candidates[index], candidates[other])) continue;
            if (candidates[other].priority > candidates[index].priority ||
                (candidates[other].priority == candidates[index].priority && other < index)) {
                winner = false;
                break;
            }
        }
        if (!winner) continue;
        const Candidate& candidate = candidates[index];
        const ChannelInfo* info    = findChannel(candidate.channel);
        if (info == nullptr) continue;
        const float value = [&]() {
            const AnimationTrack& track = *candidate.track;
            if (track.sine)
                return track.center +
                       track.amplitude * std::sin((candidate.local_seconds / track.period_seconds) * 6.28318530718f +
                                                  track.phase_radians);
            if (track.keyframe_count == 0) return 0.0f;
            if (candidate.local_seconds <= track.keyframes[0].time_seconds) return track.keyframes[0].value;
            for (uint8_t frame = 1; frame < track.keyframe_count; ++frame) {
                const auto& previous = track.keyframes[frame - 1];
                const auto& next     = track.keyframes[frame];
                if (candidate.local_seconds <= next.time_seconds) {
                    float amount =
                        (candidate.local_seconds - previous.time_seconds) / (next.time_seconds - previous.time_seconds);
                    if (track.easing == pet::preset::MotionEasing::Smoothstep)
                        amount = amount * amount * (3.0f - 2.0f * amount);
                    return previous.value + (next.value - previous.value) * amount;
                }
            }
            return track.keyframes[track.keyframe_count - 1].value;
        }();
        setChannelValue(nodes[candidate.node], *info, candidate.element, value);
        changed = true;
    }
    return changed;
}

}  // namespace kiwi::studio
