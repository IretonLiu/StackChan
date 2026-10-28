/* SPDX-License-Identifier: MIT */
#pragma once

// The single document-validity contract. Persistence, session save and any other
// consumer route through here, so "valid" means exactly one thing.

#include "types.h"

#include <string>

namespace kiwi::studio {

/** Machine- and human-friendly validation failure. */
struct Problem {
    std::string path;     // e.g. "nodes[2].style.opacity"; empty for scene-level
    std::string message;  // e.g. "must be a finite number in [0, 1]"

    std::string formatted() const
    {
        return path.empty() ? message : path + ": " + message;
    }
};

/** Lowercase kebab-case id: starts with a-z, then a-z0-9-. */
bool validId(const char* text);

/** Checks ids, parent ordering, numeric ranges, convexity, sampleability and motion
 *  applicability. On failure fills `problem`. */
bool validateScene(const SceneDoc& document, Problem& problem);

/** Convenience wrapper that formats the problem as "path: message". */
bool validateScene(const SceneDoc& document, std::string& error);

}  // namespace kiwi::studio
