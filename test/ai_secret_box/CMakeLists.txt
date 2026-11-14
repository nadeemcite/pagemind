# Real wolfCrypt (same release as the firmware's Arduino-wolfSSL 5.7.2), AES-GCM and SHA-256 only.
FetchContent_Declare(
  wolfssl_src
  GIT_REPOSITORY https://github.com/wolfSSL/wolfssl.git
  GIT_TAG v5.7.2-stable
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(wolfssl_src)
if(NOT wolfssl_src_POPULATED)
  FetchContent_Populate(wolfssl_src)
endif()

add_library(wolfcrypt_host STATIC
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/aes.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/sha256.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/hash.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/memory.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/wc_port.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/logging.c
  ${wolfssl_src_SOURCE_DIR}/wolfcrypt/src/error.c
)
target_include_directories(wolfcrypt_host PUBLIC ${wolfssl_src_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/wolfssl_config)
target_compile_definitions(wolfcrypt_host PUBLIC WOLFSSL_USER_SETTINGS)
target_compile_options(wolfcrypt_host PRIVATE -w)

add_executable(AiSecretBoxTest
  AiSecretBoxTest.cpp
  ${REPO_ROOT}/src/ai/SecretBox.cpp
  stubs/FakeDeviceSecret.cpp
)
target_include_directories(AiSecretBoxTest PRIVATE
  stubs
  ../library_builder/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/DeviceSecret
)
target_link_libraries(AiSecretBoxTest PRIVATE crosspoint_test_common wolfcrypt_host GTest::gtest_main)
gtest_discover_tests(AiSecretBoxTest)
