/* SPDX-License-Identifier: MIT */
#include "editor_state.h"

namespace kiwi::studio {

void History::record(const SceneDoc& document)
{
    if (_undo.size() == kMaxEntries) _undo.pop_front();
    _undo.push_back(document);
    _redo.clear();
}

bool History::undo(SceneDoc& document)
{
    if (_undo.empty()) return false;
    _redo.push_back(document);
    document = _undo.back();
    _undo.pop_back();
    return true;
}

bool History::redo(SceneDoc& document)
{
    if (_redo.empty()) return false;
    _undo.push_back(document);
    document = _redo.back();
    _redo.pop_back();
    return true;
}

void History::clear()
{
    _undo.clear();
    _redo.clear();
}

}  // namespace kiwi::studio
