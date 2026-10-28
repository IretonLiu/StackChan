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

bool loadSession(SceneDoc& document, EditorState& editor)
{
    std::string error;
    // load() is transactional: on failure it leaves `document` untouched.
    if (!load(document, editor.file_path.c_str(), error)) {
        editor.status = error;
        return false;
    }
    editor.selection = document.count == 0 ? -1 : 0;
    editor.dirty     = false;
    editor.status    = "Loaded " + editor.file_path;
    return true;
}

}  // namespace kiwi::studio
