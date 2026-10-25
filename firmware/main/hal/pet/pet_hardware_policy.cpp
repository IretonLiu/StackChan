/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_hardware_policy.h"

void PetHardwarePolicy::establishSafeIdle(PetSafeIdlePort& port, uint32_t status_light_rgb)
{
    // This policy intentionally has no path that can energize or command a
    // servo. Future behaviour must opt into a separate, explicit capability.
    port.setServoPowerEnabled(false);
    port.setStatusLights(status_light_rgb);
}
