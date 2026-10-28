/*
 * SPDX-License-Identifier: MIT
 */
#include <pet/preset/face_preset_evaluator.h>
#include <pet/preset/face_preset_renderer.h>
#include <pet/shape/circle.h>
#include <pet/shape/rounded_polygon.h>
#include <pet/shape/shape_face_adapter.h>
#include <pet/shape/soft_polygon.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using pet::shape::Affine2D;
using pet::shape::PathCommandType;
using pet::shape::SoftPolygonEarParameters;
using pet::shape::Transform2D;
using pet::shape::Vec2;

bool near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

void testAffinePivotAndComposition()
{
    Transform2D parent_pose;
    parent_pose.translation      = {100.0f, 40.0f};
    parent_pose.rotation_degrees = 90.0f;
    const Affine2D parent        = pet::shape::makeAffine(parent_pose);

    Transform2D local_pose;
    local_pose.translation = {10.0f, 0.0f};
    const Affine2D local   = pet::shape::makeAffine(local_pose);
    const Vec2 combined    = pet::shape::transformPoint(pet::shape::compose(parent, local), {});
    assert(near(combined.x, 100.0f));
    assert(near(combined.y, 50.0f));

    Transform2D pivot_pose;
    pivot_pose.translation      = {20.0f, 30.0f};
    pivot_pose.scale            = {7.0f, 9.0f};
    pivot_pose.rotation_degrees = 33.0f;
    pivot_pose.pivot            = {0.25f, -0.5f};
    const Vec2 pivot            = pet::shape::transformPoint(pet::shape::makeAffine(pivot_pose), pivot_pose.pivot);
    assert(near(pivot.x, pivot_pose.translation.x));
    assert(near(pivot.y, pivot_pose.translation.y));

    pet::shape::Affine2D inverse;
    assert(pet::shape::invert(pet::shape::makeAffine(pivot_pose), inverse));
    const Vec2 source{0.63f, -0.27f};
    const Vec2 round_trip =
        pet::shape::transformPoint(inverse, pet::shape::transformPoint(pet::shape::makeAffine(pivot_pose), source));
    assert(near(round_trip.x, source.x));
    assert(near(round_trip.y, source.y));

    Transform2D singular_pose;
    singular_pose.scale = {0.0f, 1.0f};
    assert(!pet::shape::invert(pet::shape::makeAffine(singular_pose), inverse));
}

void testEarPathAndBounds()
{
    Transform2D transform;
    transform.translation = {160.0f, 140.0f};
    transform.scale       = {80.0f, 100.0f};
    pet::shape::SoftPolygonEarSample sample;
    assert(pet::shape::sampleSoftPolygonEar({}, pet::shape::makeAffine(transform), sample));

    // Move + three edge cubics + three corner cubics + close.
    assert(sample.path.command_count == 8);
    assert(sample.path.commands[0].type == PathCommandType::Move);
    assert(sample.path.commands[7].type == PathCommandType::Close);
    assert(sample.path.bounds.valid);
    assert(near(sample.landmarks.base_left.x, 120.0f));
    assert(near(sample.landmarks.base_left.y, 140.0f));
    assert(near(sample.landmarks.tip.x, 160.0f));
    assert(near(sample.landmarks.tip.y, 40.0f));

    for (uint8_t index = 0; index < sample.path.command_count; ++index) {
        const auto& command = sample.path.commands[index];
        assert(std::isfinite(command.first.x));
        assert(std::isfinite(command.first.y));
        assert(std::isfinite(command.second.x));
        assert(std::isfinite(command.second.y));
        assert(std::isfinite(command.end.x));
        assert(std::isfinite(command.end.y));
    }
}

float cross(Vec2 left, Vec2 right)
{
    return left.x * right.y - left.y * right.x;
}

Vec2 subtract(Vec2 left, Vec2 right)
{
    return {left.x - right.x, left.y - right.y};
}

