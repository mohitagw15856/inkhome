#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "apps/storyscapes/core/Ssp.h"

using namespace storyscapes;

// The demo pack shipped on the simulator card.
TEST(StoryScapes, DemoPackParses) {
  std::ifstream in(REPO_ROOT_DIR "/scripts/inkhome/sim-sd/apps/storyscapes/isle-of-serpents.ssp", std::ios::binary);
  ASSERT_TRUE(in.good());
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  inkkit::MemoryReader reader(bytes);
  PackInfo info;
  ASSERT_TRUE(parsePack(reader, info));
  EXPECT_FALSE(info.title.empty());
  ASSERT_FALSE(info.pages.empty());
  bool sawImage = false;
  for (const auto& page : info.pages) {
    EXPECT_LE(page.offset + page.length, bytes.size());
    if (page.type == PageType::Image1bpp) {
      ImageHeader h;
      ASSERT_TRUE(readImageHeader(reader, page, h));
      EXPECT_GT(h.width, 0);
      sawImage = true;
    }
  }
  EXPECT_TRUE(sawImage);
}

TEST(StoryScapes, ProgressRoundTrips) {
  Progress p;
  p.page = 17;
  EXPECT_EQ(Progress::parse(p.serialise()).page, 17u);
}
