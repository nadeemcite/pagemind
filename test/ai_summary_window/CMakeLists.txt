# Uses arduinojson_src (declared in ../ai_settings) through AiProtocol.cpp (truncateUtf8).
FetchContent_GetProperties(arduinojson_src)
add_executable(AiSummaryWindowTest
  AiSummaryWindowTest.cpp
  ${REPO_ROOT}/src/ai/SummaryWindow.cpp
  ${REPO_ROOT}/src/ai/AiProtocol.cpp
)
target_include_directories(AiSummaryWindowTest PRIVATE
  ${arduinojson_src_SOURCE_DIR}/src
  ${REPO_ROOT}/src
)
target_link_libraries(AiSummaryWindowTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(AiSummaryWindowTest)
