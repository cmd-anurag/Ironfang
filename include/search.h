#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "bitboard.h"

constexpr int INF = 1000000;                    // General infinity value
constexpr int MAX_DEPTH = 20;                   // Max depth till the engine can search  
constexpr int MATE_SCORE = 900000;              // Base mate score
constexpr int MATE_THRESHOLD = 800000;          // Threshold to detect mate scores
constexpr int MAX_PLY = 100; 



class Search {
    public:

        static Move findBestMove(BitBoard &board, int depth, int timeLimit);
    private:
        static bool hasNonPawnMaterial(const BitBoard& board, Color side);
        static int minimaxAlphaBeta(BitBoard &board, int depth, int alpha, int beta, int ply);
        static int quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth = 0, int ply=0);
};  

#endif