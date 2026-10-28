/*
 * SPDX-License-Identifier: MIT
 */
#include "app_pet_base.h"

#include <hal/pet/pet_hardware.h>
#include <pet/preset/face_preset_registry.h>
#include <pet/preset/face_preset_renderer.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>
#include <string_view>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
constexpr std::string_view kTag               = "ProjectKiwi";
constexpr uint32_t kStackChanUpdateIntervalMs = 20;
constexpr uint32_t kAlertDurationMs           = 1200;
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
    if (_preset == nullptr || !pet::preset::evaluateFacePreset(*_preset, 0, _preset_evaluation) ||
        !pet::preset::selectFacePose(*_preset, "idle", _preset_evaluation, _pose_runtime) ||
        !pet::preset::advanceFacePose(_pose_runtime, 0.0f, _preset_evaluation) ||
        !pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing)) {
        mclog::tagError(kTag, "compiled default face preset or idle pose is invalid");
        _preset = nullptr;
        return;
    }
    _face_renderer->render(_preset_drawing);

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

    if (_preset != nullptr && (petted || shaken)) {
        if (pet::preset::selectFacePose(*_preset, "alert", _preset_evaluation, _pose_runtime)) {
            _alert_active   = true;
            _alert_until_ms = now + kAlertDurationMs;
        } else {
            mclog::tagError(kTag, "compiled alert pose is invalid");
        }
    } else if (_preset != nullptr && _alert_active && static_cast<int32_t>(now - _alert_until_ms) >= 0) {
        // A missing idle pose cannot be recovered by retrying: clear the flag so the app
        // does not re-log every frame and stay latched in the alert pose forever.
        if (pet::preset::selectFacePose(*_preset, "idle", _preset_evaluation, _pose_runtime)) {
            _alert_active = false;
        } else {
            _alert_active = false;
            mclog::tagError(kTag, "compiled idle pose is invalid; staying on the previous pose");
        }
    }

    if (_preset != nullptr && now - _last_face_update_ms >= kStackChanUpdateIntervalMs) {
        const float delta_seconds = static_cast<float>(now - _last_face_update_ms) / 1000.0f;
        _last_face_update_ms      = now;
        if (!pet::preset::evaluateFacePreset(*_preset, now, _preset_evaluation) ||
            !pet::preset::advanceFacePose(_pose_runtime, delta_seconds, _preset_evaluation) ||
            !pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing)) {
            mclog::tagError(kTag, "face pose evaluation failed");
        } else {
            PetHardware::DisplayLock lock;
            _face_renderer->render(_preset_drawing);
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