void testEveryEdgeCornerJoinIsTangentAligned()
{
    SoftPolygonEarParameters parameters;
    parameters.left_side.bulge  = 0.30f;
    parameters.right_side.bulge = -0.22f;
    parameters.base.bulge       = 0.12f;
    pet::shape::SoftPolygonEarSample sample;
    assert(pet::shape::sampleSoftPolygonEar(parameters, {}, sample));
    for (size_t edge_index = 0; edge_index < sample.edges.size(); ++edge_index) {
        const size_t next_corner     = (edge_index + 1) % sample.corners.size();
        const auto& edge             = sample.edges[edge_index];
        const auto& following_corner = sample.corners[next_corner];
        const auto& preceding_corner = sample.corners[edge_index];

        // End of edge -> start of following corner.
        assert(std::abs(cross(subtract(edge.end, edge.second_control),
                              subtract(following_corner.first_control, following_corner.start))) < 0.0001f);
        // End of preceding corner -> start of edge.
        assert(std::abs(cross(subtract(preceding_corner.end, preceding_corner.second_control),
                              subtract(edge.first_control, edge.start))) < 0.0001f);
    }
}

void testParameterClamping()
{
    SoftPolygonEarParameters unsafe;
    unsafe.lean                   = 9.0f;
    unsafe.tip_corner.roundness   = -1.0f;
    unsafe.base_corners.roundness = 3.0f;
    unsafe.tip_corner.cap_tension = 9.0f;
    unsafe.left_side.bulge        = 9.0f;
    unsafe.left_side.start        = -9.0f;
    unsafe.left_side.end          = 9.0f;
    unsafe.tip_bend               = 9.0f;
    pet::shape::SoftPolygonEarSample sample;
    assert(pet::shape::sampleSoftPolygonEar(unsafe, {}, sample));
    assert(sample.parameters_were_clamped);
    assert(near(sample.parameters.lean, 0.25f));
    assert(near(sample.parameters.tip_corner.roundness, 0.0f));
    assert(near(sample.parameters.base_corners.roundness, 0.35f));
    assert(near(sample.parameters.tip_corner.cap_tension, 0.90f));
    assert(near(sample.parameters.left_side.bulge, 0.35f));
    assert(near(sample.parameters.left_side.start, 0.08f));
    assert(near(sample.parameters.left_side.end, 0.45f));
    assert(near(sample.parameters.tip_bend, 0.20f));
}

void testNonFiniteInputsAreRejected()
{
    SoftPolygonEarParameters parameters;
    pet::shape::SoftPolygonEarSample sample;

    parameters.lean = std::numeric_limits<float>::quiet_NaN();
    assert(!pet::shape::sampleSoftPolygonEar(parameters, {}, sample));
    assert(sample.error == pet::shape::SoftPolygonEarSampleError::NonFiniteParameters);
    assert(sample.path.command_count == 0);

    parameters          = {};
    parameters.tip_bend = std::numeric_limits<float>::infinity();
    assert(!pet::shape::sampleSoftPolygonEar(parameters, {}, sample));
    assert(sample.error == pet::shape::SoftPolygonEarSampleError::NonFiniteParameters);

    pet::shape::Affine2D invalid_world;
    invalid_world.tx = -std::numeric_limits<float>::infinity();
    assert(!pet::shape::sampleSoftPolygonEar({}, invalid_world, sample));
    assert(sample.error == pet::shape::SoftPolygonEarSampleError::NonFiniteTransform);

    pet::shape::PathStyle invalid_style;
    invalid_style.opacity = std::numeric_limits<float>::quiet_NaN();
    assert(!pet::shape::sampleSoftPolygonEar({}, {}, sample, invalid_style));
    assert(sample.error == pet::shape::SoftPolygonEarSampleError::NonFiniteStyle);
}

