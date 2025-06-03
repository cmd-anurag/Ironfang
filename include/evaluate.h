#pragma once
#include "board.h"
#include "bitboard.h"

extern const int pawnPST[64];
extern const int knightPST[64];
extern const int bishopPST[64];
extern const int rookPST[64];
extern const int queenPST[64];
extern const int kingMdPST[64];
extern const int kingEgPST[64];

extern const int *PST[7];

class Evaluation {
    public:
    static int evaluate(const BitBoard &board);
    static constexpr int pieceValue[7] = {0, 100, 500, 320, 330, 900, 20000};
    static int inline mirror(int square) {
        return (7 - square / 8) * 8 + (square % 8);
    };
};