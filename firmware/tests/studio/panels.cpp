/* SPDX-License-Identifier: MIT */
#include "panels.h"

#include "animations.h"

#include "document.h"
#include "geometry.h"
#include "motion_model.h"
#include "persistence.h"
#include "session.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace kiwi::studio {
namespace {
using namespace pet::shape;
}  // namespace

static bool resetPopup(const char* label, const char* text)
{
    const std::string id = std::string("##reset-") + label;
    if (!ImGui::BeginPopupContextItem(id.c_str())) return false;
    const bool reset = ImGui::MenuItem(text);
    ImGui::EndPopup();
    return reset;
}

static bool floatField(const char* label, float& value, float minimum, float maximum, float default_value, float speed,
                       const char* format = "%.2f")
{
    bool changed      = false;
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(80.0f, width - 116.0f));
    changed |= ImGui::DragFloat(label, &value, speed, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(76.0f);
    changed |= ImGui::InputFloat((std::string("##exact-") + label).c_str(), &value, 0.0f, 0.0f, format);
    if (!std::isfinite(value)) value = default_value;
    value = std::clamp(value, minimum, maximum);
    if (ImGui::IsItemHovered()) {
        char hint[96];
        snprintf(hint, sizeof(hint), "Safe range %.3g to %.3g · right-click to reset", minimum, maximum);
        ImGui::SetTooltip("%s", hint);
    }
    char reset_text[64];
    snprintf(reset_text, sizeof(reset_text), "Reset to %.3g", default_value);
    changed |= resetPopup(label, reset_text);
    return changed;
}

static bool pointField(const char* label, Vec2& value, Vec2 default_value, float speed,
                       float minimum = kMinNodeCoordinate, float maximum = kMaxNodeCoordinate)
{
    bool changed = ImGui::DragFloat2(label, &value.x, speed, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) value = default_value;
    value.x = std::clamp(value.x, minimum, maximum);
    value.y = std::clamp(value.y, minimum, maximum);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+click a value to type it exactly");
    char reset_text[96];
    snprintf(reset_text, sizeof(reset_text), "Reset to {%.3g, %.3g}", default_value.x, default_value.y);
    changed |= resetPopup(label, reset_text);
    return changed;
}

