#include "board.h"
#include "bitboard.h"
#include "evaluate.h"
#include "search.h"
#include "mailbox.h"
#include "uci.h"
#include "zobrist.h"
#include "perft.h"
#include <iostream>
#include <vector>

int main() {
    // uciLoop();
    BitBoard board;
    // board.setStartPosition();
    board.setPositionFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");

    std::cout << perft(board, 1) << std::endl;
    std::cout << perft(board, 2) << std::endl;
    std::cout << perft(board, 3) << std::endl;
    std::cout << perft(board, 4) << std::endl;
    // std::cout << perft(board, 5) << std::endl;


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