void testRoundedPolygonAndCircle()
{
    pet::shape::RoundedPolygonParameters polygon;
    polygon.corner_count     = 5;
    polygon.corner_positions = {{{0.0f, -1.0f}, {0.95f, -0.3f}, {0.59f, 0.81f}, {-0.59f, 0.81f}, {-0.95f, -0.3f}}};
    for (uint8_t index = 0; index < polygon.corner_count; ++index) {
        polygon.corners[index] = {0.12f, 0.55f};
        polygon.edges[index]   = {(index % 2 == 0) ? 0.20f : -0.12f, 0.28f, 0.28f};
    }
    pet::shape::RoundedPolygonSample polygon_sample;
    assert(pet::shape::sampleRoundedPolygon(polygon, {}, polygon_sample));
    assert(polygon_sample.path.command_count == 12);
    assert(polygon_sample.path.commands[0].type == PathCommandType::Move);
    assert(polygon_sample.path.commands[11].type == PathCommandType::Close);
    for (uint8_t index = 0; index < polygon.corner_count; ++index) {
        const size_t next_corner     = (index + 1) % polygon.corner_count;
        const auto& edge             = polygon_sample.edges[index];
        const auto& following_corner = polygon_sample.corners[next_corner];
        const auto& preceding_corner = polygon_sample.corners[index];
        assert(std::abs(cross(subtract(edge.end, edge.second_control),
                              subtract(following_corner.first_control, following_corner.start))) < 0.0001f);
        assert(std::abs(cross(subtract(preceding_corner.end, preceding_corner.second_control),
                              subtract(edge.first_control, edge.start))) < 0.0001f);
    }

    polygon.corner_count = 2;
    assert(!pet::shape::sampleRoundedPolygon(polygon, {}, polygon_sample));
    assert(polygon_sample.error == pet::shape::RoundedPolygonSampleError::InvalidCornerCount);
    polygon.corner_count     = 4;
    polygon.corner_positions = {{{-1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}}};
    assert(!pet::shape::sampleRoundedPolygon(polygon, {}, polygon_sample));
    assert(polygon_sample.error == pet::shape::RoundedPolygonSampleError::DegenerateOrNonConvex);

    pet::shape::CircleSample circle;
    pet::shape::CircleParameters circle_parameters{{0.0f, 0.0f}, 1.0f};
    assert(pet::shape::sampleCircle(circle_parameters, {}, circle));
    assert(circle.path.command_count == 6);
    assert(circle.path.commands[0].type == PathCommandType::Move);
    assert(circle.path.commands[5].type == PathCommandType::Close);
    circle_parameters.radius = 0.0f;
    assert(!pet::shape::sampleCircle(circle_parameters, {}, circle));
    assert(circle.error == pet::shape::CircleSampleError::InvalidRadius);
}

void testPresetEvaluatorPreservesStaticDefinitions()
{
    pet::preset::FacePresetNodeDefinition nodes[2]{};
    nodes[0].kind                  = pet::preset::ShapeKind::Circle;
    nodes[0].transform.translation = {8.0f, 9.0f};
    nodes[0].circle                = {{10.0f, 20.0f}, 5.0f};
    nodes[1].kind                  = pet::preset::ShapeKind::Circle;
    nodes[1].parent_index          = 0;
    nodes[1].circle                = {{3.0f, 4.0f}, 2.0f};
    const pet::preset::FacePresetDefinition preset{"test", {7, 16, 24}, nodes, 2};

    pet::preset::FacePresetEvaluation evaluation;
    pet::preset::FacePresetEvaluationError error = pet::preset::FacePresetEvaluationError::EmptyPreset;
    assert(pet::preset::evaluateFacePreset(preset, 1234, evaluation, &error));
    assert(error == pet::preset::FacePresetEvaluationError::None);
    assert(evaluation.node_count == 2);
    assert(evaluation.background.red == 7);
    assert(near(evaluation.nodes[0].transform.translation.x, 8.0f));
    assert(evaluation.nodes[1].parent_index == 0);
    assert(near(evaluation.nodes[1].circle.radius, 2.0f));

    const pet::preset::FacePresetEvaluation preserved = evaluation;
    nodes[1].parent_index                             = 1;
    assert(!pet::preset::evaluateFacePreset(preset, 0, evaluation, &error));
    assert(error == pet::preset::FacePresetEvaluationError::InvalidParent);
    assert(evaluation.node_count == preserved.node_count);
    assert(near(evaluation.nodes[1].circle.radius, preserved.nodes[1].circle.radius));
    nodes[1].parent_index      = 0;
    nodes[0].transform.scale.x = 0.0f;
    assert(!pet::preset::evaluateFacePreset(preset, 0, evaluation, &error));
    assert(error == pet::preset::FacePresetEvaluationError::InvalidTransform);
}

