/* SPDX-License-Identifier: MIT */
#include "animation_io.h"

#include <cmath>

namespace kiwi::studio {

namespace {

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

const char* easingId(pet::preset::MotionEasing easing)
{
    return easing == pet::preset::MotionEasing::Smoothstep ? "smoothstep" : "linear";
}

}  // namespace

bool readAnimations(JsonVariantConst poses, JsonVariantConst animations, const SceneDoc& document, AnimationDoc& out,
                    std::string& error)
{
    AnimationDoc parsed{};
    error.clear();

    if (!poses.isNull()) {
        if (!poses.is<JsonArrayConst>()) {
            error = "poses: must be an array";
            return false;
        }
        const JsonArrayConst array = poses.as<JsonArrayConst>();
        if (array.size() > kMaxPoses) {
            error = "poses: must contain at most " + std::to_string(kMaxPoses) + " entries";
            return false;
        }
        for (JsonVariantConst value : array) {
            const JsonObjectConst source = value.as<JsonObjectConst>();
            const std::string where      = "poses[" + std::to_string(parsed.pose_count) + "]";
            if (source.isNull()) {
                error = where + ": must be an object";
                return false;
            }
            Pose& pose = parsed.poses[parsed.pose_count];
            if (!readString(source["id"], pose.id, where + ".id", error)) return false;
            const JsonArrayConst targets = source["targets"].as<JsonArrayConst>();
            if (targets.isNull()) {
                error = where + ".targets: must be an array";
                return false;
            }
            if (targets.size() > kMaxPoseTargets) {
                error = where + ".targets: must contain at most " + std::to_string(kMaxPoseTargets) + " targets";
                return false;
            }
            for (JsonVariantConst target_value : targets) {
                const JsonObjectConst target_value_object = target_value.as<JsonObjectConst>();
                const std::string path = where + ".targets[" + std::to_string(pose.target_count) + "]";
                if (target_value_object.isNull()) {
                    error = path + ": must be an object";
                    return false;
                }
                PoseTarget& target = pose.targets[pose.target_count];
                if (!readString(target_value_object["node"], target.node_id, path + ".node", error)) return false;
                if (!readString(target_value_object["channel"], target.channel, path + ".channel", error)) return false;
                if (!target_value_object["element"].isNull()) {
                    int element = 0;
                    if (!readInt(target_value_object["element"], element, 0, 255, path + ".element", error))
                        return false;
                    target.element = static_cast<uint8_t>(element);
                }
                if (!readFloat(target_value_object["value"], target.value, path + ".value", error)) return false;
                const JsonObjectConst response = target_value_object["response"].as<JsonObjectConst>();
                if (!response.isNull()) {
                    if (!response["frequency_hz"].isNull()) {
                        if (!readFloat(response["frequency_hz"], target.response_frequency_hz,
                                       path + ".response.frequency_hz", error))
                            return false;
                        target.response_fields |= 1;
                    }
                    if (!response["damping_ratio"].isNull()) {
                        if (!readFloat(response["damping_ratio"], target.response_damping_ratio,
                                       path + ".response.damping_ratio", error))
                            return false;
                        target.response_fields |= 2;
                    }
                    if (!response["maximum_step_seconds"].isNull()) {
                        if (!readFloat(response["maximum_step_seconds"], target.response_maximum_step_seconds,
                                       path + ".response.maximum_step_seconds", error))
                            return false;
                        target.response_fields |= 4;
                    }
                }
                ++pose.target_count;
            }
            ++parsed.pose_count;
        }
    }

    if (!animations.isNull()) {
        const JsonObjectConst root = animations.as<JsonObjectConst>();
        if (root.isNull()) {
            error = "animations: must be an object";
            return false;
        }
        if (!readString(root["default_goal"], parsed.default_goal, "animations.default_goal", error)) return false;
        const JsonArrayConst goals = root["goals"].as<JsonArrayConst>();
        if (goals.isNull() || goals.size() == 0 || goals.size() > kMaxAnimationGoals) {
            error = "animations.goals: must contain 1.." + std::to_string(kMaxAnimationGoals) + " goals";
            return false;
        }
        for (JsonVariantConst goal_value : goals) {
            const JsonObjectConst source = goal_value.as<JsonObjectConst>();
            const std::string where      = "animations.goals[" + std::to_string(parsed.goal_count) + "]";
            if (source.isNull()) {
                error = where + ": must be an object";
                return false;
            }
            AnimationGoal& goal = parsed.goals[parsed.goal_count];
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
                for (JsonVariantConst event_value : events) {
                    std::string event;
                    if (!readString(event_value, event, where + ".events", error)) return false;
                    if (event == "pet") {
                        goal.event_mask |= 1;
                    } else if (event == "shake") {
                        goal.event_mask |= 2;
                    } else {
                        error = where + ".events: must be 'pet' or 'shake'";
                        return false;
                    }
                }
            }
            const JsonArrayConst programs = source["programs"].as<JsonArrayConst>();
            if (!programs.isNull()) {
                if (programs.size() > kMaxAnimationPrograms) {
                    error = where + ".programs: must contain at most " + std::to_string(kMaxAnimationPrograms) +
                            " programs";
                    return false;
                }
                for (JsonVariantConst program_value : programs) {
                    const JsonObjectConst program   = program_value.as<JsonObjectConst>();
                    const std::string program_where = where + ".programs[" + std::to_string(goal.program_count) + "]";
                    if (program.isNull()) {
                        error = program_where + ": must be an object";
                        return false;
                    }
                    AnimationProgram& parsed_program = goal.programs[goal.program_count];
                    if (!readString(program["id"], parsed_program.id, program_where + ".id", error)) return false;
                    if (!readFloat(program["duration_seconds"], parsed_program.duration_seconds,
                                   program_where + ".duration_seconds", error))
                        return false;
                    if (!program["loop"].is<bool>()) {
                        error = program_where + ".loop: must be a boolean";
                        return false;
                    }
                    parsed_program.loop = program["loop"].as<bool>();
                    if (!program["priority"].isNull()) {
                        int priority = 0;
                        if (!readInt(program["priority"], priority, 0, 255, program_where + ".priority", error))
                            return false;
                        parsed_program.priority = static_cast<uint8_t>(priority);
                    }
                    const JsonArrayConst tracks = program["tracks"].as<JsonArrayConst>();
                    if (tracks.isNull()) {
                        error = program_where + ".tracks: must be an array";
                        return false;
                    }
                    if (tracks.size() > kMaxAnimationTracks) {
                        error = program_where + ".tracks: must contain at most " + std::to_string(kMaxAnimationTracks) +
                                " tracks";
                        return false;
                    }
                    for (JsonVariantConst track_value : tracks) {
                        const JsonObjectConst track_object = track_value.as<JsonObjectConst>();
                        const std::string track_where =
                            program_where + ".tracks[" + std::to_string(parsed_program.track_count) + "]";
                        if (track_object.isNull()) {
                            error = track_where + ": must be an object";
                            return false;
                        }
                        AnimationTrack& track = parsed_program.tracks[parsed_program.track_count];
                        if (!readString(track_object["node"], track.node_id, track_where + ".node", error))
                            return false;
                        if (!readString(track_object["channel"], track.channel, track_where + ".channel", error))
                            return false;
                        if (!track_object["element"].isNull()) {
                            int element = 0;
                            if (!readInt(track_object["element"], element, 0, 255, track_where + ".element", error))
                                return false;
                            track.element = static_cast<uint8_t>(element);
                        }
                        if (!track_object["priority"].isNull()) {
                            int priority = 0;
                            if (!readInt(track_object["priority"], priority, 0, 255, track_where + ".priority", error))
                                return false;
                            track.priority = static_cast<uint8_t>(priority);
                        }
                        const JsonObjectConst source_object = track_object["source"].as<JsonObjectConst>();
                        if (source_object.isNull()) {
                            error = track_where + ".source: must be an object";
                            return false;
                        }
                        std::string kind;
                        if (!readString(source_object["kind"], kind, track_where + ".source.kind", error)) return false;
                        if (kind == "sine") {
                            track.sine = true;
                            if (!readFloat(source_object["center"], track.center, track_where + ".source.center",
                                           error))
                                return false;
                            if (!readFloat(source_object["amplitude"], track.amplitude,
                                           track_where + ".source.amplitude", error))
                                return false;
                            if (!readFloat(source_object["period_seconds"], track.period_seconds,
                                           track_where + ".source.period_seconds", error))
                                return false;
                            if (!source_object["phase_radians"].isNull() &&
                                !readFloat(source_object["phase_radians"], track.phase_radians,
                                           track_where + ".source.phase_radians", error))
                                return false;
                        } else if (kind == "keyframes") {
                            track.sine = false;
                            if (!source_object["easing"].isNull()) {
                                std::string easing;
                                if (!readString(source_object["easing"], easing, track_where + ".source.easing", error))
                                    return false;
                                if (easing == "smoothstep") {
                                    track.easing = pet::preset::MotionEasing::Smoothstep;
                                } else if (easing != "linear") {
                                    error = track_where + ".source.easing: must be 'linear' or 'smoothstep'";
                                    return false;
                                }
                            }
                            const JsonArrayConst keyframes = source_object["keyframes"].as<JsonArrayConst>();
                            if (keyframes.isNull() || keyframes.size() < 2 ||
                                keyframes.size() > pet::preset::kMaxMotionKeyframes) {
                                error = track_where + ".source.keyframes: must contain 2.." +
                                        std::to_string(pet::preset::kMaxMotionKeyframes) + " keyframes";
                                return false;
                            }
                            for (JsonVariantConst frame_value : keyframes) {
                                const JsonObjectConst frame = frame_value.as<JsonObjectConst>();
                                if (frame.isNull()) {
                                    error = track_where + ".source.keyframes: entries must be objects";
                                    return false;
                                }
                                pet::preset::MotionKeyframe& keyframe = track.keyframes[track.keyframe_count];
                                if (!readFloat(frame["time_seconds"], keyframe.time_seconds,
                                               track_where + ".source.keyframes.time_seconds", error))
                                    return false;
                                if (!readFloat(frame["value"], keyframe.value, track_where + ".source.keyframes.value",
                                               error))
                                    return false;
                                ++track.keyframe_count;
                            }
                        } else {
                            error = track_where + ".source.kind: must be 'keyframes' or 'sine'";
                            return false;
                        }
                        const JsonObjectConst response = track_object["response"].as<JsonObjectConst>();
                        if (!response.isNull()) {
                            if (!response["frequency_hz"].isNull()) {
                                if (!readFloat(response["frequency_hz"], track.response_frequency_hz,
                                               track_where + ".response.frequency_hz", error))
                                    return false;
                                track.response_fields |= 1;
                            }
                            if (!response["damping_ratio"].isNull()) {
                                if (!readFloat(response["damping_ratio"], track.response_damping_ratio,
                                               track_where + ".response.damping_ratio", error))
                                    return false;
                                track.response_fields |= 2;
                            }
                            if (!response["maximum_step_seconds"].isNull()) {
                                if (!readFloat(response["maximum_step_seconds"], track.response_maximum_step_seconds,
                                               track_where + ".response.maximum_step_seconds", error))
                                    return false;
                                track.response_fields |= 4;
                            }
                        }
                        ++parsed_program.track_count;
                    }
                    ++goal.program_count;
                }
            }
            ++parsed.goal_count;
        }
    }

