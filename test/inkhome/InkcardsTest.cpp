#include <gtest/gtest.h>

#include <string>

#include "apps/inkcards/core/BuiltInDecks.h"
#include "apps/inkcards/core/DeckReader.h"
#include "apps/inkcards/core/ReviewState.h"
#include "apps/inkcards/core/Session.h"

using namespace inkcards;

namespace {
class StringWriter final : public ByteWriter {
 public:
  size_t write(const void* src, size_t n) override {
    out.append(static_cast<const char*>(src), n);
    return n;
  }
  std::string out;
};
}  // namespace

TEST(Inkcards, BuiltInDecksOpen) {
  MemoryReader demo(BUILT_IN_DEMO, BUILT_IN_DEMO_SIZE);
  DeckReader d(demo);
  ASSERT_TRUE(d.open());
  EXPECT_GT(d.cardCount(), 0u);
  MemoryReader caps(BUILT_IN_CAPITALS, BUILT_IN_CAPITALS_SIZE);
  DeckReader c(caps);
  ASSERT_TRUE(c.open());
  DeckCard card;
  ASSERT_TRUE(c.readCard(0, card));
  EXPECT_FALSE(card.front.empty());
  EXPECT_FALSE(card.back.empty());
}

TEST(Inkcards, SessionGradesAndStateRoundTrips) {
  MemoryReader caps(BUILT_IN_CAPITALS, BUILT_IN_CAPITALS_SIZE);
  DeckReader deck(caps);
  ASSERT_TRUE(deck.open());
  ReviewStore store;
  Session session(deck, store, SessionConfig{});
  ASSERT_TRUE(session.build(20000));
  ASSERT_TRUE(session.hasCurrent());
  const auto before = session.stats().remaining;
  session.grade(GRADE_GOOD, 20000);
  EXPECT_EQ(session.stats().remaining, before - 1);

  StringWriter w;
  ASSERT_TRUE(store.save(w));
  MemoryReader r(reinterpret_cast<const uint8_t*>(w.out.data()), static_cast<uint32_t>(w.out.size()));
  ReviewStore loaded;
  ASSERT_TRUE(loaded.load(r));
  // The graded card is not due again the same day.
  Session again(deck, loaded, SessionConfig{});
  ASSERT_TRUE(again.build(20000));
  EXPECT_EQ(again.stats().remaining, before - 1);
}
