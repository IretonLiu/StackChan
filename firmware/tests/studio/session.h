/* SPDX-License-Identifier: MIT */
#pragma once

// Editor-facing glue that composes validation + persistence and translates failures into
// the editor's status/dirty fields. This is the boundary where errors become UI feedback.

#include "editor_state.h"
#include "types.h"

namespace kiwi::studio {

/** Validates then writes the document; updates status/dirty. */
bool saveSession(SceneDoc& document, EditorState& editor);

/** Loads `path`; commits the document and file_path only on success, resets selection. */
bool loadSession(SceneDoc& document, EditorState& editor, const std::string& path);

}  // namespace kiwi::studio
