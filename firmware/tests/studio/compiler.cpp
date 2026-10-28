/* SPDX-License-Identifier: MIT */
#include "compiler.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace kiwi::studio {
namespace {

constexpr const char* kLogPath = "/tmp/kiwi-studio-validation.log";

std::string shellQuote(const std::string& value)
{
    std::string quoted{"'"};
    for (char character : value) quoted += character == '\'' ? "'\\\"'\\\"'" : std::string(1, character);
    return quoted + "'";
}

std::string readLog()
{
    std::ifstream file(kLogPath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    return text.empty() ? "Preset compiler produced no output." : text;
}

}  // namespace

CompilerResult runPresetCompiler(const std::string& filename)
{
    const std::string command = "python3 tools/compile_face_presets.py " + shellQuote(filename) +
                                " --output /tmp/kiwi-studio-preset.h > " + kLogPath + " 2>&1";
    CompilerResult result;
    result.ok  = std::system(command.c_str()) == 0;
    result.log = readLog();

    std::getline(std::istringstream(result.log), result.first_line);
    if (result.first_line.empty()) result.first_line = "Compiler rejected the scene; see " + std::string(kLogPath);
    return result;
}

}  // namespace kiwi::studio
