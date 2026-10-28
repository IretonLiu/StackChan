/* SPDX-License-Identifier: MIT */
#pragma once

// `.petface.json` (de)serialisation and SVG export. Pure document I/O: no editor state,
// no status strings. Callers translate failures into whatever feedback they own.

#include "types.h"

#include <string>

namespace kiwi::studio {

/** Loads and validates a scene. On failure `out_document` is left untouched. */
bool load(SceneDoc& out_document, const char* filename, std::string& error);

/** Writes the document. Returns false when the file or pose table cannot be written. */
bool save(const SceneDoc& document, const char* filename);

/** Writes a 320x240 SVG preview of the document. */
bool exportSvg(const SceneDoc& document, const char* filename);

}  // namespace kiwi::studio
