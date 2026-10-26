/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "glyph_paths.h"

#include <cstdint>

namespace pet::face {

/** The first four user-authored procedural face states. */
enum class PetFaceState : uint8_t {
    Idle,
    Mischievous,
    Happy,
    Dizzy,
};

/**
 * Deterministic, asset-free face state machine and animation rig.
 *
 * It owns no clock, task, sensor, or renderer. The application feeds it input
 * intents and timestamps; the resulting shared path drawing can be consumed by
 * either the LVGL renderer or a host SVG preview.
 */
class PetFaceAnimator {
public:
    explicit PetFaceAnimator(uint32_t seed = 0xFACE1234);

    void reset(uint32_t now_ms);
    void onPetted(uint32_t now_ms);
    void onShaken(uint32_t now_ms);
    const FaceDrawing& sample(uint32_t now_ms);

    PetFaceState state() const;

private:
    uint32_t nextRandom();
    uint32_t nextIdleIntervalMs();
    void setState(PetFaceState state, uint32_t now_ms);

    PetFaceState state_ = PetFaceState::Idle;
    uint32_t state_started_ms_ = 0;
    uint32_t state_ends_ms_ = 0;
    uint32_t next_idle_change_ms_ = 0;
    uint32_t random_state_;
    FaceDrawing drawing_{};
};

}  // namespace pet::face
