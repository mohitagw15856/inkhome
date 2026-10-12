#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <vector>

#include "apps/dailydrop/core/DropDoc.h"

using namespace dailydrop;

namespace {
struct Sink final : DropSink {
  std::vector<DropEvent> events;
  void onEvent(const DropEvent& e) override { events.push_back(e); }
};
}  // namespace

TEST(DailyDrop, SampleDigestParses) {
  std::ifstream in(REPO_ROOT_DIR "/scripts/inkhome/sim-sd/apps/dailydrop/2026-10-12.drop");
  ASSERT_TRUE(in.good());
  Sink sink;
  DropParser parser(sink);
  std::string line;
  while (std::getline(in, line)) ASSERT_TRUE(parser.feedLine(line));
  EXPECT_TRUE(parser.sawMagic());
  EXPECT_EQ(parser.sectionCount(), 4);
}

TEST(DailyDrop, RejectsBadMagicAndRecognisesNames) {
  Sink sink;
  DropParser parser(sink);
  EXPECT_FALSE(parser.feedLine("DROP 9"));
  EXPECT_TRUE(isDropFilename("2026-10-12.drop"));
  EXPECT_FALSE(isDropFilename("notes.txt"));
}
