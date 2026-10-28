/* SPDX-License-Identifier: MIT */
#include "studio/animations.h"
#include "studio/canvas.h"
#include "studio/compiler.h"
#include "studio/document.h"
#include "studio/editor_state.h"
#include "studio/file_picker.h"
#include "studio/panels.h"
#include "studio/persistence.h"
#include "studio/session.h"
#include "studio/types.h"
#include "studio/validate.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace kiwi::studio;

int runCli(int argc, char** argv)
{
    if (argc < 2) return -1;
    const std::string mode = argv[1];
    if (mode == "--selftest") {
        // Headless check of the document growth path the inspector uses. The extra shapes
        // are skewed/extreme rings that used to grow into non-convex (invalid) polygons.
        struct GrowthCase {
            const char* name;
            uint8_t corners;
            std::array<pet::shape::Vec2, 5> positions;
        };
        const GrowthCase cases[] = {
            {"ear", 3, {{{-30, 20}, {0, -35}, {30, 20}}}},
            {"thin", 3, {{{32.9f, 73.6f}, {29.4f, 89.4f}, {29.4f, -89.3f}}}},
            {"flat", 3, {{{-201.9f, 18.0f}, {-261.7f, 1.4f}, {242.3f, -10.7f}}}},
            {"rectangle", 4, {{{121, 116}, {139, 116}, {139, 160}, {121, 160}}}},
            {"skew-quad", 4, {{{232.5f, 77.4f}, {214.6f, 163.6f}, {160.4f, -281.6f}, {222.7f, -132.6f}}}},
        };
        int failures = 0;
        for (const GrowthCase& test : cases) {
            SceneDoc scene;
            scene.nodes[0].id                       = "test";
            scene.nodes[0].kind                     = Kind::Polygon;
            scene.nodes[0].polygon.corner_count     = test.corners;
            scene.nodes[0].polygon.corner_positions = test.positions;
            scene.count                             = 1;
            for (const uint8_t target : {uint8_t(4), uint8_t(5)}) {
                if (target <= test.corners) continue;
                SceneDoc trial = scene;
                std::string error;
                if (!setCornerCount(trial, 0, target, error)) {
                    printf("selftest: %s to %u failed: %s\n", test.name, target, error.c_str());
                    ++failures;
                    continue;
                }
                Problem problem;
                if (!validateScene(trial, problem)) {
                    printf("selftest: %s at %u invalid: %s\n", test.name, target, problem.formatted().c_str());
                    ++failures;
                    continue;
                }
                pet::shape::RoundedPolygonSample sample;
                if (!pet::shape::sampleRoundedPolygon(trial.nodes[0].polygon, {}, sample, trial.nodes[0].style)) {
                    printf("selftest: %s at %u does not sample\n", test.name, target);
                    ++failures;
                    continue;
                }
                printf("selftest: %s at %u corners ok (%u path commands)\n", test.name, target,
                       sample.path.command_count);
            }
        }
        return failures == 0 ? 0 : 2;
    }
    if (argc < 3) return -1;
    if (mode == "--list-scenes") {
        std::vector<FilePickerEntry> entries;
        std::string error;
        if (!listSceneDirectory(argv[2], entries, error)) {
            fprintf(stderr, "kiwi_studio: %s\n", error.c_str());
            return 2;
        }
        for (const FilePickerEntry& entry : entries) printf("%s%s\n", entry.name.c_str(), entry.directory ? "/" : "");
        return 0;
    }
    if (mode != "--validate" && mode != "--roundtrip" && mode != "--export-svg" && mode != "--animation-selftest")
        return -1;
    SceneDoc document;
    std::string error;
    if (!load(document, argv[2], error)) {
        fprintf(stderr, "kiwi_studio: %s\n", error.c_str());
        return 2;
    }
    printf("kiwi_studio: loaded %s (%zu nodes)\n", argv[2], document.count);
    AnimationDoc animations;
    if (!parseAnimations(document.poses_json, document.animations_json, document, animations, error)) {
        fprintf(stderr, "kiwi_studio: %s\n", error.c_str());
        return 2;
    }
    if (!animations.empty())
        printf("kiwi_studio: %u animation goals, %u poses\n", unsigned(animations.goal_count),
               unsigned(animations.pose_count));
    if (mode == "--animation-selftest") {
        if (animations.empty()) {
            fprintf(stderr, "kiwi_studio: %s has no animations\n", argv[2]);
            return 2;
        }
        const int goal_index = animations.findGoal(animations.default_goal);
        if (goal_index < 0) {
            fprintf(stderr, "kiwi_studio: default goal is not defined\n");
            return 2;
        }
        int left_eye = -1, right_eye = -1, left_ear = -1;
        for (size_t index = 0; index < document.count; ++index) {
            if (document.nodes[index].id == "left-eye") left_eye = int(index);
            if (document.nodes[index].id == "right-eye") right_eye = int(index);
            if (document.nodes[index].id == "left-ear") left_ear = int(index);
        }
        if (left_eye < 0 || right_eye < 0 || left_ear < 0) {
            fprintf(stderr, "kiwi_studio: expected left/right eye and left ear nodes\n");
            return 2;
        }
        const auto approx = [](float a, float b) { return std::fabs(a - b) < 0.01f; };
        const auto sample = [&](float time) {
            std::array<Node, kMaxNodes> nodes = document.nodes;
            applyAnimationFrame(animations, goal_index, time, nodes, document.count);
            return nodes;
        };
        int failures = 0;
        {
            const auto nodes = sample(0.0f);
            if (!approx(nodes[left_eye].transform.scale.y, 1.0f) || !approx(nodes[right_eye].transform.scale.y, 1.0f)) {
                printf("animation-selftest: eyes are not open at t=0\n");
                ++failures;
            }
        }
        {
            const auto nodes = sample(3.62f);
            if (!approx(nodes[left_eye].transform.scale.y, 0.08f) ||
                !approx(nodes[right_eye].transform.scale.y, 0.08f)) {
                printf("animation-selftest: blink is not closed at t=3.62 (%.3f/%.3f)\n",
                       nodes[left_eye].transform.scale.y, nodes[right_eye].transform.scale.y);
                ++failures;
            }
        }
        {
            const auto nodes = sample(0.7f);
            if (!approx(nodes[left_ear].transform.rotation_degrees, -22.0f)) {
                printf("animation-selftest: left ear is %.3f at t=0.7, expected -22\n",
                       nodes[left_ear].transform.rotation_degrees);
                ++failures;
            }
        }
        if (failures == 0) printf("animation-selftest: blink and ear-drift sampled correctly\n");
        return failures == 0 ? 0 : 1;
    }
    if (mode == "--roundtrip") {
        const char* output = argc >= 4 ? argv[3] : "/tmp/kiwi-studio-roundtrip.petface.json";
        if (!save(document, output)) {
            fprintf(stderr, "kiwi_studio: could not write %s\n", output);
            return 2;
        }
        printf("kiwi_studio: wrote %s\n", output);
        const CompilerResult result = runPresetCompiler(output);
        if (!result.ok) {
            fprintf(stderr, "kiwi_studio: %s", result.log.c_str());
            return 2;
        }
        printf("kiwi_studio: compiler accepted %s\n", output);
    } else if (mode == "--export-svg") {
        const char* output = argc >= 4 ? argv[3] : "kiwi-face.svg";
        if (!exportSvg(document, output)) {
            fprintf(stderr, "kiwi_studio: could not write %s\n", output);
            return 2;
        }
        printf("kiwi_studio: wrote %s\n", output);
    }
    return 0;
}

