/* SPDX-License-Identifier: MIT */
#pragma once

// Editor-only state. Nothing here is persisted into the document, and history snapshots
// only the document, so undo/redo can never resurrect status text, file paths or a
// previous selection.

#include "animations.h"
#include "history.h"
#include "types.h"

#include <imgui.h>

#include <string>

namespace kiwi::studio {

/** Camera, overlays, active interaction, motion playback and transient session feedback. */
struct EditorState {
    // Selection + camera.
    int selection = 0;
    float zoom    = 1.0f;
    ImVec2 pan{};
    bool frame_requested = false;
    bool reset_requested = false;

    // Overlays.
    bool show_bounds  = false;
    bool show_guides  = false;
    bool show_corners = true;   // corner joint + two tips
    bool show_edges   = false;  // edge in/out rails

    // Active interaction. `drag_handle_kind`: 0=edge in, 1=edge out, 2=corner tip A,
    // 3=corner tip B, 4=corner joint position. Corners and edges are the only
    // grabbable primitives; a bare vertex is not a thing you can select.
    int drag_node         = -1;
    int drag_handle_node  = -1;
    int drag_handle_kind  = -1;
    int drag_handle_index = -1;

    // Motion preview clock shared by every node.
    MotionPreview motion{};

    // Compiler-owned animations parsed for display + preview. Read-only for now: the
    // document keeps the authored JSON verbatim, so this never marks the scene dirty.
    AnimationDoc animation{};
    struct AnimationPreview {
        bool playing       = false;
        int goal_index     = -1;
        float time_seconds = 0.0f;
    } animation_preview{};

    // Session state (not part of the document).
    std::string file_path{"kiwi-face.petface.json"};
    std::string status;
    bool dirty = false;
};

}  // namespace kiwi::studio
