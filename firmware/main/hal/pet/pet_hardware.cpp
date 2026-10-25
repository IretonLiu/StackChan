/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_hardware.h"
#include "pet_hardware_policy.h"

#include <hal/hal.h>
#include <stackchan/stackchan.h>

namespace {

class StackChanSafeIdlePort final : public PetSafeIdlePort {
public:
    void setServoPowerEnabled(bool enabled) override
    {
        GetHAL().setServoPowerEnabled(enabled);
    }

    void setStatusLights(uint32_t rgb) override
    {
        GetStackChan().leftNeonLight().setColor(rgb);
        GetStackChan().rightNeonLight().setColor(rgb);
    }
};

}  // namespace

void PetHardware::initialize()
{
    GetHAL().init();
}

void PetHardware::delay(uint32_t milliseconds)
{
    GetHAL().delay(milliseconds);
}

uint32_t PetHardware::millis()
{
    return GetHAL().millis();
}

void PetHardware::feedWatchdog()
{
    GetHAL().feedTheDog();
}

void PetHardware::enterSafeIdle(uint32_t status_light_rgb)
{
    StackChanSafeIdlePort port;
    PetHardwarePolicy::establishSafeIdle(port, status_light_rgb);
}

void PetHardware::update()
{
    GetStackChan().update();
}

PetHardware::DisplayLock::DisplayLock()
{
    GetHAL().lvglLock();
}

PetHardware::DisplayLock::~DisplayLock()
{
    GetHAL().lvglUnlock();
}
