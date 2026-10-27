/*
 * SPDX-License-Identifier: MIT
 *
 * Desktop-only Dear ImGui inspector for the renderer-neutral shape core.
 * Nothing in this file is compiled into the ESP-IDF image.
 */
#include <pet/shape/soft_polygon.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using pet::shape::Affine2D;
using pet::shape::Color;
using pet::shape::CornerControls;
using pet::shape::EdgeTension;
using pet::shape::PathCommandType;
using pet::shape::ShapePath;
using pet::shape::SoftPolygonEarParameters;
using pet::shape::SoftPolygonEarSample;
using pet::shape::Transform2D;
using pet::shape::Vec2;

struct StudioState {
    Transform2D transform{{160.0f, 142.0f}, 0.0f, {88.0f, 96.0f}, {0.0f, 0.0f}};
    SoftPolygonEarParameters parameters{};
    bool show_landmarks = true;
    bool show_bounds = true;
    bool show_control_polygon = true;
    bool show_top_join_focus = true;
    std::string export_status;
};

struct EarPreset {
    const char* name;
    Transform2D transform;
    SoftPolygonEarParameters parameters;
};

const std::array<EarPreset, 4> kPresets = {{
    {"Neutral", {{160.0f, 142.0f}, 0.0f, {88.0f, 96.0f}, {0.0f, 0.0f}}, {0.0f, 0.0f, {0.10f, 0.55f}, {0.07f, 0.55f}, {0.0f, 0.28f, 0.28f}, {0.0f, 0.28f, 0.28f}, {0.0f, 0.28f, 0.28f}}},
    {"Perked", {{160.0f, 142.0f}, -8.0f, {82.0f, 108.0f}, {0.0f, 0.0f}}, {0.0f, 0.0f, {0.12f, 0.55f}, {0.07f, 0.55f}, {0.03f, 0.28f, 0.28f}, {-0.03f, 0.28f, 0.28f}, {0.0f, 0.28f, 0.28f}}},
    {"Twitch", {{160.0f, 142.0f}, 12.0f, {88.0f, 96.0f}, {0.0f, 0.0f}}, {0.0f, 0.10f, {0.10f, 0.62f}, {0.07f, 0.55f}, {0.16f, 0.22f, 0.34f}, {-0.10f, 0.34f, 0.22f}, {0.0f, 0.28f, 0.28f}}},
    {"Droop", {{160.0f, 142.0f}, -31.0f, {92.0f, 72.0f}, {0.0f, 0.0f}}, {0.0f, -0.10f, {0.10f, 0.48f}, {0.07f, 0.55f}, {-0.12f, 0.32f, 0.22f}, {0.08f, 0.22f, 0.32f}, {0.0f, 0.28f, 0.28f}}},
}};

void applyPreset(StudioState& state, size_t index)
{
    state.transform = kPresets[index].transform;
    state.parameters = kPresets[index].parameters;
    state.export_status.clear();
}

