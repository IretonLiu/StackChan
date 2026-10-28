/* SPDX-License-Identifier: MIT */
#pragma once

// Host-side bridge to tools/compile_face_presets.py. Owns the shell invocation and log
// parsing so the UI never has to know about process plumbing.

#include <string>

namespace kiwi::studio {

struct CompilerResult {
    bool ok = false;
    std::string log;         // full compiler output
    std::string first_line;  // first line of the log, or a fallback message
};

/** Runs the preset compiler on `filename`. Thread-safe enough for a single UI thread. */
CompilerResult runPresetCompiler(const std::string& filename);

}  // namespace kiwi::studio