void toolbar(SceneDoc& document, EditorState& editor, History& history, ToolbarCommands& commands)
{
    commands.replaced         = false;
    commands.browse_requested = false;
    if (ImGui::Button("New")) {
        if (editor.dirty)
            commands.pending = Pending::New;
        else {
            clearScene(document);
            editor.selection  = -1;
            editor.dirty      = false;
            commands.replaced = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (editor.dirty)
            commands.pending = Pending::Reset;
        else {
            resetScene(document);
            editor.selection  = 0;
            editor.dirty      = false;
            commands.replaced = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) commands.browse_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Save")) saveSession(document, editor);
    ImGui::SameLine();
    if (ImGui::Button("Validate")) commands.validate_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Export SVG"))
        editor.status = exportSvg(document, "kiwi-face.svg") ? "Wrote kiwi-face.svg at StackChan LCD dimensions"
                                                             : "Could not export kiwi-face.svg";
    ImGui::SameLine();
    ImGui::Dummy({8.0f, 0.0f});
    ImGui::SameLine();
    ImGui::BeginDisabled(!history.canUndo());
    if (ImGui::Button("Undo")) {
        history.undo(document);
        editor.dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history.canRedo());
    if (ImGui::Button("Redo")) {
        history.redo(document);
        editor.dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("|  Ctrl+S save  Ctrl+Z/Y undo/redo  Ctrl+D duplicate  Del delete  F frame  R reset view");

    char path[256];
    snprintf(path, sizeof(path), "%s", editor.file_path.c_str());
    ImGui::SetNextItemWidth(420.0f);
    if (ImGui::InputText("Scene file", path, sizeof(path))) editor.file_path = path;
    ImGui::SameLine();
    if (ImGui::Button("Frame all")) editor.frame_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) editor.reset_requested = true;
    ImGui::SameLine();
    ImGui::Checkbox("Bounds", &editor.show_bounds);
    ImGui::SameLine();
    ImGui::Checkbox("Guides", &editor.show_guides);
    ImGui::SameLine();
    ImGui::Checkbox("Corners", &editor.show_corners);
    ImGui::SameLine();
    ImGui::Checkbox("Edges", &editor.show_edges);
}

bool scenePanel(SceneDoc& document, EditorState& editor)
{
    bool changed = false;
    ImGui::SeparatorText("Scene");
    char scene_id[48];
    snprintf(scene_id, sizeof(scene_id), "%s", document.id.c_str());
    if (ImGui::InputText("Id", scene_id, sizeof(scene_id))) document.id = scene_id, changed = true;
    float background[3] = {document.background.red / 255.0f, document.background.green / 255.0f,
                           document.background.blue / 255.0f};
    if (ImGui::ColorEdit3("Background", background)) {
        document.background = {uint8_t(background[0] * 255), uint8_t(background[1] * 255),
                               uint8_t(background[2] * 255)};
        changed             = true;
    }

    ImGui::SeparatorText("Nodes");
    if (ImGui::Button("Add circle") && document.count < kMaxNodes) {
        document.nodes[document.count] = makeNode(document, Kind::Circle);
        editor.selection               = int(document.count++);
        changed                        = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add polygon") && document.count < kMaxNodes) {
        document.nodes[document.count] = makeNode(document, Kind::Polygon);
        editor.selection               = int(document.count++);
        changed                        = true;
    }
    ImGui::BeginChild("node-list", {0, std::max(120.0f, ImGui::GetContentRegionAvail().y - 90.0f)}, true);
    for (size_t index = 0; index < document.count; ++index) {
        Node& node = document.nodes[index];
        ImGui::PushID(int(index));
        changed |= ImGui::Checkbox("##visible", &node.visible);
        ImGui::SameLine();
        if (nodeDepth(document, int(index)) > 0) ImGui::Indent(12.0f * nodeDepth(document, int(index)));
        if (ImGui::Selectable(node.id.c_str(), editor.selection == int(index))) editor.selection = int(index);
        if (nodeDepth(document, int(index)) > 0) ImGui::Unindent(12.0f * nodeDepth(document, int(index)));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(editor.selection < 0 || editor.selection >= int(document.count));
    if (ImGui::Button("Duplicate")) {
        int new_selection = editor.selection;
        std::string error;
        if (duplicateNode(document, editor.selection, new_selection, error)) {
            editor.selection = new_selection;
            changed          = true;
        } else {
            editor.status = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        int new_selection = editor.selection;
        std::string error;
        if (deleteSelected(document, editor.selection, new_selection, error)) {
            editor.selection = new_selection;
            changed          = true;
        } else {
            editor.status = error;
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(editor.selection <= 0 || editor.selection >= int(document.count));
    if (ImGui::Button("Move up")) {
        int new_selection = editor.selection;
        std::string error;
        if (moveNode(document, editor.selection, editor.selection - 1, new_selection, error)) {
            editor.selection = new_selection;
            changed          = true;
        } else {
            editor.status = error;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(editor.selection < 0 || size_t(editor.selection + 1) >= document.count);
    if (ImGui::Button("Move down")) {
        int new_selection = editor.selection;
        std::string error;
        if (moveNode(document, editor.selection, editor.selection + 1, new_selection, error)) {
            editor.selection = new_selection;
            changed          = true;
        } else {
            editor.status = error;
        }
    }
    ImGui::EndDisabled();
    return changed;
}

static bool idField(const char* label, std::string& id)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s", id.c_str());
    if (!ImGui::InputText(label, buffer, sizeof(buffer))) return false;
    id = buffer;
    return true;
}

static bool nodeCombo(const char* label, const SceneDoc& document, std::string& id)
{
    bool changed = false;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo(label, id.empty() ? "(none)" : id.c_str())) {
        for (size_t index = 0; index < document.count; ++index) {
            const bool selected = id == document.nodes[index].id;
            if (ImGui::Selectable(document.nodes[index].id.c_str(), selected)) {
                id      = document.nodes[index].id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool channelCombo(const char* label, std::string& channel, bool is_polygon)
{
    bool changed = false;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo(label, channel.empty() ? "(none)" : channel.c_str())) {
        for (size_t index = 0; index < channelInfoCount(); ++index) {
            const ChannelInfo& info = channelInfos()[index];
            if (!channelAppliesTo(info.node_kind, is_polygon)) continue;
            const bool selected = channel == info.id;
            if (ImGui::Selectable(info.id, selected)) {
                channel = info.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename T, size_t N>
static void eraseItem(uint8_t& count, std::array<T, N>& items, int index)
{
    if (index < 0 || index >= static_cast<int>(count)) return;
    for (int move = index; move + 1 < static_cast<int>(count); ++move) items[move] = std::move(items[move + 1]);
    --count;
}

/** Picks the first channel valid for `is_polygon` when a node/kind change invalidates one. */
static void repairChannel(std::string& channel, bool is_polygon)
{
    const ChannelInfo* info = findChannel(channel.c_str());
    if (info != nullptr && channelAppliesTo(info->node_kind, is_polygon)) return;
    for (size_t index = 0; index < channelInfoCount(); ++index) {
        if (channelAppliesTo(channelInfos()[index].node_kind, is_polygon)) {
            channel = channelInfos()[index].id;
            return;
        }
    }
}

static void seedKeyframes(AnimationTrack& track, const Node& node, const ChannelInfo& info, float duration)
{
    const float value    = channelValue(node, info, track.element);
    track.keyframe_count = 2;
    track.keyframes[0]   = {0.0f, value};
    track.keyframes[1]   = {duration, value};
}

static bool animationPanel(SceneDoc& document, EditorState& editor)
{
    bool changed            = false;
    AnimationDoc& animation = document.animations;
    ImGui::SeparatorText("Animation");

    if (animation.goal_count == 0) {
        ImGui::TextDisabled("No animation goals.");
        if (ImGui::Button("Create idle goal")) {
            makeDefaultAnimation(animation);
            editor.animation_preview.goal_index = 0;
            changed                             = true;
        }
        return changed;
    }

    ImGui::TextUnformatted("Default goal");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##default-goal", animation.default_goal.c_str())) {
        for (int index = 0; index < int(animation.goal_count); ++index) {
            const bool selected = animation.default_goal == animation.goals[index].id;
            if (ImGui::Selectable(animation.goals[index].id.c_str(), selected)) {
                animation.default_goal = animation.goals[index].id;
                changed                = true;
            }
        }
        ImGui::EndCombo();
    }

    const int goal_count = int(animation.goal_count);
    if (editor.animation_preview.goal_index < 0 || editor.animation_preview.goal_index >= goal_count)
        editor.animation_preview.goal_index = animation.findGoal(animation.default_goal);
    const char* preview = editor.animation_preview.goal_index >= 0
                              ? animation.goals[editor.animation_preview.goal_index].id.c_str()
                              : "(none)";
    ImGui::TextUnformatted("Preview goal");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##preview-goal", preview)) {
        for (int index = 0; index < goal_count; ++index) {
            if (ImGui::Selectable(animation.goals[index].id.c_str(), editor.animation_preview.goal_index == index)) {
                editor.animation_preview.goal_index   = index;
                editor.animation_preview.time_seconds = 0.0f;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button(editor.animation_preview.playing ? "Pause" : "Play"))
        editor.animation_preview.playing = !editor.animation_preview.playing;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) editor.animation_preview.time_seconds = 0.0f;
    ImGui::SameLine();
    ImGui::Text("%.2fs", editor.animation_preview.time_seconds);

    if (ImGui::CollapsingHeader("Poses")) {
        for (int pose_index = 0; pose_index < int(animation.pose_count); ++pose_index) {
            Pose& pose = animation.poses[pose_index];
            ImGui::PushID(pose_index);
            const std::string title = "Pose " + pose.id;
            ImGui::SeparatorText(title.c_str());
            changed |= idField("Id##pose", pose.id);
            bool referenced = false;
            for (int goal = 0; goal < int(animation.goal_count); ++goal)
                referenced |= animation.goals[goal].pose_id == pose.id;
            ImGui::BeginDisabled(referenced);
            if (ImGui::Button("Delete pose")) {
                eraseItem(animation.pose_count, animation.poses, pose_index);
                changed = true;
                ImGui::EndDisabled();
                ImGui::PopID();
                --pose_index;
                continue;
            }
            ImGui::EndDisabled();
            for (int target_index = 0; target_index < int(pose.target_count); ++target_index) {
                PoseTarget& target = pose.targets[target_index];
                ImGui::PushID(1000 + target_index);
                const int node_index = findNodeIndex(document, target.node_id);
                if (nodeCombo("Node", document, target.node_id)) {
                    const int moved = findNodeIndex(document, target.node_id);
                    if (moved >= 0) repairChannel(target.channel, document.nodes[moved].kind == Kind::Polygon);
                    changed = true;
                }
                const bool is_polygon = node_index >= 0 && document.nodes[node_index].kind == Kind::Polygon;
                if (channelCombo("Channel", target.channel, is_polygon)) {
                    target.element = 0;
                    changed        = true;
                }
                const ChannelInfo* info = findChannel(target.channel.c_str());
                if (info != nullptr) {
                    if (info->needs_element && node_index >= 0) {
                        int element       = target.element;
                        const int maximum = std::max(0, int(document.nodes[node_index].polygon.corner_count) - 1);
                        if (ImGui::DragInt("Element", &element, 0.1f, 0, maximum)) {
                            target.element = uint8_t(std::clamp(element, 0, maximum));
                            changed        = true;
                        }
                    }
                    const float step = (info->maximum - info->minimum) / 200.0f;
                    if (ImGui::DragFloat("Value", &target.value, step, info->minimum, info->maximum)) changed = true;
                    bool custom = target.response_fields != 0;
                    if (ImGui::Checkbox("Spring override", &custom)) {
                        target.response_fields = custom ? 7 : 0;
                        changed                = true;
                    }
                    if (target.response_fields != 0) {
                        if (ImGui::DragFloat("Hz", &target.response_frequency_hz, 0.05f, 0.01f, 30.0f)) {
                            target.response_fields |= 1;
                            changed = true;
                        }
                        if (ImGui::DragFloat("Damping", &target.response_damping_ratio, 0.01f, 0.0f, 4.0f)) {
                            target.response_fields |= 2;
                            changed = true;
                        }
                        if (ImGui::DragFloat("Max step", &target.response_maximum_step_seconds, 0.0005f, 0.001f, 0.1f,
                                             "%.4f")) {
                            target.response_fields |= 4;
                            changed = true;
                        }
                    }
                }
                if (ImGui::Button("Remove target")) {
                    eraseItem(pose.target_count, pose.targets, target_index);
                    changed = true;
                    ImGui::PopID();
                    --target_index;
                    continue;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add target") && pose.target_count < kMaxPoseTargets && document.count > 0) {
                const Node& node   = document.nodes[0];
                const bool polygon = node.kind == Kind::Polygon;
                for (size_t channel = 0; channel < channelInfoCount(); ++channel) {
                    const ChannelInfo* info = &channelInfos()[channel];
                    if (!channelAppliesTo(info->node_kind, polygon)) continue;
                    if (ensurePoseTarget(pose, node, *info, 0)) changed = true;
                    break;
                }
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add pose") && animation.pose_count < kMaxPoses) {
            Pose& pose = animation.poses[animation.pose_count];
            pose       = {};
            pose.id    = "pose-" + std::to_string(animation.pose_count + 1);
            ++animation.pose_count;
            changed = true;
        }
    }

    if (ImGui::CollapsingHeader("Goals", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int goal_index = 0; goal_index < int(animation.goal_count); ++goal_index) {
            AnimationGoal& goal = animation.goals[goal_index];
            ImGui::PushID(goal_index);
            const std::string title = "Goal " + goal.id;
            if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= idField("Id##goal", goal.id);
                ImGui::TextUnformatted("Pose");
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##goal-pose", goal.pose_id.c_str())) {
                    for (int pose_index = 0; pose_index < int(animation.pose_count); ++pose_index) {
                        if (ImGui::Selectable(animation.poses[pose_index].id.c_str(),
                                              goal.pose_id == animation.poses[pose_index].id)) {
                            goal.pose_id = animation.poses[pose_index].id;
                            changed      = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                int priority = goal.priority;
                if (ImGui::DragInt("Priority", &priority, 0.1f, 0, 255)) {
                    goal.priority = uint8_t(priority);
                    changed       = true;
                }
                bool pet            = (goal.event_mask & 1u) != 0;
                bool shake          = (goal.event_mask & 2u) != 0;
                bool events_changed = ImGui::Checkbox("Pet", &pet);
                ImGui::SameLine();
                events_changed |= ImGui::Checkbox("Shake", &shake);
                if (events_changed) {
                    goal.event_mask = uint8_t((pet ? 1 : 0) | (shake ? 2 : 0));
                    if (goal.event_mask == 0)
                        goal.duration_ms = 0;
                    else if (goal.duration_ms == 0)
                        goal.duration_ms = 1000;
                    changed = true;
                }
                int duration = int(goal.duration_ms);
                ImGui::BeginDisabled(goal.event_mask == 0);
                if (ImGui::DragInt("Duration (ms)", &duration, 10.0f, 1, 600000)) {
                    goal.duration_ms = uint32_t(duration);
                    changed          = true;
                }
                ImGui::EndDisabled();
                if (ImGui::Button("Delete goal")) {
                    eraseItem(animation.goal_count, animation.goals, goal_index);
                    changed = true;
                    ImGui::TreePop();
                    ImGui::PopID();
                    --goal_index;
                    continue;
                }
                ImGui::SeparatorText("Programs");
                for (int program_index = 0; program_index < int(goal.program_count); ++program_index) {
                    AnimationProgram& program = goal.programs[program_index];
                    ImGui::PushID(2000 + program_index);
                    const std::string program_title = "Program " + program.id;
                    if (ImGui::TreeNode(program_title.c_str())) {
                        changed |= idField("Id##program", program.id);
                        if (ImGui::DragFloat("Duration (s)", &program.duration_seconds, 0.01f, 0.001f, 600.0f))
                            changed = true;
                        changed |= ImGui::Checkbox("Loop", &program.loop);
                        int program_priority = program.priority;
                        if (ImGui::DragInt("Priority##program", &program_priority, 0.1f, 0, 255)) {
                            program.priority = uint8_t(program_priority);
                            changed          = true;
                        }
                        for (int track_index = 0; track_index < int(program.track_count); ++track_index) {
                            AnimationTrack& track = program.tracks[track_index];
                            ImGui::PushID(3000 + track_index);
                            const std::string track_title = "Track " + track.node_id + " - " + track.channel;
                            if (ImGui::TreeNode(track_title.c_str())) {
                                if (nodeCombo("Node", document, track.node_id)) {
                                    const int moved = findNodeIndex(document, track.node_id);
                                    if (moved >= 0)
                                        repairChannel(track.channel, document.nodes[moved].kind == Kind::Polygon);
                                    changed = true;
                                }
                                const int node_index = findNodeIndex(document, track.node_id);
                                const bool is_polygon =
                                    node_index >= 0 && document.nodes[node_index].kind == Kind::Polygon;
                                if (channelCombo("Channel", track.channel, is_polygon)) {
                                    track.element = 0;
                                    changed       = true;
                                }
                                const ChannelInfo* info = findChannel(track.channel.c_str());
                                if (info != nullptr && info->needs_element && node_index >= 0) {
                                    int element = track.element;
                                    const int maximum =
                                        std::max(0, int(document.nodes[node_index].polygon.corner_count) - 1);
                                    if (ImGui::DragInt("Element", &element, 0.1f, 0, maximum)) {
                                        track.element = uint8_t(std::clamp(element, 0, maximum));
                                        changed       = true;
                                    }
                                }
                                int track_priority = track.priority;
                                if (ImGui::DragInt("Priority##track", &track_priority, 0.1f, 0, 255)) {
                                    track.priority = uint8_t(track_priority);
                                    changed        = true;
                                }
                                bool sine = track.sine;
                                if (ImGui::Checkbox("Sine source", &sine)) {
                                    track.sine = sine;
                                    if (!sine && track.keyframe_count < 2 && node_index >= 0 && info != nullptr)
                                        seedKeyframes(track, document.nodes[node_index], *info,
                                                      program.duration_seconds);
                                    changed = true;
                                }
                                if (track.sine) {
                                    if (ImGui::DragFloat("Center", &track.center, 0.05f)) changed = true;
                                    if (ImGui::DragFloat("Amplitude", &track.amplitude, 0.05f, 0.0f)) changed = true;
                                    if (ImGui::DragFloat("Period (s)", &track.period_seconds, 0.01f, 0.001f))
                                        changed = true;
                                    if (ImGui::DragFloat("Phase (rad)", &track.phase_radians, 0.01f)) changed = true;
                                    bool custom = track.response_fields != 0;
                                    if (ImGui::Checkbox("Spring override##track", &custom)) {
                                        track.response_fields = custom ? 7 : 0;
                                        changed               = true;
                                    }
                                    if (track.response_fields != 0) {
                                        if (ImGui::DragFloat("Hz##track", &track.response_frequency_hz, 0.05f, 0.01f,
                                                             30.0f)) {
                                            track.response_fields |= 1;
                                            changed = true;
                                        }
                                        if (ImGui::DragFloat("Damping##track", &track.response_damping_ratio, 0.01f,
                                                             0.0f, 4.0f)) {
                                            track.response_fields |= 2;
                                            changed = true;
                                        }
                                        if (ImGui::DragFloat("Max step##track", &track.response_maximum_step_seconds,
                                                             0.0005f, 0.001f, 0.1f, "%.4f")) {
                                            track.response_fields |= 4;
                                            changed = true;
                                        }
                                    }
                                } else {
                                    ImGui::TextDisabled("Keyframes");
                                    for (int frame = 0; frame < int(track.keyframe_count); ++frame) {
                                        ImGui::PushID(frame);
                                        float time        = track.keyframes[frame].time_seconds;
                                        float frame_value = track.keyframes[frame].value;
                                        if (ImGui::DragFloat("t", &time, 0.01f, 0.0f, program.duration_seconds,
                                                             "%.2f")) {
                                            track.keyframes[frame].time_seconds = time;
                                            changed                             = true;
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::DragFloat("v", &frame_value, 0.05f)) {
                                            track.keyframes[frame].value = frame_value;
                                            changed                      = true;
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("x")) {
                                            eraseItem(track.keyframe_count, track.keyframes, frame);
                                            changed = true;
                                            ImGui::PopID();
                                            --frame;
                                            continue;
                                        }
                                        ImGui::PopID();
                                    }
                                    if (ImGui::Button("Add keyframe") &&
                                        track.keyframe_count < pet::preset::kMaxMotionKeyframes) {
                                        const float last_time =
                                            track.keyframe_count == 0
                                                ? 0.0f
                                                : track.keyframes[track.keyframe_count - 1].time_seconds;
                                        const float next_time = std::min(program.duration_seconds, last_time + 0.1f);
                                        if (next_time > last_time) {
                                            track.keyframes[track.keyframe_count++] = {
                                                next_time, track.keyframe_count == 0
                                                               ? 0.0f
                                                               : track.keyframes[track.keyframe_count - 1].value};
                                            changed = true;
                                        }
                                    }
                                    ImGui::SameLine();
                                    int easing = track.easing == pet::preset::MotionEasing::Smoothstep ? 1 : 0;
                                    if (ImGui::Combo("Easing", &easing, "linear\0smoothstep\0")) {
                                        track.easing = easing == 1 ? pet::preset::MotionEasing::Smoothstep
                                                                   : pet::preset::MotionEasing::Linear;
                                        changed      = true;
                                    }
                                }
                                if (info != nullptr && node_index >= 0) {
                                    const int pose_index = animation.findPose(goal.pose_id);
                                    if (pose_index >= 0) {
                                        const uint8_t before = animation.poses[pose_index].target_count;
                                        ensurePoseTarget(animation.poses[pose_index], document.nodes[node_index], *info,
                                                         track.element);
                                        if (animation.poses[pose_index].target_count != before) changed = true;
                                    }
                                }
                                if (ImGui::Button("Remove track")) {
                                    eraseItem(program.track_count, program.tracks, track_index);
                                    changed = true;
                                    ImGui::TreePop();
                                    ImGui::PopID();
                                    --track_index;
                                    continue;
                                }
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        if (ImGui::Button("Add track") && program.track_count < kMaxAnimationTracks &&
                            document.count > 0) {
                            const Node& node      = document.nodes[0];
                            const bool polygon    = node.kind == Kind::Polygon;
                            AnimationTrack& track = program.tracks[program.track_count];
                            track                 = {};
                            track.node_id         = node.id;
                            track.sine            = true;
                            track.period_seconds  = std::max(0.001f, program.duration_seconds);
                            for (size_t channel = 0; channel < channelInfoCount(); ++channel) {
                                const ChannelInfo* info = &channelInfos()[channel];
                                if (!channelAppliesTo(info->node_kind, polygon)) continue;
                                track.channel = info->id;
                                track.center  = channelValue(node, *info, 0);
                                break;
                            }
                            const int pose_index = animation.findPose(goal.pose_id);
                            if (pose_index >= 0) {
                                const ChannelInfo* info = findChannel(track.channel.c_str());
                                if (info != nullptr) ensurePoseTarget(animation.poses[pose_index], node, *info, 0);
                            }
                            ++program.track_count;
                            changed = true;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (ImGui::Button("Delete program") && goal.program_count > 0) {
                    eraseItem(goal.program_count, goal.programs, goal.program_count - 1);
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Add program") && goal.program_count < kMaxAnimationPrograms) {
                    AnimationProgram& program = goal.programs[goal.program_count];
                    program                   = {};
                    program.id                = "program-" + std::to_string(goal.program_count + 1);
                    program.duration_seconds  = 2.0f;
                    program.loop              = true;
                    ++goal.program_count;
                    changed = true;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add goal") && animation.goal_count < kMaxAnimationGoals && animation.pose_count > 0) {
            AnimationGoal& goal = animation.goals[animation.goal_count];
            goal                = {};
            goal.id             = "goal-" + std::to_string(animation.goal_count + 1);
            goal.pose_id        = animation.poses[0].id;
            ++animation.goal_count;
            changed = true;
        }
    }
    return changed;
}

bool inspector(SceneDoc& document, EditorState& editor)
{
    bool changed = animationPanel(document, editor);
    if (editor.selection < 0 || size_t(editor.selection) >= document.count) {
        ImGui::TextDisabled("No node selected");
        return changed;
    }
    auto& n = document.nodes[editor.selection];
    std::array<Affine2D, kMaxNodes> worlds{};
    const bool valid_hierarchy = resolveWorlds(document, worlds);
    const char* problem = valid_hierarchy ? shapeProblem(n, worlds[editor.selection]) : "invalid parent ordering";

    ImGui::Text("%s · %s", n.id.c_str(), n.kind == Kind::Circle ? "circle" : "rounded polygon");
    if (problem != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 140, 120, 255));
        ImGui::TextWrapped("Invalid: %s", problem);
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Node");
    char id[48];
    snprintf(id, sizeof(id), "%s", n.id.c_str());
    if (ImGui::InputText("Name", id, sizeof(id))) n.id = id, changed = true;
    changed |= ImGui::Checkbox("Visible", &n.visible);

    ImGui::SeparatorText("Shape");
    if (n.kind == Kind::Circle) {
        changed |= pointField("Center", n.circle.center, {160.0f, 120.0f}, .5f);
        changed |= floatField("Radius", n.circle.radius, 1.0f, 160.0f, 20.0f, .2f);
    } else {
        int count = n.polygon.corner_count;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("Corners", &count, 3, int(kMaxRoundedPolygonCorners))) {
            std::string error;
            if (setCornerCount(document, size_t(editor.selection), uint8_t(count), error))
                changed = true;
            else
                editor.status = error;
        }
        if (ImGui::BeginCombo("Shape preset", "Apply to shape")) {
            if (ImGui::Selectable("Neutral")) {
                for (int i = 0; i < n.polygon.corner_count; ++i) {
                    n.polygon.corners[i].roundness   = .08f;
                    n.polygon.corners[i].cap_tension = .55f;
                    n.polygon.edges[i].bulge         = 0.0f;
                    n.polygon.edges[i].start         = .28f;
                    n.polygon.edges[i].end           = .28f;
                }
                changed = true;
            }
            if (ImGui::Selectable("Pointed")) {
                for (int i = 0; i < n.polygon.corner_count; ++i) {
                    n.polygon.corners[i].roundness   = 0.0f;
                    n.polygon.corners[i].cap_tension = .70f;
                }
                changed = true;
            }
            if (ImGui::Selectable("Rounded")) {
                for (int i = 0; i < n.polygon.corner_count; ++i) {
                    n.polygon.corners[i].roundness   = .30f;
                    n.polygon.corners[i].cap_tension = .45f;
                }
                changed = true;
            }
            ImGui::EndCombo();
        }
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            ImGui::SeparatorText(("Corner " + std::to_string(i)).c_str());
            changed |= pointField("Position", n.polygon.corner_positions[i], {}, .5f);
            changed |= floatField("Roundness", n.polygon.corners[i].roundness, 0.0f, .35f, .08f, .005f, "%.3f");
            changed |= floatField("Cap tension", n.polygon.corners[i].cap_tension, .10f, .90f, .55f, .005f, "%.3f");
            ImGui::SeparatorText(("Edge " + std::to_string(i)).c_str());
            changed |= floatField("Bulge", n.polygon.edges[i].bulge, -.35f, .35f, 0.0f, .005f, "%.3f");
            changed |= floatField("Start", n.polygon.edges[i].start, .08f, .45f, .28f, .005f, "%.3f");
            changed |= floatField("End", n.polygon.edges[i].end, .08f, .45f, .28f, .005f, "%.3f");
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Style");
    float color_value[3] = {n.style.color.red / 255.f, n.style.color.green / 255.f, n.style.color.blue / 255.f};
    if (ImGui::ColorEdit3("Color", color_value)) {
        n.style.color = {uint8_t(color_value[0] * 255), uint8_t(color_value[1] * 255), uint8_t(color_value[2] * 255)};
        changed       = true;
    }
    changed |= floatField("Opacity", n.style.opacity, 0.0f, 1.0f, 1.0f, .01f);
    changed |= ImGui::Checkbox("Fill", &n.style.fill);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Stroke", &n.style.stroke);
    if (n.style.stroke) changed |= floatField("Stroke width", n.style.stroke_width, 0.0f, 24.0f, 0.0f, .1f);

    ImGui::SeparatorText("Transform");
    const char* parent_label = n.parent_index < 0 ? "World" : document.nodes[n.parent_index].id.c_str();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("Parent", parent_label)) {
        if (ImGui::Selectable("World", n.parent_index < 0)) n.parent_index = -1, changed = true;
        for (int index = 0; index < editor.selection; ++index)
            if (ImGui::Selectable(document.nodes[index].id.c_str(), n.parent_index == index))
                n.parent_index = index, changed = true;
        ImGui::EndCombo();
    }
    changed |= pointField("Translate", n.transform.translation, {}, .5f);
    changed |= floatField("Rotation", n.transform.rotation_degrees, -180.0f, 180.0f, 0.0f, .5f);
    changed |= pointField("Scale", n.transform.scale, {1.0f, 1.0f}, .01f, -240.0f, 240.0f);
    changed |= pointField("Pivot", n.transform.pivot, {}, .5f);

    ImGui::SeparatorText("Motion");
    MotionPreview& motion = n.motion;
    changed |= ImGui::Checkbox("Enabled", &motion.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Preview playing", &editor.motion.playing);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Loop", &motion.loop);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("Property", motionPropertyLabel(motion.property))) {
        for (size_t descriptor_index = 0; descriptor_index < motionDescriptorCount(); ++descriptor_index) {
            const MotionDescriptor& descriptor = motionDescriptors()[descriptor_index];
            if (!motionPropertyApplies(descriptor.property, n.kind)) continue;
            const bool selected = descriptor.property == motion.property;
            if (ImGui::Selectable(descriptor.label, selected)) {
                motion.property = descriptor.property;
                changed         = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(160.0f);
    int motion_type = motion.use_keyframes ? 1 : 0;
    if (ImGui::Combo("Type", &motion_type, "Loop\0Keyframes\0")) {
        motion.use_keyframes = motion_type == 1;
        changed              = true;
    }
    if (!motion.use_keyframes) {
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Amplitude", &motion.amplitude, .1f, -4096.0f, 4096.0f, "%.2f");
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Period", &motion.period_seconds, .01f, .1f, 10.0f, "%.2f s");
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Phase", &motion.phase_radians, .01f, -3.14159f, 3.14159f, "%.2f rad");
    } else {
        ImGui::SetNextItemWidth(160.0f);
        changed |=
            ImGui::DragFloat("Keyframe duration", &motion.keyframes.duration_seconds, .01f, .1f, 10.0f, "%.2f s");
        changed |= ImGui::Checkbox("Loop keyframes", &motion.keyframes.loop);
        for (int index = 0; index < motion.keyframes.count; ++index) {
            ImGui::PushID(index);
            ImGui::SetNextItemWidth(100.0f);
            changed |= ImGui::DragFloat("Time", &motion.keyframes.keyframes[index].time_seconds, .01f, 0.0f,
                                        motion.keyframes.duration_seconds, "%.2f s");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            changed |=
                ImGui::DragFloat("Value", &motion.keyframes.keyframes[index].value, .1f, -4096.0f, 4096.0f, "%.2f");
            ImGui::PopID();
        }
        if (motion.keyframes.count < pet::preset::kMaxMotionKeyframes && ImGui::Button("Add keyframe")) {
            pet::preset::MotionRange range{};
            float* target = resolveMotionTarget(n, motion.property, range);
            if (target != nullptr) {
                auto& frame            = motion.keyframes.keyframes[motion.keyframes.count];
                frame.time_seconds     = editor.motion.time_seconds;
                frame.value            = *target;
                motion.keyframes.count = uint8_t(motion.keyframes.count + 1);
                changed                = true;
            }
        }
        if (motion.keyframes.count > 0 && ImGui::Button("Remove last keyframe")) {
            --motion.keyframes.count;
            changed = true;
        }
    }
    ImGui::SetNextItemWidth(160.0f);
    int easing_index = motion.easing == pet::preset::MotionEasing::Smoothstep ? 1 : 0;
    if (ImGui::Combo("Easing", &easing_index, "Linear\0Smoothstep\0")) {
        motion.easing = easing_index == 1 ? pet::preset::MotionEasing::Smoothstep : pet::preset::MotionEasing::Linear;
        changed       = true;
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Preview duration", &editor.motion.duration_seconds, .01f, .1f, 10.0f, "%.2f s");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Preview time", &editor.motion.time_seconds, .01f, 0.0f, editor.motion.duration_seconds, "%.2f s");
    if (ImGui::Button("Restart preview")) editor.motion.time_seconds = 0.0f;
    ImGui::SameLine();
    if (ImGui::Button("Blink (scale Y)")) {
        motion.property                   = MotionProperty::ScaleY;
        motion.use_keyframes              = true;
        motion.keyframes                  = {};
        motion.keyframes.loop             = true;
        motion.keyframes.duration_seconds = 2.5f;
        motion.keyframes.count            = 4;
        motion.keyframes.keyframes[0]     = {0.0f, 1.0f};
        motion.keyframes.keyframes[1]     = {0.08f, 0.12f};
        motion.keyframes.keyframes[2]     = {0.16f, 1.0f};
        motion.keyframes.keyframes[3]     = {1.0f, 1.0f};
        motion.enabled                    = true;
        editor.motion.playing             = true;
        editor.motion.time_seconds        = 0.0f;
        changed                           = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Ear twitch (rotation)")) {
        motion.property                   = MotionProperty::Rotation;
        motion.use_keyframes              = true;
        motion.keyframes                  = {};
        motion.keyframes.loop             = true;
        motion.keyframes.duration_seconds = 3.0f;
        motion.keyframes.count            = 4;
        motion.keyframes.keyframes[0]     = {0.0f, 0.0f};
        motion.keyframes.keyframes[1]     = {0.12f, 8.0f};
        motion.keyframes.keyframes[2]     = {0.24f, 0.0f};
        motion.keyframes.keyframes[3]     = {1.0f, 0.0f};
        motion.enabled                    = true;
        editor.motion.playing             = true;
        editor.motion.time_seconds        = 0.0f;
        changed                           = true;
    }
    return changed;
}

}  // namespace kiwi::studio