ImU32 color(Color value, float opacity = 1.0f)
{
    return IM_COL32(value.red, value.green, value.blue,
                    static_cast<int>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
}

Vec2 cubicPoint(Vec2 start, Vec2 first, Vec2 second, Vec2 end, float t)
{
    const float inverse = 1.0f - t;
    return {
        inverse * inverse * inverse * start.x + 3.0f * inverse * inverse * t * first.x +
            3.0f * inverse * t * t * second.x + t * t * t * end.x,
        inverse * inverse * inverse * start.y + 3.0f * inverse * inverse * t * first.y +
            3.0f * inverse * t * t * second.y + t * t * t * end.y,
    };
}

ImVec2 screenPoint(Vec2 point, ImVec2 origin, float scale)
{
    return {origin.x + point.x * scale, origin.y + point.y * scale};
}

std::vector<ImVec2> flatten(const ShapePath& path, ImVec2 origin, float scale)
{
    std::vector<ImVec2> result;
    Vec2 current{};
    for (uint8_t index = 0; index < path.command_count; ++index) {
        const auto& command = path.commands[index];
        if (command.type == PathCommandType::Move) {
            current = command.first;
            result.push_back(screenPoint(current, origin, scale));
        } else if (command.type == PathCommandType::Cubic) {
            for (int step = 1; step <= 18; ++step) {
                const float progress = static_cast<float>(step) / 18.0f;
                result.push_back(screenPoint(cubicPoint(current, command.first, command.second, command.end, progress), origin, scale));
            }
            current = command.end;
        }
    }
    return result;
}

void drawLabel(ImDrawList* draw_list, ImVec2 point, const char* label, ImU32 label_color)
{
    draw_list->AddCircleFilled(point, 4.0f, label_color);
    draw_list->AddText({point.x + 7.0f, point.y - 14.0f}, label_color, label);
}

void drawCanvas(const SoftPolygonEarSample& sample, StudioState& state, const Affine2D& world)
{
    ImGui::BeginChild("Ear canvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float scale = std::max(0.1f, std::min((available.x - 24.0f) / 320.0f, (available.y - 24.0f) / 240.0f));
    const ImVec2 size{320.0f * scale, 240.0f * scale};
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 origin{cursor.x + (available.x - size.x) * 0.5f, cursor.y + (available.y - size.y) * 0.5f};
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(7, 16, 24, 255));
    draw_list->AddRect(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(70, 90, 105, 255));

    const std::vector<ImVec2> points = flatten(sample.path, origin, scale);
    if (points.size() >= 3) {
        // The flattened closed path may become mildly concave under permitted
        // bulge controls, so use ImGui's general simple-polygon tessellator.
        draw_list->AddConcavePolyFilled(points.data(), static_cast<int>(points.size()), color(sample.path.style.color, sample.path.style.opacity));
        draw_list->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(255, 255, 255, 125), ImDrawFlags_Closed, 1.0f);
    }

    if (state.show_bounds && sample.path.bounds.valid) {
        const ImVec2 low = screenPoint(sample.path.bounds.minimum, origin, scale);
        const ImVec2 high = screenPoint(sample.path.bounds.maximum, origin, scale);
        draw_list->AddRect(low, high, IM_COL32(100, 116, 139, 255), 0.0f, 0, 1.0f);
    }
    if (state.show_landmarks) {
        const ImVec2 base_left = screenPoint(sample.landmarks.base_left, origin, scale);
        const ImVec2 tip = screenPoint(sample.landmarks.tip, origin, scale);
        const ImVec2 base_right = screenPoint(sample.landmarks.base_right, origin, scale);
        const ImVec2 pivot = screenPoint(pet::shape::transformPoint(world, state.transform.pivot), origin, scale);
        draw_list->AddLine(base_left, tip, IM_COL32(71, 85, 105, 255), 1.0f);
        draw_list->AddLine(tip, base_right, IM_COL32(71, 85, 105, 255), 1.0f);
        drawLabel(draw_list, base_left, "base_left", IM_COL32(34, 197, 94, 255));
        drawLabel(draw_list, tip, "tip", IM_COL32(245, 158, 11, 255));
        drawLabel(draw_list, base_right, "base_right", IM_COL32(34, 197, 94, 255));
        drawLabel(draw_list, pivot, "pivot", IM_COL32(244, 114, 182, 255));
    }
    if (state.show_control_polygon) {
        Vec2 current{};
        for (uint8_t index = 0; index < sample.path.command_count; ++index) {
            const auto& command = sample.path.commands[index];
            if (command.type == PathCommandType::Move) {
                current = command.first;
            } else if (command.type == PathCommandType::Cubic) {
                const ImVec2 start = screenPoint(current, origin, scale);
                const ImVec2 first = screenPoint(command.first, origin, scale);
                const ImVec2 second = screenPoint(command.second, origin, scale);
                const ImVec2 end = screenPoint(command.end, origin, scale);
                draw_list->AddLine(start, first, IM_COL32(96, 165, 250, 120), 1.0f);
                draw_list->AddLine(second, end, IM_COL32(96, 165, 250, 120), 1.0f);
                draw_list->AddCircleFilled(first, 2.5f, IM_COL32(96, 165, 250, 180));
                draw_list->AddCircleFilled(second, 2.5f, IM_COL32(96, 165, 250, 180));
                current = command.end;
            }
        }
    }
    if (state.show_top_join_focus) {
        const auto& left_edge = sample.edges[0];
        const auto& top_corner = sample.corners[1];
        const auto& right_edge = sample.edges[1];
        const ImVec2 left_handle = screenPoint(pet::shape::transformPoint(world, left_edge.second_control), origin, scale);
        const ImVec2 cap_start = screenPoint(pet::shape::transformPoint(world, top_corner.start), origin, scale);
        const ImVec2 cap_first = screenPoint(pet::shape::transformPoint(world, top_corner.first_control), origin, scale);
        const ImVec2 cap_second = screenPoint(pet::shape::transformPoint(world, top_corner.second_control), origin, scale);
        const ImVec2 cap_end = screenPoint(pet::shape::transformPoint(world, top_corner.end), origin, scale);
        const ImVec2 right_handle = screenPoint(pet::shape::transformPoint(world, right_edge.first_control), origin, scale);
        const ImU32 edge_colour = IM_COL32(34, 197, 94, 255);
        const ImU32 cap_colour = IM_COL32(245, 158, 11, 255);
        draw_list->AddLine(left_handle, cap_start, edge_colour, 2.0f);
        draw_list->AddLine(cap_start, cap_first, cap_colour, 2.0f);
        draw_list->AddLine(cap_second, cap_end, cap_colour, 2.0f);
        draw_list->AddLine(cap_end, right_handle, edge_colour, 2.0f);
        drawLabel(draw_list, left_handle, "L2", edge_colour);
        drawLabel(draw_list, cap_first, "C1", cap_colour);
        drawLabel(draw_list, cap_second, "C2", cap_colour);
        drawLabel(draw_list, right_handle, "R1", edge_colour);
    }
    ImGui::Dummy(available);
    ImGui::EndChild();
}

