#include "ChessInkApp.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppPaths.h"
#include "components/UITheme.h"
#include "core/BuiltInPacks.h"
#include "core/XqGlyphs.h"
#include "fontIds.h"
#include "inkhome/SdState.h"

namespace fui = freeink::ui;

namespace chessink {

namespace {

constexpr char APP_ID[] = "chessink";
constexpr char LEGACY_DIR[] = "/chessink/puzzles";
constexpr char STATS_DIR[] = "/.crosspoint/apps/chessink";
constexpr char STATS_PATH[] = "/.crosspoint/apps/chessink/stats.txt";
constexpr char LEGACY_STATS_PATH[] = "/chessink/stats.txt";
constexpr char BUILTIN_CHESS[] = "builtin:chess";
constexpr char BUILTIN_XIANGQI[] = "builtin:xiangqi";
constexpr size_t MAX_PACKS = 40;
constexpr size_t MAX_PUZZLES = 300;
constexpr size_t MAX_FILE_BYTES = 64 * 1024;

bool loadPack(const std::string& source, Pack& out) {
  if (source == BUILTIN_CHESS) return parsePack(BUILT_IN_CHESS, out, MAX_PUZZLES);
  if (source == BUILTIN_XIANGQI) return parsePack(BUILT_IN_XIANGQI, out, MAX_PUZZLES);
  if (!inkhome::sdMounted()) return false;
  HalFile file;
  if (!Storage.openFileForRead("CHS", source.c_str(), file)) return false;
  const size_t size = file.fileSize();
  if (size == 0 || size > MAX_FILE_BYTES) return false;
  std::string text(size, '\0');
  if (file.read(text.data(), size) != static_cast<int>(size)) return false;
  return parsePack(text, out, MAX_PUZZLES);
}

void addFolder(const char* dir, std::vector<PackEntry>& out) {
  if (!Storage.exists(dir)) return;
  for (const auto& e : Storage.listFiles(dir, MAX_PACKS)) {
    if (out.size() >= MAX_PACKS) return;
    std::string name = e.c_str();
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() < 5 || name[0] == '.' || name.compare(name.size() - 4, 4, ".pzl") != 0) continue;
    out.push_back({name.substr(0, name.size() - 4), std::string(dir) + "/" + name});
  }
}

// Solid disc of radius r, black or white, drawn as horizontal spans.
void disc(const GfxRenderer& renderer, const int cx, const int cy, const int r, const bool black) {
  for (int dy = -r; dy <= r; ++dy) {
    int dx = 0;
    while ((dx + 1) * (dx + 1) + dy * dy <= r * r) ++dx;
    renderer.drawLine(cx - dx, cy + dy, cx + dx, cy + dy, black);
  }
}

void markers(const GfxRenderer& renderer, const int x, const int y, const int size, const bool isCursor,
             const bool isSelected, const bool isTarget, const bool occupied) {
  if (isCursor) renderer.drawRect(x + 1, y + 1, size - 2, size - 2, 3, true);
  if (isSelected) {
    const int t = size / 4;
    renderer.fillRect(x, y, t, 4, true);
    renderer.fillRect(x, y, 4, t, true);
    renderer.fillRect(x + size - t, y + size - 4, t, 4, true);
    renderer.fillRect(x + size - 4, y + size - t, 4, t, true);
  }
  if (isTarget) {
    // A dot on an empty square, a ring around a piece that can be taken.
    if (occupied) {
      renderer.drawRect(x + 4, y + 4, size - 8, size - 8, 2, true);
    } else {
      disc(renderer, x + size / 2, y + size / 2, std::max(3, size / 10), true);
    }
  }
}

}  // namespace

std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PackListActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- pack list

void PackListActivity::onEnter() {
  UiListActivity::onEnter();
  entries.clear();
  entries.reserve(8);
  entries.push_back({tr(STR_CHESS_BUILTIN_CHESS), BUILTIN_CHESS});
  entries.push_back({tr(STR_CHESS_BUILTIN_XIANGQI), BUILTIN_XIANGQI});
  if (inkhome::sdMounted()) {
    inkhome::apps::ensureDirs(APP_ID);
    char dir[48];
    inkhome::apps::contentDir(APP_ID, dir, sizeof(dir));
    addFolder(dir, entries);
    addFolder(LEGACY_DIR, entries);
  }
  rows.assign(entries.size(), fui::ListItem{});
  for (size_t i = 0; i < entries.size(); ++i) {
    rows[i].label = entries[i].label.c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
}

void PackListActivity::onExit() {
  rows = {};
  entries = {};
  UiListActivity::onExit();
}

int PackListActivity::listCount() const { return static_cast<int>(rows.size()); }
const char* PackListActivity::headerTitle() const { return tr(STR_CHESS_TITLE); }

void PackListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  auto puzzle = makeUniqueNoThrow<PuzzleActivity>(renderer, mappedInput, entries[static_cast<size_t>(index)]);
  if (!puzzle) return;
  app.clearTapFlash();
  activityManager.pushActivity(std::move(puzzle));
}

void PackListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

// ---------------------------------------------------------------- puzzle

void PuzzleActivity::onEnter() {
  Activity::onEnter();
  if (inkhome::sdMounted()) {
    const char* path = Storage.exists(STATS_PATH) ? STATS_PATH : LEGACY_STATS_PATH;
    if (Storage.exists(path)) solveStats = SolveStats::parse(Storage.readFile(path).c_str());
  }
  if (!loadPack(entry.source, pack) || pack.puzzles.empty()) {
    phase = Phase::LoadFailed;
  } else {
    startPuzzle(0);
  }
  requestUpdate();
}

void PuzzleActivity::onExit() {
  pack = Pack{};
  targets = {};
  Activity::onExit();
}

void PuzzleActivity::startPuzzle(const size_t index) {
  puzzleIndex = index % pack.puzzles.size();
  const Puzzle& p = pack.puzzles[puzzleIndex];
  solutionPly = 0;
  selected = -1;
  targets.clear();
  phase = Phase::Playing;
  if (pack.game == Game::Chess) {
    chessPos.setFen(p.fen);
    cursor = 28;  // e4, the middle of the board
  } else {
    xqPos.setFen(p.fen);
    cursor = xiangqi::sqOf(4, 4);
  }
}

void PuzzleActivity::computeTargets() {
  targets.clear();
  if (selected < 0) return;
  if (pack.game == Game::Chess) {
    std::vector<chess::Move> moves;
    chessPos.legalMoves(moves);
    for (const auto& m : moves) {
      if (m.from == selected) targets.push_back(m.to);
    }
  } else {
    std::vector<xiangqi::Move> moves;
    xqPos.legalMoves(moves);
    for (const auto& m : moves) {
      if (m.from == selected) targets.push_back(m.to);
    }
  }
}

void PuzzleActivity::moveCursor(const int dx, const int dy) {
  if (pack.game == Game::Chess) {
    const int f = ((cursor & 7) + dx + 8) % 8;
    const int r = ((cursor >> 3) + dy + 8) % 8;
    cursor = r * 8 + f;
  } else {
    const int f = (xiangqi::fileOf(cursor) + dx + xiangqi::kFiles) % xiangqi::kFiles;
    const int r = (xiangqi::rankOf(cursor) + dy + xiangqi::kRanks) % xiangqi::kRanks;
    cursor = xiangqi::sqOf(f, r);
  }
}

void PuzzleActivity::trySelectOrMove() {
  if (selected < 0) {
    selected = cursor;
    computeTargets();
    if (targets.empty()) selected = -1;  // nothing of ours to move here
    return;
  }
  if (cursor == selected) {
    selected = -1;
    targets.clear();
    return;
  }
  std::string text;
  bool legal = false;
  if (pack.game == Game::Chess) {
    chess::Move m;
    m.from = static_cast<uint8_t>(selected);
    m.to = static_cast<uint8_t>(cursor);
    text = chess::Position::uci(m);
    chess::Move probe;
    legal = chessPos.parseUci(text, probe);
    if (!legal && chessPos.parseUci(text + "q", probe)) {  // promotions default to a queen
      text += "q";
      legal = true;
    }
  } else {
    xiangqi::Move m;
    m.from = static_cast<uint8_t>(selected);
    m.to = static_cast<uint8_t>(cursor);
    text = xiangqi::Position::moveText(m);
    xiangqi::Move probe;
    legal = xqPos.parseMove(text, probe);
  }
  selected = -1;
  targets.clear();
  if (legal) applyPlayerMove(text);
}

