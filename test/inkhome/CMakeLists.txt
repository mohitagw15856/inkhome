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
  AchievementsTest.cpp
  ${REPO_ROOT}/src/apps/stats/core/Achievements.cpp
  VerseTest.cpp
  ${REPO_ROOT}/src/apps/inkverse/core/VerseDoc.cpp
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

# Inkcards core (its own byte streams, no Arduino).
target_sources(InkhomeTests PRIVATE
  InkcardsTest.cpp
  ${REPO_ROOT}/src/apps/inkcards/core/DeckReader.cpp
  ${REPO_ROOT}/src/apps/inkcards/core/ReviewState.cpp
  ${REPO_ROOT}/src/apps/inkcards/core/Session.cpp
  ${REPO_ROOT}/src/apps/inkcards/core/Sm2.cpp
)

# Habitink's HabitCore.
target_sources(InkhomeTests PRIVATE
  HabitsTest.cpp
  ${REPO_ROOT}/src/apps/habits/core/CompletionLog.cpp
  ${REPO_ROOT}/src/apps/habits/core/Date.cpp
  ${REPO_ROOT}/src/apps/habits/core/Habit.cpp
  ${REPO_ROOT}/src/apps/habits/core/HabitConfig.cpp
  ${REPO_ROOT}/src/apps/habits/core/IconSet.cpp
  ${REPO_ROOT}/src/apps/habits/core/Stats.cpp
)
target_include_directories(InkhomeTests PRIVATE ${REPO_ROOT}/src/apps/habits/core)

# ChessInk cores.
target_sources(InkhomeTests PRIVATE
  ChessTest.cpp
  ${REPO_ROOT}/src/apps/chessink/core/Puzzle.cpp
  ${REPO_ROOT}/src/apps/chessink/core/chess/Chess.cpp
  ${REPO_ROOT}/src/apps/chessink/core/xiangqi/Xiangqi.cpp
)

# StoryScapes pack parser.
target_sources(InkhomeTests PRIVATE
  StoryScapesTest.cpp
  ${REPO_ROOT}/src/apps/storyscapes/core/Ssp.cpp
)
target_compile_definitions(InkhomeTests PRIVATE REPO_ROOT_DIR="${REPO_ROOT}")
