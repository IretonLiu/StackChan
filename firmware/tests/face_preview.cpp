/*
 * SPDX-License-Identifier: MIT
 */
#include "face_svg_renderer.h"

#include <pet/face/face.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using pet::face::ExpressionController;
using pet::face::ExpressionId;
using pet::face::FacePose;
using pet::face::Transition;

struct Options {
    ExpressionId from = ExpressionId::Idle;
    ExpressionId to = ExpressionId::Idle;
    float intensity = 1.0f;
    uint32_t duration_ms = 0;
    uint32_t frames = 1;
    uint32_t seed = 0x5EED1234;
    std::string output = "face-preview.svg";
    bool output_is_directory = false;
};

void usage(const char* command)
{
    std::cerr << "Usage:\n"
              << "  " << command << " --expression happy --output happy.svg\n"
              << "  " << command
              << " --from idle --to excited --duration-ms 220 --frames 12 --output preview-dir\n"
              << "Expressions: idle, curious, happy, excited, hungry, lonely, sleepy, sleeping, waking, petted, fed, startled, paired\n";
}

bool expressionFromName(const std::string& input, ExpressionId& expression)
{
    std::string name;
    name.reserve(input.size());
    for (char character : input) {
        name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    const std::pair<const char*, ExpressionId> names[] = {
        {"idle", ExpressionId::Idle},         {"curious", ExpressionId::Curious},
        {"happy", ExpressionId::Happy},       {"excited", ExpressionId::Excited},
        {"hungry", ExpressionId::Hungry},     {"lonely", ExpressionId::Lonely},
        {"sleepy", ExpressionId::Sleepy},     {"sleeping", ExpressionId::Sleeping},
        {"waking", ExpressionId::Waking},     {"petted", ExpressionId::Petted},
        {"fed", ExpressionId::Fed},           {"startled", ExpressionId::Startled},
        {"paired", ExpressionId::Paired},
    };
    for (const auto& [label, value] : names) {
        if (name == label) {
            expression = value;
            return true;
        }
    }
    return false;
}

bool parseUnsigned(const char* text, uint32_t& value)
{
    try {
        value = static_cast<uint32_t>(std::stoul(text));
        return true;
    } catch (...) {
        return false;
    }
}

bool parseFloat(const char* text, float& value)
{
    try {
        value = std::stof(text);
        return true;
    } catch (...) {
        return false;
    }
}

bool writeFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Could not open " << path << " for writing\n";
        return false;
    }
    file << content;
    return static_cast<bool>(file);
}

std::string frameName(uint32_t frame)
{
    char name[32];
    std::snprintf(name, sizeof(name), "frame-%03u.svg", frame);
    return name;
}

std::string contactSheet(uint32_t frame_count)
{
    std::string html = "<!doctype html><title>StackChan face preview</title>\n"
                       "<style>body{background:#111;color:#eee;font-family:sans-serif}main{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0}img{width:320px;image-rendering:auto}figcaption{text-align:center}</style>\n"
                       "<h1>StackChan face preview</h1><main>\n";
    for (uint32_t frame = 0; frame < frame_count; ++frame) {
        html += "<figure><img src=\"" + frameName(frame) + "\"><figcaption>frame " + std::to_string(frame) +
                "</figcaption></figure>\n";
    }
    return html + "</main>\n";
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    bool expression_given = false;
    bool to_given = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto next = [&]() -> const char* {
            if (++index >= argc) {
                std::cerr << "Missing value for " << argument << '\n';
                return nullptr;
            }
            return argv[index];
        };

        if (argument == "--expression") {
            const char* value = next();
            if (!value || !expressionFromName(value, options.to)) return 2;
            expression_given = true;
        } else if (argument == "--from") {
            const char* value = next();
            if (!value || !expressionFromName(value, options.from)) return 2;
        } else if (argument == "--to") {
            const char* value = next();
            if (!value || !expressionFromName(value, options.to)) return 2;
            to_given = true;
        } else if (argument == "--intensity") {
            const char* value = next();
            if (!value || !parseFloat(value, options.intensity)) return 2;
        } else if (argument == "--duration-ms") {
            const char* value = next();
            if (!value || !parseUnsigned(value, options.duration_ms)) return 2;
        } else if (argument == "--frames") {
            const char* value = next();
            if (!value || !parseUnsigned(value, options.frames) || options.frames == 0) return 2;
        } else if (argument == "--seed") {
            const char* value = next();
            if (!value || !parseUnsigned(value, options.seed)) return 2;
        } else if (argument == "--output") {
            const char* value = next();
            if (!value) return 2;
            options.output = value;
        } else if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << argument << '\n';
            usage(argv[0]);
            return 2;
        }
    }

    if (!expression_given && !to_given) {
        std::cerr << "An expression is required\n";
        usage(argv[0]);
        return 2;
    }
    options.output_is_directory = options.duration_ms > 0 || options.frames > 1 || to_given;
    options.frames = options.output_is_directory ? std::max<uint32_t>(options.frames, 2) : 1;

    ExpressionController controller(options.seed);
    controller.request(options.from, 1.0f, Transition{0}, 0);
    controller.request(options.to, options.intensity, Transition{options.duration_ms}, 0);

    if (!options.output_is_directory) {
        return writeFile(options.output, pet::face::test::renderSvg(controller.sample(0))) ? 0 : 1;
    }

    const std::filesystem::path output_dir(options.output);
    std::filesystem::create_directories(output_dir);
    for (uint32_t frame = 0; frame < options.frames; ++frame) {
        const float progress = static_cast<float>(frame) / static_cast<float>(options.frames - 1);
        const uint32_t now = static_cast<uint32_t>(progress * options.duration_ms);
        if (!writeFile(output_dir / frameName(frame), pet::face::test::renderSvg(controller.sample(now)))) {
            return 1;
        }
    }
    return writeFile(output_dir / "index.html", contactSheet(options.frames)) ? 0 : 1;
}
