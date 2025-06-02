#include "board.h"
#include "evaluate.h"
#include "search.h"
#include "mailbox.h"
#include "uci.h"
#include "zobrist.h"
#include <iostream>
#include <vector>


int main() {

    // uciLoop();
    Board b;
    b.setPositionFromFEN("7k/4R3/3p1r2/4p2p/4P3/1Q3N2/4KPq1/8 b - - 3 45");
    Search::findBestMove(b, 7);
    // std::cout << '\n' << bestMove << "\n";
    
    // Board b;
    // for(int i = 0; i < 64; ++i) {
    //     b.setPiece(NONE, i);
    // }

    // b.setPiece(WP, 28);
    // b.setPiece(BP, 13);
    // b.sideToMove = BLACK;
    // b.zobristKey = generateZobristHashKey(b);

    // Move push = uciToMove("f7f5", b);
    // b.makeMove(push);
    // assert(b.enPassantSquare == 21);
    // uint64_t computed = generateZobristHashKey(b);
    // assert(computed == b.zobristKey);
    // std::cout << "Double push test passed succesfully";

    // Move enPassant = uciToMove("e5f6", b);
    // if(b.makeMove(enPassant)) {
    //     uint64_t computed = generateZobristHashKey(b);
    //     assert(computed == b.zobristKey);
    //     std::cout << "\nAll tests passed";
    // } else {
    //     std::cout << "illegal";
    // }
    
    // Move bestMove = Search::findBestMove(b, 6);
    // std::cout << '\n' << bestMove << '\n';
}