    if (!validateAnimationDoc(document, parsed, error)) return false;
    out = std::move(parsed);
    return true;
}

void writeAnimationDoc(JsonDocument& document, const SceneDoc& scene)
{
    const AnimationDoc& animation = scene.animations;
    if (animation.pose_count > 0) {
        auto poses = document["poses"].to<JsonArray>();
        for (uint8_t pose_index = 0; pose_index < animation.pose_count; ++pose_index) {
            const Pose& pose  = animation.poses[pose_index];
            auto pose_object  = poses.add<JsonObject>();
            pose_object["id"] = pose.id;
            auto targets      = pose_object["targets"].to<JsonArray>();
            for (uint8_t target_index = 0; target_index < pose.target_count; ++target_index) {
                const PoseTarget& target = pose.targets[target_index];
                auto target_object       = targets.add<JsonObject>();
                target_object["node"]    = target.node_id;
                target_object["channel"] = target.channel;
                const ChannelInfo* info  = findChannel(target.channel.c_str());
                if (info != nullptr && info->needs_element) target_object["element"] = target.element;
                target_object["value"] = target.value;
                if (target.response_fields != 0) {
                    auto response = target_object["response"].to<JsonObject>();
                    if ((target.response_fields & 1u) != 0) response["frequency_hz"] = target.response_frequency_hz;
                    if ((target.response_fields & 2u) != 0) response["damping_ratio"] = target.response_damping_ratio;
                    if ((target.response_fields & 4u) != 0)
                        response["maximum_step_seconds"] = target.response_maximum_step_seconds;
                }
            }
        }
    }
    if (animation.goal_count > 0) {
        auto animations            = document["animations"].to<JsonObject>();
        animations["default_goal"] = animation.default_goal;
        auto goals                 = animations["goals"].to<JsonArray>();
        for (uint8_t goal_index = 0; goal_index < animation.goal_count; ++goal_index) {
            const AnimationGoal& goal = animation.goals[goal_index];
            auto goal_object          = goals.add<JsonObject>();
            goal_object["id"]         = goal.id;
            goal_object["pose"]       = goal.pose_id;
            auto events               = goal_object["events"].to<JsonArray>();
            if ((goal.event_mask & 1u) != 0) events.add("pet");
            if ((goal.event_mask & 2u) != 0) events.add("shake");
            goal_object["priority"]    = goal.priority;
            goal_object["duration_ms"] = goal.duration_ms;
            auto programs              = goal_object["programs"].to<JsonArray>();
            for (uint8_t program_index = 0; program_index < goal.program_count; ++program_index) {
                const AnimationProgram& program    = goal.programs[program_index];
                auto program_object                = programs.add<JsonObject>();
                program_object["id"]               = program.id;
                program_object["duration_seconds"] = program.duration_seconds;
                program_object["loop"]             = program.loop;
                program_object["priority"]         = program.priority;
                auto tracks                        = program_object["tracks"].to<JsonArray>();
                for (uint8_t track_index = 0; track_index < program.track_count; ++track_index) {
                    const AnimationTrack& track = program.tracks[track_index];
                    auto track_object           = tracks.add<JsonObject>();
                    track_object["node"]        = track.node_id;
                    track_object["channel"]     = track.channel;
                    const ChannelInfo* info     = findChannel(track.channel.c_str());
                    if (info != nullptr && info->needs_element) track_object["element"] = track.element;
                    track_object["priority"] = track.priority;
                    if (track.response_fields != 0) {
                        auto response = track_object["response"].to<JsonObject>();
                        if ((track.response_fields & 1u) != 0) response["frequency_hz"] = track.response_frequency_hz;
                        if ((track.response_fields & 2u) != 0) response["damping_ratio"] = track.response_damping_ratio;
                        if ((track.response_fields & 4u) != 0)
                            response["maximum_step_seconds"] = track.response_maximum_step_seconds;
                    }
                    auto source = track_object["source"].to<JsonObject>();
                    if (track.sine) {
                        source["kind"]           = "sine";
                        source["center"]         = track.center;
                        source["amplitude"]      = track.amplitude;
                        source["period_seconds"] = track.period_seconds;
                        source["phase_radians"]  = track.phase_radians;
                    } else {
                        source["kind"]   = "keyframes";
                        source["easing"] = easingId(track.easing);
                        auto keyframes   = source["keyframes"].to<JsonArray>();
                        for (uint8_t frame = 0; frame < track.keyframe_count; ++frame) {
                            auto keyframe            = keyframes.add<JsonObject>();
                            keyframe["time_seconds"] = track.keyframes[frame].time_seconds;
                            keyframe["value"]        = track.keyframes[frame].value;
                        }
                    }
                }
            }
        }
    }
}

}  // namespace kiwi::studio
