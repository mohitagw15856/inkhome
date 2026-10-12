#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "apps/pocketwiki/core/PwaReader.h"
#include "apps/pocketwiki/core/WikiLayout.h"

using namespace pocketwiki;

namespace {

std::vector<uint8_t> sample() {
  std::ifstream in(REPO_ROOT_DIR "/scripts/inkhome/sim-sd/apps/pocketwiki/foundations-of-science.pwa",
                   std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// 10 px per byte, bold or not.
int fixedWidth(const void*, const char* text, Font, uint8_t) { return static_cast<int>(std::strlen(text)) * 10; }

LayoutMetrics metrics(const int width) {
  LayoutMetrics m;
  m.width = width;
  m.bodyLineHeight = 20;
  m.headingLineHeight = 30;
  m.indent = 30;
  m.measure = &fixedWidth;
  return m;
}

}  // namespace

TEST(PocketWiki, SampleArchiveOpensAndSearches) {
  const auto bytes = sample();
  ASSERT_FALSE(bytes.empty());
  inkkit::MemoryReader src(bytes);
  PwaReader reader(src);
  ASSERT_TRUE(reader.open());
  EXPECT_EQ(reader.header().collectionName, "Foundations of Science");
  EXPECT_EQ(reader.search("", 200).size(), reader.articleCount());
  const auto hits = reader.search("mar", 10);
  ASSERT_FALSE(hits.empty());
  for (const auto& h : hits) EXPECT_TRUE(h.title.rfind("Mar", 0) == 0) << h.title;
}

TEST(PocketWiki, EveryArticleLaysOutWithinWidth) {
  const auto bytes = sample();
  inkkit::MemoryReader src(bytes);
  PwaReader reader(src);
  ASSERT_TRUE(reader.open());
  bool sawLink = false;
  for (uint32_t id = 0; id < reader.articleCount(); ++id) {
    WikiLayout layout(metrics(400));
    ASSERT_TRUE(reader.readArticle(id, layout)) << id;
    ASSERT_FALSE(layout.lines().empty()) << id;
    for (const auto& line : layout.lines()) {
      for (const auto& s : line.segments) {
        // A single word longer than the line may overflow; nothing else does.
        if (s.text.find(' ') != std::string::npos) EXPECT_LE(s.x + s.width, 400) << s.text;
        if (s.link != kNoLink) {
          sawLink = true;
          EXPECT_LT(s.link, reader.articleCount());
        }
      }
    }
  }
  EXPECT_TRUE(sawLink);
}

TEST(PocketWiki, WrapsAndKeepsLinksSeparate) {
  WikiLayout layout(metrics(120));
  layout.onBlockStart(BlockInfo{});
  const char text[] = "aaa bbb ";
  layout.onText(text, sizeof(text) - 1, 0);
  layout.onLink(7, "ccc", 3);
  layout.onText(" ddd", 4, 0);
  layout.onEnd();
  ASSERT_EQ(layout.lines().size(), 2u);
  const auto& first = layout.lines()[0].segments;
  ASSERT_EQ(first.size(), 2u);
  EXPECT_EQ(first[0].text, "aaa bbb");
  EXPECT_EQ(first[1].link, 7u);
  EXPECT_EQ(first[1].x, 80);
  EXPECT_EQ(layout.lines()[1].segments[0].text, "ddd");
  EXPECT_EQ(layout.lines()[1].segments[0].x, 0);
}

TEST(PocketWiki, PaginatesByHeight) {
  WikiLayout layout(metrics(50));
  for (int i = 0; i < 10; ++i) {
    layout.onBlockStart(BlockInfo{});
    layout.onText("word", 4, 0);
  }
  layout.onEnd();
  const auto pages = layout.paginate(60);
  ASSERT_GT(pages.size(), 1u);
  EXPECT_EQ(pages[0], 0u);
}

TEST(PocketWiki, RejectsOversizedBlocks) {
  auto bytes = sample();
  bytes[36] = 0xFF;  // maxBlockRaw far beyond the cap
  bytes[37] = 0xFF;
  bytes[38] = 0x0F;
  inkkit::MemoryReader src(bytes);
  PwaReader reader(src);
  EXPECT_FALSE(reader.open());
}
