/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/shape/soft_polygon.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

using pet::shape::Affine2D;
using pet::shape::EarLandmarks;
using pet::shape::PathCommandType;
using pet::shape::ShapePath;
using pet::shape::SoftPolygonEarParameters;
using pet::shape::SoftPolygonEarSample;
using pet::shape::Transform2D;
using pet::shape::Vec2;

constexpr float kUnset = std::numeric_limits<float>::quiet_NaN();

struct Options {
    std::string pose = "neutral";
    std::string output = "ear-preview.svg";
    bool debug = false;
    float x = kUnset;
    float y = kUnset;
    float rotation = kUnset;
    float scale_x = kUnset;
    float scale_y = kUnset;
    float lean = kUnset;
    float tip_roundness = kUnset;
    float base_roundness = kUnset;
    float left_bulge = kUnset;
    float right_bulge = kUnset;
    float base_bulge = kUnset;
    float left_start_tension = kUnset;
    float left_end_tension = kUnset;
    float right_start_tension = kUnset;
    float right_end_tension = kUnset;
    float base_start_tension = kUnset;
    float base_end_tension = kUnset;
    float tip_bend = kUnset;
};

struct EarPose {
    Transform2D transform{};
    SoftPolygonEarParameters parameters{};
};

void usage(const char* command)
{
    std::cerr << "Usage:\n"
              << "  " << command << " --pose neutral --debug --output neutral-ear.svg\n"
              << "  " << command << " --pose twitch --tip-bend 0.12 --rotation 8 --output twitch.svg\n\n"
              << "Poses: neutral, perked, twitch, droop\n"
              << "Overrides: --x --y --rotation --scale-x --scale-y --lean --tip-roundness\n"
              << "           --base-roundness --tip-bend\n"
              << "Corners:  --tip-roundness --base-roundness\n"
              << "Edges:    --left-bulge --left-start-tension --left-end-tension\n"
              << "           --right-bulge --right-start-tension --right-end-tension\n"
              << "           --base-bulge --base-start-tension --base-end-tension\n";
}

bool parseFloat(const char* input, float& value)
{
    try {
        value = std::stof(input);
        return std::isfinite(value);
    } catch (...) {
        return false;
    }
}

EarPose poseFromName(const std::string& name, bool& found)
{
    EarPose pose;
    pose.transform.translation = {160.0f, 142.0f};
    pose.transform.scale = {88.0f, 96.0f};
    pose.transform.pivot = {0.0f, 0.0f};
    pose.parameters.tip_corner.roundness = 0.10f;
    pose.parameters.base_corners.roundness = 0.07f;

    found = true;
    if (name == "neutral") {
        return pose;
    }
    if (name == "perked") {
        pose.transform.rotation_degrees = -8.0f;
        pose.transform.scale = {82.0f, 108.0f};
        return pose;
    }
    if (name == "twitch") {
        pose.transform.rotation_degrees = 12.0f;
        pose.parameters.tip_bend = 0.15f;
        return pose;
    }
    if (name == "droop") {
        pose.transform.rotation_degrees = -31.0f;
        pose.transform.scale = {92.0f, 72.0f};
        pose.parameters.tip_bend = -0.10f;
        return pose;
    }
    found = false;
    return pose;
}

void assignIfSet(float value, float& target)
{
    if (!std::isnan(value)) {
        target = value;
    }
}

void applyOverrides(const Options& options, EarPose& pose)
{
    assignIfSet(options.x, pose.transform.translation.x);
    assignIfSet(options.y, pose.transform.translation.y);
    assignIfSet(options.rotation, pose.transform.rotation_degrees);
    assignIfSet(options.scale_x, pose.transform.scale.x);
    assignIfSet(options.scale_y, pose.transform.scale.y);
    assignIfSet(options.lean, pose.parameters.lean);
    assignIfSet(options.tip_roundness, pose.parameters.tip_corner.roundness);
    assignIfSet(options.base_roundness, pose.parameters.base_corners.roundness);
    assignIfSet(options.left_bulge, pose.parameters.left_side.bulge);
    assignIfSet(options.right_bulge, pose.parameters.right_side.bulge);
    assignIfSet(options.base_bulge, pose.parameters.base.bulge);
    assignIfSet(options.left_start_tension, pose.parameters.left_side.start);
    assignIfSet(options.left_end_tension, pose.parameters.left_side.end);
    assignIfSet(options.right_start_tension, pose.parameters.right_side.start);
    assignIfSet(options.right_end_tension, pose.parameters.right_side.end);
    assignIfSet(options.base_start_tension, pose.parameters.base.start);
    assignIfSet(options.base_end_tension, pose.parameters.base.end);
    assignIfSet(options.tip_bend, pose.parameters.tip_bend);
}

std::string color(pet::shape::Color value)
{
    std::ostringstream result;
    result << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value.red) << std::setw(2)
           << static_cast<int>(value.green) << std::setw(2) << static_cast<int>(value.blue);
    return result.str();
}

void writePath(std::ostringstream& svg, const ShapePath& path)
{
    svg << "<path d=\"";
    for (uint8_t index = 0; index < path.command_count; ++index) {
        const auto& command = path.commands[index];
        if (command.type == PathCommandType::Move) {
            svg << "M " << command.first.x << ' ' << command.first.y << ' ';
        } else if (command.type == PathCommandType::Cubic) {
            svg << "C " << command.first.x << ' ' << command.first.y << ' ' << command.second.x << ' '
                << command.second.y << ' ' << command.end.x << ' ' << command.end.y << ' ';
        } else {
            svg << "Z ";
        }
    }
    svg << "\" fill=\"" << (path.style.fill ? color(path.style.color) : "none") << "\" stroke=\""
        << (path.style.stroke ? color(path.style.color) : "none") << "\" stroke-width=\"" << path.style.stroke_width
        << "\" opacity=\"" << path.style.opacity << "\" stroke-linejoin=\"round\"/>\n";
}

