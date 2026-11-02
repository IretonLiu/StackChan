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

std::string_view animationEventId(pet::behaviour::PetFaceAnimationCommand command)
{
    using Command = pet::behaviour::PetFaceAnimationCommand;
    switch (command) {
        case Command::ScreenTap:
            return "screen-tap";
        case Command::HeadStroke:
            return "pet-stroke";
        case Command::Shake:
            return "shake";
        case Command::SleepTimeout:
            return "sleep-timeout";
        case Command::Wake:
            return "wake";
        case Command::HappyComplete:
            return "happy-complete";
        case Command::WakeComplete:
            return "wake-complete";
        case Command::None:
            return {};
    }
    return {};
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
    lv_obj_add_event_cb(_panel->get(), &AppPetBase::onScreenTap, LV_EVENT_CLICKED, this);

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
    if (!_drawing_mailbox.publish(_preset_drawing)) {
        mclog::tagError(kTag, "initial face drawing could not be published");
        _preset = nullptr;
        return;
    }
    _face_presenter = std::make_unique<pet::face::LvglFacePresenter>(_panel->get(), _drawing_mailbox);
    _behaviour.initialize(now);
    _screen_tapped.store(false, std::memory_order_release);
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
    const uint32_t now        = PetHardware::millis();
    const uint8_t brightness  = PetHardware::displayBrightness();

    if (_screen_tapped.exchange(false, std::memory_order_acq_rel)) {
        applyBehaviourOutput(_behaviour.handle(pet::behaviour::PetFaceInput::ScreenTap, now, brightness));
    }
    if (PetHardware::consumeHeadInteraction(PetHeadInteraction::Press)) {
        applyBehaviourOutput(_behaviour.handle(pet::behaviour::PetFaceInput::HeadPress, now, brightness));
    }
    if (PetHardware::consumeHeadInteraction(PetHeadInteraction::SwipeForward) ||
        PetHardware::consumeHeadInteraction(PetHeadInteraction::SwipeBackward)) {
        applyBehaviourOutput(_behaviour.handle(pet::behaviour::PetFaceInput::HeadStroke, now, brightness));
    }
    if (PetHardware::consumeHeadInteraction(PetHeadInteraction::Release)) {
        applyBehaviourOutput(_behaviour.handle(pet::behaviour::PetFaceInput::HeadRelease, now, brightness));
    }
    if (PetHardware::consumeShake()) {
        applyBehaviourOutput(_behaviour.handle(pet::behaviour::PetFaceInput::Shake, now, brightness));
    }
    applyBehaviourOutput(_behaviour.advance(now, brightness));

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

        // The app task is the sole producer. It builds at simulation cadence,
        // then replaces any pending state in the fixed mailbox. LVGL consumes
        // only the newest completed drawing on its own task.
        bool built = true;
        if (evaluated && advanced && (pose_dirty || changed)) {
            started_us = esp_timer_get_time();
            built = pet::preset::buildFaceDrawing(_preset_evaluation, _preset_drawing);
            accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us), _face_diagnostics.build_us,
                             _face_diagnostics.max_build_us);
            ++_face_diagnostics.builds;

            if (built) {
                started_us = esp_timer_get_time();
                const bool published = _drawing_mailbox.publish(_preset_drawing);
                accumulateMicros(static_cast<uint32_t>(esp_timer_get_time() - started_us),
                                 _face_diagnostics.publish_us, _face_diagnostics.max_publish_us);
                if (published) {
                    ++_face_diagnostics.publishes;
                } else {
                    ++_face_diagnostics.dropped_publishes;
                    _pose_dirty = true;
                }
            }
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
            const uint32_t builds = _face_diagnostics.builds;
            const pet::face::LvglFacePresentationMetrics presentation_metrics =
                _face_presenter == nullptr ? pet::face::LvglFacePresentationMetrics{} : _face_presenter->takeMetrics();
            mclog::tagInfo(
                kTag,
                "face perf {}ms: simulation={} ({}/s) tick ms avg/max={}/{} published={}/{} dropped={} lvgl renders={} (target <={}/s) failures={} eval us avg/max={}/{} advance={}/{} build={}/{} publish={}/{} lvgl render={}/{} stack free min={}B",
                static_cast<unsigned>(diagnostic_elapsed_ms), static_cast<unsigned>(frames),
                static_cast<unsigned>(frames * 1000u / diagnostic_elapsed_ms),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.tick_ms / frames),
                static_cast<unsigned>(_face_diagnostics.max_tick_ms),
                static_cast<unsigned>(_face_diagnostics.publishes), static_cast<unsigned>(builds),
                static_cast<unsigned>(_face_diagnostics.dropped_publishes),
                static_cast<unsigned>(presentation_metrics.renders),
                static_cast<unsigned>(1000u / pet::face::LvglFacePresenter::kPresentationIntervalMs),
                static_cast<unsigned>(_face_diagnostics.failures),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.evaluate_us / frames),
                static_cast<unsigned>(_face_diagnostics.max_evaluate_us),
                static_cast<unsigned>(frames == 0 ? 0 : _face_diagnostics.advance_us / frames),
                static_cast<unsigned>(_face_diagnostics.max_advance_us),
                static_cast<unsigned>(builds == 0 ? 0 : _face_diagnostics.build_us / builds),
                static_cast<unsigned>(_face_diagnostics.max_build_us),
                static_cast<unsigned>(builds == 0 ? 0 : _face_diagnostics.publish_us / builds),
                static_cast<unsigned>(_face_diagnostics.max_publish_us),
                static_cast<unsigned>(presentation_metrics.renders == 0
                                          ? 0
                                          : presentation_metrics.render_us / presentation_metrics.renders),
                static_cast<unsigned>(presentation_metrics.max_render_us),
                static_cast<unsigned>(presentation_metrics.min_stack_free_bytes));
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

void AppPetBase::onScreenTap(lv_event_t* event)
{
    auto* app = static_cast<AppPetBase*>(lv_event_get_user_data(event));
    if (app != nullptr) {
        // LVGL owns this callback. Only hand the generic event to the app task;
        // never evaluate a pose or render from the input callback.
        app->_screen_tapped.store(true, std::memory_order_release);
    }
}

void AppPetBase::applyBehaviourOutput(const pet::behaviour::PetFaceBehaviourOutput& output)
{
    if (output.brightness_changed) {
        PetHardware::setDisplayBrightness(output.brightness);
    }
    const std::string_view event_id = animationEventId(output.animation);
    if (event_id.empty() || _preset == nullptr) return;
    const pet::preset::FaceAnimationEvent event = pet::preset::findFaceAnimationEvent(*_preset, event_id);
    if (event == pet::preset::kNoFaceAnimationEvent) {
        mclog::tagError(kTag, "compiled face preset does not define behaviour event {}", event_id);
        return;
    }
    if (!pet::preset::dispatchFaceAnimation(*_preset, event, _preset_evaluation, _animation_runtime)) {
        mclog::tagError(kTag, "compiled face animation rejected behaviour event {}", static_cast<unsigned>(event));
        return;
    }
    _pose_dirty = true;
}

void AppPetBase::onClose()
{
    mclog::tagInfo(kTag, "close Project Kiwi app");

    // Sleep dimming is a temporary behaviour override, never a user setting.
    // Restore it before removing the UI that owns the face behaviour.
    applyBehaviourOutput(_behaviour.shutdown());

    PetHardware::DisplayLock lock;
    _face_presenter.reset();
    _panel.reset();
}
