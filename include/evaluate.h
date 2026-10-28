#pragma once
#include "bitboard.h"



extern const int *PST[7];

class Evaluation {
    public:
    struct Breakdown {
        int whiteMaterial = 0;
        int whitePieceSquare = 0;
        int whiteMopUp = 0;
        int whitePawn = 0;
        int whiteKingSafety = 0;
        int whitePositional = 0;
        int blackMaterial = 0;
        int blackPieceSquare = 0;
        int blackMopUp = 0;
        int blackPawn = 0;
        int blackKingSafety = 0;
        int blackPositional = 0;
        int score = 0;
    };

    static int evaluate(const BitBoard &board);
    static Breakdown evaluateBreakdown(const BitBoard &board);
    static constexpr int pieceValue[7] = {0, 100, 300, 320, 500, 900, 20000};
    static int inline mirror(int square) {
            return (7-(square >> 3)) << 3 | (square & 7);
    };
};
