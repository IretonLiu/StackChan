/* SPDX-License-Identifier: MIT */
#pragma once

// Editor-only state. Nothing here is persisted into the document, and history snapshots
// only the document, so undo/redo can never resurrect status text, file paths or a
// previous selection.

#include "types.h"

#include <imgui.h>

#include <array>
#include <string>
#include <deque>

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
    bool show_bounds          = false;
    bool show_guides          = false;
    bool show_vertices        = true;
    bool show_bezier_controls = false;

    // Active interaction. `drag_handle_kind`: 0=edge in, 1=edge out, 2=corner tip A,
    // 3=corner tip B, 4=vertex position.
    int drag_node         = -1;
    int drag_handle_node  = -1;
    int drag_handle_kind  = -1;
    int drag_handle_index = -1;

    // Motion preview clock shared by every node.
    MotionPreview motion{};

    // Session state (not part of the document).
    std::string file_path{"kiwi-face.petface.json"};
    std::string status;
    bool dirty = false;
};

/**
 * Bounded undo/redo over the document only. Recording a snapshot clears the redo stack.
 */
class History {
public:
    static constexpr size_t kMaxEntries = 64;

    void record(const SceneDoc& document);
    bool undo(SceneDoc& document);
    bool redo(SceneDoc& document);
    void clear();

    bool canUndo() const
    {
        return !_undo.empty();
    }
    bool canRedo() const
    {
        return !_redo.empty();
    }

private:
    std::deque<SceneDoc> _undo;
    std::deque<SceneDoc> _redo;
};

}  // namespace kiwi::studio
