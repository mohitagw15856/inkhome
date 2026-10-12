// From ChessInk (github.com/mohitagw15856/chessink, MIT), unchanged apart from include paths.
// Pure, hardware-free chess move generation and validation.
//
// Correctness over speed: pseudo-legal generation followed by a make/undo
// legality filter (own king may not remain attacked). Castling, en passant
// (including the pinned-ep edge case, caught by the filter), promotions and
// double pushes are fully handled. Validated against standard perft vectors
// in the native test suite.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chessink {
namespace chess {

// Piece codes: positive white, negative black, 0 empty.
enum : int8_t { EMPTY = 0, WP = 1, WN = 2, WB = 3, WR = 4, WQ = 5, WK = 6 };

struct Move {
  uint8_t from = 0;
  uint8_t to = 0;
  int8_t promo = 0;  // WN..WQ (sign applied by side) or 0
  bool isEnPassant = false;
  bool isCastle = false;

  bool operator==(const Move& o) const { return from == o.from && to == o.to && promo == o.promo; }
};

struct Undo {
  int8_t captured = 0;
  uint8_t castling = 0;
  int8_t epFile = -1;
  uint8_t halfmove = 0;
};

class Position {
 public:
  Position() { setStart(); }

  void setStart();
  // FEN board+side+castling+ep fields; move counters optional. False on parse error.
  bool setFen(const std::string& fen);
  std::string fen() const;

  bool whiteToMove() const { return whiteToMove_; }
  int8_t at(int sq) const { return board_[sq]; }

  // All strictly legal moves for the side to move.
  void legalMoves(std::vector<Move>& out) const;

  // True if `sq` is attacked by the given side.
  bool attacked(int sq, bool byWhite) const;

  bool inCheck(bool white) const;

  void makeMove(const Move& m, Undo& u);
  void unmakeMove(const Move& m, const Undo& u);

  // Perft node count (legal-move tree leaves at `depth`).
  uint64_t perft(int depth);

  // Long algebraic ("e2e4", "e7e8q") to a legal move. False if not legal.
  bool parseUci(const std::string& uci, Move& out) const;
  static std::string uci(const Move& m);

 private:
  void pseudoMoves(std::vector<Move>& out) const;
  void addPawnMoves(int sq, std::vector<Move>& out) const;
  int kingSquare(bool white) const;

  int8_t board_[64] = {0};
  bool whiteToMove_ = true;
  uint8_t castling_ = 0;  // 1 WK, 2 WQ, 4 BK, 8 BQ
  int8_t epFile_ = -1;    // file of a double push last move, else -1
  uint8_t halfmove_ = 0;
};

}  // namespace chess
}  // namespace chessink
