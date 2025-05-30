#pragma once
#include "board.h"

class Evaluation {
    public:
    static int evaluate(const Board &board);
    static constexpr int pieceValue[7] = {0, 100, 500, 320, 330, 900, 20000};
};