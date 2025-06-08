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
    // board.setPositionFromFEN("r2qk2r/ppp1nBpp/2np4/4p3/4P1b1/2P2N2/PP1P1PPP/R1BQ1RK1 b kq - 0 9");
    // std::vector<Move> captures = board.generateCaptures();
    // for(Move &move : captures) {
    //     std::cout << moveToAlgrebraic(move) << " ";
    // }
    // Move move = Search::findBestMove(board, MAX_DEPTH, 5000);
    // std::cout << '\n' << move << '\n';
    // return 0;
}