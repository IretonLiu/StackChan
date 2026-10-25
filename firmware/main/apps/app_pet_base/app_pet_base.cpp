/*
 * SPDX-License-Identifier: MIT
 */
#include "app_pet_base.h"

#include <hal/hal.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>
#include <stackchan/stackchan.h>
#include <string_view>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
constexpr std::string_view kTag = "PetBase";
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

    LvglLockGuard lock;

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setAlign(LV_ALIGN_CENTER);
    _panel->setBgColor(lv_color_hex(0x071018));
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);

    _title = std::make_unique<Label>(_panel->get());
    _title->setText("PET BASE");
    _title->setTextFont(&lv_font_montserrat_24);
    _title->setTextColor(lv_color_hex(0xD9F1FF));
    _title->align(LV_ALIGN_CENTER, 0, -14);

    _status = std::make_unique<Label>(_panel->get());
    _status->setText("custom firmware is running");
    _status->setTextFont(&lv_font_montserrat_16);
    _status->setTextColor(lv_color_hex(0x6FA7C6));
    _status->align(LV_ALIGN_CENTER, 0, 17);

    // This image is a safe, inert starting point: no autonomous servo motion
    // until the behaviour layer owns it.
    GetHAL().setServoPowerEnabled(false);
    GetStackChan().leftNeonLight().setColor(kPetBlue);
    GetStackChan().rightNeonLight().setColor(kPetBlue);
    _last_stackchan_update_ms = GetHAL().millis();
}

void AppPetBase::onRunning()
{
    const uint32_t now = GetHAL().millis();
    if (now - _last_stackchan_update_ms < kStackChanUpdateIntervalMs) {
        return;
    }
    _last_stackchan_update_ms = now;

    // Keeps hardware effects (including the LEDs) progressing without the
    // old XiaoZhi background task. Future pet behaviour runs above this call.
    GetStackChan().update();
}

void AppPetBase::onClose()
{
    mclog::tagInfo(kTag, "close custom pet base app");

    LvglLockGuard lock;
    _status.reset();
    _title.reset();
    _panel.reset();
}