void writeLandmark(std::ostringstream& svg, Vec2 point, const char* label, const char* colour)
{
    svg << "<circle cx=\"" << point.x << "\" cy=\"" << point.y << "\" r=\"3.5\" fill=\"" << colour
        << "\"/><text x=\"" << point.x + 6.0f << "\" y=\"" << point.y - 5.0f
        << "\" fill=\"" << colour << "\" font-size=\"10\">" << label << "</text>\n";
}

std::string renderSvg(const SoftPolygonEarSample& sample, Vec2 pivot, bool debug)
{
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 240\" width=\"320\" height=\"240\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#071018\"/>\n";
    writePath(svg, sample.path);
    if (debug) {
        const auto& bounds = sample.path.bounds;
        svg << "<rect x=\"" << bounds.minimum.x << "\" y=\"" << bounds.minimum.y << "\" width=\""
            << bounds.maximum.x - bounds.minimum.x << "\" height=\"" << bounds.maximum.y - bounds.minimum.y
            << "\" fill=\"none\" stroke=\"#64748b\" stroke-dasharray=\"3 3\"/>\n";
        svg << "<path d=\"M " << sample.landmarks.base_left.x << ' ' << sample.landmarks.base_left.y << " L "
            << sample.landmarks.tip.x << ' ' << sample.landmarks.tip.y << " L " << sample.landmarks.base_right.x
            << ' ' << sample.landmarks.base_right.y << "\" fill=\"none\" stroke=\"#475569\" stroke-dasharray=\"2 2\"/>\n";
        writeLandmark(svg, sample.landmarks.base_left, "base_left", "#22c55e");
        writeLandmark(svg, sample.landmarks.tip, "tip", "#f59e0b");
        writeLandmark(svg, sample.landmarks.base_right, "base_right", "#22c55e");
        writeLandmark(svg, pivot, "pivot", "#f472b6");
    }
    return svg.str() + "</svg>\n";
}

bool writeFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Could not open " << path << " for writing\n";
        return false;
    }
    file << content;
    return static_cast<bool>(file);
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto next = [&]() -> const char* {
            if (++index >= argc) {
                std::cerr << "Missing value for " << argument << '\n';
                return nullptr;
            }
            return argv[index];
        };
        const auto parse = [&](float& destination) -> bool {
            const char* value = next();
            return value && parseFloat(value, destination);
        };

        if (argument == "--pose") {
            const char* value = next();
            if (!value) return 2;
            options.pose = value;
        } else if (argument == "--output") {
            const char* value = next();
            if (!value) return 2;
            options.output = value;
        } else if (argument == "--debug") {
            options.debug = true;
        } else if (argument == "--x") { if (!parse(options.x)) return 2;
        } else if (argument == "--y") { if (!parse(options.y)) return 2;
        } else if (argument == "--rotation") { if (!parse(options.rotation)) return 2;
        } else if (argument == "--scale-x") { if (!parse(options.scale_x)) return 2;
        } else if (argument == "--scale-y") { if (!parse(options.scale_y)) return 2;
        } else if (argument == "--lean") { if (!parse(options.lean)) return 2;
        } else if (argument == "--tip-roundness") { if (!parse(options.tip_roundness)) return 2;
        } else if (argument == "--base-roundness") { if (!parse(options.base_roundness)) return 2;
        } else if (argument == "--left-bulge") { if (!parse(options.left_bulge)) return 2;
        } else if (argument == "--right-bulge") { if (!parse(options.right_bulge)) return 2;
        } else if (argument == "--base-bulge") { if (!parse(options.base_bulge)) return 2;
        } else if (argument == "--left-start-tension") { if (!parse(options.left_start_tension)) return 2;
        } else if (argument == "--left-end-tension") { if (!parse(options.left_end_tension)) return 2;
        } else if (argument == "--right-start-tension") { if (!parse(options.right_start_tension)) return 2;
        } else if (argument == "--right-end-tension") { if (!parse(options.right_end_tension)) return 2;
        } else if (argument == "--base-start-tension") { if (!parse(options.base_start_tension)) return 2;
        } else if (argument == "--base-end-tension") { if (!parse(options.base_end_tension)) return 2;
        } else if (argument == "--tip-bend") { if (!parse(options.tip_bend)) return 2;
        } else if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << argument << '\n';
            usage(argv[0]);
            return 2;
        }
    }

    bool known_pose = false;
    EarPose pose = poseFromName(options.pose, known_pose);
    if (!known_pose) {
        std::cerr << "Unknown pose: " << options.pose << '\n';
        usage(argv[0]);
        return 2;
    }
    applyOverrides(options, pose);

    const Affine2D world = pet::shape::makeAffine(pose.transform);
    SoftPolygonEarSample sample;
    if (!pet::shape::sampleSoftPolygonEar(pose.parameters, world, sample)) {
        std::cerr << "Could not sample ear: invalid numeric input (error "
                  << static_cast<int>(sample.error) << ")\n";
        return 2;
    }
    if (sample.parameters_were_clamped) {
        std::cerr << "Warning: one or more values were clamped to the template-safe range\n";
    }
    const Vec2 pivot = pet::shape::transformPoint(world, pose.transform.pivot);
    return writeFile(options.output, renderSvg(sample, pivot, options.debug)) ? 0 : 1;
}
