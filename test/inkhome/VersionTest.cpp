#include <gtest/gtest.h>

#include "inkhome/InkhomeVersion.h"

using inkhome::isVersionNewer;

TEST(InkhomeVersion, ForkSegmentBumpIsNewer) {
  EXPECT_TRUE(isVersionNewer("1.6.5.2-inkhome", "1.6.5.1-inkhome"));
  EXPECT_FALSE(isVersionNewer("1.6.5.1-inkhome", "1.6.5.2-inkhome"));
}

TEST(InkhomeVersion, UpstreamBumpIsNewer) {
  EXPECT_TRUE(isVersionNewer("1.6.6.1-inkhome", "1.6.5.9-inkhome"));
  EXPECT_TRUE(isVersionNewer("1.7.0.1-inkhome", "1.6.5.1-inkhome"));
}

TEST(InkhomeVersion, EqualIsNotNewer) {
  EXPECT_FALSE(isVersionNewer("1.6.5.1-inkhome", "1.6.5.1-inkhome"));
}

TEST(InkhomeVersion, DevBuildSuffixIgnored) {
  EXPECT_FALSE(isVersionNewer("1.6.5.1-inkhome", "1.6.5.1-inkhome-dev-main-abc1234"));
  EXPECT_TRUE(isVersionNewer("1.6.5.2-inkhome", "1.6.5.1-inkhome-dev-main-abc1234"));
}

TEST(InkhomeVersion, ThreePartUpstreamTagParses) {
  // A plain upstream tag reads as fork segment 0, so it never beats a fork build of the same base.
  EXPECT_FALSE(isVersionNewer("1.6.5", "1.6.5.1-inkhome"));
}

TEST(InkhomeVersion, GarbageIsNeverNewer) {
  EXPECT_FALSE(isVersionNewer("latest", "1.6.5.1-inkhome"));
  EXPECT_FALSE(isVersionNewer("1.6.5.1", "nonsense"));
}
