/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_hardware.h"
#include "pet_hardware_policy.h"

#include <hal/hal.h>
#include <stackchan/stackchan.h>

#include <atomic>

namespace {

std::atomic_bool s_head_pet{false};
std::atomic_bool s_shake{false};
bool s_input_bridge_installed = false;

void installInputBridge()
{
    if (s_input_bridge_installed) {
        return;
    }
    s_input_bridge_installed = true;
    GetHAL().onHeadPetGesture.connect([](HeadPetGesture gesture) {
        // A normal touch is the immediate pet affordance. Swipes remain valid
        // too, but requiring one made the first visual response appear inert.
        if (gesture == HeadPetGesture::Press || gesture == HeadPetGesture::SwipeForward ||
            gesture == HeadPetGesture::SwipeBackward) {
            s_head_pet.store(true, std::memory_order_release);
        }
    });
    GetHAL().onImuMotionEvent.connect([](ImuMotionEvent event) {
        if (event == ImuMotionEvent::Shake) {
            s_shake.store(true, std::memory_order_release);
        }
    });
}

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
    installInputBridge();
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

bool PetHardware::consumeHeadPet()
{
    return s_head_pet.exchange(false, std::memory_order_acq_rel);
}

bool PetHardware::consumeShake()
{
    return s_shake.exchange(false, std::memory_order_acq_rel);
}

PetHardware::DisplayLock::DisplayLock()
{
    GetHAL().lvglLock();
}

PetHardware::DisplayLock::~DisplayLock()
{
    GetHAL().lvglUnlock();
}
