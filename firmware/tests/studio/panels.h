/* SPDX-License-Identifier: MIT */
#pragma once

// Editor panels: toolbar, node list/scene panel and the property inspector. Panels read
// and write the document + editor state and report "changed"; they never perform I/O of
// their own beyond the explicit Save/Load/Export buttons.

#include "editor_state.h"
#include "types.h"

namespace kiwi::studio {

enum class Pending { None, New, Reset, Load, Quit };

/** Values the toolbar produces for the frame/app loop to act on. */
struct ToolbarCommands {
    Pending pending         = Pending::None;  // latch: cleared by the loop after the modal
    bool validate_requested = false;          // latch: cleared by the loop after validation
    bool replaced           = false;          // per-frame: reset at toolbar entry
};

void toolbar(SceneDoc& document, EditorState& editor, History& history, ToolbarCommands& commands);

bool scenePanel(SceneDoc& document, EditorState& editor);

bool inspector(SceneDoc& document, EditorState& editor);

}  // namespace kiwi::studio
