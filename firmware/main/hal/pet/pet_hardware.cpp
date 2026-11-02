/*
 * SPDX-License-Identifier: MIT
 */
#include "pet_hardware.h"
#include "pet_hardware_policy.h"

#include <hal/hal.h>
#include <stackchan/stackchan.h>

#include <atomic>

namespace {

std::atomic_uint8_t s_head_interactions{0};
std::atomic_bool s_shake{false};
bool s_input_bridge_installed = false;

constexpr uint8_t interactionMask(PetHeadInteraction interaction)
{
    return static_cast<uint8_t>(1u << static_cast<uint8_t>(interaction));
}

void installInputBridge()
{
    if (s_input_bridge_installed) {
        return;
    }
    s_input_bridge_installed = true;
    GetHAL().onHeadPetGesture.connect([](HeadPetGesture gesture) {
        PetHeadInteraction interaction = PetHeadInteraction::Press;
        switch (gesture) {
            case HeadPetGesture::Press:
                interaction = PetHeadInteraction::Press;
                break;
            case HeadPetGesture::SwipeForward:
                interaction = PetHeadInteraction::SwipeForward;
                break;
            case HeadPetGesture::SwipeBackward:
                interaction = PetHeadInteraction::SwipeBackward;
                break;
            case HeadPetGesture::Release:
                interaction = PetHeadInteraction::Release;
                break;
            case HeadPetGesture::None:
                return;
        }
        s_head_interactions.fetch_or(interactionMask(interaction), std::memory_order_release);
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

bool PetHardware::consumeHeadInteraction(PetHeadInteraction interaction)
{
    const uint8_t mask = interactionMask(interaction);
    const uint8_t previous = s_head_interactions.fetch_and(static_cast<uint8_t>(~mask), std::memory_order_acq_rel);
    return (previous & mask) != 0;
}

bool PetHardware::consumeShake()
{
    return s_shake.exchange(false, std::memory_order_acq_rel);
}

void PetHardware::setDisplayBrightness(uint8_t brightness)
{
    GetHAL().setBackLightBrightness(brightness, false);
}

uint8_t PetHardware::displayBrightness()
{
    return GetHAL().getBackLightBrightness();
}

PetHardware::DisplayLock::DisplayLock()
{
    GetHAL().lvglLock();
}

PetHardware::DisplayLock::~DisplayLock()
{
    GetHAL().lvglUnlock();
}
