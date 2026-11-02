/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "glyph_paths.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace pet::face {

/**
 * Single-producer/single-consumer latest-state mailbox for FaceDrawing values.
 *
 * The app task publishes complete drawings; the LVGL task claims only the
 * newest ready slot. Three fixed slots ensure the producer never overwrites a
 * drawing while the renderer is using it. Pending ready drawings may be
 * replaced deliberately: presentation is latest-state, not a frame queue.
 */
class FaceDrawingMailbox {
public:
    struct ReadLease {
        const FaceDrawing* drawing = nullptr;
        uint8_t slot_index         = 0;
    };

    bool publish(const FaceDrawing& drawing);
    bool consumeLatest(ReadLease& lease);
    void release(ReadLease& lease);

private:
    enum class SlotState : uint8_t {
        Free,
        Writing,
        Ready,
        Reading,
    };

    struct Slot {
        std::atomic<SlotState> state{SlotState::Free};
        FaceDrawing drawing{};
        uint32_t generation = 0;
    };

    static constexpr size_t kSlotCount = 3;

    static bool newer(uint32_t candidate, uint32_t reference);
    int claimForWrite(SlotState expected);

    std::array<Slot, kSlotCount> slots_{};
    uint32_t next_generation_ = 0;  // Producer-owned.
};

}  // namespace pet::face
