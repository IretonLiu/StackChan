/*
 * SPDX-License-Identifier: MIT
 */
#include "system_info.h"

#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>

namespace {
constexpr const char* kTag = "PetSystemInfo";
}

size_t SystemInfo::GetFlashSize()
{
    uint32_t size = 0;
    return esp_flash_get_size(nullptr, &size) == ESP_OK ? size : 0;
}

size_t SystemInfo::GetMinimumFreeHeapSize()
{
    return esp_get_minimum_free_heap_size();
}

size_t SystemInfo::GetFreeHeapSize()
{
    return esp_get_free_heap_size();
}

std::string SystemInfo::GetMacAddress()
{
    uint8_t mac[6] = {};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    char value[18] = {};
    snprintf(value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return value;
}

std::string SystemInfo::GetChipModelName()
{
    return CONFIG_IDF_TARGET;
}

std::string SystemInfo::GetUserAgent()
{
    return "stackchan-pet-base/0";
}

void SystemInfo::PrintHeapStats()
{
    ESP_LOGI(kTag, "free internal=%u minimum internal=%u", static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
}
