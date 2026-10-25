/*
 * SPDX-License-Identifier: MIT
 */
#include <hal/pet/pet_hardware_policy.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

class FakeSafeIdlePort final : public PetSafeIdlePort {
public:
    void setServoPowerEnabled(bool enabled) override
    {
        servo_power_requests.push_back(enabled);
    }

    void setStatusLights(uint32_t rgb) override
    {
        status_light_requests.push_back(rgb);
    }

    std::vector<bool> servo_power_requests;
    std::vector<uint32_t> status_light_requests;
};

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "Pet hardware policy contract failed: " << message << '\n';
        std::exit(1);
    }
}

void testSafeIdleNeverRequestsServoPower()
{
    FakeSafeIdlePort port;
    PetHardwarePolicy::establishSafeIdle(port, 0x010203);

    expect(port.servo_power_requests.size() == 1, "safe idle issues one servo-power request");
    expect(!port.servo_power_requests.front(), "safe idle never energizes a servo");
}

void testSafeIdlePreservesCallerOwnedStatusIntent()
{
    FakeSafeIdlePort port;
    const uint32_t status_intent = 0xA1B2C3;
    PetHardwarePolicy::establishSafeIdle(port, status_intent);

    expect(port.status_light_requests.size() == 1, "safe idle issues one symmetric-light request");
    expect(port.status_light_requests.front() == status_intent,
           "safe idle passes the caller's status-light intent through unchanged");
}

void testSafeIdleRemainsSafeWhenReapplied()
{
    FakeSafeIdlePort port;
    PetHardwarePolicy::establishSafeIdle(port, 0x123456);
    PetHardwarePolicy::establishSafeIdle(port, 0x654321);

    expect(port.servo_power_requests.size() == 2, "reapplying safe idle remains explicit");
    for (bool requested_state : port.servo_power_requests) {
        expect(!requested_state, "reapplying safe idle never re-enables servo power");
    }
    expect(port.status_light_requests.size() == 2, "each safe-idle transition carries a status intent");
}

}  // namespace

int main()
{
    testSafeIdleNeverRequestsServoPower();
    testSafeIdlePreservesCallerOwnedStatusIntent();
    testSafeIdleRemainsSafeWhenReapplied();
    return 0;
}
