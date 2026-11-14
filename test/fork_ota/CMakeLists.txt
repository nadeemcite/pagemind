add_executable(ForkOtaTest
  ForkOtaTest.cpp
  ${REPO_ROOT}/lib/JsonParser/ReleaseJsonParser.cpp
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/src/StreamingJsonParser.cpp
)

target_include_directories(ForkOtaTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/JsonParser
  ${REPO_ROOT}/freeink-sdk/libs/network/JsonSax/include
)

# ForkIdentity.h builds display strings from these build-time macros.
target_compile_definitions(ForkOtaTest PRIVATE CROSSPOINT_VERSION="0.0.1" CROSSPOINT_UPSTREAM_VERSION="2.0.0")

target_link_libraries(ForkOtaTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ForkOtaTest)
