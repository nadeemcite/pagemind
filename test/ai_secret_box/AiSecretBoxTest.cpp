#include <gtest/gtest.h>

#include <set>
#include <string>

#include "FakeDeviceSecret.h"
#include "ai/SecretBox.h"

namespace {

constexpr char LABEL[] = "ai-api-key";

class SecretBoxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (int i = 0; i < 32; i++) fake_device_secret::value[i] = static_cast<uint8_t>(i + 1);
    fake_device_secret::available = true;
  }
};

TEST_F(SecretBoxTest, RoundTrip) {
  const std::string key = "pm_3tkphlYexampleExampleExample0123456789";
  const std::string sealed = secretbox::seal(LABEL, key);
  ASSERT_FALSE(sealed.empty());
  EXPECT_EQ(sealed.rfind("sb1:", 0), 0u);
  EXPECT_EQ(sealed.find(key), std::string::npos) << "plaintext must not appear in the sealed form";
  std::string out;
  ASSERT_TRUE(secretbox::open(LABEL, sealed, out));
  EXPECT_EQ(out, key);
}

TEST_F(SecretBoxTest, RoundTripAllLengths) {
  for (size_t len : {1u, 2u, 3u, 15u, 16u, 17u, 64u, 511u, 512u}) {
    const std::string key(len, 'k');
    const std::string sealed = secretbox::seal(LABEL, key);
    ASSERT_FALSE(sealed.empty()) << len;
    std::string out;
    ASSERT_TRUE(secretbox::open(LABEL, sealed, out)) << len;
    EXPECT_EQ(out, key) << len;
  }
}

TEST_F(SecretBoxTest, RejectsEmptyAndTooLong) {
  EXPECT_TRUE(secretbox::seal(LABEL, "").empty());
  EXPECT_TRUE(secretbox::seal(LABEL, std::string(513, 'k')).empty());
}

TEST_F(SecretBoxTest, EveryFlippedByteFailsCleanly) {
  const std::string sealed = secretbox::seal(LABEL, "secret-api-key");
  for (size_t i = 4; i < sealed.size(); i++) {
    std::string bad = sealed;
    bad[i] = bad[i] == 'A' ? 'B' : 'A';
    std::string out = "untouched";
    EXPECT_FALSE(secretbox::open(LABEL, bad, out)) << "position " << i;
    EXPECT_TRUE(out.empty());
  }
}

TEST_F(SecretBoxTest, OtherDeviceFails) {
  const std::string sealed = secretbox::seal(LABEL, "secret-api-key");
  fake_device_secret::value[0] ^= 0xFF;  // a different reader
  std::string out;
  EXPECT_FALSE(secretbox::open(LABEL, sealed, out));
}

TEST_F(SecretBoxTest, OtherLabelFails) {
  const std::string sealed = secretbox::seal(LABEL, "secret-api-key");
  std::string out;
  EXPECT_FALSE(secretbox::open("wifi-password", sealed, out));
}

TEST_F(SecretBoxTest, NoDeviceSecretFails) {
  const std::string sealed = secretbox::seal(LABEL, "secret-api-key");
  fake_device_secret::available = false;
  EXPECT_TRUE(secretbox::seal(LABEL, "x").empty());
  std::string out;
  EXPECT_FALSE(secretbox::open(LABEL, sealed, out));
}

TEST_F(SecretBoxTest, MalformedInputFails) {
  const std::string sealed = secretbox::seal(LABEL, "secret-api-key");
  std::string out;
  for (const std::string& bad :
       {std::string(), std::string("sb1:"), std::string("sb2:") + sealed.substr(4), sealed.substr(0, sealed.size() - 4),
        sealed.substr(0, 20), std::string("sb1:!!!!"), std::string("sb1:AAA"), std::string("plaintext-key"),
        sealed + "AAAA", std::string("sb1:") + std::string(2000, 'A')}) {
    EXPECT_FALSE(secretbox::open(LABEL, bad, out)) << bad.substr(0, 40);
  }
}

TEST_F(SecretBoxTest, InvalidLabelsRejected) {
  EXPECT_TRUE(secretbox::seal("", "x").empty());
  EXPECT_TRUE(secretbox::seal(nullptr, "x").empty());
  EXPECT_TRUE(secretbox::seal(std::string(33, 'l').c_str(), "x").empty());
}

TEST_F(SecretBoxTest, NoncesNeverRepeat) {
  std::set<std::string> seen;
  for (int i = 0; i < 1000; i++) seen.insert(secretbox::seal(LABEL, "same-key"));
  EXPECT_EQ(seen.size(), 1000u);
}

}  // namespace
