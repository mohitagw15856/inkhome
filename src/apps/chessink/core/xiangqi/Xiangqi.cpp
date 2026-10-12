// From ChessInk (github.com/mohitagw15856/chessink, MIT), include paths and one loop tidied for cppcheck.
#include "Xiangqi.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace chessink {
namespace xiangqi {

namespace {

inline bool onBoard(int f, int r) { return f >= 0 && f < kFiles && r >= 0 && r < kRanks; }

inline bool inPalace(int f, int r, bool red) {
  if (f < 3 || f > 5) return false;
  return red ? (r >= 0 && r <= 2) : (r >= 7 && r <= 9);
}

inline bool ownSide(int r, bool red) { return red ? (r <= 4) : (r >= 5); }

const int kOrthF[] = {1, -1, 0, 0};
const int kOrthR[] = {0, 0, 1, -1};
const int kDiagF[] = {1, 1, -1, -1};
const int kDiagR[] = {1, -1, 1, -1};
// Horse: leg offset then two destination offsets sharing that leg.
const int kHorseLegF[] = {1, -1, 0, 0};
const int kHorseLegR[] = {0, 0, 1, -1};
const int kHorseDstF[4][2] = {{2, 2}, {-2, -2}, {1, -1}, {1, -1}};
const int kHorseDstR[4][2] = {{1, -1}, {1, -1}, {2, 2}, {-2, -2}};

}  // namespace

void Position::setStart() { setFen("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR r"); }

bool Position::setFen(const std::string& fen) {
  for (int i = 0; i < kSquares; ++i) board_[i] = EMPTY;
  size_t i = 0;
  int f = 0;
  int r = kRanks - 1;
  for (; i < fen.size() && fen[i] != ' '; ++i) {
    const char c = fen[i];
    if (c == '/') {
      f = 0;
      --r;
      if (r < 0) return false;
    } else if (c >= '1' && c <= '9') {
      f += c - '0';
      if (f > kFiles) return false;
    } else {
      if (!onBoard(f, r)) return false;
      int8_t p = 0;
      switch (std::tolower(c)) {
        case 'k':
          p = GEN;
          break;
        case 'a':
          p = ADV;
          break;
        case 'b':
        case 'e':
          p = ELE;
          break;
        case 'n':
        case 'h':
          p = HOR;
          break;
        case 'r':
          p = CHA;
          break;
        case 'c':
          p = CAN;
          break;
        case 'p':
          p = SOL;
          break;
        default:
          return false;
      }
      board_[sqOf(f, r)] = std::isupper(c) ? p : static_cast<int8_t>(-p);
      ++f;
    }
  }
  redToMove_ = true;
  if (i < fen.size()) {
    ++i;
    if (i < fen.size()) redToMove_ = (fen[i] == 'r' || fen[i] == 'w');
  }
  return true;
}

int Position::generalSquare(bool red) const {
  const int8_t target = red ? GEN : -GEN;
  for (int i = 0; i < kSquares; ++i) {
    if (board_[i] == target) return i;
  }
  return -1;
}

bool Position::generalsFacing() const {
  const int rg = generalSquare(true);
  const int bg = generalSquare(false);
  if (rg < 0 || bg < 0) return false;
  if (fileOf(rg) != fileOf(bg)) return false;
  const int f = fileOf(rg);
  for (int r = rankOf(rg) + 1; r < rankOf(bg); ++r) {
    if (board_[sqOf(f, r)] != EMPTY) return false;
  }
  return true;
}

bool Position::attacked(int sq, bool byRed) const {
  const int f = fileOf(sq);
  const int r = rankOf(sq);
  const int8_t sign = byRed ? 1 : -1;

  // Rook lines and cannon lines share ray walks.
  for (int d = 0; d < 4; ++d) {
    int nf = f + kOrthF[d];
    int nr = r + kOrthR[d];
    bool screenSeen = false;
    while (onBoard(nf, nr)) {
      const int8_t p = board_[sqOf(nf, nr)];
      if (p != EMPTY) {
        if (!screenSeen) {
          if (p == sign * CHA) return true;
          // Adjacent general counts via the rook walk (1-step) too:
          if (p == sign * GEN && std::abs(nf - f) + std::abs(nr - r) == 1) return true;
          screenSeen = true;
        } else {
          if (p == sign * CAN) return true;
          break;
        }
      }
      nf += kOrthF[d];
      nr += kOrthR[d];
    }
  }

  // Horse attacks: reverse the leg (leg sits between attacker and target,
  // adjacent-diagonal to the target).
  for (int df = -1; df <= 1; df += 2) {
    for (int dr = -1; dr <= 1; dr += 2) {
      const int legF = f + df;
      const int legR = r + dr;
      if (!onBoard(legF, legR) || board_[sqOf(legF, legR)] != EMPTY) continue;
      for (int k = 0; k < 2; ++k) {
        const int hf = k == 0 ? f + 2 * df : f + df;
        const int hr = k == 0 ? r + dr : r + 2 * dr;
        if (onBoard(hf, hr) && board_[sqOf(hf, hr)] == sign * HOR) return true;
      }
    }
  }

  // Soldier attacks: a soldier attacks the square in front of it (from its
  // perspective) and, once across the river, sideways too.
  {
    // Red soldiers advance to higher ranks: one sits at r-1 attacking sq.
    const int sr = r - sign;  // byRed: r-1 ; byBlack: r+1
    if (onBoard(f, sr) && board_[sqOf(f, sr)] == sign * SOL) return true;
    for (int df = -1; df <= 1; df += 2) {
      const int nf = f + df;
      if (!onBoard(nf, r)) continue;
      if (board_[sqOf(nf, r)] == sign * SOL) {
        // Sideways attack only after crossing the river.
        const bool crossed = byRed ? (r >= 5) : (r <= 4);
        if (crossed) return true;
      }
    }
  }

  return false;
}

bool Position::inCheck(bool red) const {
  const int g = generalSquare(red);
  if (g < 0) return true;
  if (generalsFacing()) return true;  // both sides treat facing as illegal
  return attacked(g, !red);
}

void Position::pseudoMoves(std::vector<Move>& out) const {
  const bool red = redToMove_;
  for (int sq = 0; sq < kSquares; ++sq) {
    const int8_t p = board_[sq];
    if (p == EMPTY || (p > 0) != red) continue;
    const int f = fileOf(sq);
    const int r = rankOf(sq);
    const int8_t abs = static_cast<int8_t>(p > 0 ? p : -p);

    auto add = [&](int nf, int nr) {
      if (!onBoard(nf, nr)) return;
      const int8_t victim = board_[sqOf(nf, nr)];
      if (victim != EMPTY && (victim > 0) == red) return;
      Move m;
      m.from = static_cast<uint8_t>(sq);
      m.to = static_cast<uint8_t>(sqOf(nf, nr));
      out.push_back(m);
    };

    switch (abs) {
      case GEN:
        for (int d = 0; d < 4; ++d) {
          const int nf = f + kOrthF[d];
          const int nr = r + kOrthR[d];
          if (inPalace(nf, nr, red)) add(nf, nr);
        }
        break;
      case ADV:
        for (int d = 0; d < 4; ++d) {
          const int nf = f + kDiagF[d];
          const int nr = r + kDiagR[d];
          if (inPalace(nf, nr, red)) add(nf, nr);
        }
        break;
      case ELE:
        for (int d = 0; d < 4; ++d) {
          const int eyeF = f + kDiagF[d];
          const int eyeR = r + kDiagR[d];
          const int nf = f + 2 * kDiagF[d];
          const int nr = r + 2 * kDiagR[d];
          if (!onBoard(nf, nr) || !ownSide(nr, red)) continue;
          if (board_[sqOf(eyeF, eyeR)] != EMPTY) continue;  // blocked eye
          add(nf, nr);
        }
        break;
      case HOR:
        for (int leg = 0; leg < 4; ++leg) {
          const int legF = f + kHorseLegF[leg];
          const int legR = r + kHorseLegR[leg];
          if (!onBoard(legF, legR) || board_[sqOf(legF, legR)] != EMPTY) continue;
          for (int k = 0; k < 2; ++k) {
            add(f + kHorseDstF[leg][k], r + kHorseDstR[leg][k]);
          }
        }
        break;
      case CHA:
        for (int d = 0; d < 4; ++d) {
          int nf = f + kOrthF[d];
          int nr = r + kOrthR[d];
          while (onBoard(nf, nr)) {
            const int8_t victim = board_[sqOf(nf, nr)];
            add(nf, nr);
            if (victim != EMPTY) break;
            nf += kOrthF[d];
            nr += kOrthR[d];
          }
        }
        break;
      case CAN:
        for (int d = 0; d < 4; ++d) {
          int nf = f + kOrthF[d];
          int nr = r + kOrthR[d];
          bool screen = false;
          while (onBoard(nf, nr)) {
            const int8_t victim = board_[sqOf(nf, nr)];
            if (!screen) {
              if (victim == EMPTY) {
                add(nf, nr);
              } else {
                screen = true;
              }
            } else if (victim != EMPTY) {
              if ((victim > 0) != red) add(nf, nr);
              break;
            }
            nf += kOrthF[d];
            nr += kOrthR[d];
          }
        }
        break;
      case SOL: {
        const int dir = red ? 1 : -1;
        add(f, r + dir);
        const bool crossed = red ? (r >= 5) : (r <= 4);
        if (crossed) {
          add(f - 1, r);
          add(f + 1, r);
        }
        break;
      }
    }
  }
}

void Position::makeMove(const Move& m, Undo& u) {
  u.captured = board_[m.to];
  board_[m.to] = board_[m.from];
  board_[m.from] = EMPTY;
  redToMove_ = !redToMove_;
}

void Position::unmakeMove(const Move& m, const Undo& u) {
  redToMove_ = !redToMove_;
  board_[m.from] = board_[m.to];
  board_[m.to] = u.captured;
}

void Position::legalMoves(std::vector<Move>& out) const {
  out.clear();
  std::vector<Move> pseudo;
  pseudo.reserve(64);
  pseudoMoves(pseudo);
  Position& self = const_cast<Position&>(*this);
  const bool mover = redToMove_;
  for (const Move& m : pseudo) {
    Undo u;
    self.makeMove(m, u);
    const bool bad =
        self.generalsFacing() || self.attacked(self.generalSquare(mover), !mover) || self.generalSquare(mover) < 0;
    self.unmakeMove(m, u);
    if (!bad) out.push_back(m);
  }
}

uint64_t Position::perft(int depth) {
  if (depth == 0) return 1;
  std::vector<Move> moves;
  legalMoves(moves);
  if (depth == 1) return moves.size();
  uint64_t nodes = 0;
  for (const Move& m : moves) {
    Undo u;
    makeMove(m, u);
    nodes += perft(depth - 1);
    unmakeMove(m, u);
  }
  return nodes;
}

bool Position::parseMove(const std::string& text, Move& out) const {
  if (text.size() < 4) return false;
  const int ff = text[0] - 'a';
  const int fr = text[1] - '0';
  const int tf = text[2] - 'a';
  const int tr = text[3] - '0';
  if (!onBoard(ff, fr) || !onBoard(tf, tr)) return false;
  std::vector<Move> moves;
  legalMoves(moves);
  const auto it = std::find_if(moves.begin(), moves.end(),
                               [&](const Move& m) { return m.from == sqOf(ff, fr) && m.to == sqOf(tf, tr); });
  if (it == moves.end()) return false;
  out = *it;
  return true;
}

std::string Position::moveText(const Move& m) {
  std::string s;
  s += static_cast<char>('a' + fileOf(m.from));
  s += static_cast<char>('0' + rankOf(m.from));
  s += static_cast<char>('a' + fileOf(m.to));
  s += static_cast<char>('0' + rankOf(m.to));
  return s;
}

}  // namespace xiangqi
}  // namespace chessink
