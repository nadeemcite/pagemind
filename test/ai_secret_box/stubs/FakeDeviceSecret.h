#pragma once
#include <cstdint>
// Test control over the stand-in deviceSecret().
namespace fake_device_secret {
extern uint8_t value[32];
extern bool available;
}  // namespace fake_device_secret
