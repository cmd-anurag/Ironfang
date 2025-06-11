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
    // board.setStartPosition();
    // Move move = Search::findBestMove(board, MAX_DEPTH, 5000);
    // std::cout << moveToAlgrebraic(move);

    // board.setPositionFromFEN("r2q1k2/ppp3pp/4b1n1/8/8/P1P5/2P3PP/4R1K1 b - - 1 22");
    // std::vector<Move> moves = board.generateMoves();
    // for(Move &move : moves) {
    //     std::cout << moveToAlgrebraic(move) << " ";
    // }
   
    // std::cout << '\n' << move << '\n';
    // return 0;
}