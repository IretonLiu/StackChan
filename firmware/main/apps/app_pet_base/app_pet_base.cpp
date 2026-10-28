/*
 * SPDX-License-Identifier: MIT
 */
#include "app_pet_base.h"

#include <hal/pet/pet_hardware.h>
#include <pet/preset/face_preset_registry.h>
#include <pet/preset/face_preset_renderer.h>
#include <pet_generated/face_presets.generated.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>
#include <string_view>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
constexpr std::string_view kTag               = "ProjectKiwi";
constexpr uint32_t kStackChanUpdateIntervalMs = 20;
constexpr uint32_t kFaceErrorLogIntervalMs    = 1000;
constexpr uint32_t kPetBlue                   = 0x12304A;
}  // namespace

AppPetBase::AppPetBase()
{
    setAppInfo().name = "Project Kiwi";
}

void AppPetBase::onCreate()
{
    mclog::tagInfo(kTag, "create Project Kiwi app");
}

void AppPetBase::onOpen()
{
    mclog::tagInfo(kTag, "open Project Kiwi app");

    PetHardware::DisplayLock lock;

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setAlign(LV_ALIGN_CENTER);
    _panel->setBgColor(lv_color_hex(0x071018));
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);

    _face_renderer     = std::make_unique<pet::face::LvglFaceRenderer>(_panel->get());
    _preset            = pet::preset::defaultFacePreset();
    const uint32_t now = PetHardware::millis();

    if (_preset == nullptr) {
        mclog::tagError(kTag, "no compiled face preset is registered");
        return;
    }

    pet::preset::FacePresetEvaluationError evaluation_error = pet::preset::FacePresetEvaluationError::None;
    if (!pet::preset::evaluateFacePreset(*_preset, 0, _preset_evaluation, &evaluation_error)) {
        mclog::tagError(kTag, "compiled face preset is invalid (error {})", static_cast<int>(evaluation_error));
        _preset = nullptr;
        return;
    }
    if (!pet::preset::initializeFaceAnimation(*_preset, _preset_evaluation, _animation_runtime) ||
        !pet::preset::advanceFaceAnimation(*_preset, 0.0f, _preset_evaluation, _animation_runtime) ||
        !pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing)) {
        mclog::tagError(kTag, "compiled face animation cannot be initialized or rendered");
        _preset = nullptr;
        return;
    }
    _face_renderer->render(_preset_drawing);
    _pose_dirty = false;

    // This image is a safe, inert starting point: no autonomous servo motion
    // until the behaviour layer owns it.
    PetHardware::enterSafeIdle(kPetBlue);
    _last_stackchan_update_ms = now;
    _last_face_update_ms      = now;
}

void AppPetBase::onRunning()
{
    const uint32_t now = PetHardware::millis();
    const bool petted  = PetHardware::consumeHeadPet();
    const bool shaken  = PetHardware::consumeShake();

    if (_preset != nullptr && petted &&
        !pet::preset::dispatchFaceAnimation(*_preset, pet::preset::FaceAnimationEvent::Pet,
                                             _preset_evaluation, _animation_runtime)) {
        mclog::tagError(kTag, "compiled face animation rejected pet event");
    }
    if (_preset != nullptr && shaken &&
        !pet::preset::dispatchFaceAnimation(*_preset, pet::preset::FaceAnimationEvent::Shake,
                                             _preset_evaluation, _animation_runtime)) {
        mclog::tagError(kTag, "compiled face animation rejected shake event");
    }
    if (_preset != nullptr && (petted || shaken)) _pose_dirty = true;

    if (_preset != nullptr && now - _last_face_update_ms >= kStackChanUpdateIntervalMs) {
        const float delta_seconds = static_cast<float>(now - _last_face_update_ms) / 1000.0f;
        _last_face_update_ms      = now;
        // A newly selected pose must render even if the spring barely moves on the first
        // frame, so the explicit dirty flag is OR-ed with the engine's movement report.
        const bool pose_dirty = _pose_dirty;
        _pose_dirty           = false;
        bool changed          = false;
        const bool evaluated  = pet::preset::evaluateFacePreset(*_preset, 0, _preset_evaluation);
        const bool advanced = evaluated && pet::preset::advanceFaceAnimation(
            *_preset, delta_seconds, _preset_evaluation, _animation_runtime, &changed);
        const bool built = evaluated && pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing);
        if (built && (pose_dirty || changed)) {
            PetHardware::DisplayLock lock;
            _face_renderer->render(_preset_drawing);
        }
        if (!evaluated || !advanced || !built) {
            // A degraded advance still commits the targets it could move, so keep the
            // next frame dirty to render that progress. Rate-limit the log when the
            // failure is persistent (for example, a hand-built preset that never validates).
            _pose_dirty = true;
            if (_last_face_error_ms == 0 || now - _last_face_error_ms >= kFaceErrorLogIntervalMs) {
                mclog::tagError(kTag, "face pose evaluation failed");
                _last_face_error_ms = now == 0 ? 1 : now;
            }
        }
    }

    if (now - _last_stackchan_update_ms >= kStackChanUpdateIntervalMs) {
        _last_stackchan_update_ms = now;

        // Keeps hardware effects (including the LEDs) progressing without the
        // old XiaoZhi background task. Future pet behaviour runs above this call.
        PetHardware::update();
    }
}

void AppPetBase::onClose()
{
    mclog::tagInfo(kTag, "close Project Kiwi app");

    PetHardware::DisplayLock lock;
    _face_renderer.reset();
    _panel.reset();
}
