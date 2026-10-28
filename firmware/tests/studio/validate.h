/* SPDX-License-Identifier: MIT */
#pragma once

// The single document-validity contract. Persistence, the inspector and document
// operations all route through here so a scene that validates is a scene the compiler
// and firmware can consume.

#include "types.h"

#include <string>

namespace kiwi::studio {

/** Lowercase kebab-case id: starts with a-z, then a-z0-9-. */
bool validId(const char* text);

/** Checks ids, parent ordering, numeric ranges, convexity and sampleability. */
bool validateScene(const SceneDoc& document, std::string& error);

}  // namespace kiwi::studio
