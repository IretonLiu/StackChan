/* SPDX-License-Identifier: MIT */
#pragma once

// Bounded undo/redo over the document only. Deliberately ImGui-free so the document
// model and its history can be reasoned about (and tested) without the editor.

#include "types.h"

#include <cstddef>
#include <deque>

namespace kiwi::studio {

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