std::string svgColor(Color value)
{
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value.red) << std::setw(2)
           << static_cast<int>(value.green) << std::setw(2) << static_cast<int>(value.blue);
    return stream.str();
}

bool exportSvg(const ShapePath& path, const char* filename)
{
    std::ofstream file(filename);
    if (!file) {
        return false;
    }
    file << std::fixed << std::setprecision(2);
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 240\" width=\"320\" height=\"240\">\n";
    file << "<rect width=\"100%\" height=\"100%\" fill=\"#071018\"/>\n<path d=\"";
    for (uint8_t index = 0; index < path.command_count; ++index) {
        const auto& command = path.commands[index];
        if (command.type == PathCommandType::Move) {
            file << "M " << command.first.x << ' ' << command.first.y << ' ';
        } else if (command.type == PathCommandType::Cubic) {
            file << "C " << command.first.x << ' ' << command.first.y << ' ' << command.second.x << ' ' << command.second.y << ' '
                 << command.end.x << ' ' << command.end.y << ' ';
        } else {
            file << "Z ";
        }
    }
    file << "\" fill=\"" << (path.style.fill ? svgColor(path.style.color) : "none") << "\" stroke=\""
         << (path.style.stroke ? svgColor(path.style.color) : "none") << "\" stroke-width=\"" << path.style.stroke_width
         << "\" opacity=\"" << path.style.opacity << "\" stroke-linejoin=\"round\"/>\n</svg>\n";
    return static_cast<bool>(file);
}

