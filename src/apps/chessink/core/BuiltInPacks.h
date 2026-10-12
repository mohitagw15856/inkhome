#pragma once

// Puzzle packs built into the firmware (ChessInk's puzzles/ samplers,
// engine-verified there), so ChessInk works without a card.
namespace chessink {

constexpr char BUILT_IN_CHESS[] = R"PZL(PZL 1
M title Chess tactics sampler
M game chess
%%
F 6k1/1ppp1ppp/p7/8/8/8/8/4R2K w - - 0 1
X e1e8
R 600
T mate mateIn1 backRankMate
%%
F r1bqkbnr/pppp1ppp/8/4p3/2BnP3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1
X f3f7
R 700
T mate mateIn1 scholarsMate
%%
F rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq g3 0 1
X d8h4
R 500
T mate mateIn1 foolsMate
%%
F 6rk/1ppp2pp/p7/6N1/8/8/PPPP4/6K1 w - - 0 1
X g5f7
R 900
T mate mateIn1 smotheredMate
%%
F 6k1/1pp2ppp/3p4/8/8/8/8/R6K w - - 0 1
X a1a8
R 600
T mate mateIn1 backRankMate
%%
F 5rk1/5pp1/8/6p1/8/3B3Q/8/7K w - - 0 1
X h3h7
R 1100
T mate mateIn1
%%
F 3rkr2/8/p7/8/8/8/Q7/4K3 w - - 0 1
X a2e6
R 1000
T mate mateIn1 epauletteMate
%%
F kr6/pp4p1/7p/3N4/8/8/8/6K1 w - - 0 1
X d5c7
R 900
T mate mateIn1 smotheredMate
%%
F 8/8/8/8/8/1K6/7R/k7 w - - 0 1
X h2h1
R 700
T mate mateIn1 rookEndgame
%%
F 6k1/5ppp/p7/8/8/8/8/3Q3K w - - 0 1
X d1d8
R 600
T mate mateIn1 backRankMate
%%
F 7k/7p/p4P2/8/8/8/8/6QK w - - 0 1
X g1g7
R 800
T mate mateIn1
%%
F r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 0 1
X d5d8 e7d8 e1e8
R 1400
T mate mateIn2 backRankMate
%%
F k7/8/8/8/8/8/7R/6R1 w - - 0 1
X g1g7 a8b8 h2h8
R 1000
T mate mateIn2 ladderMate
%%
F r7/3k4/8/3N4/8/8/8/4K3 w - - 0 1
X d5c7 d7d6 c7a8
R 1200
T fork crushing
)PZL";

constexpr char BUILT_IN_XIANGQI[] = R"PZL(PZL 1
M title Xiangqi endgame studies
M game xiangqi
%%
F 4k4/R3p4/9/9/9/9/9/9/9/3K5 r
X a8a9
R 900
T mate mateIn1 rankMate
%%
F 4k2b1/R3p4/9/9/9/9/9/9/9/3K5 r
X a8a9
R 950
T mate mateIn1 rankMate
%%
F 4k4/4p3R/9/9/9/9/9/9/9/3K5 r
X i8i9
R 900
T mate mateIn1 rankMate
%%
F 5k3/R4p3/9/9/9/9/9/9/9/3K5 r
X a8a9
R 1000
T mate mateIn1 rankMate
%%
F 3k5/R2p5/9/9/9/9/9/9/9/4K4 r
X a8a9
R 1000
T mate mateIn1 rankMate
%%
F 2P1k4/C3p4/9/9/9/9/9/9/9/3K5 r
X a8a9
R 1200
T mate mateIn1 cannonMate
%%
F 3k5/9/9/9/9/9/9/9/R8/4K1R2 r
X g0g9 d9d8 a1d1
R 1300
T mate mateIn2 flyingGeneral
%%
F 3k5/R8/9/9/9/9/9/9/9/3RK4 r
X a8a9
R 1050
T mate mateIn1 rankMate
)PZL";

}  // namespace chessink
