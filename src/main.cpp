#include "board.h"
#include "bitboard.h"
#include "evaluate.h"
#include "search.h"
#include "mailbox.h"
#include "uci.h"
#include "zobrist.h"
#include <iostream>
#include <vector>


int main() {
    // uciLoop();
    BitBoard board;
    board.setPositionFromFEN("1r2r1k1/1b3p1p/p1p3p1/2p1q2n/P1N1P3/7P/2Q2PP1/3RRBK1 b - - 1 26");
    Move move = Search::findBestMove(board, MAX_DEPTH, 5000);
    std::cout << '\n' << move << '\n';
    return 0;
}