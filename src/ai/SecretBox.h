#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Seals short secrets (such as an API key) for storage on the SD card with
// AES-256-GCM. The key is sha256("<label>:" || device secret), the same scheme
// as lib/Epub/BookKey.cpp: a copied card opens only on this device, and
// different labels never share a key.
namespace secretbox {

constexpr size_t MAX_PLAINTEXT = 512;

// Returns "sb1:" + base64(nonce(12) | ciphertext | tag(16)), or "" on failure
// (empty or too long plaintext, no device secret, crypto error).
std::string seal(const char* label, std::string_view plaintext);

// False when `sealed` is malformed, was sealed on another device or with
// another label, or was changed. `plaintext` is cleared on failure.
bool open(const char* label, std::string_view sealed, std::string& plaintext);

}  // namespace secretbox
