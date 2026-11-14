#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "fork/ForkIdentity.h"
#include "lib/JsonParser/ReleaseJsonParser.h"

namespace {

// Minimal GitHub "latest release" JSON with the given tag and asset names.
std::string releaseJson(const std::string& tag, const std::vector<std::string>& assets) {
  std::string json = R"({"tag_name":")" + tag + R"(","prerelease":false,"assets":[)";
  for (size_t i = 0; i < assets.size(); i++) {
    if (i) json += ",";
    json += R"({"name":")" + assets[i] + R"(","size":5443264,"browser_download_url":"https://github.com/nadeemcite/ncrosspoint/releases/download/)" +
            tag + "/" + assets[i] + R"("})";
  }
  return json + "]}";
}

// Mirrors OtaUpdater::checkForUpdate(): build the asset name once the tag is known.
struct Check {
  bool tagFound = false;
  bool firmwareFound = false;
  std::string url;
};

Check runCheck(const std::string& json, const char* boardSuffix) {
  ReleaseJsonParser parser;
  parser.setFirmwareAssetName("");
  char assetName[48] = {};
  bool assetNameSet = false;
  for (char c : json) {
    parser.feed(&c, 1);
    if (!assetNameSet && parser.foundTag()) {
      EXPECT_TRUE(fork_identity::otaAssetName(assetName, sizeof(assetName), parser.getTagName(), boardSuffix));
      parser.setFirmwareAssetName(assetName);
      assetNameSet = true;
    }
  }
  return {parser.foundTag(), parser.foundFirmware(), parser.foundFirmware() ? parser.getFirmwareUrl() : ""};
}

}  // namespace

TEST(ForkOta, ReleasesUrlPointsAtFork) {
  EXPECT_STREQ(FORK_OTA_RELEASES_URL, "https://api.github.com/repos/nadeemcite/ncrosspoint/releases/latest");
  EXPECT_EQ(std::string(FORK_OTA_RELEASES_URL).find("crosspoint-reader/crosspoint-reader"), std::string::npos);
}

TEST(ForkOta, AssetName) {
  char out[48];
  ASSERT_TRUE(fork_identity::otaAssetName(out, sizeof(out), "0.1.0", "-x4pro"));
  EXPECT_STREQ(out, "ncrosspoint-0.1.0-x4pro.bin");
}

TEST(ForkOta, AssetNameTooLongFails) {
  char out[16];
  EXPECT_FALSE(fork_identity::otaAssetName(out, sizeof(out), "0.1.0", "-x4pro"));
}

TEST(ForkOta, DisplayVersion) { EXPECT_STREQ(FORK_DISPLAY_VERSION, "NCrossPoint 0.0.1 (CrossPoint 2.0.0)"); }

TEST(ForkOta, FindsForkAssetForThisBoard) {
  const auto json = releaseJson("0.0.2", {"ncrosspoint-0.0.2-sticky.bin", "ncrosspoint-0.0.2-x4pro.bin"});
  const Check c = runCheck(json, "-x4pro");
  EXPECT_TRUE(c.tagFound);
  ASSERT_TRUE(c.firmwareFound);
  EXPECT_EQ(c.url, "https://github.com/nadeemcite/ncrosspoint/releases/download/0.0.2/ncrosspoint-0.0.2-x4pro.bin");
}

TEST(ForkOta, IgnoresOfficialCrossPointAssets) {
  const auto json = releaseJson("2.1.0", {"crosspoint-2.1.0-x4pro.bin", "crosspoint-2.1.0-x3-x4.bin"});
  const Check c = runCheck(json, "-x4pro");
  EXPECT_TRUE(c.tagFound);
  EXPECT_FALSE(c.firmwareFound);
}

TEST(ForkOta, IgnoresOtherBoards) {
  const auto json = releaseJson("0.0.2", {"ncrosspoint-0.0.2-sticky.bin"});
  EXPECT_FALSE(runCheck(json, "-x4pro").firmwareFound);
}