void PuzzleActivity::applyPlayerMove(const std::string& text) {
  const Puzzle& p = pack.puzzles[puzzleIndex];
  if (text != p.solution[solutionPly]) {
    expectedMove = p.solution[solutionPly];
    recordResult(false);
    return;
  }
  // Play the move and, if the line continues, the opponent's reply.
  for (int i = 0; i < 2 && solutionPly < p.solution.size(); ++i) {
    const std::string& move = p.solution[solutionPly];
    if (pack.game == Game::Chess) {
      chess::Move m;
      if (chessPos.parseUci(move, m)) {
        chess::Undo u;
        chessPos.makeMove(m, u);
      }
    } else {
      xiangqi::Move m;
      if (xqPos.parseMove(move, m)) {
        xiangqi::Undo u;
        xqPos.makeMove(m, u);
      }
    }
    ++solutionPly;
  }
  if (solutionPly >= p.solution.size()) recordResult(true);
}

void PuzzleActivity::recordResult(const bool solved) {
  solveStats.record(solved, pack.puzzles[puzzleIndex].rating);
  phase = solved ? Phase::Solved : Phase::Missed;
  if (inkhome::sdMounted()) {
    Storage.mkdir(STATS_DIR, true);
    Storage.writeFile(STATS_PATH, String(solveStats.serialise().c_str()));
  }
}

void PuzzleActivity::loop() {
  using Button = MappedInputManager::Button;
  if (phase == Phase::LoadFailed) {
    if (mappedInput.wasReleased(Button::Back)) Activity::finish();
    return;
  }
  if (phase != Phase::Playing) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      startPuzzle(puzzleIndex + 1);
      requestUpdate();
    } else if (mappedInput.wasReleased(Button::Back)) {
      Activity::finish();
    }
    return;
  }
  if (mappedInput.wasReleased(Button::Back)) {
    if (selected >= 0) {
      selected = -1;
      targets.clear();
      requestUpdate();
    } else {
      Activity::finish();
    }
    return;
  }
  int dx = 0;
  int dy = 0;
  if (mappedInput.wasPressed(Button::Left)) dx = -1;
  if (mappedInput.wasPressed(Button::Right)) dx = 1;
  if (mappedInput.wasPressed(Button::Up)) dy = 1;
  if (mappedInput.wasPressed(Button::Down)) dy = -1;
  if (dx != 0 || dy != 0) {
    moveCursor(dx, dy);
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    trySelectOrMove();
    requestUpdate();
  }
}

void PuzzleActivity::drawChess(const int ox, const int oy, const int cell) const {
  static constexpr char LETTERS[] = " PNBRQK";
  for (int sq = 0; sq < 64; ++sq) {
    const int f = sq & 7;
    const int r = sq >> 3;
    const int x = ox + f * cell;
    const int y = oy + (7 - r) * cell;  // rank 8 at the top, White plays up the screen
    if ((f + r) % 2 == 0) renderer.fillRectDither(x, y, cell, cell, LightGray);
    const int8_t piece = chessPos.at(sq);
    if (piece != 0) {
      const bool white = piece > 0;
      const int radius = cell * 38 / 100;
      disc(renderer, x + cell / 2, y + cell / 2, radius, true);
      if (white) disc(renderer, x + cell / 2, y + cell / 2, radius - 2, false);
      const char letter[2] = {LETTERS[white ? piece : -piece], '\0'};
      const int w = renderer.getTextWidth(UI_12_FONT_ID, letter, EpdFontFamily::BOLD);
      const int h = renderer.getLineHeight(UI_12_FONT_ID);
      renderer.drawText(UI_12_FONT_ID, x + (cell - w) / 2, y + (cell - h) / 2, letter, white, EpdFontFamily::BOLD);
    }
    const bool target = std::find(targets.begin(), targets.end(), static_cast<uint8_t>(sq)) != targets.end();
    markers(renderer, x, y, cell, sq == cursor, sq == selected, target, piece != 0);
  }
  renderer.drawRect(ox, oy, cell * 8, cell * 8, 2, true);
}

