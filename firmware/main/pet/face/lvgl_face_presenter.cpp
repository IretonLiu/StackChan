/*
 * SPDX-License-Identifier: MIT
 */
#include "lvgl_face_presenter.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

namespace pet::face {

LvglFacePresenter::LvglFacePresenter(lv_obj_t* parent, FaceDrawingMailbox& mailbox)
    : mailbox_(mailbox), renderer_(parent)
{
    timer_ = lv_timer_create(&LvglFacePresenter::onTimer, kPresentationIntervalMs, this);
    // The initial drawing was published before construction. Do not leave the
    // opening face invisible until the first regular 67 ms timer interval.
    if (timer_ != nullptr) {
        lv_timer_ready(timer_);
    }
}

LvglFacePresenter::~LvglFacePresenter()
{
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
    }
}

LvglFacePresentationMetrics LvglFacePresenter::takeMetrics()
{
    LvglFacePresentationMetrics metrics;
    metrics.renders       = renders_.exchange(0, std::memory_order_relaxed);
    metrics.render_us     = render_us_.exchange(0, std::memory_order_relaxed);
    metrics.max_render_us       = max_render_us_.exchange(0, std::memory_order_relaxed);
    metrics.min_stack_free_bytes = min_stack_free_bytes_.exchange(std::numeric_limits<uint32_t>::max(),
                                                                    std::memory_order_relaxed);
    if (metrics.min_stack_free_bytes == std::numeric_limits<uint32_t>::max()) {
        metrics.min_stack_free_bytes = 0;
    }
    return metrics;
}

void LvglFacePresenter::onTimer(lv_timer_t* timer)
{
    auto* presenter = static_cast<LvglFacePresenter*>(lv_timer_get_user_data(timer));
    if (presenter != nullptr) {
        presenter->presentLatest();
    }
}

void LvglFacePresenter::presentLatest()
{
    FaceDrawingMailbox::ReadLease lease;
    if (!mailbox_.consumeLatest(lease)) {
        return;
    }

    const int64_t started_us = esp_timer_get_time();
    renderer_.render(*lease.drawing);
    const uint32_t stack_free_bytes = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
    recordRender(static_cast<uint32_t>(esp_timer_get_time() - started_us), stack_free_bytes);
    mailbox_.release(lease);
}

void LvglFacePresenter::recordRender(uint32_t elapsed_us, uint32_t stack_free_bytes)
{
    renders_.fetch_add(1, std::memory_order_relaxed);
    render_us_.fetch_add(elapsed_us, std::memory_order_relaxed);

    uint32_t previous = max_render_us_.load(std::memory_order_relaxed);
    while (previous < elapsed_us &&
           !max_render_us_.compare_exchange_weak(previous, elapsed_us, std::memory_order_relaxed,
                                                  std::memory_order_relaxed)) {
    }

    previous = min_stack_free_bytes_.load(std::memory_order_relaxed);
    while (previous > stack_free_bytes &&
           !min_stack_free_bytes_.compare_exchange_weak(previous, stack_free_bytes, std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {
    }
}

}  // namespace pet::face
