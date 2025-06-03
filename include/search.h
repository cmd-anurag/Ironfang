#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "bitboard.h"

constexpr int MAX_DEPTH = 7;

class Search {
    public:

        static Move findBestMove(BitBoard &board, int depth, int timeLimit);
    private:
        static bool hasNonPawnMaterial(const BitBoard& board, Color side);
        static int minimaxAlphaBeta(BitBoard &board, int depth, int alpha, int beta);
        static int quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth = 0);
};  

#endif