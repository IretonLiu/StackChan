/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

/**
 * Minimal capability surface needed to establish Pet Base's safe posture.
 *
 * Implementations may adapt physical hardware or provide a host-test fake.
 * No motion command is exposed here: this policy can only remove servo power
 * and choose a symmetric status-light indication.
 */
class PetSafeIdlePort {
public:
    virtual ~PetSafeIdlePort() = default;

    virtual void setServoPowerEnabled(bool enabled) = 0;
    virtual void setStatusLights(uint32_t rgb)      = 0;
};

/**
 * Hardware-independent safety policy for the pre-behaviour Pet Base state.
 */
class PetHardwarePolicy {
public:
    PetHardwarePolicy() = delete;

    static void establishSafeIdle(PetSafeIdlePort& port, uint32_t status_light_rgb);
};
