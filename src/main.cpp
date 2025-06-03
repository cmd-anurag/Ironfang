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
    // board.setPositionFromFEN("r1bqkb1r/ppp1pppp/2n2n2/3p4/3P4/2NQ4/PPP1PPPP/R1B1KBNR w KQkq - 3 4");
    // // board.print();
    // // std::cout << Evaluation::evaluate(board);
    // Move move = Search::findBestMove(board, 7, -1);
    // std::cout << '\n' << move << '\n';
    // std::vector<Move> moves = board.generateMoves();
    // std::cout << moves.size() << "\n";
}