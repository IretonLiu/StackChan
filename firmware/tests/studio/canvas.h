/* SPDX-License-Identifier: MIT */
#pragma once

// Viewport rendering and direct manipulation. Owns camera, handles and drag behaviour;
// returns only "did the document change", leaving dirty/undo/status to the caller.

#include "editor_state.h"
#include "types.h"

namespace kiwi::studio {

/** Draws the scene and applies viewport edits. Returns true when the document changed. */
bool drawCanvas(SceneDoc& document, EditorState& editor);

}  // namespace kiwi::studio
