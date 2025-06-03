#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "bitboard.h"

constexpr int MAX_DEPTH = 6;

class Search {
    public:
        static Move findBestMove(BitBoard &board, int depth, int timeLimit);
    private:
        static int minimaxAlphaBeta(BitBoard &board, int depth, int alpha, int beta);
};

#endif