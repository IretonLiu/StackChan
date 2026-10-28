/* SPDX-License-Identifier: MIT */
#pragma once

// Self-contained ImGui file browser. It knows nothing about scenes: it lists a directory
// and returns a chosen path, so the caller decides what to do with it. Being in-app and
// dependency-free keeps Kiwi Studio from pulling a native-dialog library (or shelling out
// to zenity/kdialog) into the lean host tool.

#include <string>
#include <vector>

namespace kiwi::studio {

struct FilePickerEntry {
    std::string name;
    bool directory = false;
};

struct FilePicker {
    bool open    = false;  // modal should be drawn
    bool opened  = false;  // OpenPopup has been issued for this session
    bool refresh = true;   // re-read `directory` next frame
    std::string directory;
    std::string error;
    std::string selected;  // full path of the highlighted file
    std::vector<FilePickerEntry> entries;
};

/**
 * Lists the sub-directories and `.json` scenes in `directory`, directories first and
 * alphabetical within each group. Hidden entries are skipped. Returns false and fills
 * `error` when the directory cannot be read.
 */
bool listSceneDirectory(const std::string& directory, std::vector<FilePickerEntry>& entries, std::string& error);

/** Opens the browser in the directory containing `start_path` (or the working directory). */
void openFilePicker(FilePicker& picker, const std::string& start_path);

/** Draws and drives the modal; returns the confirmed path once, or an empty string. */
std::string drawFilePicker(FilePicker& picker);

}  // namespace kiwi::studio
