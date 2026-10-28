/* SPDX-License-Identifier: MIT */
#pragma once

// Document mutations. Every operation takes the document plus any selection it needs,
// reports failure through `error`, and never touches editor state or status text.

#include "types.h"

#include <string>

namespace kiwi::studio {

int uniqueNodeNumber(const SceneDoc& document, Kind kind);

Node makeNode(const SceneDoc& document, Kind kind);

/** Builds the default four-node cat scene. */
void resetScene(SceneDoc& document);

/** Replaces the document with an empty one. */
void clearScene(SceneDoc& document);

/** Depth of `index` in the parent chain (0 for roots). */
int nodeDepth(const SceneDoc& document, int index);

std::string uniqueCopyId(const SceneDoc& document, const std::string& id);

/** Appends a copy of `selected`; on success writes the new index to `new_selection`. */
bool duplicateNode(SceneDoc& document, int selected, int& new_selection, std::string& error);

/** Swaps `source` with `target`, remapping child parents. Writes the new index on success. */
bool moveNode(SceneDoc& document, int source, int target, int& new_selection, std::string& error);

/** Removes node `index` when it has no children. Writes the fallback selection on success. */
bool deleteNode(SceneDoc& document, int index, int& new_selection, std::string& error);

/** Removes the selected node unless it is the last one. */
bool deleteSelected(SceneDoc& document, int selected, int& new_selection, std::string& error);

}  // namespace kiwi::studio
