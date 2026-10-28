/* SPDX-License-Identifier: MIT */
#include "file_picker.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace kiwi::studio {

namespace {

namespace fs = std::filesystem;

constexpr const char* kPickerTitle = "Open scene";

fs::path normalized(const fs::path& path)
{
    return path.lexically_normal();
}

}  // namespace

bool listSceneDirectory(const std::string& directory, std::vector<FilePickerEntry>& entries, std::string& error)
{
    entries.clear();
    error.clear();
    std::error_code code;
    fs::directory_iterator iterator(directory, fs::directory_options::skip_permission_denied, code);
    const fs::directory_iterator end;
    while (!code && iterator != end) {
        const fs::directory_entry& entry = *iterator;
        std::error_code entry_error;
        const bool is_directory = entry.is_directory(entry_error);
        if (!entry_error) {
            const std::string name = entry.path().filename().string();
            const bool visible     = !name.empty() && name.front() != '.';
            const bool is_scene = entry.path().extension() == ".json" && entry.path().stem().extension() == ".petface";
            if (visible && (is_directory || is_scene)) entries.push_back({name, is_directory});
        }
        iterator.increment(code);
    }
    if (code) {
        error = code.message();
        return false;
    }
    std::sort(entries.begin(), entries.end(), [](const FilePickerEntry& left, const FilePickerEntry& right) {
        if (left.directory != right.directory) return left.directory;
        return left.name < right.name;
    });
    return true;
}

void openFilePicker(FilePicker& picker, const std::string& start_path)
{
    std::error_code code;
    fs::path directory;
    const fs::path start(start_path);
    if (!start_path.empty()) directory = start.parent_path();
    if (directory.empty() || !fs::is_directory(directory, code)) {
        code.clear();
        directory = fs::current_path(code);
    }
    if (code || directory.empty()) directory = fs::path{"."};

    picker           = {};
    picker.directory = normalized(directory).string();
    picker.open      = true;
    picker.refresh   = true;
    if (!start_path.empty() && fs::is_regular_file(start, code)) picker.selected = normalized(start).string();
}

std::string drawFilePicker(FilePicker& picker)
{
    if (!picker.open) {
        picker.opened = false;
        return {};
    }
    if (picker.refresh) {
        if (!listSceneDirectory(picker.directory, picker.entries, picker.error)) picker.entries.clear();
        picker.refresh = false;
    }
    if (!picker.opened) {
        ImGui::OpenPopup(kPickerTitle);
        picker.opened = true;
    }

    std::string chosen;
    if (ImGui::BeginPopupModal(kPickerTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", picker.directory.c_str());
        if (ImGui::Button("Up")) {
            const fs::path parent = fs::path(picker.directory).parent_path();
            if (!parent.empty() && parent != fs::path(picker.directory)) {
                picker.directory = normalized(parent).string();
                picker.selected.clear();
                picker.refresh = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) picker.refresh = true;
        if (!picker.error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", picker.error.c_str());

        ImGui::BeginChild("##entries", ImVec2(560.0f, 300.0f), true);
        for (const FilePickerEntry& entry : picker.entries) {
            const fs::path path     = fs::path(picker.directory) / entry.name;
            const std::string label = entry.directory ? entry.name + "/" : entry.name;
            const bool selected     = !entry.directory && picker.selected == normalized(path).string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (entry.directory) {
                    picker.directory = normalized(path).string();
                    picker.selected.clear();
                    picker.refresh = true;
                } else {
                    picker.selected = normalized(path).string();
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) chosen = picker.selected;
                }
            }
        }
        if (picker.entries.empty() && picker.error.empty()) ImGui::TextDisabled("No folders or .json scenes here.");
        ImGui::EndChild();

        if (picker.selected.empty())
            ImGui::TextDisabled("Double-click a folder to enter it, or a .json scene to open it.");
        else
            ImGui::TextUnformatted(picker.selected.c_str());

        ImGui::Separator();
        ImGui::BeginDisabled(picker.selected.empty());
        if (ImGui::Button("Open")) chosen = picker.selected;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            picker.open = false;
            ImGui::CloseCurrentPopup();
        }
        if (!chosen.empty()) {
            picker.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else if (picker.opened) {
        picker.open   = false;
        picker.opened = false;
    }
    return chosen;
}

}  // namespace kiwi::studio
