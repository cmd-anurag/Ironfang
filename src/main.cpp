#include "board.h"
#include "evaluate.h"
#include "mailbox.h"
#include "uci.h"
#include <iostream>
#include<vector>


int main() {

    uciLoop();
    
    // Board b;
    // if(b.setPositionFromFEN("2rqr1k1/4bpp1/p2p1n1p/1p6/3QP3/1b4NP/PP3PP1/R1B1R1K1 w - - 0 20")) {
    //     std::cout << "Loaded Successfully\n";
    // }
    // else {
    //     std::cout << "invalid fen";
    //     return 0;
    // }
    
    // b.print();

    // int evaluation = Evaluation::evaluate(b);
    // std::cout << evaluation << "\n";

    // std::vector<Move> moves = b.generateMoves();
    // std::vector<Move> legalMoves;

    
    // for(auto move : moves) {
    //     std::cout << moveToAlgrebraic(move) << " ";
    //     Gamestate prevState = {
    //         b.sideToMove, 
    //         b.enPassantSquare,
    //         b.whiteKingsideCastle,
    //         b.whiteQueensideCastle,
    //         b.blackKingsideCastle,
    //         b.blackQueensideCastle,
    //         b.whiteKingSquare,
    //         b.blackKingSquare
    //     };

    //     if(b.makeMove(move)) {
    //         legalMoves.push_back(move);
    //         b.unmakeMove(move, prevState);
    //     }
    // }
    // std::cout << "\n\nLegal Moves\n";

    // for(auto legal : legalMoves) {
    //     std::cout << moveToAlgrebraic(legal) << " ";
    // }
    // std::cout << '\n';
    // std::cout << moves.size() << " " << legalMoves.size();

}