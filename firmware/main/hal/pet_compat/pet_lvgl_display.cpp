/*
 * SPDX-License-Identifier: MIT
 */
// Minimal LvglDisplay implementation for the custom pet firmware.
//
// StackChanAvatarDisplay supplies the actual LCD/LVGL implementation. The
// upstream LvglDisplay base also owns assistant status, network indicators,
// low-battery speech, and Application scheduling; none of those belong in the
// non-verbal Pet Base image.
#include <display/lvgl_display/lvgl_display.h>
#include <esp_log.h>

namespace {
constexpr const char* kTag = "PetLvglDisplay";
}

LvglDisplay::LvglDisplay() = default;
LvglDisplay::~LvglDisplay() = default;

void LvglDisplay::SetStatus(const char* status)
{
    (void)status;
}

void LvglDisplay::ShowNotification(const std::string& notification, int duration_ms)
{
    ShowNotification(notification.c_str(), duration_ms);
}

void LvglDisplay::ShowNotification(const char* notification, int duration_ms)
{
    (void)notification;
    (void)duration_ms;
}

void LvglDisplay::UpdateStatusBar(bool update_all)
{
    (void)update_all;
}

void LvglDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image)
{
    (void)image;
}

void LvglDisplay::SetPowerSaveMode(bool on)
{
    ESP_LOGI(kTag, "power-save display request ignored: %s", on ? "on" : "off");
}

bool LvglDisplay::SnapshotToJpeg(std::string& jpeg_data, int quality)
{
    (void)quality;
    jpeg_data.clear();
    return false;
}
