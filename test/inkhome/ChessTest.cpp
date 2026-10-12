#include <gtest/gtest.h>

#include "apps/chessink/core/BuiltInPacks.h"
#include "apps/chessink/core/Puzzle.h"
#include "apps/chessink/core/chess/Chess.h"
#include "apps/chessink/core/xiangqi/Xiangqi.h"

using namespace chessink;

// Standard perft counts from the start position (as ChessInk's own suite).
TEST(ChessInk, ChessPerftFromStart) {
  chess::Position p;
  EXPECT_EQ(p.perft(1), 20u);
  EXPECT_EQ(p.perft(2), 400u);
  EXPECT_EQ(p.perft(3), 8902u);
}

TEST(ChessInk, XiangqiPerftFromStart) {
  xiangqi::Position p;
  EXPECT_EQ(p.perft(1), 44u);
  EXPECT_EQ(p.perft(2), 1920u);
}

TEST(ChessInk, BuiltInPacksParseAndFirstMovesAreLegal) {
  Pack chessPack;
  ASSERT_TRUE(parsePack(BUILT_IN_CHESS, chessPack));
  EXPECT_EQ(chessPack.game, Game::Chess);
  for (const auto& puzzle : chessPack.puzzles) {
    chess::Position pos;
    ASSERT_TRUE(pos.setFen(puzzle.fen));
    chess::Move m;
    EXPECT_TRUE(pos.parseUci(puzzle.solution.front(), m)) << puzzle.fen;
  }
  Pack xqPack;
  ASSERT_TRUE(parsePack(BUILT_IN_XIANGQI, xqPack));
  EXPECT_EQ(xqPack.game, Game::Xiangqi);
  for (const auto& puzzle : xqPack.puzzles) {
    xiangqi::Position pos;
    ASSERT_TRUE(pos.setFen(puzzle.fen));
    xiangqi::Move m;
    EXPECT_TRUE(pos.parseMove(puzzle.solution.front(), m)) << puzzle.fen;
  }
}

TEST(ChessInk, RatingMovesTheRightWay) {
  EXPECT_GT(updateRating(1200, 1200, true), 1200);
  EXPECT_LT(updateRating(1200, 1200, false), 1200);
}
