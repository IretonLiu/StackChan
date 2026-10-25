/*
 * SPDX-License-Identifier: MIT
 */
#include "settings.h"

#include <esp_log.h>

namespace {
constexpr const char* kTag = "PetSettings";
}

Settings::Settings(const std::string& name_space, bool read_write)
    : name_space_(name_space), read_write_(read_write)
{
    const esp_err_t err = nvs_open(name_space_.c_str(), read_write_ ? NVS_READWRITE : NVS_READONLY, &handle_);
    if (err != ESP_OK) {
        handle_ = 0;
        ESP_LOGW(kTag, "open namespace %s failed: %s", name_space_.c_str(), esp_err_to_name(err));
    }
}

Settings::~Settings()
{
    if (handle_ == 0) {
        return;
    }
    if (read_write_ && dirty_) {
        ESP_ERROR_CHECK(nvs_commit(handle_));
    }
    nvs_close(handle_);
}

std::string Settings::GetString(const std::string& key, const std::string& default_value)
{
    if (handle_ == 0) return default_value;
    size_t size = 0;
    if (nvs_get_str(handle_, key.c_str(), nullptr, &size) != ESP_OK) return default_value;
    std::string value(size, '\0');
    if (nvs_get_str(handle_, key.c_str(), value.data(), &size) != ESP_OK) return default_value;
    if (!value.empty()) value.pop_back();  // NVS includes the terminating null.
    return value;
}

void Settings::SetString(const std::string& key, const std::string& value)
{
    if (!read_write_ || handle_ == 0) return;
    ESP_ERROR_CHECK(nvs_set_str(handle_, key.c_str(), value.c_str()));
    dirty_ = true;
}

int32_t Settings::GetInt(const std::string& key, int32_t default_value)
{
    int32_t value = default_value;
    return handle_ != 0 && nvs_get_i32(handle_, key.c_str(), &value) == ESP_OK ? value : default_value;
}

void Settings::SetInt(const std::string& key, int32_t value)
{
    if (!read_write_ || handle_ == 0) return;
    ESP_ERROR_CHECK(nvs_set_i32(handle_, key.c_str(), value));
    dirty_ = true;
}

bool Settings::GetBool(const std::string& key, bool default_value)
{
    uint8_t value = 0;
    return handle_ != 0 && nvs_get_u8(handle_, key.c_str(), &value) == ESP_OK ? value != 0 : default_value;
}

void Settings::SetBool(const std::string& key, bool value)
{
    if (!read_write_ || handle_ == 0) return;
    ESP_ERROR_CHECK(nvs_set_u8(handle_, key.c_str(), value ? 1 : 0));
    dirty_ = true;
}

void Settings::EraseKey(const std::string& key)
{
    if (!read_write_ || handle_ == 0) return;
    const esp_err_t err = nvs_erase_key(handle_, key.c_str());
    if (err != ESP_ERR_NVS_NOT_FOUND) ESP_ERROR_CHECK(err);
    dirty_ = true;
}

void Settings::EraseAll()
{
    if (!read_write_ || handle_ == 0) return;
    ESP_ERROR_CHECK(nvs_erase_all(handle_));
    dirty_ = true;
}
