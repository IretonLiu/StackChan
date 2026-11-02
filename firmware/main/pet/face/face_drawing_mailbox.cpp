/*
 * SPDX-License-Identifier: MIT
 */
#include "face_drawing_mailbox.h"

namespace pet::face {

bool FaceDrawingMailbox::publish(const FaceDrawing& drawing)
{
    // Prefer an unused slot. If all slots have data waiting, replacing a ready
    // slot drops an obsolete frame but never a drawing currently being read.
    int slot_index = claimForWrite(SlotState::Free);
    if (slot_index < 0) {
        slot_index = claimForWrite(SlotState::Ready);
    }
    if (slot_index < 0) {
        // The consumer is reading one slot and another producer write cannot
        // exist (this is SPSC), so this is only a transient contention case.
        return false;
    }

    Slot& slot       = slots_[static_cast<size_t>(slot_index)];
    slot.drawing     = drawing;
    slot.generation  = ++next_generation_;
    slot.state.store(SlotState::Ready, std::memory_order_release);
    return true;
}

bool FaceDrawingMailbox::consumeLatest(ReadLease& lease)
{
    release(lease);

    int selected_index       = -1;
    uint32_t selected_gen    = 0;
    for (size_t index = 0; index < slots_.size(); ++index) {
        Slot& slot            = slots_[index];
        SlotState expected    = SlotState::Ready;
        if (!slot.state.compare_exchange_strong(expected, SlotState::Reading, std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
            continue;
        }

        if (selected_index < 0 || newer(slot.generation, selected_gen)) {
            if (selected_index >= 0) {
                slots_[static_cast<size_t>(selected_index)].state.store(SlotState::Free, std::memory_order_release);
            }
            selected_index = static_cast<int>(index);
            selected_gen   = slot.generation;
        } else {
            slot.state.store(SlotState::Free, std::memory_order_release);
        }
    }

    if (selected_index < 0) {
        return false;
    }

    lease.slot_index = static_cast<uint8_t>(selected_index);
    lease.drawing    = &slots_[static_cast<size_t>(selected_index)].drawing;
    return true;
}

void FaceDrawingMailbox::release(ReadLease& lease)
{
    if (lease.drawing == nullptr) {
        return;
    }

    slots_[lease.slot_index].state.store(SlotState::Free, std::memory_order_release);
    lease = {};
}

bool FaceDrawingMailbox::newer(uint32_t candidate, uint32_t reference)
{
    return static_cast<int32_t>(candidate - reference) > 0;
}

int FaceDrawingMailbox::claimForWrite(SlotState expected)
{
    for (size_t index = 0; index < slots_.size(); ++index) {
        SlotState state = expected;
        if (slots_[index].state.compare_exchange_strong(state, SlotState::Writing, std::memory_order_acq_rel,
                                                         std::memory_order_relaxed)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}  // namespace pet::face
