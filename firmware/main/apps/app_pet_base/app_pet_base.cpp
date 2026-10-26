/*
 * SPDX-License-Identifier: MIT
 */
#include "app_pet_base.h"

#include <hal/pet/pet_hardware.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>
#include <string_view>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
constexpr std::string_view kTag = "PetBase";
constexpr uint32_t kFaceUpdateIntervalMs      = 33;
constexpr uint32_t kStackChanUpdateIntervalMs = 20;
constexpr uint32_t kPetBlue = 0x12304A;
}  // namespace

AppPetBase::AppPetBase()
{
    setAppInfo().name = "Pet Base";
}

void AppPetBase::onCreate()
{
    mclog::tagInfo(kTag, "create custom pet base app");
}

void AppPetBase::onOpen()
{
    mclog::tagInfo(kTag, "open custom pet base app");

    PetHardware::DisplayLock lock;

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setAlign(LV_ALIGN_CENTER);
    _panel->setBgColor(lv_color_hex(0x071018));
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);

    _face_renderer = std::make_unique<pet::face::LvglFaceRenderer>(_panel->get());
    const uint32_t now = PetHardware::millis();
    _face_controller.request(pet::face::ExpressionId::Idle, 1.0f, pet::face::Transition{220}, now);
    _face_renderer->render(_face_controller.sample(now));

    // This image is a safe, inert starting point: no autonomous servo motion
    // until the behaviour layer owns it.
    PetHardware::enterSafeIdle(kPetBlue);
    _last_face_update_ms       = now;
    _last_stackchan_update_ms = now;
}

void AppPetBase::onRunning()
{
    const uint32_t now = PetHardware::millis();
    if (now - _last_face_update_ms >= kFaceUpdateIntervalMs) {
        PetHardware::DisplayLock lock;
        _face_renderer->render(_face_controller.sample(now));
        _last_face_update_ms = now;
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
    mclog::tagInfo(kTag, "close custom pet base app");

    PetHardware::DisplayLock lock;
    _face_renderer.reset();
    _panel.reset();
}
