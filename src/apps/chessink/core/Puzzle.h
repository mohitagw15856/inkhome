// From ChessInk (github.com/mohitagw15856/chessink, MIT), unchanged apart from include paths.
// The .pzl puzzle-pack format (docs/FORMAT.md) and the solve-tracking maths.
// Pure logic, host-tested.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chessink {

enum class Game : uint8_t { Chess = 0, Xiangqi = 1 };

struct Puzzle {
  std::string fen;
  std::vector<std::string> solution;  // alternating, first move is the player's
  int rating = 1500;
  std::string themes;
};

struct Pack {
  std::string title;
  Game game = Game::Chess;
  std::vector<Puzzle> puzzles;
};

// Parses a whole .pzl document. False on bad magic or no puzzles.
bool parsePack(const std::string& text, Pack& out, size_t maxPuzzles = 1000);

// Elo-style rating estimate update after one puzzle attempt.
// K = 32; puzzleRating is the opponent. Returns the new player rating.
int updateRating(int playerRating, int puzzleRating, bool solved);

// Solve-streak counter (same shape as the ecosystem daily streaks, but per
// consecutive solved puzzle, reset on a miss).
struct SolveStats {
  int rating = 1200;
  uint32_t streak = 0;
  uint32_t solved = 0;
  uint32_t attempted = 0;

  void record(bool ok, int puzzleRating);
  std::string serialise() const;
  static SolveStats parse(const std::string& text);
};

}  // namespace chessink
