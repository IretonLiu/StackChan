/* SPDX-License-Identifier: MIT */
#include "animations.h"

#include "motion_model.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace kiwi::studio {

namespace {

using pet::preset::MotionEasing;
using pet::preset::MotionKeyframe;
using pet::preset::MotionRange;

bool readString(JsonVariantConst value, std::string& out, const std::string& where, std::string& error)
{
    if (!value.is<const char*>()) {
        error = where + ": must be a string";
        return false;
    }
    const char* text = value.as<const char*>();
    if (text == nullptr || text[0] == '\0') {
        error = where + ": must not be empty";
        return false;
    }
    out = text;
    return true;
}

bool readInt(JsonVariantConst value, int& out, int minimum, int maximum, const std::string& where, std::string& error)
{
    if (!value.is<int>()) {
        error = where + ": must be an integer";
        return false;
    }
    const int parsed = value.as<int>();
    if (parsed < minimum || parsed > maximum) {
        error = where + ": must be " + std::to_string(minimum) + ".." + std::to_string(maximum);
        return false;
    }
    out = parsed;
    return true;
}

bool readFloat(JsonVariantConst value, float& out, const std::string& where, std::string& error)
{
    if (!value.is<float>()) {
        error = where + ": must be a number";
        return false;
    }
    const float parsed = value.as<float>();
    if (!std::isfinite(parsed)) {
        error = where + ": must be finite";
        return false;
    }
    out = parsed;
    return true;
}

bool channelProperty(std::string_view channel, MotionProperty& property)
{
    struct Entry {
        std::string_view name;
        MotionProperty property;
    };
    static constexpr Entry kEntries[] = {
        {"transform.translation_x", MotionProperty::TranslateX},
        {"transform.translation_y", MotionProperty::TranslateY},
        {"transform.rotation_degrees", MotionProperty::Rotation},
        {"transform.scale_x", MotionProperty::ScaleX},
        {"transform.scale_y", MotionProperty::ScaleY},
        {"circle.radius", MotionProperty::CircleRadius},
        {"rounded_polygon.corner_roundness", MotionProperty::CornerRoundness},
        {"rounded_polygon.edge_bulge", MotionProperty::EdgeBulge},
    };
    for (const Entry& entry : kEntries) {
        if (entry.name == channel) {
            property = entry.property;
            return true;
        }
    }
    return false;
}

int findNodeIndex(const SceneDoc& document, const std::string& id)
{
    for (size_t index = 0; index < document.count && index < kMaxNodes; ++index) {
        if (document.nodes[index].id == id) return static_cast<int>(index);
    }
    return -1;
}

bool parseEvents(JsonArrayConst events, uint8_t& mask, const std::string& where, std::string& error)
{
    mask = 0;
    for (JsonVariantConst value : events) {
        std::string name;
        if (!readString(value, name, where + ".events", error)) return false;
        if (name == "pet") {
            mask |= 1;
        } else if (name == "shake") {
            mask |= 2;
        } else {
            error = where + ".events: must be 'pet' or 'shake'";
            return false;
        }
    }
    return true;
}

bool parseTrack(JsonObjectConst source, const SceneDoc& document, AnimationTrack& track, const std::string& where,
                std::string& error)
{
    if (!readString(source["node"], track.node_id, where + ".node", error)) return false;
    track.node_index = findNodeIndex(document, track.node_id);
    if (track.node_index < 0) {
        error = where + ".node: names an unknown node";
        return false;
    }
    if (!readString(source["channel"], track.channel, where + ".channel", error)) return false;
    MotionProperty property = MotionProperty::Rotation;
    track.property          = property;
    track.mappable          = channelProperty(track.channel, property) &&
                              motionPropertyApplies(property, document.nodes[track.node_index].kind);
    track.property          = property;

    if (!source["element"].isNull()) {
        int element = 0;
        if (!readInt(source["element"], element, 0, 255, where + ".element", error)) return false;
        track.element = static_cast<uint8_t>(element);
    }
    if (!source["priority"].isNull()) {
        int priority = 0;
        if (!readInt(source["priority"], priority, 0, 255, where + ".priority", error)) return false;
        track.priority = static_cast<uint8_t>(priority);
    }

    const JsonObjectConst payload = source["source"].as<JsonObjectConst>();
    if (payload.isNull()) {
        error = where + ".source: must be an object";
        return false;
    }
    std::string kind;
    if (!readString(payload["kind"], kind, where + ".source.kind", error)) return false;
    if (kind == "sine") {
        track.sine = true;
        if (!readFloat(payload["center"], track.center, where + ".source.center", error)) return false;
        if (!readFloat(payload["amplitude"], track.amplitude, where + ".source.amplitude", error)) return false;
        if (!readFloat(payload["period_seconds"], track.period_seconds, where + ".source.period_seconds", error))
            return false;
        if (track.period_seconds <= 0.0f) {
            error = where + ".source.period_seconds: must be > 0";
            return false;
        }
        if (!readFloat(payload["phase_radians"], track.phase_radians, where + ".source.phase_radians", error))
            return false;
        return true;
    }
    if (kind != "keyframes") {
        error = where + ".source.kind: must be 'keyframes' or 'sine'";
        return false;
    }
    if (!payload["easing"].isNull()) {
        std::string easing;
        if (!readString(payload["easing"], easing, where + ".source.easing", error)) return false;
        if (easing == "linear") {
            track.easing = MotionEasing::Linear;
        } else if (easing == "smoothstep") {
            track.easing = MotionEasing::Smoothstep;
        } else {
            error = where + ".source.easing: must be 'linear' or 'smoothstep'";
            return false;
        }
    }
    const JsonArrayConst keyframes = payload["keyframes"].as<JsonArrayConst>();
    if (keyframes.isNull() || keyframes.size() < 2 || keyframes.size() > pet::preset::kMaxMotionKeyframes) {
        error = where + ".source.keyframes: must contain 2.." + std::to_string(pet::preset::kMaxMotionKeyframes) +
                " keyframes";
        return false;
    }
    float previous_time = -1.0f;
    for (JsonVariantConst value : keyframes) {
        const JsonObjectConst frame = value.as<JsonObjectConst>();
        if (frame.isNull()) {
            error = where + ".source.keyframes: entries must be objects";
            return false;
        }
        MotionKeyframe keyframe;
        if (!readFloat(frame["time_seconds"], keyframe.time_seconds, where + ".source.keyframes.time_seconds", error))
            return false;
        if (!readFloat(frame["value"], keyframe.value, where + ".source.keyframes.value", error)) return false;
        if (track.keyframe_count == 0 && keyframe.time_seconds != 0.0f) {
            error = where + ".source.keyframes: first keyframe must start at 0";
            return false;
        }
        if (keyframe.time_seconds <= previous_time) {
            error = where + ".source.keyframes: times must strictly increase";
            return false;
        }
        previous_time                           = keyframe.time_seconds;
        track.keyframes[track.keyframe_count++] = keyframe;
    }
    return true;
}

bool parseProgram(JsonObjectConst source, const SceneDoc& document, AnimationProgram& program, const std::string& where,
                  std::string& error)
{
    if (!readString(source["id"], program.id, where + ".id", error)) return false;
    float duration = 0.0f;
    if (!readFloat(source["duration_seconds"], duration, where + ".duration_seconds", error)) return false;
    if (duration <= 0.0f) {
        error = where + ".duration_seconds: must be > 0";
        return false;
    }
    program.duration_seconds = duration;
    if (!source["loop"].is<bool>()) {
        error = where + ".loop: must be a boolean";
        return false;
    }
    program.loop = source["loop"].as<bool>();
    if (!source["priority"].isNull()) {
        int priority = 0;
        if (!readInt(source["priority"], priority, 0, 255, where + ".priority", error)) return false;
        program.priority = static_cast<uint8_t>(priority);
    }
    const JsonArrayConst tracks = source["tracks"].as<JsonArrayConst>();
    if (tracks.isNull() || tracks.size() == 0 || tracks.size() > kMaxAnimationTracks) {
        error = where + ".tracks: must contain 1.." + std::to_string(kMaxAnimationTracks) + " tracks";
        return false;
    }
    for (JsonVariantConst value : tracks) {
        const JsonObjectConst track = value.as<JsonObjectConst>();
        if (track.isNull()) {
            error = where + ".tracks: entries must be objects";
            return false;
        }
        const std::string track_where = where + ".tracks[" + std::to_string(program.track_count) + "]";
        if (!parseTrack(track, document, program.tracks[program.track_count], track_where, error)) return false;
        ++program.track_count;
    }
    return true;
}

bool parsePoseTargets(JsonArrayConst targets, const SceneDoc& document, Pose& pose, const std::string& where,
                      std::string& error)
{
    if (targets.size() > kMaxPoseTargets) {
        error = where + ".targets: must contain at most " + std::to_string(kMaxPoseTargets) + " targets";
        return false;
    }
    for (JsonVariantConst value : targets) {
        const JsonObjectConst target = value.as<JsonObjectConst>();
        if (target.isNull()) {
            error = where + ".targets: entries must be objects";
            return false;
        }
        const std::string target_where = where + ".targets[" + std::to_string(pose.target_count) + "]";
        PoseTarget& parsed             = pose.targets[pose.target_count];
        if (!readString(target["node"], parsed.node_id, target_where + ".node", error)) return false;
        parsed.node_index = findNodeIndex(document, parsed.node_id);
        if (parsed.node_index < 0) {
            error = target_where + ".node: names an unknown node";
            return false;
        }
        if (!readString(target["channel"], parsed.channel, target_where + ".channel", error)) return false;
        MotionProperty property = MotionProperty::Rotation;
        parsed.mappable         = channelProperty(parsed.channel, property) &&
                                  motionPropertyApplies(property, document.nodes[parsed.node_index].kind);
        parsed.property         = property;
        if (!target["element"].isNull()) {
            int element = 0;
            if (!readInt(target["element"], element, 0, 255, target_where + ".element", error)) return false;
            parsed.element = static_cast<uint8_t>(element);
        }
        if (!readFloat(target["value"], parsed.value, target_where + ".value", error)) return false;
        ++pose.target_count;
    }
    return true;
}

bool parsePoses(const std::string& json, const SceneDoc& document, AnimationDoc& out, std::string& error)
{
    if (json.empty()) return true;
    JsonDocument parsed;
    const DeserializationError failure =
        deserializeJson(parsed, json, DeserializationOption::NestingLimit(kMaxJsonNesting));
    if (failure) {
        error = std::string("poses: ") + failure.c_str();
        return false;
    }
    if (parsed.overflowed()) {
        error = "poses: JSON document exceeded its memory pool";
        return false;
    }
    const JsonArrayConst poses = parsed.as<JsonArrayConst>();
    if (poses.isNull()) {
        error = "poses: must be an array";
        return false;
    }
    if (poses.size() > kMaxPoses) {
        error = "poses: must contain at most " + std::to_string(kMaxPoses) + " entries";
        return false;
    }
    for (JsonVariantConst value : poses) {
        const JsonObjectConst source = value.as<JsonObjectConst>();
        const std::string where      = "poses[" + std::to_string(out.pose_count) + "]";
        if (source.isNull()) {
            error = where + ": must be an object";
            return false;
        }
        out.poses[out.pose_count] = {};
        Pose& pose                = out.poses[out.pose_count];
        if (!readString(source["id"], pose.id, where + ".id", error)) return false;
        const JsonArrayConst targets = source["targets"].as<JsonArrayConst>();
        if (targets.isNull() || targets.size() == 0) {
            error = where + ".targets: must contain at least one target";
            return false;
        }
        if (!parsePoseTargets(targets, document, pose, where, error)) return false;
        ++out.pose_count;
    }
    return true;
}

bool parseAnimationsJson(const std::string& json, const SceneDoc& document, AnimationDoc& out, std::string& error)
{
    if (json.empty()) return true;
    JsonDocument parsed;
    const DeserializationError failure =
        deserializeJson(parsed, json, DeserializationOption::NestingLimit(kMaxJsonNesting));
    if (failure) {
        error = std::string("animations: ") + failure.c_str();
        return false;
    }
    if (parsed.overflowed()) {
        error = "animations: JSON document exceeded its memory pool";
        return false;
    }
    const JsonObjectConst root = parsed.as<JsonObjectConst>();
    if (root.isNull()) {
        error = "animations: must be an object";
        return false;
    }
    if (!readString(root["default_goal"], out.default_goal, "animations.default_goal", error)) return false;
    const JsonArrayConst goals = root["goals"].as<JsonArrayConst>();
    if (goals.isNull() || goals.size() == 0 || goals.size() > kMaxAnimationGoals) {
        error = "animations.goals: must contain 1.." + std::to_string(kMaxAnimationGoals) + " goals";
        return false;
    }
    for (JsonVariantConst value : goals) {
        const JsonObjectConst source = value.as<JsonObjectConst>();
        const std::string where      = "animations.goals[" + std::to_string(out.goal_count) + "]";
        if (source.isNull()) {
            error = where + ": must be an object";
            return false;
        }
        out.goals[out.goal_count] = {};
        AnimationGoal& goal       = out.goals[out.goal_count];
        if (!readString(source["id"], goal.id, where + ".id", error)) return false;
        if (!readString(source["pose"], goal.pose_id, where + ".pose", error)) return false;
        if (!source["priority"].isNull()) {
            int priority = 0;
            if (!readInt(source["priority"], priority, 0, 255, where + ".priority", error)) return false;
            goal.priority = static_cast<uint8_t>(priority);
        }
        if (!source["duration_ms"].isNull()) {
            int duration = 0;
            if (!readInt(source["duration_ms"], duration, 0, 600000, where + ".duration_ms", error)) return false;
            goal.duration_ms = static_cast<uint32_t>(duration);
        }
        if (!source["events"].isNull()) {
            const JsonArrayConst events = source["events"].as<JsonArrayConst>();
            if (events.isNull()) {
                error = where + ".events: must be an array";
                return false;
            }
            if (!parseEvents(events, goal.event_mask, where, error)) return false;
        }
        const JsonArrayConst programs = source["programs"].as<JsonArrayConst>();
        if (!programs.isNull()) {
            if (programs.size() > kMaxAnimationPrograms) {
                error =
                    where + ".programs: must contain at most " + std::to_string(kMaxAnimationPrograms) + " programs";
                return false;
            }
            for (JsonVariantConst program_value : programs) {
                const JsonObjectConst program   = program_value.as<JsonObjectConst>();
                const std::string program_where = where + ".programs[" + std::to_string(goal.program_count) + "]";
                if (program.isNull()) {
                    error = program_where + ": must be an object";
                    return false;
                }
                if (!parseProgram(program, document, goal.programs[goal.program_count], program_where, error))
                    return false;
                ++goal.program_count;
            }
        }
        ++out.goal_count;
    }
    if (out.findGoal(out.default_goal) < 0) {
        error = "animations.default_goal: must name a defined goal";
        return false;
    }
    return true;
}

bool applyValue(Node& node, MotionProperty property, float value, bool& changed)
{
    MotionRange range{};
    float* target = resolveMotionTarget(node, property, range);
    if (target == nullptr) return false;
    const float clamped = std::clamp(value, range.minimum, range.maximum);
    if (*target != clamped) {
        *target = clamped;
        changed = true;
    }
    return true;
}

float evaluateTrack(const AnimationTrack& track, float seconds)
{
    if (track.sine) {
        return track.center +
               track.amplitude * std::sin((seconds / track.period_seconds) * 6.28318530718f + track.phase_radians);
    }
    if (track.keyframe_count == 0) return 0.0f;
    const MotionKeyframe& first = track.keyframes[0];
    if (seconds <= first.time_seconds) return first.value;
    for (uint8_t index = 1; index < track.keyframe_count; ++index) {
        const MotionKeyframe& previous = track.keyframes[index - 1];
        const MotionKeyframe& next     = track.keyframes[index];
        if (seconds <= next.time_seconds) {
            float amount = (seconds - previous.time_seconds) / (next.time_seconds - previous.time_seconds);
            if (track.easing == MotionEasing::Smoothstep) amount = amount * amount * (3.0f - 2.0f * amount);
            return previous.value + (next.value - previous.value) * amount;
        }
    }
    return track.keyframes[track.keyframe_count - 1].value;
}

}  // namespace

