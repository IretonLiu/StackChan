/* SPDX-License-Identifier: MIT */
#pragma once

// Editor panels: toolbar, node list/scene panel and the property inspector. Panels read
// and write the document + editor state and report "changed"; they never perform I/O of
// their own beyond the explicit Save/Load/Export buttons.

#include "editor_state.h"
#include "types.h"

namespace kiwi::studio {

enum class Pending { None, New, Reset, Load, Quit };

void toolbar(SceneDoc& document, EditorState& editor, History& history, Pending& pending, bool& validate_requested,
             bool& replaced);

bool scenePanel(SceneDoc& document, EditorState& editor);

bool inspector(SceneDoc& document, EditorState& editor);

// Shared ImGui field helpers (also used by panels internally).
bool resetPopup(const char* label, const char* text);
bool floatField(const char* label, float& value, float minimum, float maximum, float default_value, float speed,
                const char* format = "%.2f");
bool pointField(const char* label, pet::shape::Vec2& value, pet::shape::Vec2 default_value, float speed,
                float minimum = -4096.0f, float maximum = 4096.0f);

}  // namespace kiwi::studio
