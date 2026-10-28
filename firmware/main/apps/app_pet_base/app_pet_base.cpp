/*
 * SPDX-License-Identifier: MIT
 */
#include "app_pet_base.h"

#include <hal/pet/pet_hardware.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
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
constexpr uint32_t kFaceDiagnosticsIntervalMs = 5000;
constexpr uint32_t kPetBlue                   = 0x12304A;

void accumulateMicros(uint32_t elapsed_us, uint32_t& total_us, uint32_t& maximum_us)
{
    total_us += elapsed_us;
    if (elapsed_us > maximum_us) maximum_us = elapsed_us;
}

void logFaceMemory(std::string_view label)
{
    mclog::tagInfo(kTag,
                   "face memory {}: internal free={} largest={} min={} psram free={} largest={} min={}", label,
                   static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                   static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                   static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                   static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                   static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),
                   static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}
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
    _pose_dirty                    = false;
    _face_diagnostics.started_ms   = now;
    logFaceMemory("open");

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
        const uint32_t face_tick_ms = now - _last_face_update_ms;
        const float delta_seconds   = static_cast<float>(face_tick_ms) / 1000.0f;
        _last_face_update_ms        = now;
        // A newly selected pose must render even if the spring barely moves on the first
        // frame, so the explicit dirty flag is OR-ed with the engine's movement report.
        const bool pose_dirty = _pose_dirty;
        _pose_dirty           = false;
        bool changed          = false;
        ++_face_diagnostics.frames;
        _face_diagnostics.tick_ms += face_tick_ms;
        if (face_tick_ms > _face_diagnostics.max_tick_ms) _face_diagnostics.max_tick_ms = face_tick_ms;

        int64_t started_us = esp_timer_get_time();
        const bool evaluated = pet::preset::evaluateFacePreset(*_preset, 0, _preset_evaluation);
        accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.evaluate_us,
                         _face_diagnostics.max_evaluate_us);

        started_us = esp_timer_get_time();
        const bool advanced = evaluated && pet::preset::advanceFaceAnimation(
            *_preset, delta_seconds, _preset_evaluation, _animation_runtime, &changed);
        accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.advance_us,
                         _face_diagnostics.max_advance_us);

        started_us = esp_timer_get_time();
        const bool built = evaluated && pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing);
        accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.build_us,
                         _face_diagnostics.max_build_us);

        if (built && (pose_dirty || changed)) {
            started_us = esp_timer_get_time();
            PetHardware::DisplayLock lock;
            accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.lock_us,
                             _face_diagnostics.max_lock_us);
            started_us = esp_timer_get_time();
            _face_renderer->render(_preset_drawing);
            accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.render_us,
                             _face_diagnostics.max_render_us);
            ++_face_diagnostics.renders;
        }
        if (!evaluated || !advanced || !built) {
            ++_face_diagnostics.failures;
            // A degraded advance still commits the targets it could move, so keep the
            // next frame dirty to render that progress. Rate-limit the log when the
            // failure is persistent (for example, a hand-built preset that never validates).
            _pose_dirty = true;
            if (_last_face_error_ms == 0 || now - _last_face_error_ms >= kFaceErrorLogIntervalMs) {
                mclog::tagError(kTag, "face pose evaluation failed");
                _last_face_error_ms = now == 0 ? 1 : now;
            }
        }
        const uint32_t diagnostic_elapsed_ms = now - _face_diagnostics.started_ms;
        if (diagnostic_elapsed_ms >= kFaceDiagnosticsIntervalMs) {
            const uint32_t frames  = _face_diagnostics.frames;
            const uint32_t renders = _face_diagnostics.renders;
            mclog::tagInfo(
                kTag,
                "face perf {}ms: frames={} ({}/s) tick ms avg/max={}/{} renders={} failures={} eval us avg/max={}/{} advance={}/{} build={}/{} lock={}/{} render={}/{}",
                static_cast<unsigned>(diagnostic_elapsed_ms), static_cast<unsigned>(frames),
                static_cast<unsigned>(frames * 1000u / diagnostic_elapsed_ms),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.tick_ms / frames),
                static_cast<unsigned>(_face_diagnostics.max_tick_ms), static_cast<unsigned>(renders),
                static_cast<unsigned>(_face_diagnostics.failures),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.evaluate_us / frames),
                static_cast<unsigned>(_face_diagnostics.max_evaluate_us),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.advance_us / frames),
                static_cast<unsigned>(_face_diagnostics.max_advance_us),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.build_us / frames),
                static_cast<unsigned>(_face_diagnostics.max_build_us),
                static_cast<unsigned>(renders == 0 ? 0 : _face_diagnostics.lock_us / renders),
                static_cast<unsigned>(_face_diagnostics.max_lock_us),
                static_cast<unsigned>(renders == 0 ? 0 : _face_diagnostics.render_us / renders),
                static_cast<unsigned>(_face_diagnostics.max_render_us));
            logFaceMemory("sample");
            _face_diagnostics            = {};
            _face_diagnostics.started_ms = now;
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
