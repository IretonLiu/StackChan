/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_hardware.h"

#include <hal/hal.h>
#include <stackchan/stackchan.h>

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
    // Motion remains electrically disabled until a future pet behaviour
    // policy explicitly authorizes it.
    GetHAL().setServoPowerEnabled(false);
    GetStackChan().leftNeonLight().setColor(status_light_rgb);
    GetStackChan().rightNeonLight().setColor(status_light_rgb);
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
