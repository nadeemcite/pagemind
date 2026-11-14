#include "SecretBox.h"

#include <DeviceSecret.h>
#include <Logging.h>
#include <esp_random.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/hash.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

namespace secretbox {

namespace {

constexpr char PREFIX[] = "sb1:";
constexpr size_t PREFIX_LEN = sizeof(PREFIX) - 1;
constexpr size_t NONCE_LEN = 12;
constexpr size_t TAG_LEN = 16;
constexpr size_t MAX_LABEL = 32;
constexpr size_t MAX_RAW = NONCE_LEN + MAX_PLAINTEXT + TAG_LEN;

constexpr char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void wipe(void* p, size_t n) {
  volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
  while (n--) *v++ = 0;
}

std::string base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve((len + 2) / 3 * 4);
  for (size_t i = 0; i < len; i += 3) {
    const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) | (i + 1 < len ? data[i + 1] << 8 : 0) |
                       (i + 2 < len ? data[i + 2] : 0);
    out += B64[(n >> 18) & 63];
    out += B64[(n >> 12) & 63];
    out += i + 1 < len ? B64[(n >> 6) & 63] : '=';
    out += i + 2 < len ? B64[n & 63] : '=';
  }
  return out;
}

int b64Value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Strict decoder: padded input only. Returns the decoded length, or 0 on error.
size_t base64Decode(std::string_view in, uint8_t* out, size_t outCap) {
  if (in.empty() || in.size() % 4 != 0) return 0;
  size_t n = 0;
  for (size_t i = 0; i < in.size(); i += 4) {
    const bool last = i + 4 == in.size();
    const int a = b64Value(in[i]), b = b64Value(in[i + 1]);
    const bool pad2 = last && in[i + 2] == '=' && in[i + 3] == '=';
    const bool pad1 = last && !pad2 && in[i + 3] == '=';
    const int c = pad2 ? 0 : b64Value(in[i + 2]);
    const int d = (pad1 || pad2) ? 0 : b64Value(in[i + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0) return 0;
    const uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
    const size_t bytes = pad2 ? 1 : pad1 ? 2 : 3;
    if (n + bytes > outCap) return 0;
    out[n++] = static_cast<uint8_t>(v >> 16);
    if (bytes > 1) out[n++] = static_cast<uint8_t>(v >> 8);
    if (bytes > 2) out[n++] = static_cast<uint8_t>(v);
  }
  return n;
}

bool deriveKey(const char* label, uint8_t (&key)[32]) {
  const size_t labelLen = label ? strnlen(label, MAX_LABEL + 1) : 0;
  if (labelLen == 0 || labelLen > MAX_LABEL) return false;
  uint8_t secret[32];
  if (!deviceSecret(secret)) return false;
  uint8_t input[MAX_LABEL + 1 + sizeof(secret)];
  memcpy(input, label, labelLen);
  input[labelLen] = ':';
  memcpy(input + labelLen + 1, secret, sizeof(secret));
  const bool ok = wc_Sha256Hash(input, static_cast<word32>(labelLen + 1 + sizeof(secret)), key) == 0;
  wipe(secret, sizeof(secret));
  wipe(input, sizeof(input));
  return ok;
}

// The label is authenticated too, so a value sealed for one purpose cannot be
// opened as another.
std::string additionalData(const char* label) { return std::string(PREFIX) + label; }

// wolfSSL's Aes holds the GCM tables (KBs): heap, not the task stack.
struct AesGcm {
  std::unique_ptr<Aes> aes{new (std::nothrow) Aes};
  bool ready = aes && wc_AesInit(aes.get(), nullptr, INVALID_DEVID) == 0;
  ~AesGcm() {
    if (ready) wc_AesFree(aes.get());
  }
};

}  // namespace

std::string seal(const char* label, std::string_view plaintext) {
  if (plaintext.empty() || plaintext.size() > MAX_PLAINTEXT) return "";
  uint8_t key[32];
  if (!deriveKey(label, key)) {
    LOG_ERR("SBOX", "No key for seal");
    return "";
  }
  uint8_t raw[MAX_RAW];
  uint8_t* nonce = raw;
  uint8_t* cipher = raw + NONCE_LEN;
  uint8_t* tag = cipher + plaintext.size();
  esp_fill_random(nonce, NONCE_LEN);
  const std::string aad = additionalData(label);

  AesGcm gcm;
  const bool ok = gcm.ready && wc_AesGcmSetKey(gcm.aes.get(), key, sizeof(key)) == 0 &&
                  wc_AesGcmEncrypt(gcm.aes.get(), cipher, reinterpret_cast<const uint8_t*>(plaintext.data()),
                                   static_cast<word32>(plaintext.size()), nonce, NONCE_LEN, tag, TAG_LEN,
                                   reinterpret_cast<const uint8_t*>(aad.data()), static_cast<word32>(aad.size())) == 0;
  wipe(key, sizeof(key));
  if (!ok) {
    LOG_ERR("SBOX", "Seal failed");
    return "";
  }
  return std::string(PREFIX) + base64Encode(raw, NONCE_LEN + plaintext.size() + TAG_LEN);
}

bool open(const char* label, std::string_view sealed, std::string& plaintext) {
  plaintext.clear();
  if (sealed.size() <= PREFIX_LEN || sealed.substr(0, PREFIX_LEN) != PREFIX) return false;
  uint8_t raw[MAX_RAW];
  const size_t rawLen = base64Decode(sealed.substr(PREFIX_LEN), raw, sizeof(raw));
  if (rawLen <= NONCE_LEN + TAG_LEN) return false;
  const size_t textLen = rawLen - NONCE_LEN - TAG_LEN;

  uint8_t key[32];
  if (!deriveKey(label, key)) return false;
  const std::string aad = additionalData(label);
  uint8_t text[MAX_PLAINTEXT];
  AesGcm gcm;
  const bool ok = gcm.ready && wc_AesGcmSetKey(gcm.aes.get(), key, sizeof(key)) == 0 &&
                  wc_AesGcmDecrypt(gcm.aes.get(), text, raw + NONCE_LEN, static_cast<word32>(textLen), raw,
                                   NONCE_LEN, raw + NONCE_LEN + textLen, TAG_LEN,
                                   reinterpret_cast<const uint8_t*>(aad.data()), static_cast<word32>(aad.size())) == 0;
  wipe(key, sizeof(key));
  if (!ok) {
    wipe(text, sizeof(text));
    return false;
  }
  plaintext.assign(reinterpret_cast<const char*>(text), textLen);
  wipe(text, sizeof(text));
  return true;
}

}  // namespace secretbox