void testCompiledPresetBuildsFaceDrawing()
{
    pet::preset::FacePresetNodeDefinition nodes[2]{};
    nodes[0].kind                             = pet::preset::ShapeKind::Circle;
    nodes[0].circle                           = {{10.0f, 20.0f}, 5.0f};
    nodes[1].kind                             = pet::preset::ShapeKind::RoundedPolygon;
    nodes[1].rounded_polygon.corner_count     = 3;
    nodes[1].rounded_polygon.corner_positions = {{{0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
    pet::preset::FacePresetDefinition preset{"test", {7, 16, 24}, nodes, 2};
    pet::face::FaceDrawing drawing;
    assert(pet::preset::buildFaceDrawing(preset, drawing));
    assert(drawing.path_count == 2);
    assert(drawing.background.red == 7);

    pet::preset::FacePresetEvaluation evaluation;
    assert(pet::preset::evaluateFacePreset(preset, 0, evaluation));
    pet::face::FaceDrawing evaluated_drawing;
    assert(pet::preset::buildFaceDrawing(evaluation, evaluated_drawing));
    assert(evaluated_drawing.path_count == drawing.path_count);
    assert(near(evaluated_drawing.paths[0].commands[0].first.x, drawing.paths[0].commands[0].first.x));
    assert(near(evaluated_drawing.paths[1].commands[0].first.y, drawing.paths[1].commands[0].first.y));

    nodes[1].parent_index = 1;
    assert(!pet::preset::buildFaceDrawing(preset, drawing));
}

void testShapePathFlowsToFirmwareDrawing()
{
    pet::shape::SoftPolygonEarSample sample;
    assert(pet::shape::sampleSoftPolygonEar({}, {}, sample));

    pet::face::FaceDrawing drawing;
    drawing.background = {7, 16, 24};
    assert(pet::shape::appendToFaceDrawing(sample.path, drawing));
    assert(drawing.path_count == 1);

    // FaceDrawing is the exact input consumed by LvglFaceRenderer::render().
    const pet::face::FacePath& path = drawing.paths[0];
    assert(path.command_count == sample.path.command_count);
    assert(path.style.color.red == sample.path.style.color.red);
    assert(path.style.color.green == sample.path.style.color.green);
    assert(path.style.color.blue == sample.path.style.color.blue);
    assert(path.style.opacity == sample.path.style.opacity);
    assert(path.style.stroke_width == sample.path.style.stroke_width);
    assert(path.style.fill == sample.path.style.fill);
    assert(path.style.stroke == sample.path.style.stroke);
    assert(path.commands[0].type == pet::face::PathCommandType::Move);
    assert(path.commands[1].type == pet::face::PathCommandType::Cubic);
    assert(path.commands[path.command_count - 1].type == pet::face::PathCommandType::Close);
    assert(near(path.commands[1].first.x, sample.path.commands[1].first.x));
    assert(near(path.commands[1].second.y, sample.path.commands[1].second.y));
    assert(near(path.commands[1].end.x, sample.path.commands[1].end.x));

    drawing.path_count = static_cast<uint8_t>(drawing.paths.size());
    assert(!pet::shape::appendToFaceDrawing(sample.path, drawing));

    pet::shape::ShapePath invalid_path = sample.path;
    invalid_path.commands[0].type      = static_cast<PathCommandType>(255);
    pet::face::FacePath converted;
    converted.command_count = 99;
    assert(!pet::shape::toFacePath(invalid_path, converted));
    assert(converted.command_count == 99);

    invalid_path               = sample.path;
    invalid_path.style.opacity = std::numeric_limits<float>::quiet_NaN();
    assert(!pet::shape::toFacePath(invalid_path, converted));
}

}  // namespace

int main()
{
    testAffinePivotAndComposition();
    testEarPathAndBounds();
    testEveryEdgeCornerJoinIsTangentAligned();
    testParameterClamping();
    testNonFiniteInputsAreRejected();
    testRoundedPolygonAndCircle();
    testPresetEvaluatorPreservesStaticDefinitions();
    testCompiledPresetBuildsFaceDrawing();
    testShapePathFlowsToFirmwareDrawing();
    std::cout << "shape core tests passed\n";
}
