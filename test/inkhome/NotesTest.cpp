#include <gtest/gtest.h>

#include "apps/notes/core/NotesFormat.h"

using namespace inkhome::notes;

TEST(Notes, FileNamesAreFatSafe) {
  EXPECT_EQ(fileNameFor("Moby-Dick: or, The Whale"), "Moby-Dick -  or, The Whale.md");
  EXPECT_EQ(fileNameFor("A/B?"), "A - B.md");
  EXPECT_EQ(fileNameFor(""), "Untitled.md");
  EXPECT_EQ(fileNameFor("Title. "), "Title.md");
  const std::string longTitle(100, 'x');
  EXPECT_EQ(fileNameFor(longTitle).size(), 83u);
}

TEST(Notes, MarkdownListsMarks) {
  const std::string md =
      markdownFor("Frankenstein", "Mary Shelley", {{0.12f, "It was on a dreary night", ""}, {0.5f, "", "midpoint"}});
  EXPECT_NE(md.find("# Frankenstein\n*Mary Shelley*"), std::string::npos);
  EXPECT_NE(md.find("- **12%** \"It was on a dreary night\""), std::string::npos);
  EXPECT_NE(md.find("- **50%** (midpoint)"), std::string::npos);
}

TEST(Notes, HighlightEntryQuotesOnOneLine) {
  EXPECT_EQ(inkhome::notes::highlightEntry("It was the best\nof times", 12), "> It was the best of times\n\n*12%*\n\n");
  EXPECT_EQ(inkhome::notes::highlightsHeader("Emma"), "# Emma\n*Highlights*\n\n");
}