int main(int argc, char** argv)
{
    const int cli = runCli(argc, argv);
    if (cli >= 0) return cli;
    if (!glfwInit()) return 1;
    GLFWwindow* w = glfwCreateWindow(1200, 780, "Kiwi Studio", nullptr, nullptr);
    if (!w) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(w);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(w, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    SceneDoc document;
    EditorState editor;
    History history;
    SceneDoc transaction_start;
    bool transaction_active = false;
    ToolbarCommands commands;
    FilePicker picker;
    std::string pending_load_path;
    bool pending_modal_open    = false;
    bool validation_failed     = false;
    bool validation_modal_open = false;
    std::string validation_log;
    resetScene(document);
    bool running = true;
    while (running) {
        glfwPollEvents();
        if (glfwWindowShouldClose(w)) {
            glfwSetWindowShouldClose(w, GLFW_FALSE);
            if (editor.dirty) {
                if (commands.pending == Pending::None) commands.pending = Pending::Quit;
            } else {
                running = false;
                break;
            }
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        SceneDoc before       = document;
        bool shortcut_changed = false;
        const ImGuiIO& io     = ImGui::GetIO();
        const bool text_input = io.WantTextInput;
        if (text_input) {
            // Text fields consume their own keys; shortcuts stay inactive.
        } else {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                if (history.undo(document)) editor.dirty = true;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                if (history.redo(document)) editor.dirty = true;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) saveSession(document, editor);
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
                int new_selection = editor.selection;
                std::string error;
                if (duplicateNode(document, editor.selection, new_selection, error)) {
                    editor.selection = new_selection;
                    shortcut_changed = true;
                } else {
                    editor.status = error;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                int new_selection = editor.selection;
                std::string error;
                if (deleteSelected(document, editor.selection, new_selection, error)) {
                    editor.selection = new_selection;
                    shortcut_changed = true;
                } else {
                    editor.status = error;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) editor.frame_requested = true;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) editor.reset_requested = true;
        }
        ImGui::Begin("Kiwi Studio", nullptr, ImGuiWindowFlags_NoDecoration);
        if (editor.motion.playing) {
            editor.motion.time_seconds += ImGui::GetIO().DeltaTime;
            if (editor.motion.time_seconds > editor.motion.duration_seconds) {
                if (editor.motion.loop) {
                    editor.motion.time_seconds =
                        std::fmod(editor.motion.time_seconds, std::max(0.1f, editor.motion.duration_seconds));
                } else {
                    editor.motion.time_seconds = editor.motion.duration_seconds;
                    editor.motion.playing      = false;
                }
            }
        }
        if (editor.animation_preview.playing) {
            auto& preview = editor.animation_preview;
            preview.time_seconds += ImGui::GetIO().DeltaTime;
            if (preview.time_seconds > 600.0f) preview.time_seconds = std::fmod(preview.time_seconds, 600.0f);
            const int goal_index = preview.goal_index;
            if (goal_index < 0 || goal_index >= int(editor.animation.goal_count)) {
                preview.playing = false;
            } else {
                const AnimationGoal& goal = editor.animation.goals[goal_index];
                if (goal.duration_ms > 0 && preview.time_seconds * 1000.0f >= float(goal.duration_ms)) {
                    const int default_index = editor.animation.findGoal(editor.animation.default_goal);
                    if (default_index >= 0 && default_index != goal_index) {
                        preview.goal_index   = default_index;
                        preview.time_seconds = 0.0f;
                    } else {
                        preview.time_seconds = float(goal.duration_ms) / 1000.0f;
                        preview.playing      = false;
                    }
                }
            }
        }
        ImGui::Text("Kiwi Studio — %s%s", editor.file_path.c_str(), editor.dirty ? " *" : "");
        bool changed = shortcut_changed;
        toolbar(document, editor, history, commands);
        if (commands.replaced) {
            history.clear();
            transaction_active = false;
            editor.dirty       = false;
        }
        ImGui::Separator();

        const ImVec2 available   = ImGui::GetContentRegionAvail();
        const float left_width   = 250.0f;
        const float right_width  = 340.0f;
        const float panel_height = std::max(240.0f, available.y - 34.0f);
        const float center_width = std::max(280.0f, available.x - left_width - right_width - 26.0f);

        ImGui::BeginChild("parts", {left_width, panel_height}, true);
        changed |= scenePanel(document, editor);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("viewport", {center_width, panel_height}, false);
        changed |= drawCanvas(document, editor);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("inspector", {right_width, panel_height}, true);
        changed |= inspector(document, editor);
        ImGui::EndChild();

        if (changed) editor.dirty = true;

        if (commands.browse_requested) {
            commands.browse_requested = false;
            openFilePicker(picker, editor.file_path);
        }
        const std::string picked = drawFilePicker(picker);
        if (!picked.empty()) {
            if (editor.dirty) {
                pending_load_path = picked;
                commands.pending  = Pending::Load;
            } else if (loadSession(document, editor, picked)) {
                history.clear();
                transaction_active = false;
            }
        }

        if (commands.validate_requested) {
            commands.validate_requested = false;
            const bool saved            = !editor.dirty || saveSession(document, editor);
            if (saved) {
                const CompilerResult result = runPresetCompiler(editor.file_path);
                if (result.ok) {
                    editor.status     = "Preset compiler accepted " + editor.file_path;
                    validation_failed = false;
                } else {
                    validation_log    = result.log;
                    validation_failed = true;
                    editor.status     = result.first_line;
                }
            }
        }

        if (!editor.status.empty()) ImGui::TextWrapped("%s", editor.status.c_str());

        if (commands.pending != Pending::None && !pending_modal_open) {
            ImGui::OpenPopup("Discard unsaved changes?");
            pending_modal_open = true;
        }
        if (ImGui::BeginPopupModal("Discard unsaved changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("The current scene has unsaved changes.");
            if (ImGui::Button("Discard")) {
                const Pending action = commands.pending;
                commands.pending     = Pending::None;
                pending_modal_open   = false;
                bool replaced_action = false;
                if (action == Pending::New) {
                    clearScene(document);
                    editor.selection         = -1;
                    editor.animation         = {};
                    editor.animation_preview = {};
                    replaced_action          = true;
                } else if (action == Pending::Reset) {
                    resetScene(document);
                    editor.selection         = 0;
                    editor.animation         = {};
                    editor.animation_preview = {};
                    replaced_action          = true;
                } else if (action == Pending::Load) {
                    replaced_action = loadSession(document, editor, pending_load_path);
                } else if (action == Pending::Quit) {
                    running = false;
                }
                if (replaced_action) {
                    history.clear();
                    transaction_active = false;
                    editor.dirty       = false;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                commands.pending   = Pending::None;
                pending_modal_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else if (commands.pending != Pending::None && !ImGui::IsPopupOpen("Discard unsaved changes?")) {
            commands.pending   = Pending::None;
            pending_modal_open = false;
        }

        if (validation_failed && !validation_modal_open) {
            ImGui::OpenPopup("Preset compiler rejected the scene");
            validation_modal_open = true;
        }
        if (ImGui::BeginPopupModal("Preset compiler rejected the scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("The preset compiler rejected the saved scene.");
            ImGui::BeginChild("validation-log", {620.0f, 260.0f}, true);
            ImGui::TextUnformatted(validation_log.c_str());
            ImGui::EndChild();
            if (ImGui::Button("Close")) {
                validation_failed     = false;
                validation_modal_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else if (!ImGui::IsPopupOpen("Preset compiler rejected the scene")) {
            validation_modal_open = false;
        }

        if (changed && !transaction_active) {
            transaction_start  = before;
            transaction_active = true;
        }
        // Canvas drags are not ImGui items, so IsAnyItemActive() alone would close the
        // transaction on the first drag frame and push one undo entry per frame. Hold the
        // transaction open while the left button is down so a whole drag undoes as one step.
        const bool interaction_continues = ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (transaction_active && !interaction_continues) {
            history.record(transaction_start);
            transaction_active = false;
        }
        ImGui::End();
        ImGui::Render();
        int x, y;
        glfwGetFramebufferSize(w, &x, &y);
        glViewport(0, 0, x, y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(w);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(w);
    glfwTerminate();
}
