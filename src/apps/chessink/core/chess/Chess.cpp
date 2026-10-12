// From ChessInk (github.com/mohitagw15856/chessink, MIT), include paths and one loop tidied for cppcheck.
#include "Chess.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace chessink {
namespace chess {

namespace {

inline int fileOf(int sq) { return sq & 7; }
inline int rankOf(int sq) { return sq >> 3; }
inline bool onBoard(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }
inline int sqOf(int f, int r) { return r * 8 + f; }

const int kKnightF[] = {1, 2, 2, 1, -1, -2, -2, -1};
const int kKnightR[] = {2, 1, -1, -2, -2, -1, 1, 2};
const int kKingF[] = {1, 1, 1, 0, 0, -1, -1, -1};
const int kKingR[] = {1, 0, -1, 1, -1, 1, 0, -1};
const int kBishopF[] = {1, 1, -1, -1};
const int kBishopR[] = {1, -1, 1, -1};
const int kRookF[] = {1, -1, 0, 0};
const int kRookR[] = {0, 0, 1, -1};

}  // namespace

void Position::setStart() { setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"); }

bool Position::setFen(const std::string& fen) {
  for (int i = 0; i < 64; ++i) board_[i] = EMPTY;
  castling_ = 0;
  epFile_ = -1;
  halfmove_ = 0;

  size_t i = 0;
  int f = 0;
  int r = 7;
  // Board field.
  for (; i < fen.size() && fen[i] != ' '; ++i) {
    const char c = fen[i];
    if (c == '/') {
      f = 0;
      --r;
      if (r < 0) return false;
    } else if (c >= '1' && c <= '8') {
      f += c - '0';
      if (f > 8) return false;
    } else {
      if (!onBoard(f, r)) return false;
      int8_t p = 0;
      switch (std::tolower(c)) {
        case 'p':
          p = WP;
          break;
        case 'n':
          p = WN;
          break;
        case 'b':
          p = WB;
          break;
        case 'r':
          p = WR;
          break;
        case 'q':
          p = WQ;
          break;
        case 'k':
          p = WK;
          break;
        default:
          return false;
      }
      board_[sqOf(f, r)] = std::isupper(c) ? p : static_cast<int8_t>(-p);
      ++f;
    }
  }
  if (i >= fen.size()) return false;
  ++i;
  if (i >= fen.size()) return false;
  whiteToMove_ = fen[i] == 'w';
  while (i < fen.size() && fen[i] != ' ') ++i;
  if (i < fen.size()) ++i;
  // Castling field.
  for (; i < fen.size() && fen[i] != ' '; ++i) {
    switch (fen[i]) {
      case 'K':
        castling_ |= 1;
        break;
      case 'Q':
        castling_ |= 2;
        break;
      case 'k':
        castling_ |= 4;
        break;
      case 'q':
        castling_ |= 8;
        break;
      case '-':
        break;
      default:
        return false;
    }
  }
  if (i < fen.size()) ++i;
  // En passant field.
  if (i < fen.size() && fen[i] != '-') {
    if (fen[i] >= 'a' && fen[i] <= 'h') epFile_ = static_cast<int8_t>(fen[i] - 'a');
  }
  return true;
}

std::string Position::fen() const {
  std::string out;
  for (int r = 7; r >= 0; --r) {
    int run = 0;
    for (int f = 0; f < 8; ++f) {
      const int8_t p = board_[sqOf(f, r)];
      if (p == EMPTY) {
        ++run;
        continue;
      }
      if (run) {
        out += static_cast<char>('0' + run);
        run = 0;
      }
      const char names[] = " pnbrqk";
      char c = names[p > 0 ? p : -p];
      out += p > 0 ? static_cast<char>(std::toupper(c)) : c;
    }
    if (run) out += static_cast<char>('0' + run);
    if (r) out += '/';
  }
  out += whiteToMove_ ? " w " : " b ";
  if (!castling_) {
    out += '-';
  } else {
    if (castling_ & 1) out += 'K';
    if (castling_ & 2) out += 'Q';
    if (castling_ & 4) out += 'k';
    if (castling_ & 8) out += 'q';
  }
  out += ' ';
  if (epFile_ < 0) {
    out += '-';
  } else {
    out += static_cast<char>('a' + epFile_);
    out += whiteToMove_ ? '6' : '3';
  }
  return out;
}

int Position::kingSquare(bool white) const {
  const int8_t target = white ? WK : -WK;
  for (int i = 0; i < 64; ++i) {
    if (board_[i] == target) return i;
  }
  return -1;
}

bool Position::attacked(int sq, bool byWhite) const {
  const int f = fileOf(sq);
  const int r = rankOf(sq);
  const int8_t sign = byWhite ? 1 : -1;

  // Pawn attacks (a white pawn attacks upward, so it sits one rank below sq).
  const int pr = r - (byWhite ? 1 : -1);
  for (int df = -1; df <= 1; df += 2) {
    if (onBoard(f + df, pr) && board_[sqOf(f + df, pr)] == sign * WP) return true;
  }
  // Knights.
  for (int k = 0; k < 8; ++k) {
    const int nf = f + kKnightF[k];
    const int nr = r + kKnightR[k];
    if (onBoard(nf, nr) && board_[sqOf(nf, nr)] == sign * WN) return true;
  }
  // King.
  for (int k = 0; k < 8; ++k) {
    const int nf = f + kKingF[k];
    const int nr = r + kKingR[k];
    if (onBoard(nf, nr) && board_[sqOf(nf, nr)] == sign * WK) return true;
  }
  // Sliding: bishops/queens on diagonals.
  for (int d = 0; d < 4; ++d) {
    int nf = f + kBishopF[d];
    int nr = r + kBishopR[d];
    while (onBoard(nf, nr)) {
      const int8_t p = board_[sqOf(nf, nr)];
      if (p != EMPTY) {
        if (p == sign * WB || p == sign * WQ) return true;
        break;
      }
      nf += kBishopF[d];
      nr += kBishopR[d];
    }
  }
  // Sliding: rooks/queens on files and ranks.
  for (int d = 0; d < 4; ++d) {
    int nf = f + kRookF[d];
    int nr = r + kRookR[d];
    while (onBoard(nf, nr)) {
      const int8_t p = board_[sqOf(nf, nr)];
      if (p != EMPTY) {
        if (p == sign * WR || p == sign * WQ) return true;
        break;
      }
      nf += kRookF[d];
      nr += kRookR[d];
    }
  }
  return false;
}

bool Position::inCheck(bool white) const {
  const int k = kingSquare(white);
  return k >= 0 && attacked(k, !white);
}

void Position::addPawnMoves(int sq, std::vector<Move>& out) const {
  const bool white = board_[sq] > 0;
  const int f = fileOf(sq);
  const int r = rankOf(sq);
  const int dir = white ? 1 : -1;
  const int startRank = white ? 1 : 6;
  const int promoRank = white ? 7 : 0;

  auto push = [&](int to, bool capture) {
    (void)capture;
    if (rankOf(to) == promoRank) {
      for (int8_t promo : {WQ, WR, WB, WN}) {
        Move m;
        m.from = static_cast<uint8_t>(sq);
        m.to = static_cast<uint8_t>(to);
        m.promo = promo;
        out.push_back(m);
      }
    } else {
      Move m;
      m.from = static_cast<uint8_t>(sq);
      m.to = static_cast<uint8_t>(to);
      out.push_back(m);
    }
  };

  // Single and double pushes.
  const int one = sqOf(f, r + dir);
  if (onBoard(f, r + dir) && board_[one] == EMPTY) {
    push(one, false);
    const int two = sqOf(f, r + 2 * dir);
    if (r == startRank && board_[two] == EMPTY) {
      push(two, false);
    }
  }
  // Captures.
  for (int df = -1; df <= 1; df += 2) {
    const int nf = f + df;
    const int nr = r + dir;
    if (!onBoard(nf, nr)) continue;
    const int to = sqOf(nf, nr);
    const int8_t victim = board_[to];
    if (victim != EMPTY && (victim > 0) != white) {
      push(to, true);
    }
  }
  // En passant: our pawn on the fifth rank next to the double-pushed file.
  if (epFile_ >= 0) {
    const int epRank = white ? 4 : 3;      // rank our pawn stands on
    const int targetRank = white ? 5 : 2;  // rank we land on
    if (r == epRank && std::abs(f - epFile_) == 1) {
      Move m;
      m.from = static_cast<uint8_t>(sq);
      m.to = static_cast<uint8_t>(sqOf(epFile_, targetRank));
      m.isEnPassant = true;
      out.push_back(m);
    }
  }
}

void Position::pseudoMoves(std::vector<Move>& out) const {
  const bool white = whiteToMove_;
  for (int sq = 0; sq < 64; ++sq) {
    const int8_t p = board_[sq];
    if (p == EMPTY || (p > 0) != white) continue;
    const int f = fileOf(sq);
    const int r = rankOf(sq);
    const int8_t abs = static_cast<int8_t>(p > 0 ? p : -p);

    auto slide = [&](const int* df, const int* dr, int count) {
      for (int d = 0; d < count; ++d) {
        int nf = f + df[d];
        int nr = r + dr[d];
        while (onBoard(nf, nr)) {
          const int to = sqOf(nf, nr);
          const int8_t victim = board_[to];
          if (victim == EMPTY || (victim > 0) != white) {
            Move m;
            m.from = static_cast<uint8_t>(sq);
            m.to = static_cast<uint8_t>(to);
            out.push_back(m);
          }
          if (victim != EMPTY) break;
          nf += df[d];
          nr += dr[d];
        }
      }
    };

    switch (abs) {
      case WP:
        addPawnMoves(sq, out);
        break;
      case WN:
        for (int k = 0; k < 8; ++k) {
          const int nf = f + kKnightF[k];
          const int nr = r + kKnightR[k];
          if (!onBoard(nf, nr)) continue;
          const int to = sqOf(nf, nr);
          const int8_t victim = board_[to];
          if (victim == EMPTY || (victim > 0) != white) {
            Move m;
            m.from = static_cast<uint8_t>(sq);
            m.to = static_cast<uint8_t>(to);
            out.push_back(m);
          }
        }
        break;
      case WB:
        slide(kBishopF, kBishopR, 4);
        break;
      case WR:
        slide(kRookF, kRookR, 4);
        break;
      case WQ:
        slide(kBishopF, kBishopR, 4);
        slide(kRookF, kRookR, 4);
        break;
      case WK: {
        for (int k = 0; k < 8; ++k) {
          const int nf = f + kKingF[k];
          const int nr = r + kKingR[k];
          if (!onBoard(nf, nr)) continue;
          const int to = sqOf(nf, nr);
          const int8_t victim = board_[to];
          if (victim == EMPTY || (victim > 0) != white) {
            Move m;
            m.from = static_cast<uint8_t>(sq);
            m.to = static_cast<uint8_t>(to);
            out.push_back(m);
          }
        }
        // Castling: rights present, squares empty, king path unattacked.
        const int home = white ? sqOf(4, 0) : sqOf(4, 7);
        if (sq == home && !attacked(sq, !white)) {
          const uint8_t ksBit = white ? 1 : 4;
          const uint8_t qsBit = white ? 2 : 8;
          const int rank = white ? 0 : 7;
          if ((castling_ & ksBit) && board_[sqOf(5, rank)] == EMPTY && board_[sqOf(6, rank)] == EMPTY &&
              !attacked(sqOf(5, rank), !white) && !attacked(sqOf(6, rank), !white)) {
            Move m;
            m.from = static_cast<uint8_t>(sq);
            m.to = static_cast<uint8_t>(sqOf(6, rank));
            m.isCastle = true;
            out.push_back(m);
          }
          if ((castling_ & qsBit) && board_[sqOf(3, rank)] == EMPTY && board_[sqOf(2, rank)] == EMPTY &&
              board_[sqOf(1, rank)] == EMPTY && !attacked(sqOf(3, rank), !white) && !attacked(sqOf(2, rank), !white)) {
            Move m;
            m.from = static_cast<uint8_t>(sq);
            m.to = static_cast<uint8_t>(sqOf(2, rank));
            m.isCastle = true;
            out.push_back(m);
          }
        }
        break;
      }
    }
  }
}

void Position::makeMove(const Move& m, Undo& u) {
  const bool white = whiteToMove_;
  u.captured = board_[m.to];
  u.castling = castling_;
  u.epFile = epFile_;
  u.halfmove = halfmove_;

  int8_t piece = board_[m.from];
  board_[m.from] = EMPTY;

  if (m.isEnPassant) {
    const int victimSq = sqOf(fileOf(m.to), rankOf(m.from));
    u.captured = board_[victimSq];
    board_[victimSq] = EMPTY;
  }

  if (m.promo != 0) {
    piece = static_cast<int8_t>(white ? m.promo : -m.promo);
  }
  board_[m.to] = piece;

  if (m.isCastle) {
    const int rank = rankOf(m.to);
    if (fileOf(m.to) == 6) {  // king side: rook h -> f
      board_[sqOf(5, rank)] = board_[sqOf(7, rank)];
      board_[sqOf(7, rank)] = EMPTY;
    } else {  // queen side: rook a -> d
      board_[sqOf(3, rank)] = board_[sqOf(0, rank)];
      board_[sqOf(0, rank)] = EMPTY;
    }
  }

  // Castling rights follow king/rook movement or rook capture.
  auto clearRight = [&](int sq, uint8_t bit) {
    if (m.from == sq || m.to == sq) castling_ &= static_cast<uint8_t>(~bit);
  };
  if (piece == WK || piece == static_cast<int8_t>(-WK) || m.isCastle) {
    castling_ &= static_cast<uint8_t>(white ? ~3 : ~12);
  }
  clearRight(sqOf(7, 0), 1);
  clearRight(sqOf(0, 0), 2);
  clearRight(sqOf(7, 7), 4);
  clearRight(sqOf(0, 7), 8);

  // En passant availability.
  const int8_t absPiece = static_cast<int8_t>(piece > 0 ? piece : -piece);
  if (absPiece == WP && std::abs(rankOf(m.to) - rankOf(m.from)) == 2) {
    epFile_ = static_cast<int8_t>(fileOf(m.to));
  } else {
    epFile_ = -1;
  }

  whiteToMove_ = !whiteToMove_;
}

void Position::unmakeMove(const Move& m, const Undo& u) {
  whiteToMove_ = !whiteToMove_;
  const bool white = whiteToMove_;

  int8_t piece = board_[m.to];
  if (m.promo != 0) {
    piece = static_cast<int8_t>(white ? WP : -WP);
  }
  board_[m.from] = piece;
  board_[m.to] = EMPTY;

  if (m.isEnPassant) {
    const int victimSq = sqOf(fileOf(m.to), rankOf(m.from));
    board_[victimSq] = u.captured;
  } else {
    board_[m.to] = u.captured;
  }

  if (m.isCastle) {
    const int rank = rankOf(m.to);
    if (fileOf(m.to) == 6) {
      board_[sqOf(7, rank)] = board_[sqOf(5, rank)];
      board_[sqOf(5, rank)] = EMPTY;
    } else {
      board_[sqOf(0, rank)] = board_[sqOf(3, rank)];
      board_[sqOf(3, rank)] = EMPTY;
    }
  }

  castling_ = u.castling;
  epFile_ = u.epFile;
  halfmove_ = u.halfmove;
}

void Position::legalMoves(std::vector<Move>& out) const {
  out.clear();
  std::vector<Move> pseudo;
  pseudo.reserve(64);
  pseudoMoves(pseudo);
  Position& self = const_cast<Position&>(*this);
  const bool mover = whiteToMove_;
  for (const Move& m : pseudo) {
    Undo u;
    self.makeMove(m, u);
    if (!self.inCheck(mover)) out.push_back(m);
    self.unmakeMove(m, u);
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

bool Position::parseUci(const std::string& s, Move& out) const {
  if (s.size() < 4) return false;
  const int ff = s[0] - 'a';
  const int fr = s[1] - '1';
  const int tf = s[2] - 'a';
  const int tr = s[3] - '1';
  if (!onBoard(ff, fr) || !onBoard(tf, tr)) return false;
  int8_t promo = 0;
  if (s.size() >= 5) {
    switch (s[4]) {
      case 'q':
        promo = WQ;
        break;
      case 'r':
        promo = WR;
        break;
      case 'b':
        promo = WB;
        break;
      case 'n':
        promo = WN;
        break;
      default:
        return false;
    }
  }
  std::vector<Move> moves;
  legalMoves(moves);
  const auto it = std::find_if(moves.begin(), moves.end(), [&](const Move& m) {
    return m.from == sqOf(ff, fr) && m.to == sqOf(tf, tr) && m.promo == promo;
  });
  if (it == moves.end()) return false;
  out = *it;
  return true;
}

std::string Position::uci(const Move& m) {
  std::string s;
  s += static_cast<char>('a' + fileOf(m.from));
  s += static_cast<char>('1' + rankOf(m.from));
  s += static_cast<char>('a' + fileOf(m.to));
  s += static_cast<char>('1' + rankOf(m.to));
  if (m.promo != 0) {
    const char names[] = " pnbrq";
    s += names[m.promo];
  }
  return s;
}

}  // namespace chess
}  // namespace chessink
