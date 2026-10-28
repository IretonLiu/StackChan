/* SPDX-License-Identifier: MIT */
#include "session.h"

#include "persistence.h"
#include "validate.h"

#include <string>

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
    std::string error;
    // load() is transactional: on failure it leaves `document` untouched, so neither the
    // scene nor `file_path` moves until the new file has been accepted.
    if (!load(document, path.c_str(), error)) {
        editor.status = error;
        return false;
    }
    editor.file_path = path;
    editor.selection = document.count == 0 ? -1 : 0;
    editor.dirty     = false;
    editor.status    = "Loaded " + path;
    return true;
}

}  // namespace kiwi::studio
