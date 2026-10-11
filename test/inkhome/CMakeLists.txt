# Inkhome fork tests. Each fork module adds its sources here so upstream's
# test/CMakeLists.txt only carries one add_subdirectory line for the fork.
add_executable(InkhomeTests
  VersionTest.cpp
  IfFoundTextTest.cpp
  ${REPO_ROOT}/src/apps/iffound/core/IfFoundText.cpp
)

target_include_directories(InkhomeTests PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(InkhomeTests PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(InkhomeTests)
