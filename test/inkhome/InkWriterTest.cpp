// Host tests for the InkWriter editor and session core, from InkWriter's own
// test/host/test_core.cpp.
#include <gtest/gtest.h>

#include <string>

#include "apps/inkwriter/core/Editor.h"
#include "apps/inkwriter/core/Session.h"
#include "apps/inkwriter/core/WordLog.h"
#include "apps/inkwriter/core/WrapRows.h"

using namespace inkwriter;

TEST(InkWriter, EditorTyping) {
  Editor e;
  for (char c : std::string("hello world")) e.insertChar(c);
  EXPECT_TRUE(e.text() == "hello world\n");
  EXPECT_TRUE(e.wordCount() == 2);
  e.insertNewline();
  for (char c : std::string("second line")) e.insertChar(c);
  EXPECT_TRUE(e.lines().size() == 2);
  EXPECT_TRUE(e.cursorLine() == 1);
  EXPECT_TRUE(e.wordCount() == 4);
}

TEST(InkWriter, BackspaceJoinsLines) {
  Editor e;
  e.setText("one\ntwo\n");
  EXPECT_TRUE(e.lines().size() == 2);
  // Cursor lands at end of doc; move to start of "two" then backspace.
  e.moveHome();
  e.backspace();
  EXPECT_TRUE(e.lines().size() == 1);
  EXPECT_TRUE(e.text() == "onetwo\n");
  EXPECT_TRUE(e.cursorCol() == 3);
}

TEST(InkWriter, CursorClamping) {
  Editor e;
  e.setText("long line here\nx\n");
  e.moveUp();
  e.moveEnd();
  EXPECT_TRUE(e.cursorCol() == 14);
  e.moveDown();
  EXPECT_TRUE(e.cursorCol() == 1);  // clamped to "x"
  e.moveLeft();
  e.moveLeft();  // wraps to end of previous line
  EXPECT_TRUE(e.cursorLine() == 0);
}

TEST(InkWriter, RoundTripAndParagraph) {
  Editor e;
  e.setText("para one line a\npara one line b\n\npara two\n");
  EXPECT_TRUE(e.text() == "para one line a\npara one line b\n\npara two\n");
  size_t f, l;
  e.currentParagraph(f, l);  // cursor at end -> "para two"
  EXPECT_TRUE(f == 3 && l == 3);
}

TEST(InkWriter, WordsAndGoal) {
  EXPECT_TRUE(countWords("") == 0);
  EXPECT_TRUE(countWords("  a  b ") == 2);
  EXPECT_TRUE(goalEighths(0, 500) == 0);
  EXPECT_TRUE(goalEighths(250, 500) == 4);
  EXPECT_TRUE(goalEighths(500, 500) == 8);
  EXPECT_TRUE(goalEighths(9000, 500) == 8);
  EXPECT_TRUE(goalEighths(100, 0) == 0);
}

TEST(InkWriter, Sprint) {
  Sprint s;
  EXPECT_TRUE(s.remainingSeconds(0) == 0);
  s.start(1000, 60000);
  EXPECT_TRUE(s.running());
  EXPECT_TRUE(s.remainingSeconds(1000) == 60);
  EXPECT_TRUE(s.remainingSeconds(31000) == 30);
  EXPECT_TRUE(s.remainingSeconds(61001) == 0);
  EXPECT_TRUE(!s.running());
}

TEST(InkWriter, Streak) {
  Streak s;
  EXPECT_TRUE(s.recordWriting(100) == 1);
  EXPECT_TRUE(s.recordWriting(100) == 1);  // same day, unchanged
  EXPECT_TRUE(s.recordWriting(101) == 2);  // consecutive day
  EXPECT_TRUE(s.recordWriting(105) == 1);  // gap resets
  Streak r = Streak::parse(s.serialise());
  EXPECT_TRUE(r.lastDay == 105 && r.length == 1);
}

namespace {

int tenPerByte(const void*, const char*, const size_t len) { return static_cast<int>(len) * 10; }

}  // namespace

TEST(InkWriter, WrapsAtSpacesAndFindsCursor) {
  const std::vector<std::string> lines = {"aaa bbb ccc", "", "dddddddddddd"};
  const auto rows = wrapRows(lines, 0, 2, 80, &tenPerByte, nullptr);
  ASSERT_EQ(rows.size(), 5u);  // "aaa bbb " | "ccc" | "" | "dddddddd" | "dddd"
  EXPECT_EQ(rows[0].end, 8u);
  EXPECT_EQ(rows[1].start, 8u);
  EXPECT_EQ(rows[2].line, 1u);
  EXPECT_EQ(rows[3].end - rows[3].start, 8u);
  EXPECT_EQ(cursorRow(rows, 0, 9), 1u);
  EXPECT_EQ(cursorRow(rows, 0, 11), 1u);  // end of line stays on its last row
  EXPECT_EQ(cursorRow(rows, 1, 0), 2u);
  EXPECT_EQ(cursorRow(rows, 2, 12), 4u);
}

TEST(InkWriter, WordLogAddsPerDayAndRoundTrips) {
  WordLog log;
  log.add(20000, 120);
  log.add(19999, 30);
  log.add(20000, 80);
  log.add(0, 50);  // no clock: ignored
  ASSERT_EQ(log.days().size(), 2u);
  EXPECT_EQ(log.days()[0].day, 19999u);
  EXPECT_EQ(log.days()[1].words, 200u);
  EXPECT_EQ(log.total(20000, 20000), 200u);
  EXPECT_EQ(log.total(0, 30000), 230u);
  const WordLog again = WordLog::parse(log.serialise() + "garbage\n");
  EXPECT_EQ(again.total(0, 30000), 230u);
}
