#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "bitboard.h"

constexpr int INF = 1000000;                    // General infinity value
constexpr int MATE_SCORE = INF;                 // Base mate score
constexpr int MATE_THRESHOLD = 800000;          // Threshold to detect mate scores
constexpr int MAX_PLY = 100;
constexpr int MAX_DEPTH = MAX_PLY - 1;
constexpr int MAX_EXTENSIONS = 16;



class Search {
    public:

        static Move findBestMove(BitBoard &board, int depth, int timeLimit);
        static void setRootDiagnostics(bool enabled);
    private:
        static int minimaxAlphaBeta(BitBoard &board, int depth, int alpha, int beta, int ply,
                                    int numExtensions = 0);
        static int quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth = 0, int ply=0);
};  

#endif
