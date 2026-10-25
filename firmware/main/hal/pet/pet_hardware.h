/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

/**
 * Pet-owned gateway to physical hardware.
 *
 * Pet apps and future optional integrations must use this API rather than
 * accessing Hal, StackChan, or the inherited XiaoZhi Board singleton.  The
 * implementation is deliberately small while the existing board bring-up is
 * retained behind it.
 */
class PetHardware {
public:
    PetHardware() = delete;

    static void initialize();
    static void delay(uint32_t milliseconds);
    static uint32_t millis();
    static void feedWatchdog();

    // Establish the non-actuating Pet Base startup posture.
    static void enterSafeIdle(uint32_t status_light_rgb);

    // Advance pet-owned visual effects. This does not authorize servo motion.
    static void update();

    class DisplayLock {
    public:
        DisplayLock();
        ~DisplayLock();

        DisplayLock(const DisplayLock&)            = delete;
        DisplayLock& operator=(const DisplayLock&) = delete;
    };
};
