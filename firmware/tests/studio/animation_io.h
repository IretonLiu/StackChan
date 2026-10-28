/* SPDX-License-Identifier: MIT */
#pragma once

// JSON intake/emission for the editable animation model. Kept separate from animations.h
// so UI-free consumers do not pull ArduinoJson in.

#include "animations.h"

#include <ArduinoJson.h>

#include <string>

namespace kiwi::studio {

/**
 * Parses already-deserialized `poses` / `animations` members into `out`. Either may be
 * null/absent, in which case that block is left empty. Returns false and fills `error` on
 * malformed data.
 */
bool readAnimations(JsonVariantConst poses, JsonVariantConst animations, const SceneDoc& document, AnimationDoc& out,
                    std::string& error);

/** Emits `poses` and `animations` in the compiler-accepted schema (omitted when empty). */
void writeAnimationDoc(JsonDocument& document, const SceneDoc& scene);

}  // namespace kiwi::studio
