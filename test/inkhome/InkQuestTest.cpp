#include <gtest/gtest.h>

#include "apps/inkquest/core/BuiltInStory.h"
#include "apps/inkquest/core/SaveGame.h"
#include "apps/inkquest/core/StoryEngine.h"

using namespace inkquest;

namespace {

int firstVisible(const StoryEngine& engine, const PassageView& passage) {
  for (size_t i = 0; i < passage.choices.size(); ++i) {
    if (engine.choiceVisible(passage.choices[i])) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

TEST(InkQuest, BuiltInStoryOpens) {
  inkkit::MemoryReader reader(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  StoryEngine engine;
  ASSERT_TRUE(engine.open(reader));
  EXPECT_FALSE(engine.meta().title.empty());
  EXPECT_GT(engine.passageCount(), 1);
  engine.reset();
  PassageView passage;
  ASSERT_TRUE(engine.enter(engine.currentPassage(), passage));
  EXPECT_FALSE(engine.visibleText(passage).empty());
  EXPECT_GE(firstVisible(engine, passage), 0);
}

TEST(InkQuest, ChoosingMovesOnAndAlwaysEnds) {
  inkkit::MemoryReader reader(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  StoryEngine engine;
  ASSERT_TRUE(engine.open(reader));
  engine.reset();
  PassageView passage;
  ASSERT_TRUE(engine.enter(engine.currentPassage(), passage));
  const uint16_t start = engine.currentPassage();
  bool moved = false;
  for (int step = 0; step < 200; ++step) {
    const int c = firstVisible(engine, passage);
    if (c < 0) break;
    if (!engine.choose(passage.choices[static_cast<size_t>(c)], passage)) break;
    moved = moved || engine.currentPassage() != start;
  }
  EXPECT_TRUE(moved);
}

TEST(InkQuest, SaveRoundTrips) {
  inkkit::MemoryReader reader(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  StoryEngine engine;
  ASSERT_TRUE(engine.open(reader));
  engine.reset();
  PassageView passage;
  ASSERT_TRUE(engine.enter(engine.currentPassage(), passage));
  const int c = firstVisible(engine, passage);
  ASSERT_GE(c, 0);
  engine.choose(passage.choices[static_cast<size_t>(c)], passage);
  const uint16_t at = engine.currentPassage();

  inkkit::MemoryWriter writer;
  ASSERT_GT(writeSave(engine, writer, 0, true), 0u);

  inkkit::MemoryReader again(BUILT_IN_STORY, BUILT_IN_STORY_SIZE);
  StoryEngine restored;
  ASSERT_TRUE(restored.open(again));
  inkkit::MemoryReader saved(writer.bytes());
  ASSERT_TRUE(readSave(restored, saved));
  EXPECT_EQ(restored.currentPassage(), at);
}
