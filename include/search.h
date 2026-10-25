#ifndef SEARCH_H
#define SEARCH_H

#include "bitboard.h"
#include <mutex>

constexpr int INF = 9999999;
constexpr int MATE_SCORE = 100000;
constexpr int MATE_THRESHOLD = 99000;
constexpr int MAX_PLY = 256;
constexpr int MAX_DEPTH = 256;
constexpr int MAX_EXTENSIONS = 16;
class Search {
public:
    static Move findBestMove(BitBoard& board, int depth, int timeLimit);
    static void newGame();
    static void clearStopRequest();
    static void requestStop();

private:
    static int minimaxAlphaBeta(BitBoard& board, int depth, int alpha, int beta, int ply,
                                int numExtensions = 0);
    static int searchChild(BitBoard& board, int childDepth, int alpha, int beta,
                           int ply, int numExtensions, bool reduceMove);
    static int quiescenceSearch(BitBoard& board, int alpha, int beta, int ply = 0);
};

extern std::mutex uciOutputMutex;

#endif