void PuzzleActivity::drawXiangqi(const int ox, const int oy, const int cell) const {
  // Pieces sit on the intersections of a 9 x 10 grid; the river splits the middle.
  const int half = cell / 2;
  const int gx = ox + half;
  const int gy = oy + half;
  for (int r = 0; r < xiangqi::kRanks; ++r) renderer.drawLine(gx, gy + r * cell, gx + 8 * cell, gy + r * cell);
  for (int f = 0; f < xiangqi::kFiles; ++f) {
    const bool edge = f == 0 || f == xiangqi::kFiles - 1;
    renderer.drawLine(gx + f * cell, gy, gx + f * cell, gy + (edge ? 9 : 4) * cell);
    if (!edge) renderer.drawLine(gx + f * cell, gy + 5 * cell, gx + f * cell, gy + 9 * cell);
  }
  // Palaces.
  renderer.drawLine(gx + 3 * cell, gy, gx + 5 * cell, gy + 2 * cell);
  renderer.drawLine(gx + 5 * cell, gy, gx + 3 * cell, gy + 2 * cell);
  renderer.drawLine(gx + 3 * cell, gy + 7 * cell, gx + 5 * cell, gy + 9 * cell);
  renderer.drawLine(gx + 5 * cell, gy + 7 * cell, gx + 3 * cell, gy + 9 * cell);

  for (int sq = 0; sq < xiangqi::kSquares; ++sq) {
    const int8_t piece = xqPos.at(sq);
    const int cx = gx + xiangqi::fileOf(sq) * cell;
    const int cy = gy + (xiangqi::kRanks - 1 - xiangqi::rankOf(sq)) * cell;  // Black at the top
    if (piece != 0) {
      const bool red = piece > 0;
      const int radius = std::max(kXqGlyphSize / 2 + 3, cell * 42 / 100);
      disc(renderer, cx, cy, radius, true);
      if (red) disc(renderer, cx, cy, radius - 2, false);
      const uint32_t* glyph = kXqGlyphs[red ? piece : -piece][red ? 0 : 1];
      if (glyph) {
        for (int y = 0; y < kXqGlyphSize; ++y) {
          for (int x = 0; x < kXqGlyphSize; ++x) {
            if (glyph[y] & (1u << (kXqGlyphSize - 1 - x))) {
              renderer.drawPixel(cx - kXqGlyphSize / 2 + x, cy - kXqGlyphSize / 2 + y, red);
            }
          }
        }
      }
    }
    const bool target = std::find(targets.begin(), targets.end(), static_cast<uint8_t>(sq)) != targets.end();
    markers(renderer, cx - half + 2, cy - half + 2, cell - 4, sq == cursor, sq == selected, target, piece != 0);
  }
}

void PuzzleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int statusH = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - statusH;

  char subtitle[48] = {};
  snprintf(subtitle, sizeof(subtitle), tr(STR_CHESS_RATING_FMT), solveStats.rating,
           static_cast<unsigned>(solveStats.streak));
  const char* title = pack.title.empty() ? entry.label.c_str() : pack.title.c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title, subtitle);

  if (phase == Phase::LoadFailed) {
    renderer.drawCenteredText(UI_12_FONT_ID, (top + bottom) / 2, tr(STR_CHESS_LOAD_FAILED), true, EpdFontFamily::BOLD);
  } else {
    const bool isChess = pack.game == Game::Chess;
    const int files = isChess ? 8 : 9;
    const int ranks = isChess ? 8 : 10;
    const int cell = std::min((width - 16) / files, (bottom - top) / ranks);
    const int ox = (width - cell * files) / 2;
    const int oy = top + ((bottom - top) - cell * ranks) / 2;
    if (isChess) {
      drawChess(ox, oy, cell);
    } else {
      drawXiangqi(ox, oy, cell);
    }

    const Puzzle& p = pack.puzzles[puzzleIndex];
    char line[96];
    if (phase == Phase::Solved) {
      snprintf(line, sizeof(line), "%s", tr(STR_CHESS_SOLVED));
    } else if (phase == Phase::Missed) {
      snprintf(line, sizeof(line), tr(STR_CHESS_MISSED_FMT), expectedMove.c_str());
    } else {
      const bool toMove = isChess ? chessPos.whiteToMove() : xqPos.redToMove();
      snprintf(line, sizeof(line), tr(STR_CHESS_TO_MOVE_FMT), static_cast<unsigned>(puzzleIndex + 1),
               static_cast<unsigned>(pack.puzzles.size()),
               isChess ? (toMove ? tr(STR_CHESS_WHITE) : tr(STR_CHESS_BLACK))
                       : (toMove ? tr(STR_CHESS_RED) : tr(STR_CHESS_BLACK)),
               p.rating);
    }
    renderer.drawCenteredText(UI_10_FONT_ID, bottom + 4, line, true,
                              phase == Phase::Playing ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
  }

  const bool done = phase == Phase::Solved || phase == Phase::Missed;
  const char* confirm = phase == Phase::LoadFailed ? ""
                        : done                     ? tr(STR_CHESS_NEXT)
                        : selected >= 0            ? tr(STR_CHESS_MOVE)
                                                   : tr(STR_CHESS_SELECT);
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), confirm, done ? "" : tr(STR_DIR_LEFT), done ? "" : tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace chessink
