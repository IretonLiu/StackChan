/* SPDX-License-Identifier: MIT */
#include <pet/sdf/sdf_face_renderer.h>

#include <array>
#include <cassert>
#include <cstdint>

namespace {

uint8_t red(uint32_t pixel)
{
    return static_cast<uint8_t>((pixel >> 16u) & 0xffu);
}

}  // namespace

int main()
{
    using namespace pet;
    sdf::Scene scene{};
    scene.background = {7, 16, 24};
    scene.node_count = 1;

    sdf::Node& eye = scene.nodes[0];
    eye.paint.color = {248, 245, 237};
    eye.primitive_count = 2;
    eye.primitives[0].kind = sdf::PrimitiveKind::Ellipse;
    eye.primitives[0].combine = sdf::Combine::Union;
    eye.primitives[0].center = {8.0f, 8.0f};
    eye.primitives[0].half = {6.0f, 6.0f};
    eye.primitives[1].kind = sdf::PrimitiveKind::HalfPlane;
    eye.primitives[1].combine = sdf::Combine::Subtract;
    eye.primitives[1].center = {8.0f, 6.0f};
    eye.primitives[1].half = {0.0f, 1.0f};

    std::array<uint32_t, 16 * 16> pixels{};
    assert(sdf::render(scene, {pixels.data(), 16, 16, 16}));

    // The half-plane subtracts the top of the white eye but leaves its lower aperture.
    assert(red(pixels[4 * 16 + 8]) == 7);
    assert(red(pixels[10 * 16 + 8]) > 200);
    // Outside the eye remains the background, proving direct raster coverage is bounded.
    assert(red(pixels[0]) == 7);
    return 0;
}
