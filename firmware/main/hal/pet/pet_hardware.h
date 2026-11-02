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
enum class PetHeadInteraction : uint8_t {
    Press,
    SwipeForward,
    SwipeBackward,
    Release,
};

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

    // Consume events from the retained board touch/IMU detectors. Pet apps do
    // not access inherited Hal signals directly. Individual flags are retained
    // so press, stroke direction, and release cannot be collapsed into one
    // lossy "petted" signal.
    static bool consumeHeadInteraction(PetHeadInteraction interaction);
    static bool consumeShake();

    // Temporary face-state brightness changes never persist user settings.
    static void setDisplayBrightness(uint8_t brightness);
    static uint8_t displayBrightness();

    class DisplayLock {
    public:
        DisplayLock();
        ~DisplayLock();

        DisplayLock(const DisplayLock&)            = delete;
        DisplayLock& operator=(const DisplayLock&) = delete;
    };
};
