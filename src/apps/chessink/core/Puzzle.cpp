// From ChessInk (github.com/mohitagw15856/chessink, MIT), unchanged apart from include paths.
#include "Puzzle.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace chessink {

namespace {

std::vector<std::string> splitWords(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ' ') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

}  // namespace

bool parsePack(const std::string& text, Pack& out, size_t maxPuzzles) {
  out = Pack{};
  bool first = true;
  bool inPuzzle = false;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    const bool last = (end == text.size());
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (first) {
      if (line != "PZL 1") return false;
      first = false;
      continue;
    }
    if (line == "%%") {
      if (out.puzzles.size() < maxPuzzles) {
        out.puzzles.emplace_back();
        inPuzzle = true;
      } else {
        inPuzzle = false;
      }
    } else if (!line.empty()) {
      const char tag = line[0];
      const std::string rest = line.size() >= 2 ? line.substr(2) : std::string();
      if (!inPuzzle) {
        if (tag == 'M') {
          const size_t sp = rest.find(' ');
          const std::string key = rest.substr(0, sp);
          const std::string value = sp == std::string::npos ? "" : rest.substr(sp + 1);
          if (key == "title") out.title = value;
          if (key == "game") out.game = (value == "xiangqi") ? Game::Xiangqi : Game::Chess;
        }
      } else {
        Puzzle& p = out.puzzles.back();
        switch (tag) {
          case 'F':
            p.fen = rest;
            break;
          case 'X':
            p.solution = splitWords(rest);
            break;
          case 'R':
            p.rating = atoi(rest.c_str());
            break;
          case 'T':
            p.themes = rest;
            break;
          default:
            break;
        }
      }
    }
    if (last) break;
  }
  // Drop trailing empties and reject packs with unusable entries stripped out.
  while (!out.puzzles.empty() && (out.puzzles.back().fen.empty() || out.puzzles.back().solution.empty())) {
    out.puzzles.pop_back();
  }
  return !out.puzzles.empty();
}

int updateRating(int playerRating, int puzzleRating, bool solved) {
  const double expected = 1.0 / (1.0 + std::pow(10.0, (puzzleRating - playerRating) / 400.0));
  const double score = solved ? 1.0 : 0.0;
  const int next = playerRating + static_cast<int>(std::lround(32.0 * (score - expected)));
  return next < 100 ? 100 : next;
}

void SolveStats::record(bool ok, int puzzleRating) {
  ++attempted;
  if (ok) {
    ++solved;
    ++streak;
  } else {
    streak = 0;
  }
  rating = updateRating(rating, puzzleRating, ok);
}

std::string SolveStats::serialise() const {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "rating %d\nstreak %lu\nsolved %lu\nattempted %lu\n", rating,
                static_cast<unsigned long>(streak), static_cast<unsigned long>(solved),
                static_cast<unsigned long>(attempted));
  return buf;
}

SolveStats SolveStats::parse(const std::string& text) {
  SolveStats s;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    const size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    const std::string key = line.substr(0, sp);
    const long value = strtol(line.c_str() + sp + 1, nullptr, 10);
    if (key == "rating") s.rating = static_cast<int>(value);
    if (key == "streak") s.streak = static_cast<uint32_t>(value);
    if (key == "solved") s.solved = static_cast<uint32_t>(value);
    if (key == "attempted") s.attempted = static_cast<uint32_t>(value);
  }
  return s;
}

}  // namespace chessink
