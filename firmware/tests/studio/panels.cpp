/* SPDX-License-Identifier: MIT */
#include "panels.h"

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

bool inspector(SceneDoc& document, EditorState& editor)
{
    if (editor.selection < 0 || size_t(editor.selection) >= document.count) {
        ImGui::TextDisabled("No node selected");
        return false;
    }
    bool changed = false;
    auto& n      = document.nodes[editor.selection];
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
