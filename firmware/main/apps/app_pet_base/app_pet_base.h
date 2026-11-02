/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mooncake.h>
#include <lvgl.h>
#include <pet/behaviour/pet_face_behaviour_controller.h>
#include <pet/face/face_drawing_mailbox.h>
#include <pet/face/lvgl_face_presenter.h>
#include <pet/preset/face_preset_evaluator.h>
#include <pet/preset/face_animation_engine.h>
#include <pet/face/glyph_paths.h>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <atomic>
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
    static void onScreenTap(lv_event_t* event);
    void applyBehaviourOutput(const pet::behaviour::PetFaceBehaviourOutput& output);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    pet::face::FaceDrawingMailbox _drawing_mailbox{};
    std::unique_ptr<pet::face::LvglFacePresenter> _face_presenter;
    const pet::preset::FacePresetDefinition* _preset = nullptr;
    pet::preset::FacePresetEvaluation _preset_evaluation{};
    pet::preset::FaceAnimationRuntime _animation_runtime{};
    pet::behaviour::PetFaceBehaviourController _behaviour{};
    std::atomic_bool _screen_tapped{false};
    pet::face::FaceDrawing _preset_drawing{};
    struct FaceDiagnostics {
        uint32_t started_ms       = 0;
        uint32_t frames           = 0;
        uint32_t publishes        = 0;
        uint32_t dropped_publishes = 0;
        uint32_t builds           = 0;
        uint32_t tick_ms          = 0;
        uint32_t max_tick_ms      = 0;
        uint32_t failures         = 0;
        uint32_t evaluate_us      = 0;
        uint32_t advance_us       = 0;
        uint32_t build_us         = 0;
        uint32_t publish_us       = 0;
        uint32_t max_evaluate_us  = 0;
        uint32_t max_advance_us   = 0;
        uint32_t max_build_us     = 0;
        uint32_t max_publish_us   = 0;
    };
    FaceDiagnostics _face_diagnostics{};
    uint32_t _last_stackchan_update_ms = 0;
    uint32_t _last_face_update_ms      = 0;
    uint32_t _last_face_error_ms       = 0;
    bool _pose_dirty                   = false;
};