void drawInspector(StudioState& state, const SoftPolygonEarSample& sample, bool sample_ok)
{
    ImGui::TextUnformatted("soft_polygon / three-landmark ear");
    ImGui::TextDisabled("Normalized template; Transform2D supplies design-pixel size.");
    ImGui::Separator();

    ImGui::TextUnformatted("Presets");
    for (size_t index = 0; index < kPresets.size(); ++index) {
        if (index > 0) ImGui::SameLine();
        if (ImGui::Button(kPresets[index].name)) applyPreset(state, index);
    }

    ImGui::SeparatorText("Pose");
    ImGui::DragFloat2("Translation", &state.transform.translation.x, 0.5f, -320.0f, 640.0f, "%.1f px");
    ImGui::SliderFloat("Rotation", &state.transform.rotation_degrees, -180.0f, 180.0f, "%.1f deg");
    ImGui::DragFloat2("Scale", &state.transform.scale.x, 0.25f, 1.0f, 240.0f, "%.1f px");
    ImGui::SliderFloat2("Pivot", &state.transform.pivot.x, -1.0f, 1.0f, "%.2f");

    ImGui::SeparatorText("Six-cubic topology");
    ImGui::TextDisabled("Corners own tangent rails; edge handles are regenerated from them. Blue points show every derived Bézier control.");
    if (!sample_ok) {
        ImGui::TextColored({1.0f, 0.35f, 0.35f, 1.0f}, "Invalid numeric input; geometry was not sampled.");
    }
    ImGui::SliderFloat("Lean", &state.parameters.lean, -0.25f, 0.25f, "%.3f");
    ImGui::SliderFloat("Tip bend", &state.parameters.tip_bend, -0.20f, 0.20f, "%.3f");
    const auto cornerUi = [](const char* label, CornerControls& corner) {
        ImGui::PushID(label);
        ImGui::SeparatorText(label);
        ImGui::SliderFloat("Roundness", &corner.roundness, 0.0f, 0.35f, "%.3f");
        ImGui::SliderFloat("Cap tension", &corner.cap_tension, 0.10f, 0.90f, "%.3f");
        ImGui::PopID();
    };
    const auto edgeUi = [](const char* label, EdgeTension& edge) {
        ImGui::PushID(label);
        ImGui::SeparatorText(label);
        ImGui::SliderFloat("Bulge", &edge.bulge, -0.35f, 0.35f, "%.3f");
        ImGui::SliderFloat2("Start / end tension", &edge.start, 0.08f, 0.45f, "%.3f");
        ImGui::PopID();
    };
    cornerUi("Tip corner", state.parameters.tip_corner);
    cornerUi("Base corners", state.parameters.base_corners);
    edgeUi("Left edge: base to tip", state.parameters.left_side);
    edgeUi("Right edge: tip to base", state.parameters.right_side);
    edgeUi("Base edge: right to left", state.parameters.base);

    ImGui::SeparatorText("Inspection");
    ImGui::Checkbox("Landmarks and pivot", &state.show_landmarks);
    ImGui::Checkbox("Conservative bounds", &state.show_bounds);
    ImGui::Checkbox("Cubic control polygon", &state.show_control_polygon);
    ImGui::Checkbox("Top join focus: L2 / C1 / C2 / R1", &state.show_top_join_focus);
    if (sample.parameters_were_clamped) {
        ImGui::TextColored({1.0f, 0.65f, 0.15f, 1.0f}, "Input was clamped to a template-safe range.");
    }

    ImGui::Separator();
    if (ImGui::Button("Export SVG")) {
        state.export_status = sample_ok
                                  ? (exportSvg(sample.path, "shape-studio-ear.svg") ? "Wrote shape-studio-ear.svg"
                                                                                     : "Could not write shape-studio-ear.svg")
                                  : "Cannot export invalid geometry";
    }
    if (!state.export_status.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(state.export_status.c_str());
    }
}

void glfwError(int error, const char* description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

}  // namespace

int main()
{
    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) {
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1160, 760, "StackChan Shape Studio", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Studio state is intentionally not a user-level configuration artifact.
    // Persist reviewed geometry through SVG/scene data, never imgui.ini.
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    StudioState state;
    applyPreset(state, 0);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const Affine2D world = pet::shape::makeAffine(state.transform);
        SoftPolygonEarSample sample;
        const bool sample_ok = pet::shape::sampleSoftPolygonEar(state.parameters, world, sample);

        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                   ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("Shape Studio", nullptr, kWindowFlags);
        ImGui::Columns(2, "studio-columns", true);
        ImGui::SetColumnWidth(0, 760.0f);
        drawCanvas(sample, state, world);
        ImGui::NextColumn();
        drawInspector(state, sample, sample_ok);
        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.035f, 0.063f, 0.094f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
