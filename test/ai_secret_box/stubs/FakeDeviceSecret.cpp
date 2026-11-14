#include "FakeDeviceSecret.h"

#include <DeviceSecret.h>

#include <cstring>

namespace fake_device_secret {
uint8_t value[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
bool available = true;
}  // namespace fake_device_secret

bool deviceSecret(uint8_t (&out)[32]) {
  if (!fake_device_secret::available) return false;
  memcpy(out, fake_device_secret::value, sizeof(out));
  return true;
}
