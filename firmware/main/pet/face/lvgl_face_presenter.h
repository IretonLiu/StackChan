/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "face_drawing_mailbox.h"
#include "lvgl_face_renderer.h"

#include <atomic>
#include <cstdint>
#include <limits>

struct _lv_timer_t;
typedef struct _lv_timer_t lv_timer_t;
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace pet::face {

struct LvglFacePresentationMetrics {
    uint32_t renders       = 0;
    uint32_t render_us     = 0;
    uint32_t max_render_us       = 0;
    uint32_t min_stack_free_bytes = 0;
};

/**
 * LVGL-owned face consumer.
 *
 * Construct and destroy this object while the caller holds the LVGL lock. Its
 * persistent LVGL timer runs inside lv_timer_handler(), so render() never
 * contends with the app task for LVGL ownership.
 */
class LvglFacePresenter {
public:
    // Align to the configured LVGL refresh period. Faster timer periods did
    // not increase observed presentation throughput on the ILI9342 path.
    static constexpr uint32_t kPresentationIntervalMs = 33;

    LvglFacePresenter(lv_obj_t* parent, FaceDrawingMailbox& mailbox);
    ~LvglFacePresenter();

    LvglFacePresenter(const LvglFacePresenter&)            = delete;
    LvglFacePresenter& operator=(const LvglFacePresenter&) = delete;

    LvglFacePresentationMetrics takeMetrics();

private:
    static void onTimer(lv_timer_t* timer);
    void presentLatest();
    void recordRender(uint32_t elapsed_us, uint32_t stack_free_bytes);

    FaceDrawingMailbox& mailbox_;
    LvglFaceRenderer renderer_;
    lv_timer_t* timer_ = nullptr;
    std::atomic_uint32_t renders_{0};
    std::atomic_uint32_t render_us_{0};
    std::atomic_uint32_t max_render_us_{0};
    std::atomic_uint32_t min_stack_free_bytes_{std::numeric_limits<uint32_t>::max()};
};

}  // namespace pet::face
