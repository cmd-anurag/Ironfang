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
    // board.setPositionFromFEN("r2qkb1r/pppb1ppp/2n2n2/1B1Q4/8/2N1P3/PPP2PPP/R1B1K1NR w KQkq - 1 7");
    // Move move = Search::findBestMove(board, 7);
    // std::cout << '\n' << move << '\n';
    // std::vector<Move> moves = board.generateMoves();
    // std::cout << moves.size() << "\n";
}