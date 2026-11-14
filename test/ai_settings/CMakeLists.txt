# ArduinoJson: same release as the firmware (platformio.ini lib_deps), header-only.
FetchContent_Declare(
  arduinojson_src
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(arduinojson_src)
if(NOT arduinojson_src_POPULATED)
  FetchContent_Populate(arduinojson_src)
endif()

# Uses wolfcrypt_host from ../ai_secret_box.
add_executable(AiSettingsTest
  AiSettingsTest.cpp
  ${REPO_ROOT}/src/ai/AiSettingsStore.cpp
  ${REPO_ROOT}/src/ai/SecretBox.cpp
  ${REPO_ROOT}/test/ai_secret_box/stubs/FakeDeviceSecret.cpp
)
target_include_directories(AiSettingsTest PRIVATE
  ${REPO_ROOT}/test/ai_secret_box/stubs
  ../library_builder/stubs
  ${arduinojson_src_SOURCE_DIR}/src
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/DeviceSecret
  ${REPO_ROOT}/lib/Serialization
)
target_link_libraries(AiSettingsTest PRIVATE crosspoint_test_common wolfcrypt_host GTest::gtest_main)
gtest_discover_tests(AiSettingsTest)