int AnimationDoc::findGoal(const std::string& id) const
{
    for (uint8_t index = 0; index < goal_count; ++index) {
        if (goals[index].id == id) return static_cast<int>(index);
    }
    return -1;
}

const Pose* AnimationDoc::findPose(const std::string& id) const
{
    for (uint8_t index = 0; index < pose_count; ++index) {
        if (poses[index].id == id) return &poses[index];
    }
    return nullptr;
}

bool parseAnimations(const std::string& poses_json, const std::string& animations_json, const SceneDoc& document,
                     AnimationDoc& out, std::string& error)
{
    AnimationDoc parsed{};
    error.clear();
    if (!parsePoses(poses_json, document, parsed, error)) return false;
    if (!parseAnimationsJson(animations_json, document, parsed, error)) return false;
    out = std::move(parsed);
    return true;
}

bool applyAnimationFrame(const AnimationDoc& animations, int goal_index, float time_seconds,
                         std::array<Node, kMaxNodes>& nodes, size_t node_count)
{
    if (goal_index < 0 || goal_index >= static_cast<int>(animations.goal_count) || !std::isfinite(time_seconds) ||
        time_seconds < 0.0f)
        return false;
    const AnimationGoal& goal = animations.goals[goal_index];
    bool changed              = false;

    if (const Pose* pose = animations.findPose(goal.pose_id)) {
        for (uint8_t index = 0; index < pose->target_count; ++index) {
            const PoseTarget& target = pose->targets[index];
            if (!target.mappable || target.element != 0 || target.node_index < 0 ||
                static_cast<size_t>(target.node_index) >= node_count)
                continue;
            applyValue(nodes[target.node_index], target.property, target.value, changed);
        }
    }

    struct Candidate {
        int node                    = -1;
        MotionProperty property     = MotionProperty::Rotation;
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
        const float local = program.loop ? std::fmod(time_seconds, program.duration_seconds) : time_seconds;
        for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
            const AnimationTrack& track = program.tracks[track_index];
            if (!track.mappable || track.element != 0 || track.node_index < 0 ||
                static_cast<size_t>(track.node_index) >= node_count)
                continue;
            if (candidate_count == kMaxAnimationCandidates) break;
            Candidate& candidate    = candidates[candidate_count++];
            candidate.node          = track.node_index;
            candidate.property      = track.property;
            candidate.element       = track.element;
            candidate.track         = &track;
            candidate.local_seconds = local;
            candidate.priority      = static_cast<uint16_t>(program.priority) * 256u + track.priority;
        }
    }

    const auto sameChannel = [](const Candidate& left, const Candidate& right) {
        return left.node == right.node && left.property == right.property && left.element == right.element;
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
        applyValue(nodes[candidate.node], candidate.property, evaluateTrack(*candidate.track, candidate.local_seconds),
                   changed);
    }
    return changed;
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

}  // namespace kiwi::studio
