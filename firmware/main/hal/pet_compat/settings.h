/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <nvs.h>
#include <string>

// Small NVS facade owned by the pet firmware.  It intentionally preserves the
// existing `Settings` call shape while removing the XiaoZhi implementation.
class Settings {
public:
    Settings(const std::string& name_space, bool read_write = false);
    ~Settings();

    std::string GetString(const std::string& key, const std::string& default_value = "");
    void SetString(const std::string& key, const std::string& value);
    int32_t GetInt(const std::string& key, int32_t default_value = 0);
    void SetInt(const std::string& key, int32_t value);
    bool GetBool(const std::string& key, bool default_value = false);
    void SetBool(const std::string& key, bool value);
    void EraseKey(const std::string& key);
    void EraseAll();

private:
    std::string name_space_;
    nvs_handle_t handle_ = 0;
    bool read_write_ = false;
    bool dirty_ = false;
};
