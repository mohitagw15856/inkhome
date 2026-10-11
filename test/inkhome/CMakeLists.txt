include(FetchContent)
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
)
FetchContent_MakeAvailable(ArduinoJson)

# Inkhome fork tests. Each fork module adds its sources here so upstream's
# test/CMakeLists.txt only carries one add_subdirectory line for the fork.
add_executable(InkhomeTests
  VersionTest.cpp
  IfFoundTextTest.cpp
  ${REPO_ROOT}/src/apps/iffound/core/IfFoundText.cpp
  RecipeTest.cpp
  ${REPO_ROOT}/src/apps/inkrecipes/core/Recipe.cpp
  ReadingStatsTest.cpp
  ${REPO_ROOT}/src/apps/stats/core/ReadingStatsData.cpp
)

target_include_directories(InkhomeTests PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(InkhomeTests PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  ArduinoJson
)

gtest_discover_tests(InkhomeTests)
