# Uses arduinojson_src (declared in ../ai_settings). Reads the Phase 2 contract fixtures.
FetchContent_GetProperties(arduinojson_src)
add_executable(AiClientTest
  AiClientTest.cpp
  ${REPO_ROOT}/src/ai/AiProtocol.cpp
  ${REPO_ROOT}/src/ai/AiContext.cpp
)
target_include_directories(AiClientTest PRIVATE
  ${arduinojson_src_SOURCE_DIR}/src
  ${REPO_ROOT}/src
)
target_compile_definitions(AiClientTest PRIVATE CONTRACT_DIR="${REPO_ROOT}/cloud/ai-proxy/contract")
target_link_libraries(AiClientTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(AiClientTest)
