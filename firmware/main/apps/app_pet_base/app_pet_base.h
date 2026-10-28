/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mooncake.h>
#include <pet/face/lvgl_face_renderer.h>
#include <pet/preset/face_preset_evaluator.h>
#include <pet/preset/face_animation_engine.h>
#include <pet/face/glyph_paths.h>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <cstdint>
#include <memory>

/**
 * The first custom-pet app.
 *
 * This intentionally contains no XiaoZhi types.  It proves the custom
 * firmware boot path while keeping the existing board bring-up underneath it.
 */
class AppPetBase : public mooncake::AppAbility {
public:
    AppPetBase();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<pet::face::LvglFaceRenderer> _face_renderer;
    const pet::preset::FacePresetDefinition* _preset = nullptr;
    pet::preset::FacePresetEvaluation _preset_evaluation{};
    pet::preset::FaceAnimationRuntime _animation_runtime{};
    pet::face::FaceDrawing _preset_drawing{};
    struct FaceDiagnostics {
        uint32_t started_ms       = 0;
        uint32_t frames           = 0;
        uint32_t renders          = 0;
        uint32_t tick_ms          = 0;
        uint32_t max_tick_ms      = 0;
        uint32_t failures         = 0;
        uint32_t evaluate_us      = 0;
        uint32_t advance_us       = 0;
        uint32_t build_us         = 0;
        uint32_t lock_us          = 0;
        uint32_t render_us        = 0;
        uint32_t max_evaluate_us  = 0;
        uint32_t max_advance_us   = 0;
        uint32_t max_build_us     = 0;
        uint32_t max_lock_us      = 0;
        uint32_t max_render_us    = 0;
    };
    FaceDiagnostics _face_diagnostics{};
    uint32_t _last_stackchan_update_ms = 0;
    uint32_t _last_face_update_ms      = 0;
    uint32_t _last_face_error_ms       = 0;
    bool _pose_dirty                   = false;
};
