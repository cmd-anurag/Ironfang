#pragma once
#include "board.h"
#include "bitboard.h"



extern const int *PST[7];

class Evaluation {
    public:
    static int evaluate(const BitBoard &board);
    static constexpr int pieceValue[7] = {0, 100, 320, 330, 500, 900, 20000};
    static int inline mirror(int square) {
            return (7-(square >> 3)) << 3 | (square & 7);
    };
};