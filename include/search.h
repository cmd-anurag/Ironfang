#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"


class Search {
    public:
        static Move findBestMove(Board &board, int depth);
    private:
        static int minimaxAlphaBeta(Board &board, int depth, int alpha, int beta);
};

#endif