/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <string>

class SystemInfo {
public:
    static size_t GetFlashSize();
    static size_t GetMinimumFreeHeapSize();
    static size_t GetFreeHeapSize();
    static std::string GetMacAddress();
    static std::string GetChipModelName();
    static std::string GetUserAgent();
    static void PrintHeapStats();
};
