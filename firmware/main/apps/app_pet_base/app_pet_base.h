/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mooncake.h>
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
    std::unique_ptr<uitk::lvgl_cpp::Label> _title;
    std::unique_ptr<uitk::lvgl_cpp::Label> _status;
    uint32_t _last_stackchan_update_ms = 0;
};
