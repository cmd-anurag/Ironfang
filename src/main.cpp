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
    uciLoop();
    // BitBoard board;
    // board.setPositionFromFEN("r1bq1rk1/5ppp/p1pb4/1p1n4/8/1BPP4/PP3PPP/RNBQR1K1 b - - 2 13");
    // Move move = Search::findBestMove(board, MAX_DEPTH, 5000);
    // std::cout << '\n' << move << '\n';
    // return 0;
}