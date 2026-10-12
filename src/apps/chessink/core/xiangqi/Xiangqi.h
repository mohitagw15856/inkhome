// From ChessInk (github.com/mohitagw15856/chessink, MIT), unchanged apart from include paths.
// Pure, hardware-free Xiangqi (Chinese chess) move generation and validation.
//
// Full rules: palace confinement (general, advisors), elephant river limit
// and blocked eye, horse-leg blocking, cannon screens, soldier promotion at
// the river, and the flying-general rule. Pseudo-legal generation with a
// make/undo legality filter, validated against documented perft vectors in
// the native tests (see docs/XIANGQI_VECTORS.md).
//
// Board: 9 files (a-i, 0-8) x 10 ranks (0-9). Red plays "up" from ranks 0-4.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chessink {
namespace xiangqi {

enum : int8_t { EMPTY = 0, GEN = 1, ADV = 2, ELE = 3, HOR = 4, CHA = 5, CAN = 6, SOL = 7 };

constexpr int kFiles = 9;
constexpr int kRanks = 10;
constexpr int kSquares = kFiles * kRanks;

inline int sqOf(int f, int r) { return r * kFiles + f; }
inline int fileOf(int sq) { return sq % kFiles; }
inline int rankOf(int sq) { return sq / kFiles; }

struct Move {
  uint8_t from = 0;
  uint8_t to = 0;
  bool operator==(const Move& o) const { return from == o.from && to == o.to; }
};

struct Undo {
  int8_t captured = 0;
};

class Position {
 public:
  Position() { setStart(); }

  void setStart();
  // Xiangqi FEN (board + side). False on parse error.
  bool setFen(const std::string& fen);

  bool redToMove() const { return redToMove_; }
  int8_t at(int sq) const { return board_[sq]; }

  void legalMoves(std::vector<Move>& out) const;
  bool inCheck(bool red) const;

  void makeMove(const Move& m, Undo& u);
  void unmakeMove(const Move& m, const Undo& u);

  uint64_t perft(int depth);

  // ICCS-style coordinates ("h2e2"): files a-i from Red's left, ranks 0-9
  // from Red's back rank.
  bool parseMove(const std::string& text, Move& out) const;
  static std::string moveText(const Move& m);

 private:
  void pseudoMoves(std::vector<Move>& out) const;
  bool generalsFacing() const;
  bool attacked(int sq, bool byRed) const;
  int generalSquare(bool red) const;

  int8_t board_[kSquares] = {0};
  bool redToMove_ = true;
};

}  // namespace xiangqi
}  // namespace chessink
