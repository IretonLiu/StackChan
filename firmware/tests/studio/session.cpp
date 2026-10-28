/* SPDX-License-Identifier: MIT */
#include "session.h"

#include "persistence.h"
#include "validate.h"

#include <string>
#include <utility>

namespace kiwi::studio {

bool saveSession(SceneDoc& document, EditorState& editor)
{
    std::string error;
    if (!validateScene(document, error)) {
        editor.status = error;
        return false;
    }
    if (!save(document, editor.file_path.c_str())) {
        editor.status = "Could not write " + editor.file_path;
        return false;
    }
    editor.dirty  = false;
    editor.status = "Saved " + editor.file_path;
    return true;
}

bool loadSession(SceneDoc& document, EditorState& editor, const std::string& path)
{
    SceneDoc candidate;
    std::string error;
    // load() is transactional, so neither the scene nor `file_path` moves until the new
    // file (geometry + animations) has been accepted.
    if (!load(candidate, path.c_str(), error)) {
        editor.status = error;
        return false;
    }
    document                            = std::move(candidate);
    editor.animation_preview            = {};
    editor.animation_preview.goal_index = document.animations.findGoal(document.animations.default_goal);
    editor.file_path                    = path;
    editor.selection                    = document.count == 0 ? -1 : 0;
    editor.dirty                        = false;
    editor.status =
        document.animations.empty()
            ? "Loaded " + path
            : "Loaded " + path + " (" + std::to_string(document.animations.goal_count) + " animation goals)";
    return true;
}

}  // namespace kiwi::studio
