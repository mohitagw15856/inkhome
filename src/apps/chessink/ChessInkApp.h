#pragma once

// ChessInk: chess and xiangqi puzzles with legal-move highlighting, an
// Elo-style rating and a solve streak. Native port of ChessInk
// (github.com/mohitagw15856/chessink, MIT). Packs (.pzl) come from
// /apps/chessink/ and the legacy /chessink/puzzles/; a chess and a xiangqi
// sampler are built in. Stats live in /.crosspoint/apps/chessink/stats.txt.
// Heap: one parsed pack (capped at 300 puzzles, 64 KB file) while a pack is open.

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/UiListActivity.h"
#include "apps/AppRegistry.h"
#include "core/Puzzle.h"
#include "core/chess/Chess.h"
#include "core/xiangqi/Xiangqi.h"

namespace chessink {

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

constexpr inkhome::apps::AppDescriptor DESCRIPTOR{"chessink", StrId::STR_CHESS_TITLE, StrId::STR_CHESS_SUMMARY, &create,
                                                  nullptr};

struct PackEntry {
  std::string label;
  std::string source;  // "builtin:chess", "builtin:xiangqi" or an SD path
};

class PackListActivity final : public UiListActivity {
 public:
  explicit PackListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("ChessInk", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<PackEntry> entries;
  std::vector<freeink::ui::ListItem> rows;
};

class PuzzleActivity final : public Activity {
 public:
  PuzzleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, PackEntry entry)
      : Activity("ChessPuzzle", renderer, mappedInput), entry(std::move(entry)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Phase : uint8_t { Playing, Solved, Missed, LoadFailed };

  void startPuzzle(size_t index);
  void computeTargets();
  void moveCursor(int dx, int dy);
  void trySelectOrMove();
  void applyPlayerMove(const std::string& text);
  void recordResult(bool solved);
  void drawChess(int ox, int oy, int cell) const;
  void drawXiangqi(int ox, int oy, int cell) const;

  PackEntry entry;
  Pack pack;
  SolveStats solveStats;
  chess::Position chessPos;
  xiangqi::Position xqPos;
  size_t puzzleIndex = 0;
  size_t solutionPly = 0;
  int cursor = 0;
  int selected = -1;
  std::vector<uint8_t> targets;
  std::string expectedMove;
  Phase phase = Phase::Playing;
};

}  // namespace chessink
