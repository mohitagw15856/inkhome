#include <gtest/gtest.h>

#include "apps/inkverse/core/BuiltInVerses.h"
#include "apps/inkverse/core/VerseDoc.h"

using namespace inkverse;

TEST(InkVerse, BuiltInCollectionsParse) {
  Collection poetry;
  ASSERT_TRUE(parseVerse(BUILT_IN_POETRY, poetry));
  EXPECT_EQ(poetry.title, "Classic English poetry");
  EXPECT_GE(poetry.verses.size(), 2u);
  EXPECT_EQ(poetry.verses[0].author, "William Wordsworth");
  Collection stoic;
  ASSERT_TRUE(parseVerse(BUILT_IN_STOIC, stoic));
  EXPECT_GE(stoic.verses.size(), 5u);
}

TEST(InkVerse, EverySleepRotationWalksAndWraps) {
  EXPECT_EQ(pickVerse(Rotation::EverySleep, 3, 0, 0, 0), 0u);
  EXPECT_EQ(pickVerse(Rotation::EverySleep, 3, 0, 4, 0), 1u);
  EXPECT_EQ(pickVerse(Rotation::Daily, 0, 5, 0, 0), 0u);
}

TEST(InkVerse, RejectsBadMagic) {
  Collection c;
  EXPECT_FALSE(parseVerse("VERSE 2\n%%\nT x\n", c));
}
