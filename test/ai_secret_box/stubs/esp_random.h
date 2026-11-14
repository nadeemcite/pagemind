#pragma once
#include <cstddef>
#include <cstdint>
#include <random>
// Host stand-in for the ESP32 hardware RNG.
inline void esp_fill_random(void* buffer, size_t size) {
  static std::random_device rd;
  auto* p = static_cast<uint8_t*>(buffer);
  for (size_t i = 0; i < size; i++) p[i] = static_cast<uint8_t>(rd());
}
